# MetaDrive シナリオファイル作成詳細

## 概要

MetaDriveシミュレーションを記録し、シナリオファイルとして保存・再生する方法を詳しく説明します。

---

## シナリオファイルの作成手順

### ステップ1: シミュレーション実行と記録

MetaDriveEnvでシミュレーションを実行し、データを記録します。

```python
from metadrive import MetaDriveEnv
import numpy as np

# 記録モード有効化は不要（export_scenarios()で取得可能）
config = {
    "map": 7,
    "traffic_density": 0.2,
    "num_scenarios": 1,
    "start_seed": 100,
}

env = MetaDriveEnv(config)

# エピソード実行
obs, info = env.reset()
print("エピソード開始")

for step in range(500):
    # ランダムアクション
    action = env.action_space.sample()
    
    obs, reward, terminated, truncated, info = env.step(action)
    
    if terminated or truncated:
        print(f"エピソード終了: Step {step}")
        break

env.close()
```

---

### ステップ2: シナリオデータのエクスポート

MetaDriveは内部で全フレームデータを保持しているため、`export_scenarios()`でエクスポートできます。

```python
from metadrive import MetaDriveEnv
from metadrive.scenario.utils import convert_recorded_scenario_exported
import pickle

config = {
    "map": 4,
    "traffic_density": 0.3,
    "num_scenarios": 1,
}

env = MetaDriveEnv(config)
obs, info = env.reset()

# エピソード実行
for _ in range(300):
    obs, reward, terminated, truncated, info = env.step([0, 1])
    if terminated or truncated:
        break

# シナリオデータ取得
try:
    record_data = env.export_scenarios()
    print("記録データキー:", record_data.keys())
    # dict_keys(['map_data', 'scenario_index', 'global_seed', 'frame', 'manager_metadata'])
    
    # ScenarioDescription形式に変換
    scenario = convert_recorded_scenario_exported(
        record_data,
        scenario_log_interval=0.1,  # 10Hz
        to_dict=True
    )
    
    print(f"シナリオID: {scenario['id']}")
    print(f"長さ: {scenario['length']} steps")
    print(f"トラック数: {len(scenario['tracks'])}")
    
    # ファイル保存
    with open("my_scenario.pkl", "wb") as f:
        pickle.dump(scenario, f)
    
    print("シナリオファイル保存完了: my_scenario.pkl")
    
except Exception as e:
    print(f"エクスポートエラー: {e}")

env.close()
```

---

### ステップ3: シナリオファイルの構造確認

```python
import pickle
import numpy as np

# シナリオロード
with open("my_scenario.pkl", "rb") as f:
    scenario = pickle.load(f)

# 構造確認
print("=== シナリオ構造 ===")
print(f"ID: {scenario['id']}")
print(f"バージョン: {scenario['version']}")
print(f"長さ: {scenario['length']} steps")
print(f"メタデータ: {scenario['metadata']}")
print(f"トラック数: {len(scenario['tracks'])}")
print(f"マップ要素数: {len(scenario['map_features'])}")

# トラック情報
for track_id, track in list(scenario['tracks'].items())[:3]:
    print(f"\nトラック: {track_id}")
    print(f"  タイプ: {track['type']}")
    print(f"  状態キー: {track['state'].keys()}")
    print(f"  Position shape: {track['state']['position'].shape}")
    print(f"  Velocity shape: {track['state']['velocity'].shape}")

# マップ要素
for map_id, map_feature in list(scenario['map_features'].items())[:3]:
    print(f"\nマップ要素: {map_id}")
    print(f"  タイプ: {map_feature['type']}")
    if 'polyline' in map_feature:
        print(f"  Polyline shape: {map_feature['polyline'].shape}")
```

#### ScenarioDescription構造

```python
scenario = {
    "id": str,                    # シナリオID
    "version": str,               # "MetaDrive v0.3.0.1"
    "length": int,                # ステップ数
    "metadata": {
        "ts": np.ndarray,         # タイムスタンプ [length]
        "metadrive_processed": bool,
        "coordinate": str,        # "metadrive"
        "dataset": str,           # "metadrive" or "nuscenes" or "waymo"
        "sdc_id": str,            # 自車ID
    },
    "tracks": {
        "track_id": {
            "type": str,          # "VEHICLE" or "PEDESTRIAN" or "CYCLIST"
            "state": {
                "position": np.ndarray,  # [length, 3] (x, y, z)
                "heading": np.ndarray,   # [length] (度)
                "velocity": np.ndarray,  # [length, 2] (vx, vy)
                "valid": np.ndarray,     # [length] (bool)
            },
            "metadata": {
                "type": str,
                "track_length": int,
                "object_id": str,
            }
        },
        # ... more tracks
    },
    "map_features": {
        "feature_id": {
            "type": str,          # "LANE_*", "ROAD_LINE_*", etc.
            "polyline": np.ndarray,  # [N, 2] or [N, 3]
        },
        # ... more features
    },
    "dynamic_map_states": {
        # 信号機などの動的マップ情報
    }
}
```

---

### ステップ4: シナリオファイルの再生

```python
from metadrive.envs.scenario_env import ScenarioEnv
import pickle

# シナリオロード
with open("my_scenario.pkl", "rb") as f:
    scenario = pickle.load(f)

# ScenarioEnvで再生
config = {
    "num_scenarios": 1,
    "use_render": True,
    "manual_control": False,
}

env = ScenarioEnv(config)

# シナリオを手動設定
env.engine.data_manager.scenarios = [scenario]

# 再生
obs, info = env.reset()
print("シナリオ再生開始")

for step in range(scenario['length']):
    # ダミーアクション（実際の動きは記録データから再生）
    obs, reward, terminated, truncated, info = env.step([0, 0])
    env.render()
    
    if step % 50 == 0:
        print(f"Step {step}/{scenario['length']}")
    
    if terminated or truncated:
        break

env.close()
```

---

## カスタムシナリオ作成（手動）

完全に手動でScenarioDescriptionを作成することも可能です。

### 基本的なカスタムシナリオ

```python
import numpy as np
from metadrive.scenario.scenario_description import ScenarioDescription
from metadrive.type import MetaDriveType

# カスタムシナリオ作成
scenario = ScenarioDescription()

scenario["id"] = "custom-scenario-001"
scenario["version"] = "MetaDrive v0.3.0.1"
scenario["length"] = 100  # 100ステップ

# メタデータ
scenario["metadata"] = {
    "ts": np.arange(0, 10.0, 0.1, dtype=np.float32),  # 10秒、0.1秒間隔
    "metadrive_processed": True,
    "coordinate": "metadrive",
    "dataset": "custom",
    "sdc_id": "ego",
}

# 自車トラック
scenario["tracks"] = {
    "ego": {
        "type": MetaDriveType.VEHICLE,
        "state": {
            "position": np.zeros((100, 3), dtype=np.float32),
            "heading": np.zeros((100,), dtype=np.float32),
            "velocity": np.zeros((100, 2), dtype=np.float32),
            "valid": np.ones((100,), dtype=bool),
        },
        "metadata": {
            "type": "VEHICLE",
            "track_length": 100,
            "object_id": "ego",
        }
    }
}

# トラジェクトリ設定（直線加速）
for i in range(100):
    scenario["tracks"]["ego"]["state"]["position"][i] = [i * 1.0, 0, 0]  # 1m/step
    scenario["tracks"]["ego"]["state"]["velocity"][i] = [10.0, 0]  # 10m/s
    scenario["tracks"]["ego"]["state"]["heading"][i] = 0.0

# マップ要素（簡易版）
scenario["map_features"] = {
    "lane_1": {
        "type": "LANE_SURFACE_STREET",
        "polyline": np.array([[i, 0] for i in range(0, 200, 2)], dtype=np.float32),
    }
}

scenario["dynamic_map_states"] = {}

# 保存
import pickle
with open("custom_scenario.pkl", "wb") as f:
    pickle.dump(scenario, f)

print("カスタムシナリオ作成完了")
```

---

### 複雑なカスタムシナリオ（交差点）

```python
def create_intersection_scenario():
    """交差点での右折シナリオ"""
    
    scenario = ScenarioDescription()
    scenario["id"] = "intersection-right-turn-001"
    scenario["version"] = "MetaDrive v0.3.0.1"
    scenario["length"] = 200
    
    scenario["metadata"] = {
        "ts": np.arange(0, 20.0, 0.1, dtype=np.float32),
        "metadrive_processed": True,
        "coordinate": "metadrive",
        "dataset": "custom",
        "sdc_id": "ego",
    }
    
    scenario["tracks"] = {}
    
    # 自車（右折）
    scenario["tracks"]["ego"] = {
        "type": MetaDriveType.VEHICLE,
        "state": {
            "position": np.zeros((200, 3), dtype=np.float32),
            "heading": np.zeros((200,), dtype=np.float32),
            "velocity": np.zeros((200, 2), dtype=np.float32),
            "valid": np.ones((200,), dtype=bool),
        },
        "metadata": {"type": "VEHICLE", "track_length": 200, "object_id": "ego"}
    }
    
    # 自車のトラジェクトリ（直進→減速→右折→加速）
    for i in range(200):
        t = i * 0.1
        
        if t < 5.0:
            # 直進 (0-5秒)
            x = 10.0 * t
            y = 0
            heading = 0
            speed = 10.0
        elif t < 8.0:
            # 減速・停止 (5-8秒)
            t_brake = t - 5.0
            x = 50.0 + 10.0 * t_brake - 3.0 * t_brake**2
            y = 0
            heading = 0
            speed = 10.0 - 6.0 * t_brake
        elif t < 12.0:
            # 右折 (8-12秒)
            t_turn = t - 8.0
            theta = (t_turn / 4.0) * 90.0  # 90度回転
            radius = 5.0
            
            x = 50.0 + radius * np.sin(np.radians(theta))
            y = radius * (1 - np.cos(np.radians(theta)))
            heading = theta
            speed = 5.0
        else:
            # 加速 (12-20秒)
            t_accel = t - 12.0
            x = 55.0
            y = 5.0 + 5.0 * t_accel + 1.0 * t_accel**2
            heading = 90
            speed = 5.0 + 2.0 * t_accel
        
        scenario["tracks"]["ego"]["state"]["position"][i] = [x, y, 0]
        scenario["tracks"]["ego"]["state"]["heading"][i] = heading
        scenario["tracks"]["ego"]["state"]["velocity"][i] = [
            speed * np.cos(np.radians(heading)),
            speed * np.sin(np.radians(heading))
        ]
    
    # 対向車（直進）
    scenario["tracks"]["oncoming_vehicle"] = {
        "type": MetaDriveType.VEHICLE,
        "state": {
            "position": np.zeros((200, 3), dtype=np.float32),
            "heading": np.zeros((200,), dtype=np.float32),
            "velocity": np.zeros((200, 2), dtype=np.float32),
            "valid": np.ones((200,), dtype=bool),
        },
        "metadata": {"type": "VEHICLE", "track_length": 200, "object_id": "oncoming"}
    }
    
    for i in range(200):
        t = i * 0.1
        # 対向車は定速直進
        x = 100.0 - 12.0 * t
        y = 3.5  # 反対車線
        scenario["tracks"]["oncoming_vehicle"]["state"]["position"][i] = [x, y, 0]
        scenario["tracks"]["oncoming_vehicle"]["state"]["heading"][i] = 180.0
        scenario["tracks"]["oncoming_vehicle"]["state"]["velocity"][i] = [-12.0, 0]
    
    # マップ（交差点）
    scenario["map_features"] = {
        "lane_ns_1": {
            "type": "LANE_SURFACE_STREET",
            "polyline": np.array([[0, i] for i in range(-50, 50, 2)], dtype=np.float32),
        },
        "lane_ns_2": {
            "type": "LANE_SURFACE_STREET",
            "polyline": np.array([[3.5, i] for i in range(-50, 50, 2)], dtype=np.float32),
        },
        "lane_ew_1": {
            "type": "LANE_SURFACE_STREET",
            "polyline": np.array([[i, 0] for i in range(-50, 100, 2)], dtype=np.float32),
        },
        "lane_ew_2": {
            "type": "LANE_SURFACE_STREET",
            "polyline": np.array([[i, 3.5] for i in range(-50, 100, 2)], dtype=np.float32),
        },
    }
    
    scenario["dynamic_map_states"] = {}
    
    return scenario

# シナリオ作成
scenario = create_intersection_scenario()

# 保存
with open("intersection_scenario.pkl", "wb") as f:
    pickle.dump(scenario, f)

print("交差点シナリオ作成完了")
```

---

## シナリオの編集

既存のシナリオを読み込んで編集することもできます。

```python
import pickle
import numpy as np

# シナリオロード
with open("my_scenario.pkl", "rb") as f:
    scenario = pickle.load(f)

# トラック追加
new_track_id = "additional_vehicle"
scenario["tracks"][new_track_id] = {
    "type": "VEHICLE",
    "state": {
        "position": np.zeros((scenario["length"], 3), dtype=np.float32),
        "heading": np.zeros((scenario["length"],), dtype=np.float32),
        "velocity": np.zeros((scenario["length"], 2), dtype=np.float32),
        "valid": np.ones((scenario["length"],), dtype=bool),
    },
    "metadata": {
        "type": "VEHICLE",
        "track_length": scenario["length"],
        "object_id": new_track_id
    }
}

# トラジェクトリ設定
for i in range(scenario["length"]):
    t = i * 0.1
    scenario["tracks"][new_track_id]["state"]["position"][i] = [20.0 + 15.0 * t, -3.5, 0]
    scenario["tracks"][new_track_id]["state"]["velocity"][i] = [15.0, 0]

# 保存
with open("my_scenario_modified.pkl", "wb") as f:
    pickle.dump(scenario, f)

print("シナリオ編集完了")
```

---

## バッチシナリオ生成

複数のシナリオを自動生成する例です。

```python
import pickle
import numpy as np
from metadrive.scenario.scenario_description import ScenarioDescription
from metadrive.type import MetaDriveType

def generate_scenario_batch(num_scenarios=10):
    """複数のシナリオを生成"""
    scenarios = []
    
    for idx in range(num_scenarios):
        scenario = ScenarioDescription()
        scenario["id"] = f"batch-scenario-{idx:03d}"
        scenario["version"] = "MetaDrive v0.3.0.1"
        scenario["length"] = 100
        
        scenario["metadata"] = {
            "ts": np.arange(0, 10.0, 0.1, dtype=np.float32),
            "metadrive_processed": True,
            "coordinate": "metadrive",
            "dataset": "custom",
            "sdc_id": "ego",
        }
        
        # ランダムパラメータ
        initial_speed = np.random.uniform(10, 20)
        acceleration = np.random.uniform(-2, 2)
        
        scenario["tracks"] = {
            "ego": {
                "type": MetaDriveType.VEHICLE,
                "state": {
                    "position": np.zeros((100, 3), dtype=np.float32),
                    "heading": np.zeros((100,), dtype=np.float32),
                    "velocity": np.zeros((100, 2), dtype=np.float32),
                    "valid": np.ones((100,), dtype=bool),
                },
                "metadata": {"type": "VEHICLE", "track_length": 100, "object_id": "ego"}
            }
        }
        
        # トラジェクトリ
        for i in range(100):
            t = i * 0.1
            speed = initial_speed + acceleration * t
            speed = max(0, speed)  # 負の速度を防ぐ
            
            if i == 0:
                x = 0
            else:
                prev_speed = scenario["tracks"]["ego"]["state"]["velocity"][i-1, 0]
                x = scenario["tracks"]["ego"]["state"]["position"][i-1, 0] + (speed + prev_speed) / 2 * 0.1
            
            scenario["tracks"]["ego"]["state"]["position"][i] = [x, 0, 0]
            scenario["tracks"]["ego"]["state"]["velocity"][i] = [speed, 0]
        
        scenario["map_features"] = {
            "lane_1": {
                "type": "LANE_SURFACE_STREET",
                "polyline": np.array([[i, 0] for i in range(0, 200, 2)], dtype=np.float32),
            }
        }
        scenario["dynamic_map_states"] = {}
        
        scenarios.append(scenario)
    
    return scenarios

# バッチ生成
scenarios = generate_scenario_batch(num_scenarios=10)

# 各シナリオを保存
for idx, scenario in enumerate(scenarios):
    filename = f"scenario_batch_{idx:03d}.pkl"
    with open(filename, "wb") as f:
        pickle.dump(scenario, f)
    print(f"保存: {filename}")

print(f"\n{len(scenarios)}個のシナリオを生成")
```

---

## nuScenes/Waymoデータの使用

実世界のデータセットからシナリオを読み込む方法です。

### nuScenesデータの使用

```python
from metadrive.envs.scenario_env import ScenarioEnv
from metadrive.engine.asset_loader import AssetLoader

config = {
    "data_directory": AssetLoader.file_path("nuscenes"),
    "num_scenarios": 10,
    "reactive_traffic": False,  # 元データをそのまま再生
}

env = ScenarioEnv(config)

# ランダムにシナリオを選択して再生
for episode in range(5):
    obs, info = env.reset()
    print(f"\nエピソード {episode + 1}")
    print(f"シナリオID: {env.engine.current_seed}")
    
    for step in range(200):
        obs, reward, terminated, truncated, info = env.step([0, 0])
        env.render()
        
        if terminated or truncated:
            break

env.close()
```

### Waymoデータの使用

```python
config = {
    "data_directory": AssetLoader.file_path("waymo"),
    "num_scenarios": 20,
    "reactive_traffic": True,  # IDMで反応的に動作
}

env = ScenarioEnv(config)
```

---

## まとめ

本ドキュメントでは、MetaDriveシナリオファイルの作成・編集・再生方法を説明しました：

1. **シナリオ記録**: `export_scenarios()` でシミュレーションを記録
2. **シナリオ再生**: ScenarioEnvで再生
3. **カスタムシナリオ**: 手動でScenarioDescriptionを作成
4. **シナリオ編集**: 既存シナリオにトラックを追加
5. **バッチ生成**: 複数のシナリオを自動生成
6. **実世界データ**: nuScenes/Waymoデータの使用

これらの技術を使うことで、複雑な交通シナリオを作成・再現できます。

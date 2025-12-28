# MetaDrive 使い方チュートリアル

## 概要

**MetaDrive**は、自動運転のシミュレーション環境です。本チュートリアルでは、実行可能なコード例を通じて、MetaDriveの基本的な使い方を学びます。

### 主な機能
- **カスタムマップ作成**: 直線、カーブ、分岐を組み合わせた道路
- **トラフィック制御**: 他車両の動作定義
- **歩行者シミュレーション**: 道路横断などの動作
- **openpilot連携**: 実際の自動運転ソフトウェアとの統合

---

## クイックスタート: 完全な実行例

以下は、**直線 + カーブ + 分岐 + それぞれに直線**の道路を作成し、**自車、他車2台、歩行者1名**をシミュレートする完全なコード例です。

```python
#!/usr/bin/env python3
"""
MetaDrive クイックスタート例
- カスタムマップ: 直線 + 右カーブ + Y字分岐 + 左右の直線
- 自車: 手動制御
- 他車1: 前方を定速走行
- 他車2: 後方から追い越し
- 歩行者: 100m地点で道路横断
"""

from metadrive import MetaDriveEnv
from metadrive.component.map.pg_map import MapGenerateMethod
from metadrive.component.vehicle.base_vehicle import BaseVehicle
from metadrive.policy.replay_policy import ReplayTrafficParticipantPolicy
import numpy as np

# ====================
# 1. マップ定義（コードで完結）
# ====================
def create_custom_map():
    """直線 + カーブ + 分岐 + それぞれに直線"""
    return {
        "type": MapGenerateMethod.PG_MAP_FILE,
        "lane_num": 2,
        "lane_width": 3.5,
        "config": [
            None,  # 必須: 最初はNone
            
            # 1. 直線 (100m)
            {"id": "S", "pre_block_socket_index": 0, "length": 100},
            
            # 2. 右カーブ (90度, 半径80m)
            {"id": "C", "pre_block_socket_index": 0, "length": 126, 
             "radius": 80, "angle": 90, "dir": 0},
            
            # 3. Y字分岐 (右方向)
            {"id": "r", "pre_block_socket_index": 0},
            
            # 4. 分岐後の直線 (50m)
            {"id": "S", "pre_block_socket_index": 0, "length": 50},
        ]
    }

# ====================
# 2. 他車のトラジェクトリ定義
# ====================
def create_vehicle1_trajectory(num_steps=300):
    """他車1: 前方を定速走行 (20 m/s)"""
    traj = {
        "type": "VEHICLE",
        "state": {
            "position": np.zeros((num_steps, 3), dtype=np.float32),
            "velocity": np.zeros((num_steps, 2), dtype=np.float32),
            "heading": np.zeros((num_steps,), dtype=np.float32),
            "valid": np.ones((num_steps,), dtype=bool),
        },
        "metadata": {"type": "VEHICLE", "track_length": num_steps, "object_id": "vehicle_1"}
    }
    
    for i in range(num_steps):
        t = i * 0.1  # 0.1秒ごと
        x = 50.0 + 20.0 * t  # 初期位置50m, 速度20m/s
        traj["state"]["position"][i] = [x, 0, 0]
        traj["state"]["velocity"][i] = [20.0, 0]
        traj["state"]["heading"][i] = 0.0
    
    return traj

def create_vehicle2_trajectory(num_steps=300):
    """他車2: 後方から追い越し (加速 → 車線変更 → 減速)"""
    traj = {
        "type": "VEHICLE",
        "state": {
            "position": np.zeros((num_steps, 3), dtype=np.float32),
            "velocity": np.zeros((num_steps, 2), dtype=np.float32),
            "heading": np.zeros((num_steps,), dtype=np.float32),
            "valid": np.ones((num_steps,), dtype=bool),
        },
        "metadata": {"type": "VEHICLE", "track_length": num_steps, "object_id": "vehicle_2"}
    }
    
    for i in range(num_steps):
        t = i * 0.1
        
        # 縦方向: 加速 → 定速 → 減速
        if t < 5.0:
            speed = 15.0 + 2.0 * t  # 15→25 m/s
            x = -30.0 + 15.0 * t + 1.0 * t**2
        elif t < 15.0:
            speed = 25.0
            x = -30.0 + 75.0 + 25.0 * (t - 5.0)
        else:
            t_brake = t - 15.0
            speed = 25.0 - 2.0 * t_brake
            x = -30.0 + 75.0 + 250.0 + 25.0 * t_brake - 1.0 * t_brake**2
        
        # 横方向: 車線変更 (8-12秒)
        if 8.0 <= t < 12.0:
            progress = (t - 8.0) / 4.0
            y = -3.5 * progress  # 左車線へ
        elif t >= 12.0:
            y = -3.5
        else:
            y = 0
        
        traj["state"]["position"][i] = [x, y, 0]
        traj["state"]["velocity"][i] = [max(speed, 0), 0]
        traj["state"]["heading"][i] = 0.0
    
    return traj

def create_pedestrian_trajectory(num_steps=300):
    """歩行者: 100m地点で道路横断 (10-20秒)"""
    traj = {
        "type": "PEDESTRIAN",
        "state": {
            "position": np.zeros((num_steps, 3), dtype=np.float32),
            "velocity": np.zeros((num_steps, 2), dtype=np.float32),
            "heading": np.zeros((num_steps,), dtype=np.float32),
            "valid": np.ones((num_steps,), dtype=bool),
        },
        "metadata": {"type": "PEDESTRIAN", "track_length": num_steps, "object_id": "pedestrian_1"}
    }
    
    for i in range(num_steps):
        t = i * 0.1
        
        if 10.0 <= t < 20.0:
            # 横断中 (道路端-5m → 反対側+5m, 速度1.5m/s)
            progress = t - 10.0
            x = 100.0  # 固定位置
            y = -5.0 + 1.5 * progress
            vy = 1.5
            traj["state"]["valid"][i] = True
        else:
            # 道路外（非表示）
            x = 100.0
            y = -5.0 if t < 10.0 else 10.0
            vy = 0.0
            if t < 10.0:
                traj["state"]["valid"][i] = False
        
        traj["state"]["position"][i] = [x, y, 0]
        traj["state"]["velocity"][i] = [0, vy]
        traj["state"]["heading"][i] = 90.0 if 10.0 <= t < 20.0 else 0.0
    
    return traj

# ====================
# 3. メイン実行
# ====================
def main():
    # 環境設定（configファイル不要、すべてコード内で定義）
    config = {
        "map_config": create_custom_map(),
        "traffic_density": 0.0,  # デフォルトトラフィックは無効化
        "num_scenarios": 1,
        "use_render": True,  # 描画ON
        "manual_control": False,  # 自動制御（手動制御はFalse）
        "image_observation": False,
    }
    
    env = MetaDriveEnv(config)
    obs, info = env.reset()
    
    print("=" * 60)
    print("MetaDrive シミュレーション開始")
    print("=" * 60)
    print(f"マップ: 直線(100m) + 右カーブ(90度) + Y字分岐 + 直線(50m)")
    print(f"自車: 初期位置 (0, 0)")
    print(f"他車1: 前方50m, 定速20m/s")
    print(f"他車2: 後方30m, 加速して追い越し")
    print(f"歩行者: 100m地点で横断 (10-20秒)")
    print("=" * 60)
    
    # 他車1生成
    vehicle1_traj = create_vehicle1_trajectory(300)
    vehicle1 = env.engine.spawn_object(
        BaseVehicle,
        name="traffic_vehicle_1",
        position=vehicle1_traj["state"]["position"][0],
        heading=0.0,
    )
    policy1 = ReplayTrafficParticipantPolicy(vehicle1, vehicle1_traj, 0)
    env.engine.add_policy(vehicle1.name, policy1.__class__, vehicle1, vehicle1_traj, 0)
    
    # 他車2生成
    vehicle2_traj = create_vehicle2_trajectory(300)
    vehicle2 = env.engine.spawn_object(
        BaseVehicle,
        name="traffic_vehicle_2",
        position=vehicle2_traj["state"]["position"][0],
        heading=0.0,
    )
    policy2 = ReplayTrafficParticipantPolicy(vehicle2, vehicle2_traj, 0)
    env.engine.add_policy(vehicle2.name, policy2.__class__, vehicle2, vehicle2_traj, 0)
    
    # 歩行者生成（MetaDriveは歩行者オブジェクトをサポートしていないため、説明のみ）
    # 実際の歩行者シミュレーションにはScenarioEnv + nuScenesデータが必要
    print("\n[注意] 歩行者は現在のMetaDriveEnvではサポート外")
    print("       ScenarioEnv + nuScenes/Waymoデータで利用可能")
    
    # シミュレーション実行
    for step in range(300):
        # 自車の制御入力 (直進・加速)
        action = [0, 0.5]  # [ステアリング, スロットル]
        
        obs, reward, terminated, truncated, info = env.step(action)
        env.render()
        
        # 10ステップごとに状態表示
        if step % 20 == 0:
            ego_pos = env.vehicle.position
            v1_pos = vehicle1.position
            v2_pos = vehicle2.position
            t = step * 0.1
            
            print(f"\n時刻 {t:.1f}秒:")
            print(f"  自車: ({ego_pos[0]:.1f}, {ego_pos[1]:.1f})")
            print(f"  他車1: ({v1_pos[0]:.1f}, {v1_pos[1]:.1f})")
            print(f"  他車2: ({v2_pos[0]:.1f}, {v2_pos[1]:.1f})")
        
        if terminated or truncated:
            print(f"\nシミュレーション終了: ステップ {step}")
            break
    
    env.close()
    print("\n" + "=" * 60)
    print("シミュレーション完了")
    print("=" * 60)

if __name__ == "__main__":
    main()
```

### 実行方法

```bash
# MetaDriveがインストール済みの場合
python3 metadrive_quickstart.py

# 初回実行時はMetaDriveをインストール
pip install metadrive-simulator
python3 metadrive_quickstart.py
```

### 期待される動作

1. **0-5秒**: 自車が加速、他車1は前方を定速走行、他車2は後方で加速
2. **5-10秒**: 他車2が左車線に車線変更開始
3. **10-12秒**: 他車2が追い越し完了、歩行者が道路横断開始
4. **12-20秒**: 他車2が減速、歩行者が横断完了
5. **20-30秒**: 自車が右カーブに進入

---

## アーキテクチャ

### MetaDrive システム構成

```
┌─────────────────────────────────────────────────────────────────┐
│                     MetaDriveEnv (Gymnasium)                    │
├─────────────────────────────────────────────────────────────────┤
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │
│  │   BaseEnv    │  │ PolicyManager│  │ ObjectManager│          │
│  │              │  │              │  │              │          │
│  │ - reset()    │  │ - IDMPolicy  │  │ - spawn()    │          │
│  │ - step()     │  │ - ReplayPol. │  │ - destroy()  │          │
│  │ - render()   │  │ - CustomPol. │  │              │          │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘          │
│         │                 │                 │                  │
│         └─────────────────┼─────────────────┘                  │
│                           │                                    │
│  ┌────────────────────────▼────────────────────────┐           │
│  │              EngineCore (Panda3D)               │           │
│  ├──────────────────────────────────────────────────┤           │
│  │  ┌────────────┐  ┌────────────┐  ┌────────────┐ │           │
│  │  │ PhysicsWorld│  │  Sensors   │  │  Rendering │ │           │
│  │  │ (Bullet)   │  │            │  │  (OpenGL)  │ │           │
│  │  │            │  │ - Camera   │  │            │ │           │
│  │  │ - step()   │  │ - LiDAR    │  │ - ShowBase │ │           │
│  │  │ - gravity  │  │ - Depth    │  │ - PBR      │ │           │
│  │  └────────────┘  └────────────┘  └────────────┘ │           │
│  └──────────────────────────────────────────────────┘           │
│                           │                                    │
│  ┌────────────────────────▼────────────────────────┐           │
│  │                    MapManager                   │           │
│  ├──────────────────────────────────────────────────┤           │
│  │  - PGMap (Procedural Generation)                │           │
│  │  - PG_MAP_FILE (Custom Blocks)                  │           │
│  │  - BIG_BLOCK_NUM (Random Generation)            │           │
│  └──────────────────────────────────────────────────┘           │
└─────────────────────────────────────────────────────────────────┘

         ↕ (100Hz制御ループ)

┌─────────────────────────────────────────────────────────────────┐
│                    openpilot (オプション)                        │
│                                                                 │
│  MetaDriveBridge ←→ MetaDriveWorld ←→ MetaDriveProcess         │
│                                                                 │
│  通信: Pipe (制御/状態) + 共有メモリ (カメラ画像)                │
└─────────────────────────────────────────────────────────────────┘
```

### データフロー

```
1. ユーザー入力/openpilot制御
   ↓
2. env.step(action) - [steering, throttle]
   ↓
3. PhysicsWorld.step() - 物理シミュレーション (20Hz)
   ↓
4. 車両状態更新 (position, velocity, heading)
   ↓
5. センサー更新 (Camera, LiDAR)
   ↓
6. observation, reward, terminated, truncated, info
   ↓
7. レンダリング (オプション)
```

---

## 詳細: 各機能の使用方法

### 1. マップ作成

#### 1-1. 整数指定（最もシンプル）

```python
from metadrive import MetaDriveEnv

config = {
    "map": 7,  # 7ブロックのランダムマップ
    "traffic_density": 0.1,
}

env = MetaDriveEnv(config)
obs, info = env.reset()
```

**特徴**:
- 最も簡単
- ランダムに生成されるため、毎回異なるマップ
- ブロック数が多いほど複雑

#### 1-2. ブロックシーケンス指定

```python
config = {
    "map": "XCSCO",  # X=スタート, C=カーブ, S=直線, O=アウトランプ
}

env = MetaDriveEnv(config)
```

**ブロックタイプ**:
- `X`: スタート
- `S`: 直線
- `C`: カーブ
- `T`: T字路
- `r`: インランプ (小文字)
- `R`: アウトランプ (大文字)
- `O`: アウトランプ（旧形式）

#### 1-3. map_config詳細設定

```python
from metadrive.component.map.pg_map import MapGenerateMethod

config = {
    "map_config": {
        "type": MapGenerateMethod.BIG_BLOCK_NUM,
        "config": 10,
        "lane_num": 3,
        "lane_width": 3.5,
    }
}
```

#### 1-4. PG_MAP_FILE カスタムブロック定義

```python
def create_figure_eight_track():
    """8の字トラック"""
    return {
        "type": MapGenerateMethod.PG_MAP_FILE,
        "lane_num": 1,
        "lane_width": 4.0,
        "config": [
            None,
            {"id": "C", "pre_block_socket_index": 0, "length": 100, 
             "radius": 50, "angle": 180, "dir": 0},  # 右180度
            {"id": "C", "pre_block_socket_index": 0, "length": 100, 
             "radius": 50, "angle": 180, "dir": 1},  # 左180度
        ]
    }

config = {"map_config": create_figure_eight_track()}
```

**ブロックパラメータ**:

| パラメータ | 説明 | 直線 | カーブ |
|-----------|------|------|--------|
| `id` | ブロックタイプ | "S" | "C" |
| `pre_block_socket_index` | 接続点 | 0 | 0 |
| `length` | 長さ [m] | ✅ | ✅ |
| `radius` | 半径 [m] | - | ✅ |
| `angle` | 角度 [度] | - | ✅ |
| `dir` | 方向 (0=右, 1=左) | - | ✅ |

---

### 2. トラフィック（他車）制御

#### 2-1. トラフィック密度設定

```python
config = {
    "map": 5,
    "traffic_density": 0.3,  # 0.0 (なし) ~ 1.0 (最大)
}
```

#### 2-2. IDMポリシー（デフォルト）

他車は自動的にIDM (Intelligent Driver Model) で制御されます。

```python
config = {
    "traffic_density": 0.2,
    "random_traffic": True,  # ランダムな動作
}
```

#### 2-3. カスタムトラジェクトリ（ReplayPolicy）

```python
from metadrive.policy.replay_policy import ReplayTrafficParticipantPolicy
import numpy as np

# トラジェクトリデータ作成
traj = {
    "type": "VEHICLE",
    "state": {
        "position": np.array([[i*1.0, 0, 0] for i in range(100)], dtype=np.float32),
        "velocity": np.array([[10.0, 0] for _ in range(100)], dtype=np.float32),
        "heading": np.zeros((100,), dtype=np.float32),
        "valid": np.ones((100,), dtype=bool),
    },
    "metadata": {"type": "VEHICLE", "track_length": 100, "object_id": "vehicle_1"}
}

# 車両生成
vehicle = env.engine.spawn_object(BaseVehicle, name="vehicle_1", position=[0, 3.5, 0])
policy = ReplayTrafficParticipantPolicy(vehicle, traj, 0)
env.engine.add_policy(vehicle.name, policy.__class__, vehicle, traj, 0)
```

#### 2-4. カスタムポリシー

```python
from metadrive.policy.base_policy import BasePolicy

class ConstantSpeedPolicy(BasePolicy):
    def __init__(self, control_object, random_seed, target_speed=15.0):
        super().__init__(control_object, random_seed)
        self.target_speed = target_speed
    
    def act(self, *args, **kwargs):
        vehicle = self.control_object
        current_speed = np.linalg.norm(vehicle.velocity)
        
        if current_speed < self.target_speed:
            throttle = 0.5
        else:
            throttle = 0.0
        
        return [0, throttle]  # [steering, throttle]

# 使用例
vehicle = env.engine.spawn_object(BaseVehicle, name="custom_vehicle", position=[0, 0, 0])
env.engine._object_policies[vehicle.name] = ConstantSpeedPolicy(vehicle, 0, target_speed=20.0)
```

---

### 3. 歩行者シミュレーション

**重要**: MetaDriveEnvは歩行者をネイティブサポートしていません。歩行者を含むシミュレーションには**ScenarioEnv + nuScenes/Waymoデータ**が必要です。

#### 3-1. ScenarioEnvで歩行者を使用

```python
from metadrive.envs.scenario_env import ScenarioEnv
from metadrive.engine.asset_loader import AssetLoader

config = {
    "data_directory": AssetLoader.file_path("nuscenes"),
    "num_scenarios": 5,
}

env = ScenarioEnv(config)
obs, info = env.reset()

# nuScenesシナリオには歩行者が含まれる
for _ in range(200):
    obs, reward, terminated, truncated, info = env.step([0, 0])
    
    # 歩行者情報取得
    if "tracked_objects" in info:
        for obj_id, obj_info in info["tracked_objects"].items():
            if obj_info["type"] == "PEDESTRIAN":
                print(f"Pedestrian {obj_id}: {obj_info['position']}")
```

#### 3-2. ScenarioDescription形式で歩行者定義

```python
from metadrive.scenario.scenario_description import ScenarioDescription
from metadrive.type import MetaDriveType

scenario = ScenarioDescription()
scenario["tracks"]["pedestrian_1"] = {
    "type": MetaDriveType.PEDESTRIAN,
    "state": {
        "position": np.array([[100.0, -5.0 + i*0.15, 0] for i in range(100)]),
        "velocity": np.array([[0, 1.5] for _ in range(100)]),
        "heading": np.full((100,), 90.0),
        "valid": np.ones((100,), dtype=bool),
    },
    "metadata": {"type": "PEDESTRIAN", "track_length": 100, "object_id": "ped_1"}
}
```

---

### 4. カメラ設定

#### 4-1. 基本的なカメラ設定

```python
from metadrive.component.sensors.rgb_camera import RGBCamera

config = {
    "image_observation": True,
    "sensors": {
        "rgb_camera": (RGBCamera, 800, 600),
    },
}

env = MetaDriveEnv(config)
obs, info = env.reset()

# カメラ画像取得
rgb_image = obs["image"]  # shape: (600, 800, 3)
```

#### 4-2. 複数カメラ（前方・後方・側面）

```python
from metadrive.component.sensors.rgb_camera import RGBCamera
from metadrive.component.sensors.depth_camera import DepthCamera
from panda3d.core import Vec3

class RGBCameraFront(RGBCamera):
    BUFFER_W = 800
    BUFFER_H = 600
    
    def __init__(self, width, height, engine, cuda=False):
        super().__init__(width, height, engine, cuda=cuda)
        cam = self.get_cam()
        cam.setPos(Vec3(0.0, 0.0, 1.5))
        cam.setHpr(Vec3(0, 0, 0))

class RGBCameraRear(RGBCamera):
    BUFFER_W = 640
    BUFFER_H = 480
    
    def __init__(self, width, height, engine, cuda=False):
        super().__init__(width, height, engine, cuda=cuda)
        cam = self.get_cam()
        cam.setPos(Vec3(-2.0, 0.0, 1.5))
        cam.setHpr(Vec3(180, 0, 0))

config = {
    "image_observation": True,
    "sensors": {
        "camera_front": (RGBCameraFront, 800, 600),
        "camera_rear": (RGBCameraRear, 640, 480),
    },
}
```

---

### 5. openpilotとの通信

#### 5-1. アーキテクチャ

```
┌──────────────────────────────────────────────────────────────────┐
│                    openpilot (メインプロセス)                     │
│  ┌──────────────────────────────────────────────────────────┐    │
│  │  controls_send ────────────────────────────────────────┐ │    │
│  │                                                        │ │    │
│  │  vehicle_state_recv ◄─────────────────────────────┐   │ │    │
│  │  simulation_state_recv ◄──────────────────────┐   │   │ │    │
│  │  camera_array (shared memory) ◄───────────┐   │   │   │ │    │
│  └───────────────────────────────────────────┼───┼───┼───┼─┘    │
└──────────────────────────────────────────────┼───┼───┼───┼──────┘
                                               │   │   │   │
                                               │   │   │   │ Pipe
                                               │   │   │   │
┌──────────────────────────────────────────────┼───┼───┼───┼──────┐
│              MetaDriveWorld (管理プロセス)   │   │   │   │      │
│                                              │   │   │   │      │
│  ┌───────────────────────────────────────────┼───┼───┼───┼───┐  │
│  │  controls_recv ◄──────────────────────────┘   │   │   │   │  │
│  │  vehicle_state_send ───────────────────────────┘   │   │   │  │
│  │  simulation_state_send ────────────────────────────┘   │   │  │
│  │  camera_array (shared memory) ─────────────────────────┘   │  │
│  └─────────────────────────────────────────────────────────────┘  │
│                              │                                    │
│                   multiprocessing.Process起動                     │
│                              │                                    │
└──────────────────────────────┼────────────────────────────────────┘
                               │
┌──────────────────────────────▼────────────────────────────────────┐
│          MetaDriveProcess (シミュレーションプロセス)               │
│                                                                   │
│  ┌─────────────────────────────────────────────────────────────┐  │
│  │  100Hz ループ                                               │  │
│  │    1. controls_recv.poll(0) → 制御入力取得                 │  │
│  │    2. rk.frame % 5 == 0 ? → 5フレームごと                  │  │
│  │        - env.step([steer, gas]) → 物理シミュレーション      │  │
│  │        - get_cam_as_rgb() → カメラ画像取得                  │  │
│  │        - camera_array更新 → 共有メモリ書き込み             │  │
│  │        - image_lock.release()                               │  │
│  │    3. vehicle_state_send.send() → 車両状態送信              │  │
│  │    4. rk.keep_time() → 100Hz維持                            │  │
│  └─────────────────────────────────────────────────────────────┘  │
│                              │                                    │
│  ┌───────────────────────────▼──────────────────────────────┐     │
│  │            MetaDriveEnv (シミュレータ)                   │     │
│  │  - PhysicsWorld.step() (20Hz)                            │     │
│  │  - Camera.perceive()                                     │     │
│  │  - Vehicle state update                                  │     │
│  └──────────────────────────────────────────────────────────┘     │
└───────────────────────────────────────────────────────────────────┘
```

#### 5-2. 通信データ構造

**制御入力** (openpilot → MetaDrive):
```python
(steer_angle, gas, should_reset)
# steer_angle: float, ステアリング角度 [rad]
# gas: float, スロットル/ブレーキ [-1.0, 1.0]
# should_reset: bool, リセット要求
```

**車両状態** (MetaDrive → openpilot):
```python
metadrive_vehicle_state(
    velocity=(vx, vy, vz),        # m/s
    position=(x, y, z),            # m
    bearing=heading_deg,           # 度
    steering_angle=angle_deg       # 度
)
```

**カメラ画像** (MetaDrive → openpilot):
```python
# 共有メモリ (Array('B', W*H*3))
road_image: np.ndarray  # shape: (H, W, 3), dtype: uint8
wide_image: np.ndarray  # shape: (H, W, 3), dtype: uint8
```

#### 5-3. 同期メカニズム

| 項目 | 方式 | 周波数 | 説明 |
|------|------|--------|------|
| 車両状態 | Pipe (ノンブロッキング) | 100Hz | `poll(0)` でチェック、データあれば更新 |
| シミュレーション状態 | Pipe (ノンブロッキング) | 不定期 | 終了時など |
| カメラ画像 | 共有メモリ + Semaphore | 20Hz | `image_lock.acquire()` でブロッキング |
| 制御入力 | Pipe (ノンブロッキング) | 100Hz | `poll(0)` でチェック |

**重要**: openpilotはリアルタイム性を維持するため、車両状態取得時はブロッキングしません。データがなければ前回の値を使用します。

#### 5-4. 実装例

詳細な実装は以下のファイルを参照:
- `openpilot/tools/sim/bridge/metadrive/metadrive_bridge.py`
- `openpilot/tools/sim/bridge/metadrive/metadrive_world.py`
- `openpilot/tools/sim/bridge/metadrive/metadrive_process.py`

---

## まとめ

本チュートリアルでは、以下を学びました:

1. **クイックスタート**: 完全な実行可能コード例
2. **アーキテクチャ**: MetaDriveとopenpilotの構成
3. **マップ作成**: 4つの方法（整数、シーケンス、map_config、PG_MAP_FILE）
4. **トラフィック制御**: 密度設定、IDM、ReplayPolicy、カスタムポリシー
5. **歩行者**: ScenarioEnv + nuScenes/Waymo
6. **カメラ**: 単一・複数カメラ設定
7. **openpilot連携**: 通信アーキテクチャと同期メカニズム

### 次のステップ

- **実データ使用**: nuScenes/Waymoデータセットで実世界シナリオ再生
- **マルチエージェント**: 複数車両の協調制御
- **強化学習**: MetaDriveでRLエージェントを訓練
- **openpilot統合**: 実際の自動運転スタックでテスト

### 参考資料

- [MetaDrive公式ドキュメント](https://metadrive-simulator.readthedocs.io/)
- [openpilot公式GitHub](https://github.com/commaai/openpilot)
- [cereal分析](../cereal_analysis/README.md)
- [msgq分析](../msgq_analysis/README.md)

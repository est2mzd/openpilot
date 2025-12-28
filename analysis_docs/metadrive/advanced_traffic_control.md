# MetaDrive 高度なトラフィック制御

## 概要

MetaDriveでは、他車両や歩行者の動作を詳細に制御できます。本ドキュメントでは、時系列トラジェクトリ制御やカスタムポリシーなど、高度な制御方法を説明します。

---

## 時系列トラジェクトリ制御（CAEソフト的アプローチ）

**一般的なCAEソフトウェアのような、時系列で経路に沿った縦横方向の動きを定義する方法**

MetaDriveでは、以下の3つの方法で時系列トラジェクトリを制御できます：

### 方法1: ReplayPolicyで時系列トラジェクトリ再生

事前に記録された時系列位置・速度データを再生する方法です。

```python
from metadrive import MetaDriveEnv
from metadrive.policy.replay_policy import ReplayTrafficParticipantPolicy
from metadrive.component.vehicle.base_vehicle import BaseVehicle
import numpy as np

def create_trajectory_data(num_steps=200):
    """
    CAEソフト的な時系列トラジェクトリ定義
    
    縦方向: 加速・減速プロファイル
    横方向: 車線変更動作
    """
    trajectory = {
        "type": "VEHICLE",
        "state": {
            "position": np.zeros((num_steps, 3), dtype=np.float32),
            "velocity": np.zeros((num_steps, 2), dtype=np.float32),
            "heading": np.zeros((num_steps,), dtype=np.float32),
            "valid": np.ones((num_steps,), dtype=bool),
        },
        "metadata": {
            "type": "VEHICLE",
            "track_length": num_steps,
            "object_id": "traffic_vehicle_1",
        }
    }
    
    # 時系列で動きを定義
    for i in range(num_steps):
        t = i * 0.1  # 時間 [秒] (10Hz)
        
        # 縦方向制御（加速→定速→減速）
        if t < 5.0:
            # 加速フェーズ (0-5秒): 0 → 20 m/s
            speed = 4.0 * t
            x = 2.0 * t**2
        elif t < 15.0:
            # 定速フェーズ (5-15秒): 20 m/s
            speed = 20.0
            x = 50.0 + 20.0 * (t - 5.0)
        else:
            # 減速フェーズ (15-20秒): 20 → 0 m/s
            t_brake = t - 15.0
            speed = 20.0 - 4.0 * t_brake
            x = 250.0 + 20.0 * t_brake - 2.0 * t_brake**2
        
        # 横方向制御（車線変更）
        if 8.0 < t < 12.0:
            # 車線変更 (8-12秒): 0 → 3.5m (車線幅分移動)
            t_lc = t - 8.0
            y = 3.5 * (t_lc / 4.0)  # 線形補間
        else:
            y = 3.5 if t >= 12.0 else 0.0
        
        # 位置・速度設定
        trajectory["state"]["position"][i] = [x, y, 0]
        trajectory["state"]["velocity"][i] = [speed, 0]
        trajectory["state"]["heading"][i] = 0.0
    
    return trajectory

# 使用例
config = {
    "map": 7,
    "traffic_density": 0.0,  # デフォルトトラフィック無効
    "num_scenarios": 1,
}

env = MetaDriveEnv(config)
obs, info = env.reset()

# トラフィック車両を手動生成
trajectory_data = create_trajectory_data(num_steps=200)

# 車両生成
traffic_vehicle = env.engine.spawn_object(
    BaseVehicle,
    name="traffic_vehicle_1",
    position=trajectory_data["state"]["position"][0],
    heading=0.0,
)

# ReplayPolicyを設定
replay_policy = ReplayTrafficParticipantPolicy(
    control_object=traffic_vehicle,
    track=trajectory_data,
    random_seed=0
)
env.engine.add_policy(
    traffic_vehicle.name, 
    replay_policy.__class__, 
    traffic_vehicle, 
    trajectory_data, 
    0
)

# シミュレーション実行
print("時系列トラジェクトリ再生開始")
for step in range(200):
    obs, reward, terminated, truncated, info = env.step([0, 1])
    env.render()
    
    if step % 20 == 0:
        pos = traffic_vehicle.position
        vel = np.linalg.norm(traffic_vehicle.velocity)
        print(f"Step {step}: Position=({pos[0]:.1f}, {pos[1]:.1f}), Speed={vel:.1f} m/s")

env.close()
```

---

### 方法2: ScenarioDescriptionで直接時系列定義

ScenarioDescription形式で、より詳細に時系列動作を定義できます。

```python
from metadrive.envs.scenario_env import ScenarioEnv
from metadrive.scenario.scenario_description import ScenarioDescription
from metadrive.type import MetaDriveType
import numpy as np
import pickle

def create_custom_scenario_with_traffic():
    """
    カスタムシナリオ作成（複数車両の時系列動作定義）
    """
    num_steps = 300  # 30秒 @ 10Hz
    
    scenario = ScenarioDescription()
    scenario["id"] = "custom-traffic-scenario-001"
    scenario["version"] = "MetaDrive v0.3.0.1"
    scenario["length"] = num_steps
    
    # メタデータ
    scenario["metadata"] = {
        "ts": np.arange(0, 30.0, 0.1, dtype=np.float32),
        "metadrive_processed": True,
        "coordinate": "metadrive",
        "dataset": "custom",
        "sdc_id": "ego",
    }
    
    scenario["tracks"] = {}
    
    # 自車（Ego Vehicle）
    scenario["tracks"]["ego"] = {
        "type": MetaDriveType.VEHICLE,
        "state": {
            "position": np.zeros((num_steps, 3), dtype=np.float32),
            "heading": np.zeros((num_steps,), dtype=np.float32),
            "velocity": np.zeros((num_steps, 2), dtype=np.float32),
            "valid": np.ones((num_steps,), dtype=bool),
        },
        "metadata": {"type": "VEHICLE", "track_length": num_steps, "object_id": "ego"}
    }
    
    # 自車の動き（定速直進）
    for i in range(num_steps):
        t = i * 0.1
        scenario["tracks"]["ego"]["state"]["position"][i] = [20.0 * t, 0, 0]
        scenario["tracks"]["ego"]["state"]["velocity"][i] = [20.0, 0]
    
    # トラフィック車両1（前方を走行）
    scenario["tracks"]["vehicle_1"] = {
        "type": MetaDriveType.VEHICLE,
        "state": {
            "position": np.zeros((num_steps, 3), dtype=np.float32),
            "heading": np.zeros((num_steps,), dtype=np.float32),
            "velocity": np.zeros((num_steps, 2), dtype=np.float32),
            "valid": np.ones((num_steps,), dtype=bool),
        },
        "metadata": {"type": "VEHICLE", "track_length": num_steps, "object_id": "vehicle_1"}
    }
    
    # 車両1の動き（加減速パターン）
    for i in range(num_steps):
        t = i * 0.1
        
        # 縦方向：サイン波状の加減速
        base_speed = 18.0
        speed_variation = 5.0 * np.sin(2 * np.pi * t / 10.0)
        speed = base_speed + speed_variation
        
        # 位置積分
        if i == 0:
            x = 50.0  # 初期位置（自車の前方50m）
        else:
            prev_speed = scenario["tracks"]["vehicle_1"]["state"]["velocity"][i-1, 0]
            x = scenario["tracks"]["vehicle_1"]["state"]["position"][i-1, 0] + (speed + prev_speed) / 2 * 0.1
        
        scenario["tracks"]["vehicle_1"]["state"]["position"][i] = [x, 0, 0]
        scenario["tracks"]["vehicle_1"]["state"]["velocity"][i] = [speed, 0]
    
    # トラフィック車両2（隣車線を追い越し）
    scenario["tracks"]["vehicle_2"] = {
        "type": MetaDriveType.VEHICLE,
        "state": {
            "position": np.zeros((num_steps, 3), dtype=np.float32),
            "heading": np.zeros((num_steps,), dtype=np.float32),
            "velocity": np.zeros((num_steps, 2), dtype=np.float32),
            "valid": np.ones((num_steps,), dtype=bool),
        },
        "metadata": {"type": "VEHICLE", "track_length": num_steps, "object_id": "vehicle_2"}
    }
    
    # 車両2の動き（車線変更を含む追い越し）
    for i in range(num_steps):
        t = i * 0.1
        
        # 縦方向：追い越し加速
        if t < 10.0:
            speed = 22.0  # 高速で接近
            x = 30.0 + 22.0 * t
        else:
            speed = 20.0  # 追い越し後は同速
            x = 250.0 + 20.0 * (t - 10.0)
        
        # 横方向：車線変更（10-15秒で左車線へ、20-25秒で元車線へ）
        if 10.0 <= t < 15.0:
            # 左車線へ移動
            progress = (t - 10.0) / 5.0
            y = -3.5 * progress
        elif 15.0 <= t < 20.0:
            y = -3.5  # 左車線を維持
        elif 20.0 <= t < 25.0:
            # 元車線へ戻る
            progress = (t - 20.0) / 5.0
            y = -3.5 * (1.0 - progress)
        else:
            y = 0.0
        
        scenario["tracks"]["vehicle_2"]["state"]["position"][i] = [x, y, 0]
        scenario["tracks"]["vehicle_2"]["state"]["velocity"][i] = [speed, 0]
    
    # 歩行者（横断）
    scenario["tracks"]["pedestrian_1"] = {
        "type": MetaDriveType.PEDESTRIAN,
        "state": {
            "position": np.zeros((num_steps, 3), dtype=np.float32),
            "heading": np.zeros((num_steps,), dtype=np.float32),
            "velocity": np.zeros((num_steps, 2), dtype=np.float32),
            "valid": np.ones((num_steps,), dtype=bool),
        },
        "metadata": {"type": "PEDESTRIAN", "track_length": num_steps, "object_id": "pedestrian_1"}
    }
    
    # 歩行者の動き（道路横断: 15-25秒）
    for i in range(num_steps):
        t = i * 0.1
        
        if 15.0 <= t < 25.0:
            # 横断中（1.5 m/s で横断）
            progress = (t - 15.0)
            y = -5.0 + 1.5 * progress  # 道路端(-5m)から横断
            x = 300.0  # 固定位置で横断
            vy = 1.5
            scenario["tracks"]["pedestrian_1"]["state"]["valid"][i] = True
        else:
            # 道路外（非表示）
            x, y = 300.0, -5.0
            vy = 0.0
            if t < 15.0:
                scenario["tracks"]["pedestrian_1"]["state"]["valid"][i] = False
        
        scenario["tracks"]["pedestrian_1"]["state"]["position"][i] = [x, y, 0]
        scenario["tracks"]["pedestrian_1"]["state"]["velocity"][i] = [0, vy]
    
    # 簡易マップ
    scenario["map_features"] = {
        "lane_1": {
            "type": "LANE_SURFACE_STREET",
            "polyline": np.array([[i, 0] for i in range(0, 800, 5)], dtype=np.float32),
        },
        "lane_2": {
            "type": "LANE_SURFACE_STREET",
            "polyline": np.array([[i, -3.5] for i in range(0, 800, 5)], dtype=np.float32),
        }
    }
    
    scenario["dynamic_map_states"] = {}
    
    return scenario

# シナリオ作成と保存
scenario = create_custom_scenario_with_traffic()
with open("traffic_scenario.pkl", "wb") as f:
    pickle.dump(scenario, f)

print(f"シナリオ作成完了: {scenario['id']}")
print(f"車両数: {len([k for k, v in scenario['tracks'].items() if v['type'] == 'VEHICLE'])}")
print(f"歩行者数: {len([k for k, v in scenario['tracks'].items() if v['type'] == 'PEDESTRIAN'])}")

# シナリオ再生
config = {
    "num_scenarios": 1,
    "use_render": True,
    "reactive_traffic": False,  # 時系列データをそのまま再生
}

env = ScenarioEnv(config)
env.engine.data_manager.scenarios = [scenario]

obs, info = env.reset()
print("\n時系列シナリオ再生開始")

for step in range(scenario["length"]):
    obs, reward, terminated, truncated, info = env.step([0, 0])
    env.render()
    
    # 各車両の状態確認
    if step % 50 == 0:
        t = step * 0.1
        print(f"\n時刻 {t:.1f}秒:")
        for track_id in ["ego", "vehicle_1", "vehicle_2", "pedestrian_1"]:
            pos = scenario["tracks"][track_id]["state"]["position"][step]
            valid = scenario["tracks"][track_id]["state"]["valid"][step]
            if valid:
                print(f"  {track_id}: ({pos[0]:.1f}, {pos[1]:.1f})")

env.close()
```

---

### 方法3: カスタムポリシーで制御入力の時系列定義

制御入力（ステアリング、スロットル/ブレーキ）を時系列で定義する方法です。

```python
from metadrive.policy.base_policy import BasePolicy
import numpy as np

class TimeSeriesControlPolicy(BasePolicy):
    """
    時系列制御入力ポリシー
    
    CAEソフト的に、時刻ごとの縦横制御入力を定義
    """
    
    def __init__(self, control_object, control_sequence, random_seed=None):
        """
        Args:
            control_object: 制御対象車両
            control_sequence: 制御入力の時系列データ
                [{
                    "time": 0.0,
                    "steering": 0.0,      # -1 ~ 1
                    "throttle": 0.5,      # 0 ~ 1
                }, ...]
        """
        super().__init__(control_object, random_seed)
        self.control_sequence = control_sequence
        self.dt = 0.1  # タイムステップ [秒]
    
    def act(self, *args, **kwargs):
        """
        現在時刻に対応する制御入力を返す
        """
        current_time = self.episode_step * self.dt
        
        # 線形補間で現在の制御入力を取得
        steering, throttle = self._interpolate_control(current_time)
        
        return [steering, throttle]
    
    def _interpolate_control(self, t):
        """時刻tにおける制御入力を線形補間"""
        
        # 最初の時刻より前
        if t <= self.control_sequence[0]["time"]:
            return self.control_sequence[0]["steering"], self.control_sequence[0]["throttle"]
        
        # 最後の時刻より後
        if t >= self.control_sequence[-1]["time"]:
            return self.control_sequence[-1]["steering"], self.control_sequence[-1]["throttle"]
        
        # 線形補間
        for i in range(len(self.control_sequence) - 1):
            t1 = self.control_sequence[i]["time"]
            t2 = self.control_sequence[i + 1]["time"]
            
            if t1 <= t <= t2:
                # 補間係数
                alpha = (t - t1) / (t2 - t1)
                
                # ステアリング補間
                s1 = self.control_sequence[i]["steering"]
                s2 = self.control_sequence[i + 1]["steering"]
                steering = s1 + alpha * (s2 - s1)
                
                # スロットル補間
                th1 = self.control_sequence[i]["throttle"]
                th2 = self.control_sequence[i + 1]["throttle"]
                throttle = th1 + alpha * (th2 - th1)
                
                return steering, throttle
        
        return 0.0, 0.0

# 制御入力シーケンス定義（CAEソフト的）
control_sequence = [
    # 時刻, ステアリング, スロットル
    {"time": 0.0, "steering": 0.0, "throttle": 0.5},   # 直進加速
    {"time": 3.0, "steering": 0.0, "throttle": 0.8},   # 加速増加
    {"time": 5.0, "steering": 0.0, "throttle": 0.3},   # 定速
    {"time": 8.0, "steering": 0.2, "throttle": 0.3},   # 右旋回開始
    {"time": 12.0, "steering": 0.0, "throttle": 0.3},  # 直進復帰
    {"time": 15.0, "steering": -0.3, "throttle": 0.2}, # 左旋回+減速
    {"time": 18.0, "steering": 0.0, "throttle": 0.2},  # 直進
    {"time": 20.0, "steering": 0.0, "throttle": 0.0},  # 停止
]

# 環境設定
config = {
    "map": 7,
    "traffic_density": 0.0,
    "num_scenarios": 1,
}

env = MetaDriveEnv(config)
obs, info = env.reset()

# トラフィック車両生成
traffic_vehicle = env.engine.spawn_object(
    BaseVehicle,
    name="controlled_vehicle",
    position=[20.0, 3.5, 0],  # 隣車線
    heading=0.0,
)

# 時系列制御ポリシー設定
policy = TimeSeriesControlPolicy(
    control_object=traffic_vehicle,
    control_sequence=control_sequence,
    random_seed=0
)
env.engine._object_policies[traffic_vehicle.name] = policy

# シミュレーション実行
print("時系列制御シーケンス実行開始")
for step in range(250):
    # 自車は手動制御、トラフィック車両は時系列制御
    ego_action = [0, 0.5]
    
    # トラフィック車両の制御実行
    traffic_action = policy.act()
    traffic_vehicle.before_step(traffic_action)
    
    obs, reward, terminated, truncated, info = env.step(ego_action)
    env.render()
    
    # 状態確認
    if step % 25 == 0:
        t = step * 0.1
        pos = traffic_vehicle.position
        vel = np.linalg.norm(traffic_vehicle.velocity)
        steering = traffic_vehicle.steering
        print(f"時刻 {t:.1f}s: Pos=({pos[0]:.1f},{pos[1]:.1f}), Speed={vel:.1f}m/s, Steering={steering:.2f}")

env.close()
```

---

## 時系列制御のまとめ

| 方法 | 用途 | 特徴 | CAEソフトとの対応 |
|------|------|------|------------------|
| **ReplayPolicy** | 実データ再生 | 位置・速度を直接設定 | ○ 位置の時系列データ |
| **ScenarioDescription** | 複雑なシナリオ | 複数車両の協調動作 | ◎ 完全な時系列定義 |
| **TimeSeriesControlPolicy** | 制御入力定義 | ステアリング・スロットルの時系列 | ○ 制御入力の時系列 |
| **TrajectoryIDMPolicy** | 経路追従+IDM | 経路指定で自動制御 | △ 経路のみ定義 |

**推奨用途**:
- **完全な再現性**: ScenarioDescription（位置・速度を全時刻で定義）
- **実データ再生**: ReplayPolicy（nuScenes/Waymoデータ）
- **制御入力重視**: TimeSeriesControlPolicy（ステアリング・スロットル指定）
- **経路ベース**: TrajectoryIDMPolicy（経路に沿って自動運転）

---

## マルチエージェント環境

複数の制御可能な車両を動かす場合は、マルチエージェント環境を使用します。

```python
from metadrive.envs.marl_envs.multi_agent_metadrive import MultiAgentMetaDrive

config = {
    "num_agents": 4,  # 制御可能な車両数
    "map": "SSSS",
    "traffic_density": 0.1,
    "agent_configs": {
        "agent0": {"spawn_longitude": 0},
        "agent1": {"spawn_longitude": 10},
        "agent2": {"spawn_longitude": 20},
        "agent3": {"spawn_longitude": 30},
    }
}

env = MultiAgentMetaDrive(config)
obs_dict, info = env.reset()

# 各エージェントの行動を指定
for _ in range(500):
    actions = {
        "agent0": [0.0, 1.0],
        "agent1": [0.1, 0.8],
        "agent2": [-0.1, 0.9],
        "agent3": [0.0, 0.7],
    }
    obs_dict, reward_dict, terminated_dict, truncated_dict, info_dict = env.step(actions)
    env.render()

env.close()
```

---

## トラフィックモード詳細

### TrafficMode の種類

| モード | 説明 | 用途 | 特徴 |
|--------|------|------|------|
| `TrafficMode.Basic` | 一度だけ生成 | 短いエピソード | メモリ効率的 |
| `TrafficMode.Respawn` | 目的地到達後に再生成 | 長いエピソード、ループトラック | 永続的なトラフィック |
| `TrafficMode.Trigger` | 自車が近づくと生成 | メモリ効率重視 | 動的生成 |
| `TrafficMode.Hybrid` | 混合モード | 複雑なシナリオ | 柔軟性高い |

### 使用例

```python
from metadrive.manager.traffic_manager import TrafficMode

config = {
    "map": 7,
    "traffic_density": 0.2,
    "traffic_mode": TrafficMode.Respawn,  # リスポーンモード
    "random_traffic": True,  # ランダムな動作
}

env = MetaDriveEnv(config)
obs, info = env.reset()

# 長時間実行してもトラフィックが維持される
for _ in range(5000):
    obs, reward, terminated, truncated, info = env.step([0, 1])
    env.render()

env.close()
```

---

## カスタムポリシーの高度な例

### 車線維持ポリシー

```python
from metadrive.policy.base_policy import BasePolicy
import numpy as np

class LaneKeepingPolicy(BasePolicy):
    """車線中央を維持しながら走行"""
    
    def __init__(self, control_object, random_seed, target_speed=15.0):
        super().__init__(control_object, random_seed)
        self.target_speed = target_speed
    
    def act(self, *args, **kwargs):
        vehicle = self.control_object
        
        # 現在の車線情報取得
        lane_idx, lane_info, on_lane = vehicle.navigation._get_current_lane(vehicle)
        
        # 速度制御
        current_speed = np.linalg.norm(vehicle.velocity)
        if current_speed < self.target_speed:
            throttle = 0.5
        else:
            throttle = 0.0
        
        # ステアリング制御（車線中央維持）
        if on_lane and lane_info is not None:
            # 車線中央からの横方向オフセット
            lateral_offset = vehicle.position[1] - lane_info[0][1]
            
            # PD制御
            kp = 0.5
            kd = 0.1
            steering = -kp * lateral_offset - kd * vehicle.velocity[1]
            steering = np.clip(steering, -1, 1)
        else:
            steering = 0.0
        
        return [steering, throttle]
```

### 追従走行ポリシー

```python
class FollowingPolicy(BasePolicy):
    """前方車両を追従"""
    
    def __init__(self, control_object, random_seed, target_distance=20.0):
        super().__init__(control_object, random_seed)
        self.target_distance = target_distance
    
    def act(self, *args, **kwargs):
        vehicle = self.control_object
        
        # 前方車両検出
        front_vehicle = None
        min_distance = float('inf')
        
        for obj_id, obj in vehicle.engine.get_objects().items():
            if obj_id != vehicle.name and isinstance(obj, BaseVehicle):
                # 前方の車両のみチェック
                dx = obj.position[0] - vehicle.position[0]
                dy = abs(obj.position[1] - vehicle.position[1])
                
                if dx > 0 and dy < 2.0:  # 同じ車線
                    distance = np.linalg.norm(obj.position - vehicle.position)
                    if distance < min_distance:
                        min_distance = distance
                        front_vehicle = obj
        
        # 速度制御（車間距離維持）
        if front_vehicle is not None:
            distance_error = min_distance - self.target_distance
            
            # PD制御
            kp = 0.05
            kd = 0.1
            
            front_speed = np.linalg.norm(front_vehicle.velocity)
            current_speed = np.linalg.norm(vehicle.velocity)
            speed_error = front_speed - current_speed
            
            throttle = kp * distance_error + kd * speed_error
            throttle = np.clip(throttle, -1, 1)
        else:
            # 前方に車両がいない場合は定速走行
            current_speed = np.linalg.norm(vehicle.velocity)
            if current_speed < 20.0:
                throttle = 0.5
            else:
                throttle = 0.0
        
        return [0, throttle]
```

---

## まとめ

本ドキュメントでは、MetaDriveでの高度なトラフィック制御方法を説明しました：

1. **時系列トラジェクトリ制御**: ReplayPolicy、ScenarioDescription、TimeSeriesControlPolicy
2. **マルチエージェント環境**: 複数車両の協調制御
3. **トラフィックモード**: Basic、Respawn、Trigger、Hybrid
4. **カスタムポリシー**: 車線維持、追従走行

これらの技術を組み合わせることで、複雑な交通シナリオをシミュレートできます。

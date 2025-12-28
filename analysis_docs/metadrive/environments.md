# Environments - 環境定義

## 概要

MetaDriveの**environments**は、強化学習タスクを定義するGym互換環境です。

- **ベース**: `BaseEnv`（全環境の基底クラス）
- **主要環境**: MetaDrive、SafeMetaDrive、Scenario、MultiAgent
- **互換性**: Gymnasium API（Gym v0.26+）

---

## 環境階層

```
BaseEnv (base_env.py)
  ├── MetaDriveEnv (metadrive_env.py)
  │     ├── SafeMetaDriveEnv (safe_metadrive_env.py)
  │     └── TopDownEnv (top_down_env.py)
  │
  ├── ScenarioEnv (scenario_env.py)
  │     └── 実世界データ（nuScenes/Waymo）
  │
  └── MultiAgentMetaDrive (multi_agent_metadrive.py)
        ├── MultiAgentRoundaboutEnv
        ├── MultiAgentIntersectionEnv
        ├── MultiAgentTollgateEnv
        ├── MultiAgentBottleneckEnv
        └── MultiAgentParkingLotEnv
```

---

## 1. BaseEnv - 基底クラス

**ファイル**: `metadrive/envs/base_env.py`（937行）

### 主要機能

**Gymnasium互換**:
```python
class BaseEnv(gym.Env):
    def reset(self, seed=None) -> Tuple[obs, info]:
        """環境リセット"""
        
    def step(self, action) -> Tuple[obs, reward, terminated, truncated, info]:
        """1ステップ実行"""
        
    def render(self):
        """レンダリング"""
        
    def close(self):
        """環境クローズ"""
```

---

### 設定項目

```python
BASE_DEFAULT_CONFIG = dict(
    # ===== Agent =====
    random_agent_model=False,
    num_agents=1,
    is_multi_agent=False,
    allow_respawn=False,
    
    # ===== Action/Control =====
    agent_policy=EnvInputPolicy,  # 制御ポリシー
    manual_control=False,         # 手動制御
    discrete_action=False,        # 離散アクション
    discrete_steering_dim=5,      # ステア離散化数
    discrete_throttle_dim=5,      # スロットル離散化数
    
    # ===== Observation =====
    norm_pixel=True,              # ピクセル正規化（0-1）
    stack_size=3,                 # フレームスタック数
    image_observation=False,      # 画像観測（False=LiDAR）
    
    # ===== Termination =====
    horizon=None,                 # エピソード最大長
    truncate_as_terminate=False,  # Truncateを終了扱い
    
    # ===== Camera =====
    camera_height=2.2,
    camera_dist=7.5,
    camera_smooth=True,
    
    # ===== Rendering =====
    use_render=False,             # 3D描画
    offscreen_render=False,       # オフスクリーンレンダリング
)
```

---

### 主要メソッド

#### `reset(seed=None)`

環境リセット

```python
env = MetaDriveEnv()
obs, info = env.reset(seed=42)

# obs: 観測（LiDARまたは画像）
# info: {"agent_id": vehicle, "map": map_info, ...}
```

---

#### `step(action)`

1ステップ実行

```python
action = [steering, throttle]  # [-1, 1] × 2
obs, reward, terminated, truncated, info = env.step(action)

# terminated: エピソード終了（成功/失敗）
# truncated: タイムアウト
# info: {
#     "cost": コスト,
#     "crash_vehicle": 車両衝突,
#     "out_of_road": 道路外,
#     "arrive_dest": ゴール到達,
#     ...
# }
```

---

#### `render(text=None)`

レンダリング

```python
env.render(text={
    "speed": vehicle.speed,
    "reward": reward,
})
```

**モード**:
- `use_render=True`: 3Dウィンドウ
- トップダウン: 2D俯瞰図

---

## 2. MetaDriveEnv - 基本環境

**ファイル**: `metadrive/envs/metadrive_env.py`（323行）

### 概要

**ランダムマップ生成**による汎用運転環境:

- プロシージャル生成マップ
- トラフィック車両
- ゴール到達タスク

---

### 設定

```python
METADRIVE_DEFAULT_CONFIG = dict(
    # ===== Generalization =====
    start_seed=0,           # 開始シード
    num_scenarios=1,        # シナリオ数
    
    # ===== Map =====
    map=3,                  # マップ（int: ブロック数、str: シーケンス）
    random_lane_width=False,
    random_lane_num=False,
    map_config={
        "LANE_WIDTH": 3.5,
        "LANE_NUM": 3,
        "exit_length": 50,
    },
    
    # ===== Traffic =====
    traffic_density=0.1,    # トラフィック密度
    traffic_mode="Trigger", # "Trigger" or "Respawn"
    random_traffic=False,   # ランダムトラフィック
    
    # ===== Object =====
    accident_prob=0.0,      # 事故確率
    
    # ===== Reward =====
    success_reward=10.0,
    out_of_road_penalty=5.0,
    crash_vehicle_penalty=5.0,
    driving_reward=1.0,
    speed_reward=0.1,
    
    # ===== Termination =====
    out_of_road_done=True,
    crash_vehicle_done=True,
    horizon=1000,
)
```

---

### 使用例

```python
from metadrive import MetaDriveEnv

env = MetaDriveEnv(config={
    "num_scenarios": 100,
    "traffic_density": 0.1,
    "map": 5,  # 5ブロックマップ
    "use_render": True,
})

obs, info = env.reset()

for step in range(1000):
    action = env.action_space.sample()  # ランダムアクション
    obs, reward, terminated, truncated, info = env.step(action)
    
    env.render(text={"reward": reward})
    
    if terminated or truncated:
        print(f"Episode ended: {info}")
        break

env.close()
```

---

### マップ設定

**数値指定**（ブロック数）:
```python
config = {"map": 3}  # 3ブロック
```

**文字列指定**（ブロックシーケンス）:
```python
config = {"map": "SCXTCS"}
# S: Straight
# C: Curve
# X: 4-way intersection
# T: T-intersection
# O: Roundabout
```

---

### 報酬関数

```python
def reward_function(self, vehicle_id):
    vehicle = self.agents[vehicle_id]
    
    # 基本報酬
    reward = self.config["driving_reward"]
    
    # 速度報酬
    reward += self.config["speed_reward"] * vehicle.speed
    
    # 成功報酬
    if self._is_arrive_destination(vehicle):
        reward += self.config["success_reward"]
    
    # ペナルティ
    if vehicle.crash_vehicle:
        reward -= self.config["crash_vehicle_penalty"]
    if self._is_out_of_road(vehicle):
        reward -= self.config["out_of_road_penalty"]
    
    return reward
```

---

## 3. SafeMetaDriveEnv - 安全運転環境

**ファイル**: `metadrive/envs/safe_metadrive_env.py`（91行）

### 概要

**コスト関数**を持つ安全運転タスク:

- 複雑な障害物（accident_prob）
- コスト評価（衝突、道路外）
- 安全運転学習

---

### 設定

```python
config = {
    "num_scenarios": 100,
    "accident_prob": 0.8,     # 事故確率（高い）
    "traffic_density": 0.05,
    "crash_vehicle_done": False,  # 衝突でも続行
    "crash_object_done": False,
    "cost_to_reward": False,  # コストを報酬に含めない
}
```

---

### コスト関数

```python
def cost_function(self, vehicle_id):
    vehicle = self.agents[vehicle_id]
    cost = 0
    
    # 衝突コスト
    if vehicle.crash_vehicle:
        cost += self.config["crash_vehicle_cost"]
    if vehicle.crash_object:
        cost += self.config["crash_object_cost"]
    
    # 道路外コスト
    if self._is_out_of_road(vehicle):
        cost += self.config["out_of_road_cost"]
    
    step_info = {
        "cost": cost,
        "total_cost": self.episode_cost + cost,
    }
    
    return cost, step_info
```

---

### 使用例

```python
from metadrive import SafeMetaDriveEnv

env = SafeMetaDriveEnv(config={
    "accident_prob": 0.5,
    "traffic_density": 0.15,
    "use_render": True,
})

obs, info = env.reset()
total_cost = 0

for step in range(1000):
    action = [0, 0.5]  # 直進、軽くアクセル
    obs, reward, terminated, truncated, info = env.step(action)
    
    total_cost += info["cost"]
    
    env.render(text={
        "reward": reward,
        "cost": info["cost"],
        "total_cost": total_cost,
    })
    
    if terminated or truncated:
        break

env.close()
```

---

## 4. ScenarioEnv - 実世界データ環境

**ファイル**: `metadrive/envs/scenario_env.py`

### 概要

**実世界ログ**（nuScenes/Waymo）でのシミュレーション:

- 実際の道路構造
- 実トラフィック
- センサーデータ再現

---

### 設定

```python
config = {
    "data_directory": "/path/to/nuscenes",
    "num_scenarios": 1000,
    "reactive_traffic": True,  # トラフィック反応
    "sequential_seed": True,   # シーケンシャルシナリオ
}
```

---

### 使用例

```python
from metadrive import ScenarioEnv

env = ScenarioEnv(config={
    "data_directory": "/data/nuscenes",
    "num_scenarios": 100,
    "reactive_traffic": True,
    "use_render": True,
})

obs, info = env.reset()

for step in range(1000):
    action = env.action_space.sample()
    obs, reward, terminated, truncated, info = env.step(action)
    
    if terminated or truncated:
        break

env.close()
```

**詳細**: [scenarios.md](scenarios.md)

---

## 5. MultiAgent - マルチエージェント環境

**ディレクトリ**: `metadrive/envs/marl_envs/`

### 主要環境

| 環境 | 説明 | エージェント数 |
|------|------|---------------|
| `MultiAgentRoundaboutEnv` | ラウンドアバウト | 4~40 |
| `MultiAgentIntersectionEnv` | 交差点 | 4~40 |
| `MultiAgentTollgateEnv` | 料金所 | 4~40 |
| `MultiAgentBottleneckEnv` | ボトルネック | 4~40 |
| `MultiAgentParkingLotEnv` | 駐車場 | 4~10 |

---

### 設定

```python
config = {
    "num_agents": 4,          # エージェント数
    "allow_respawn": True,    # リスポーン許可
    "delay_done": 25,         # 終了後の遅延
    "is_multi_agent": True,   # マルチエージェントフラグ
}
```

---

### 使用例

```python
from metadrive import MultiAgentRoundaboutEnv

env = MultiAgentRoundaboutEnv(config={
    "num_agents": 4,
    "use_render": True,
})

obs, info = env.reset()
# obs: {"agent0": obs0, "agent1": obs1, ...}

for step in range(1000):
    # 各エージェントのアクション
    actions = {
        agent_id: env.action_space.sample()
        for agent_id in obs.keys()
    }
    
    obs, rewards, terminateds, truncateds, infos = env.step(actions)
    # rewards: {"agent0": reward0, "agent1": reward1, ...}
    
    # 全エージェント終了
    if all(terminateds.values()):
        break

env.close()
```

---

## 観測空間

### LiDAR観測（デフォルト）

```python
config = {"image_observation": False}

# 観測形状: (240,)
# 240点のLiDARスキャン
# 各点: 最近障害物までの距離（0~50m）
```

**内訳**:
- 前方180度: 180点
- 後方: 60点

---

### 画像観測

```python
config = {
    "image_observation": True,
    "norm_pixel": True,  # 0-1正規化
    "stack_size": 3,     # 3フレームスタック
}

# 観測形状: (84, 84, 9)
# RGB画像 × 3フレーム
```

---

## アクション空間

### 連続アクション

```python
# action: [steering, throttle/brake]
# steering: -1.0 ~ 1.0（左～右）
# throttle/brake: -1.0 ~ 1.0（ブレーキ～アクセル）

env.action_space  # Box(2,)
```

---

### 離散アクション

```python
config = {
    "discrete_action": True,
    "discrete_steering_dim": 5,
    "discrete_throttle_dim": 5,
}

# action: 0~24（5×5グリッド）
env.action_space  # Discrete(25)
```

**マッピング**:
```python
action_idx = 12  # 中央
steering = -1 + (action_idx % 5) * 0.5  # 0.0
throttle = -1 + (action_idx // 5) * 0.5  # 0.0
```

---

## 終了条件

### terminated（エピソード終了）

```python
# 設定
config = {
    "out_of_road_done": True,
    "crash_vehicle_done": True,
    "crash_object_done": True,
}

# info['terminated']の理由:
# - TerminationState.SUCCESS: ゴール到達
# - TerminationState.CRASH_VEHICLE: 車両衝突
# - TerminationState.CRASH_OBJECT: オブジェクト衝突
# - TerminationState.OUT_OF_ROAD: 道路外
```

---

### truncated（タイムアウト）

```python
config = {
    "horizon": 1000,  # 最大1000ステップ
    "truncate_as_terminate": False,
}

# horizon到達でtruncated=True
```

---

## パフォーマンス最適化

### 高速化設定

```python
config = {
    # レンダリング無効
    "use_render": False,
    "offscreen_render": False,
    
    # トラフィック削減
    "traffic_density": 0.05,
    
    # センサー無効
    "show_lidar": False,
    "show_lane_line_detector": False,
    "show_side_detector": False,
    
    # 画像観測無効
    "image_observation": False,
}
```

**FPS**: 1000+

---

## まとめ

環境の選択ガイド:

1. **MetaDriveEnv**: 基本的なRL訓練
2. **SafeMetaDriveEnv**: 安全運転学習（コスト関数）
3. **ScenarioEnv**: 実世界データ評価
4. **MultiAgent**: MARL研究

---

## 関連ドキュメント

- [README.md](README.md) - MetaDrive全体の概要
- [engine.md](engine.md) - シミュレーションエンジン
- [scenarios.md](scenarios.md) - シナリオシステム

# MetaDrive Scenario System 詳細ドキュメント

## 目次
1. [概要](#概要)
2. [ScenarioDescription](#scenariodescription)
3. [ScenarioEnv](#scenarioenv)
4. [データセット統合](#データセット統合)
5. [シナリオ記録と再生](#シナリオ記録と再生)
6. [マネージャーシステム](#マネージャーシステム)
7. [使用例](#使用例)

---

## 概要

MetaDriveのシナリオシステムは、**実世界のドライビングログ**を再現するためのフレームワークです。

### 主要機能

- **データセット統合**: nuScenes, Waymoデータセット対応
- **ScenarioDescription**: 統一シナリオフォーマット
- **シナリオ再生**: 実世界ログの再生と可視化
- **記録機能**: MetaDriveシミュレーション結果の記録
- **カリキュラム学習**: 難易度別シナリオ分割

### 対応データセット

| データセット | 説明 | シナリオ数 | ソース |
|-------------|------|-----------|--------|
| **nuScenes** | 自動運転データセット (Boston/Singapore) | 1000+ | [nuScenes](https://www.nuscenes.org/) |
| **Waymo Open Dataset** | Waymo自動運転車のログデータ | 100,000+ | [Waymo Open](https://waymo.com/open/) |
| **MetaDrive録画** | MetaDriveで記録したシミュレーションログ | カスタム | `env.export_scenarios()` |

### ファイル構成

```
metadrive/scenario/
├── scenario_description.py    # ScenarioDescription データフォーマット
├── utils.py                   # シナリオ変換/可視化ユーティリティ
└── parse_object_state.py      # オブジェクト状態パーサー

metadrive/envs/
└── scenario_env.py            # ScenarioEnv (シナリオ再生環境)

metadrive/manager/
├── scenario_data_manager.py   # データロード
├── scenario_map_manager.py    # マップ管理
├── scenario_traffic_manager.py # トラフィック管理
└── scenario_light_manager.py  # 信号機管理
```

**コード規模**: scenario/ + scenario関連マネージャー **約4,000行** (Python)

---

## ScenarioDescription

**ファイル**: [metadrive/scenario/scenario_description.py](../openpilot/metadrive/metadrive/scenario/scenario_description.py)  
**行数**: 694行

### データフォーマット

ScenarioDescriptionは、**統一シナリオフォーマット**を定義します。nuScenes、Waymo等の異なるデータセットを共通フォーマットに変換。

```python
scenario = {
    # ===== メタデータ =====
    "id": "Waymo-001",                  # シナリオID
    "version": "MetaDrive v0.3.0.1",    # データフォーマットバージョン
    "length": 200,                      # タイムステップ数 (T)
    
    # ===== メタ情報 =====
    "metadata": {
        "ts": np.array([0.0, 0.1, 0.2, ...]),  # タイムスタンプ (shape: (T,))
        "metadrive_processed": True,            # MetaDrive処理済みフラグ
        "coordinate": "metadrive",              # 座標系
        "source_file": "training_20s.tfrecord-00014-of-01000",
        "dataset": "waymo",                     # データソース
        "scenario_id": "dd0c8c27fdd6ef59",      # 元データのID
        "sdc_id": "172",                        # 自車ID (tracks内のキー)
    },
    
    # ===== オブジェクトトラジェクトリ =====
    "tracks": {
        "vehicle1": {
            "type": "VEHICLE",                  # MetaDriveType
            "state": {
                "position": np.ndarray(shape=(200, 3)),   # XYZ座標 (T, 3)
                "heading": np.ndarray(shape=(200,)),      # 方位角 (T,)
                "velocity": np.ndarray(shape=(200, 2)),   # 速度XY (T, 2)
                "valid": np.ndarray(shape=(200,)),        # 有効フラグ (T,)
            },
            "metadata": {
                "type": "VEHICLE",
                "track_length": 200,
                "object_id": "vehicle1",
            }
        },
        "pedestrian1": {...},
    },
    
    # ===== 動的マップ状態 (信号機等) =====
    "dynamic_map_states": {
        "trafficlight1": {
            "type": "TRAFFIC_LIGHT",
            "state": {
                "object_state": np.array([...]),  # 信号状態 (RED/YELLOW/GREEN)
            },
            "metadata": {...},
        }
    },
    
    # ===== マップ要素 =====
    "map_features": {
        "219": {
            "type": "LANE_SURFACE_STREET",      # 車線タイプ
            "polyline": np.array([[x1,y1], [x2,y2], ...]),  # 中心線 (N, 2)
            "polygon": np.array([[x1,y1], ...]),            # 車線ポリゴン (オプション)
        },
        "182": {...},
    }
}
```

### ScenarioDescription クラス

```python
class ScenarioDescription(dict):
    """
    シナリオデータのキー定義
    """
    # トップレベルキー
    TRACKS = "tracks"
    VERSION = "version"
    ID = "id"
    DYNAMIC_MAP_STATES = "dynamic_map_states"
    MAP_FEATURES = "map_features"
    LENGTH = "length"
    METADATA = "metadata"
    
    # 車線キー
    POLYLINE = "polyline"
    POLYGON = "polygon"
    LEFT_BOUNDARIES = "left_boundaries"
    RIGHT_BOUNDARIES = "right_boundaries"
    LEFT_NEIGHBORS = "left_neighbor"
    RIGHT_NEIGHBORS = "right_neighbor"
    ENTRY = "entry_lanes"
    EXIT = "exit_lanes"
    
    # オブジェクトキー
    TYPE = "type"
    STATE = "state"
    OBJECT_ID = "object_id"
```

### MetaDriveType

**ファイル**: [metadrive/type.py](../openpilot/metadrive/metadrive/type.py)

```python
class MetaDriveType:
    """
    オブジェクト/車線タイプ定義
    """
    # ===== オブジェクトタイプ =====
    VEHICLE = "VEHICLE"
    PEDESTRIAN = "PEDESTRIAN"
    CYCLIST = "CYCLIST"
    TRAFFIC_LIGHT = "TRAFFIC_LIGHT"
    TRAFFIC_CONE = "TRAFFIC_CONE"
    TRAFFIC_BARRIER = "TRAFFIC_BARRIER"
    
    # ===== 車線タイプ =====
    LANE_SURFACE_STREET = "LANE_SURFACE_STREET"
    LANE_BIKE_LANE = "LANE_BIKE_LANE"
    ROAD_LINE_BROKEN_SINGLE_WHITE = "ROAD_LINE_BROKEN_SINGLE_WHITE"
    ROAD_LINE_SOLID_SINGLE_WHITE = "ROAD_LINE_SOLID_SINGLE_WHITE"
    ROAD_LINE_SOLID_DOUBLE_WHITE = "ROAD_LINE_SOLID_DOUBLE_WHITE"
    ROAD_EDGE_BOUNDARY = "ROAD_EDGE_BOUNDARY"
    
    # ===== 信号機状態 =====
    LIGHT_UNKNOWN = "LIGHT_UNKNOWN"
    LIGHT_ARROW_STOP = "LIGHT_ARROW_STOP"
    LIGHT_ARROW_CAUTION = "LIGHT_ARROW_CAUTION"
    LIGHT_ARROW_GO = "LIGHT_ARROW_GO"
    LIGHT_STOP = "LIGHT_STOP"
    LIGHT_CAUTION = "LIGHT_CAUTION"
    LIGHT_GO = "LIGHT_GO"
    
    # ===== 座標系 =====
    COORDINATE_METADRIVE = "metadrive"
    COORDINATE_WAYMO = "waymo"
    
    @classmethod
    def is_lane(cls, type_string):
        """車線タイプかどうか判定"""
        return type_string and "LANE" in type_string
    
    @classmethod
    def from_waymo(cls, waymo_type_string: str):
        """Waymoタイプ文字列を変換"""
        return waymo_type_string
```

### 状態配列

#### トラック状態 (tracks)

| キー | 形状 | dtype | 説明 |
|------|------|-------|------|
| `position` | (T, 3) | float32 | XYZ座標 [m] |
| `heading` | (T,) | float32 | 方位角 [rad] |
| `velocity` | (T, 2) | float32 | 速度XY [m/s] |
| `valid` | (T,) | bool | 有効フラグ (0=無効, 1=有効) |
| `size` (オプション) | (T, 3) | float32 | サイズ (長さ, 幅, 高さ) [m] |

**T**: タイムステップ数 (例: 200)

#### 信号機状態 (dynamic_map_states)

| キー | 形状 | dtype | 説明 |
|------|------|-------|------|
| `object_state` | (T,) | str | 信号状態 (LIGHT_STOP/CAUTION/GO) |
| `position` | (3,) | float32 | 信号機位置 XYZ [m] |
| `lane` | - | str | 対応車線ID |

---

## ScenarioEnv

**ファイル**: [metadrive/envs/scenario_env.py](../openpilot/metadrive/metadrive/envs/scenario_env.py)  
**行数**: 533行

### 概要

**ScenarioEnv**は、ScenarioDescriptionフォーマットのシナリオを再生するGymnasium環境です。

```python
class ScenarioEnv(BaseEnv):
    """
    実世界シナリオ再生環境
    
    - nuScenes/Waymoデータセット対応
    - MetaDrive録画データ再生
    - 実車両トラジェクトリ追従
    - マップ/信号機/トラフィック再現
    """
```

### デフォルト設定

```python
SCENARIO_ENV_CONFIG = dict(
    # ===== シナリオ設定 =====
    data_directory=AssetLoader.file_path("nuscenes", unix_style=False),
    start_scenario_index=0,
    num_scenarios=3,                # ロードするシナリオ数 (-1で全て)
    sequential_seed=False,          # シードを連番で設定
    
    # ===== カリキュラム設定 =====
    curriculum_level=1,             # 難易度レベル数
    episodes_to_evaluate_curriculum=None,
    target_success_rate=0.8,
    
    # ===== マップ設定 =====
    store_map=True,
    need_lane_localization=True,
    no_map=False,
    map_region_size=1024,           # マップ領域サイズ [m]
    
    # ===== シナリオ要素 =====
    no_traffic=False,               # トラフィック無効化
    no_static_vehicles=False,       # 静止車両削除
    no_light=False,                 # 信号機無効化
    reactive_traffic=False,         # IDM制御トラフィック有効化
    filter_overlapping_car=True,    # 衝突車両フィルタリング
    
    # ===== 車両設定 =====
    vehicle_config=dict(
        navigation_module=TrajectoryNavigation,  # トラジェクトリナビゲーション
        lidar=dict(num_lasers=120, distance=50),
        lane_line_detector=dict(num_lasers=0, distance=50),
        side_detector=dict(num_lasers=12, distance=50),
    ),
    set_static=False,               # 静的車両 (物理無効)
    
    # ===== 報酬設定 =====
    success_reward=5.0,
    out_of_road_penalty=5.0,
    crash_vehicle_penalty=1.0,
    crash_object_penalty=1.0,
    crash_human_penalty=1.0,
    driving_reward=1.0,
    no_negative_reward=True,
    
    # ===== コスト設定 =====
    crash_vehicle_cost=1.0,
    crash_object_cost=1.0,
    out_of_road_cost=1.0,
    crash_human_cost=1.0,
    
    # ===== 終了条件 =====
    out_of_route_done=False,
    crash_vehicle_done=False,
    crash_object_done=False,
    crash_human_done=False,
)
```

### 初期化

```python
def __init__(self, config=None):
    super(ScenarioEnv, self).__init__(config)
    
    # カリキュラム検証
    if self.config["curriculum_level"] > 1:
        assert self.config["num_scenarios"] % self.config["curriculum_level"] == 0
    
    # マルチワーカー検証
    if self.config["num_workers"] > 1:
        assert self.config["sequential_seed"]
    
    self.start_index = self.config["start_scenario_index"]
```

### エンジンセットアップ

```python
def setup_engine(self):
    super(ScenarioEnv, self).setup_engine()
    
    # マネージャー登録
    self.engine.register_manager("data_manager", ScenarioDataManager())
    self.engine.register_manager("map_manager", ScenarioMapManager())
    
    if not self.config["no_traffic"]:
        self.engine.register_manager("traffic_manager", ScenarioTrafficManager())
    
    if not self.config["no_light"]:
        self.engine.register_manager("light_manager", ScenarioLightManager())
    
    self.engine.register_manager("curriculum_manager", ScenarioCurriculumManager())
```

### 終了条件

```python
def done_function(self, vehicle_id: str):
    vehicle = self.agents[vehicle_id]
    done_info = {
        TerminationState.CRASH_VEHICLE: vehicle.crash_vehicle,
        TerminationState.CRASH_OBJECT: vehicle.crash_object,
        TerminationState.CRASH_HUMAN: vehicle.crash_human,
        TerminationState.OUT_OF_ROAD: self._is_out_of_road(vehicle),
        TerminationState.SUCCESS: self._is_arrive_destination(vehicle),
        TerminationState.MAX_STEP: self.episode_lengths[vehicle_id] >= self.config["horizon"],
    }
    
    # 終了判定
    if done_info[TerminationState.SUCCESS]:
        return True
    elif done_info[TerminationState.OUT_OF_ROAD]:
        return True
    # ... その他の条件
```

### ScenarioWaypointEnv

```python
class ScenarioWaypointEnv(ScenarioEnv):
    """
    ウェイポイント追従環境
    
    実車両トラジェクトリを離散ウェイポイントとして提供
    """
    def default_config(cls):
        config = super().default_config()
        config.update(dict(
            waypoint_horizon=5,         # ウェイポイント数
            agent_policy=WaypointPolicy,  # ウェイポイント追従ポリシー
            set_static=True,            # 物理無効 (ドリフト防止)
        ))
        return config
```

---

## データセット統合

### nuScenes統合

**データセット**: [nuScenes](https://www.nuscenes.org/)  
**提供元**: Motional (aptiv + nuTonomy)  
**シーン数**: 1000シーン (23時間分)  
**場所**: Boston, Singapore  
**センサー**: カメラ6台, LiDAR, レーダー5台, IMU, GPS

#### データ構造

```python
# nuScenesシナリオ
scenario = {
    "id": "nuScenes-scene-0001",
    "metadata": {
        "dataset": "nuscenes",
        "source_file": "v1.0-mini/scene-0001.json",
        "coordinate": "metadrive",
        "sdc_id": "ego_vehicle",
    },
    "tracks": {
        "ego_vehicle": {  # 自車
            "type": "VEHICLE",
            "state": {
                "position": ...,
                "heading": ...,
                "velocity": ...,
            }
        },
        "vehicle_1": {...},  # 周辺車両
        "pedestrian_1": {...},  # 歩行者
    },
    "map_features": {
        "lane_1": {
            "type": "LANE_SURFACE_STREET",
            "polyline": ...,
        }
    }
}
```

#### データロード

```python
from metadrive.envs.scenario_env import ScenarioEnv
from metadrive.engine.asset_loader import AssetLoader

env = ScenarioEnv({
    "data_directory": AssetLoader.file_path("nuscenes", unix_style=False),
    "num_scenarios": 10,  # 10シナリオロード
})
```

### Waymo統合

**データセット**: [Waymo Open Dataset](https://waymo.com/open/)  
**提供元**: Waymo (Google)  
**シーン数**: 103,354シーン  
**場所**: Phoenix, San Francisco等  
**センサー**: カメラ5台, LiDAR5台

#### Waymo座標系変換

```python
# Waymo座標系 → MetaDrive座標系
def convert_waymo_to_metadrive(waymo_scenario):
    """
    Waymo座標系 (右手系, Z-up) をMetaDrive座標系 (右手系, Z-up) に変換
    
    座標系は同じだが、オブジェクトタイプ文字列等を変換
    """
    scenario = ScenarioDescription()
    scenario["metadata"]["coordinate"] = "metadrive"
    
    # タイプ変換
    for track_id, track in waymo_scenario["tracks"].items():
        track["type"] = MetaDriveType.from_waymo(track["type"])
    
    return scenario
```

#### データロード

```python
env = ScenarioEnv({
    "data_directory": AssetLoader.file_path("waymo", unix_style=False),
    "num_scenarios": 100,
})
```

**CLIオプション**:
```bash
# nuScenes可視化
python -m metadrive.examples.drive_in_real_env

# Waymo可視化
python -m metadrive.examples.drive_in_real_env --waymo
```

---

## シナリオ記録と再生

### シナリオ記録

**ユーティリティ**: [metadrive/scenario/utils.py](../openpilot/metadrive/metadrive/scenario/utils.py)  
**関数**: `convert_recorded_scenario_exported()`

```python
def convert_recorded_scenario_exported(record_episode, 
                                       scenario_log_interval=0.1, 
                                       to_dict=True):
    """
    MetaDrive内部記録データをScenarioDescriptionに変換
    
    Args:
        record_episode: env.export_scenarios()で取得した記録データ
        scenario_log_interval: タイムステップ間隔 (秒)
        to_dict: dict形式で返すか
        
    Returns:
        ScenarioDescription形式のデータ
    """
    result = ScenarioDescription()
    
    # メタデータ設定
    result["id"] = f"{record_episode['map_data']['map_type']}-{record_episode['scenario_index']}"
    result["version"] = DATA_VERSION
    result["length"] = len(record_episode["frame"])
    
    result["metadata"] = {
        "metadrive_processed": True,
        "dataset": "metadrive",
        "seed": record_episode["global_seed"],
        "scenario_id": record_episode["scenario_index"],
        "coordinate": "metadrive",
    }
    
    # トラック抽出
    frames = [step_frame[-1] for step_frame in record_episode["frame"]]
    all_objects = set()
    for frame in frames:
        all_objects.update(frame.step_info.keys())
    
    # 各オブジェクトのトラジェクトリ構築
    result["tracks"] = {}
    for obj_id in all_objects:
        result["tracks"][obj_id] = {
            "type": MetaDriveType.VEHICLE,
            "state": {
                "position": np.zeros((result["length"], 3)),
                "heading": np.zeros((result["length"],)),
                "velocity": np.zeros((result["length"], 2)),
                "valid": np.zeros((result["length"],)),
            },
            "metadata": {
                "track_length": result["length"],
                "object_id": obj_id,
            }
        }
        
        # フレームごとに状態を記録
        for i, frame in enumerate(frames):
            if obj_id in frame.step_info:
                obj_state = frame.step_info[obj_id]
                result["tracks"][obj_id]["state"]["position"][i] = obj_state["position"]
                result["tracks"][obj_id]["state"]["heading"][i] = obj_state["heading"]
                result["tracks"][obj_id]["state"]["velocity"][i] = obj_state["velocity"]
                result["tracks"][obj_id]["state"]["valid"][i] = 1
    
    # マップ情報コピー
    result["map_features"] = record_episode["map_data"]["map_features"]
    
    return result
```

### 記録手順

```python
from metadrive.envs.metadrive_env import MetaDriveEnv

# 記録モード有効化
env = MetaDriveEnv({
    "record_episode": True,
    "num_scenarios": 1,
})

# エピソード実行
obs, info = env.reset()
for _ in range(1000):
    obs, reward, terminated, truncated, info = env.step(env.action_space.sample())
    if terminated or truncated:
        break

# シナリオエクスポート
scenarios = env.export_scenarios()
print(scenarios.keys())
# ['map_data', 'scenario_index', 'global_seed', 'frame', 'manager_metadata']

# ScenarioDescription形式に変換
from metadrive.scenario.utils import convert_recorded_scenario_exported
scenario = convert_recorded_scenario_exported(scenarios)

# 保存
import pickle
with open("scenario_001.pkl", "wb") as f:
    pickle.dump(scenario, f)
```

### 再生手順

```python
# シナリオロード
with open("scenario_001.pkl", "rb") as f:
    scenario = pickle.load(f)

# ScenarioEnvで再生
env = ScenarioEnv({
    "data_directory": None,  # ディレクトリ不要
    "num_scenarios": 1,
})

# シナリオ手動設定
env.engine.data_manager.scenarios = [scenario]

obs, info = env.reset()
for _ in range(scenario["length"]):
    obs, reward, terminated, truncated, info = env.step([0, 0])  # ダミーアクション
    env.render()
```

### マップ可視化

```python
from metadrive.scenario.utils import draw_map

# マップ描画
draw_map(scenario["map_features"], show=True)
```

**出力**: matplotlibで車線、道路境界を可視化

---

## マネージャーシステム

ScenarioEnvは、複数の専用マネージャーでシナリオ要素を管理します。

### ScenarioDataManager

**ファイル**: `metadrive/manager/scenario_data_manager.py`

```python
class ScenarioDataManager(BaseManager):
    """
    シナリオデータロード・管理
    
    - ディスクからシナリオファイル読み込み
    - 現在のシナリオIDトラッキング
    - シナリオ切り替え
    """
    def __init__(self):
        super().__init__()
        self.scenarios = []             # ロード済みシナリオリスト
        self.current_scenario = None    # 現在のシナリオ
        self.current_scenario_id = None
        self.current_scenario_index = 0
    
    def load_scenarios(self, data_directory, num_scenarios):
        """
        ディレクトリからシナリオファイルをロード
        
        Args:
            data_directory: シナリオディレクトリパス
            num_scenarios: ロードするシナリオ数 (-1で全て)
        """
        pass
    
    def set_scenario(self, scenario_index):
        """
        現在のシナリオを設定
        
        Args:
            scenario_index: シナリオインデックス
        """
        self.current_scenario_index = scenario_index
        self.current_scenario = self.scenarios[scenario_index]
        self.current_scenario_id = self.current_scenario["id"]
```

### ScenarioMapManager

**ファイル**: `metadrive/manager/scenario_map_manager.py`

```python
class ScenarioMapManager(MapManager):
    """
    シナリオマップ管理
    
    - map_featuresから車線生成
    - 車線接続関係構築
    - ナビゲーション用経路グラフ作成
    """
    def reset(self):
        """
        現在のシナリオからマップ生成
        """
        scenario = self.engine.data_manager.current_scenario
        map_features = scenario["map_features"]
        
        # 車線生成
        for lane_id, lane_data in map_features.items():
            if MetaDriveType.is_lane(lane_data["type"]):
                self.create_lane(lane_id, lane_data)
        
        # 車線接続
        for lane_id, lane_data in map_features.items():
            if "entry_lanes" in lane_data:
                for entry_id in lane_data["entry_lanes"]:
                    self.connect_lanes(entry_id, lane_id)
    
    def create_lane(self, lane_id, lane_data):
        """
        車線オブジェクト生成
        
        Args:
            lane_id: 車線ID
            lane_data: 車線データ (polyline, polygon等)
        """
        pass
```

### ScenarioTrafficManager

**ファイル**: `metadrive/manager/scenario_traffic_manager.py`

```python
class ScenarioTrafficManager(TrafficManager):
    """
    シナリオトラフィック管理
    
    - tracksから車両/歩行者生成
    - ReplayPolicyで元トラジェクトリ再生
    - reactive_traffic=True時はIDMポリシー適用
    """
    def reset(self):
        """
        現在タイムステップのトラフィック生成
        """
        scenario = self.engine.data_manager.current_scenario
        tracks = scenario["tracks"]
        current_step = self.engine.episode_step
        
        for track_id, track in tracks.items():
            if track_id == scenario["metadata"]["sdc_id"]:
                continue  # 自車はスキップ
            
            # 有効チェック
            if not track["state"]["valid"][current_step]:
                continue
            
            # 車両生成
            if track["type"] == MetaDriveType.VEHICLE:
                vehicle = self.spawn_vehicle(track_id, track, current_step)
                
                # ポリシー設定
                if self.config["reactive_traffic"]:
                    policy = IDMPolicy(vehicle)
                else:
                    policy = ReplayPolicy(vehicle, track)
                
                self.add_policy(track_id, policy)
    
    def spawn_vehicle(self, track_id, track, step):
        """
        車両生成
        
        Args:
            track_id: トラックID
            track: トラックデータ
            step: 現在ステップ
        """
        position = track["state"]["position"][step]
        heading = track["state"]["heading"][step]
        
        vehicle = self.engine.spawn_object(
            BaseVehicle,
            name=track_id,
            position=position,
            heading=heading,
        )
        return vehicle
```

### ScenarioLightManager

**ファイル**: `metadrive/manager/scenario_light_manager.py`

```python
class ScenarioLightManager(BaseManager):
    """
    シナリオ信号機管理
    
    - dynamic_map_statesから信号機状態読み込み
    - 各ステップで信号状態更新
    """
    def reset(self):
        """
        信号機生成
        """
        scenario = self.engine.data_manager.current_scenario
        dynamic_states = scenario.get("dynamic_map_states", {})
        
        for light_id, light_data in dynamic_states.items():
            if light_data["type"] == MetaDriveType.TRAFFIC_LIGHT:
                self.create_traffic_light(light_id, light_data)
    
    def step(self):
        """
        信号状態更新
        """
        scenario = self.engine.data_manager.current_scenario
        current_step = self.engine.episode_step
        
        for light_id, light in self.traffic_lights.items():
            state = scenario["dynamic_map_states"][light_id]["state"]["object_state"][current_step]
            light.set_state(state)  # LIGHT_STOP/CAUTION/GO
```

### ScenarioCurriculumManager

**ファイル**: `metadrive/manager/scenario_curriculum_manager.py`

```python
class ScenarioCurriculumManager(BaseManager):
    """
    カリキュラム学習管理
    
    - シナリオを難易度レベルに分割
    - 成功率に応じてレベルアップ
    """
    def __init__(self):
        super().__init__()
        self.current_level = 0
        self.success_rate = 0.0
        self.episodes_per_level = []
    
    def reset(self):
        """
        現在レベルからシナリオ選択
        """
        level = self.current_level
        scenarios_per_level = len(self.scenarios) // self.num_levels
        
        start_idx = level * scenarios_per_level
        end_idx = (level + 1) * scenarios_per_level
        
        scenario_index = np.random.randint(start_idx, end_idx)
        return scenario_index
    
    def update(self, success):
        """
        成功率更新とレベルアップ判定
        
        Args:
            success: エピソード成功フラグ
        """
        self.episodes_per_level[self.current_level] += 1
        self.success_rate = (
            self.success_rate * 0.9 + success * 0.1
        )
        
        # レベルアップ
        if self.success_rate > self.target_success_rate:
            self.current_level = min(
                self.current_level + 1, 
                self.num_levels - 1
            )
            self.success_rate = 0.0
```

---

## 使用例

### nuScenesシナリオ再生

```python
from metadrive.envs.scenario_env import ScenarioEnv
from metadrive.engine.asset_loader import AssetLoader

# nuScenesデータロード
env = ScenarioEnv({
    "data_directory": AssetLoader.file_path("nuscenes", unix_style=False),
    "num_scenarios": 10,
    "use_render": True,  # 可視化
    "agent_policy": ReplayEgoCarPolicy,  # 実トラジェクトリ追従
})

obs, info = env.reset()
for step in range(200):
    obs, reward, terminated, truncated, info = env.step([0, 0])
    env.render()
    
    if terminated or truncated:
        break

env.close()
```

### Waymoシナリオ再生

```python
env = ScenarioEnv({
    "data_directory": AssetLoader.file_path("waymo", unix_style=False),
    "num_scenarios": 100,
    "use_render": True,
})

obs, info = env.reset()
# ... 同様に再生
```

### カリキュラム学習

```python
env = ScenarioEnv({
    "data_directory": AssetLoader.file_path("nuscenes"),
    "num_scenarios": 1000,
    "curriculum_level": 5,          # 5段階レベル
    "target_success_rate": 0.8,     # 80%成功でレベルアップ
})

for episode in range(500):
    obs, info = env.reset()
    success = False
    
    for step in range(200):
        action = agent.act(obs)
        obs, reward, terminated, truncated, info = env.step(action)
        
        if info["arrive_dest"]:
            success = True
        
        if terminated or truncated:
            break
    
    # カリキュラム更新
    env.engine.curriculum_manager.update(success)
    
    print(f"Episode {episode}, Level {env.engine.curriculum_manager.current_level}")
```

### シナリオ記録と保存

```python
from metadrive.envs.metadrive_env import MetaDriveEnv
from metadrive.scenario.utils import convert_recorded_scenario_exported
import pickle

# 記録モード
env = MetaDriveEnv({
    "num_scenarios": 1,
    "traffic_density": 0.3,
})

# エピソード実行
obs, info = env.reset()
for _ in range(500):
    obs, reward, terminated, truncated, info = env.step(env.action_space.sample())
    if terminated or truncated:
        break

# エクスポート
record_data = env.export_scenarios()

# ScenarioDescription変換
scenario = convert_recorded_scenario_exported(
    record_data, 
    scenario_log_interval=0.1
)

# 保存
with open("my_scenario.pkl", "wb") as f:
    pickle.dump(scenario, f)

print(f"シナリオ保存: {scenario['id']}, 長さ: {scenario['length']} steps")
```

### カスタムシナリオ再生

```python
import pickle

# ロード
with open("my_scenario.pkl", "rb") as f:
    scenario = pickle.load(f)

# ScenarioEnvで再生
env = ScenarioEnv({
    "num_scenarios": 1,
    "use_render": True,
})

# シナリオ手動設定
env.engine.data_manager.scenarios = [scenario]

obs, info = env.reset()
for step in range(scenario["length"]):
    obs, reward, terminated, truncated, info = env.step([0, 0])
    env.render()

env.close()
```

### reactive_traffic有効化

```python
# トラフィック車両をIDMポリシーで制御
env = ScenarioEnv({
    "data_directory": AssetLoader.file_path("nuscenes"),
    "num_scenarios": 10,
    "reactive_traffic": True,  # IDMポリシー有効化
})

obs, info = env.reset()
for _ in range(200):
    # トラフィック車両がIDMで反応的に運転
    obs, reward, terminated, truncated, info = env.step([1.0, 0.0])
    env.render()
```

**効果**: トラフィック車両が元トラジェクトリではなく、自車の行動に応じて加減速・車線変更

### マップ可視化

```python
from metadrive.scenario.utils import draw_map
import pickle

# シナリオロード
with open("scenario.pkl", "rb") as f:
    scenario = pickle.load(f)

# マップ描画
draw_map(scenario["map_features"], show=True)
```

**出力**: matplotlibでmap_features内の車線を可視化

### マルチワーカーサンプリング (Rllib)

```python
from ray.rllib.algorithms.ppo import PPO

# Rllib用設定
config = {
    "data_directory": AssetLoader.file_path("nuscenes"),
    "num_scenarios": 1000,
    "sequential_seed": True,    # 必須
    "num_workers": 8,           # 8ワーカー並列
    "worker_index": 0,          # Rllibが自動設定
}

# Rllibトレーニング
algo = PPO(env=ScenarioEnv, config={
    "env_config": config,
    "num_workers": 8,
})

for i in range(100):
    result = algo.train()
    print(f"Iteration {i}, reward: {result['episode_reward_mean']}")
```

---

## まとめ

MetaDriveのシナリオシステムは、実世界データと統合するための強力なフレームワークです。

### 主要特徴

1. **統一フォーマット**: ScenarioDescriptionで異なるデータセットを統合
2. **nuScenes/Waymo対応**: 主要な自動運転データセット統合
3. **記録/再生**: MetaDriveシミュレーション結果を記録・再生
4. **専用マネージャー**: データ/マップ/トラフィック/信号機を分離管理
5. **カリキュラム学習**: 難易度レベル別シナリオ分割
6. **reactive_traffic**: 実データをベースに反応的トラフィック生成

### データフロー

```
┌─────────────────┐
│ データセット     │ (nuScenes/Waymo/MetaDrive録画)
└─────────────────┘
         ↓
┌─────────────────┐
│ ScenarioDescription │ (統一フォーマット)
└─────────────────┘
         ↓
┌─────────────────┐
│ ScenarioEnv      │
└─────────────────┘
         ↓
┌─────────────────────────────────┐
│ マネージャーシステム              │
│ ・ScenarioDataManager (データ)   │
│ ・ScenarioMapManager (マップ)    │
│ ・ScenarioTrafficManager (車両)  │
│ ・ScenarioLightManager (信号)    │
└─────────────────────────────────┘
         ↓
┌─────────────────┐
│ シミュレーション  │ (再生/学習/評価)
└─────────────────┘
```

### 適用例

- **実世界シナリオ評価**: nuScenes/Waymoデータで自動運転アルゴリズムをテスト
- **合成データ拡張**: MetaDrive生成シナリオと実データを混合
- **シム2リアル**: シミュレーションで訓練したポリシーを実データで評価
- **カリキュラム学習**: 簡単なシナリオから徐々に難易度を上げる学習

MetaDriveシナリオシステムは、openpilotのシミュレーション環境において、実世界データとの橋渡しを実現する重要なコンポーネントです。

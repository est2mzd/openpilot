# openpilot 統合詳細

## 概要

rednose は openpilot の位置推定システム（locationd）で重要な役割を果たしています。

- **使用箇所**: `selfdrive/locationd/`
- **主要フィルタ**: PoseKalman（姿勢推定）、CarKalman（車両状態推定）
- **役割**: センサーフュージョン、状態推定、不確実性管理

---

## locationd アーキテクチャ

### システム構成

```
┌─────────────────────────────────────────────────────────────┐
│                         locationd                            │
├─────────────────────────────────────────────────────────────┤
│                                                               │
│  ┌──────────────┐         ┌──────────────┐                  │
│  │  PoseKalman  │         │  CarKalman   │                  │
│  │  (rednose)   │         │  (rednose)   │                  │
│  └──────┬───────┘         └──────┬───────┘                  │
│         │                        │                           │
│         │ State Estimation       │ Vehicle Params           │
│         │                        │                           │
│  ┌──────▼────────────────────────▼───────┐                  │
│  │        Localizer (Fusion)             │                  │
│  └──────────────────────────────────────┘                  │
│                                                               │
└───────────────────────────┬───────────────────────────────────┘
                            │
        ┌───────────────────┼───────────────────┐
        │                   │                   │
   ┌────▼────┐         ┌────▼────┐        ┌────▼────┐
   │   IMU   │         │ Camera  │        │   GPS   │
   │(Gyro+   │         │  Odo    │        │  GNSS   │
   │ Accel)  │         │         │        │         │
   └─────────┘         └─────────┘        └─────────┘
```

---

## PoseKalman の統合

### ファイル

- **定義**: `selfdrive/locationd/models/pose_kf.py`
- **使用**: `selfdrive/locationd/locationd.py`

---

### 状態ベクトル（18次元）

```python
class States:
  NED_ORIENTATION = slice(0, 3)    # ロール、ピッチ、ヨー [rad]
  DEVICE_VELOCITY = slice(3, 6)    # NED座標系での速度 [m/s]
  ANGULAR_VELOCITY = slice(6, 9)   # 角速度 [rad/s]
  GYRO_BIAS = slice(9, 12)         # ジャイロバイアス [rad/s]
  ACCELERATION = slice(12, 15)     # デバイス座標系での加速度 [m/s²]
  ACCEL_BIAS = slice(15, 18)       # 加速度計バイアス [m/s²]
```

**座標系の説明**:
- **NED**: North-East-Down（北-東-下）地理座標系
  - X軸: 北向き
  - Y軸: 東向き
  - Z軸: 下向き
- **デバイス座標系**: comma 3X の向き基準
  - X軸: デバイスの前方
  - Y軸: デバイスの右方
  - Z軸: デバイスの下方

---

### 初期化

```python
# locationd.py

from openpilot.selfdrive.locationd.models.pose_kf import PoseKalman, States

class Localizer:
  def __init__(self):
    # PoseKalman インスタンス生成
    self.kf = PoseKalman(GENERATED_DIR, MAX_FILTER_REWIND_TIME)
    
  def reset(self, t: float, 
            x_initial: np.ndarray = PoseKalman.initial_x, 
            P_initial: np.ndarray = PoseKalman.initial_P):
    # フィルタをリセット
    self.kf.init_state(x_initial, covs=P_initial, filter_time=t)
```

**GENERATED_DIR**: コード生成先（ビルド時に作成）
**MAX_FILTER_REWIND_TIME**: リワインド可能な最大時間（遅延センサー対応）

---

### センサー入力の処理

#### 1. IMU（ジャイロ・加速度計）

```python
def handle_sensor_events(self, t, kind, meas):
  if kind == ObservationKind.PHONE_GYRO:
    # ジャイロデータ
    # meas = [ωx, ωy, ωz] (rad/s)
    self.kf.predict_and_observe(t, kind, meas)
    
  elif kind == ObservationKind.PHONE_ACCEL:
    # 加速度計データ
    # meas = [ax, ay, az] (m/s²)
    self.kf.predict_and_observe(t, kind, meas)
```

**観測方程式**:
```python
# ジャイロ観測
h_gyro = angular_velocity + gyro_bias

# 加速度計観測
h_accel = R(θ)^T * [0, 0, -g] + acceleration + ω × v + accel_bias
```

**物理的意味**:
- ジャイロ: 真の角速度 + バイアス
- 加速度計: 重力（回転後） + 真の加速度 + 遠心力 + バイアス

---

#### 2. カメラオドメトリ

```python
def handle_camera_odometry(self, t, trans, rot):
  # 並進観測
  # trans = [vx, vy, vz] (m/s)
  self.kf.predict_and_observe(
    t, 
    ObservationKind.CAMERA_ODO_TRANSLATION, 
    trans
  )
  
  # 回転観測
  # rot = [ωx, ωy, ωz] (rad/s)
  self.kf.predict_and_observe(
    t,
    ObservationKind.CAMERA_ODO_ROTATION,
    rot
  )
```

**観測方程式**:
```python
# 並進観測（カメラから推定した速度）
h_translation = device_velocity

# 回転観測（カメラから推定した角速度）
h_rotation = angular_velocity
```

**カメラオドメトリの仕組み**:
1. 連続フレーム間の特徴点追跡
2. エピポーラ幾何で相対姿勢推定
3. スケールは別センサー（IMU）で補正
4. 推定された移動・回転をカルマンフィルタに入力

---

### 状態の取得と利用

```python
def get_position_velocity(self):
  # 現在の状態
  state = self.kf.x
  P = self.kf.P
  
  # 姿勢（オイラー角）
  orientation = state[States.NED_ORIENTATION]
  roll, pitch, yaw = orientation
  
  # 速度（NED座標系）
  velocity = state[States.DEVICE_VELOCITY]
  vn, ve, vd = velocity  # 北、東、下方向の速度
  
  # 角速度
  angular_velocity = state[States.ANGULAR_VELOCITY]
  
  # 不確実性（標準偏差）
  orientation_std = np.sqrt(np.diag(P)[States.NED_ORIENTATION])
  velocity_std = np.sqrt(np.diag(P)[States.DEVICE_VELOCITY])
  
  return {
    'position': ...,  # 積分して計算
    'velocity': velocity,
    'orientation': orientation,
    'velocity_std': velocity_std,
    'orientation_std': orientation_std,
  }
```

---

## CarKalman の統合

### ファイル

- **定義**: `selfdrive/locationd/models/car_kf.py`
- **使用**: `selfdrive/locationd/locationd.py`, `selfdrive/controls/controlsd.py`

---

### 状態ベクトル（9次元）

```python
class States:
  STIFFNESS = slice(0, 1)         # タイヤ剛性係数 [-]
  STEER_RATIO = slice(1, 2)       # ステアリング比 [-]
  ANGLE_OFFSET = slice(2, 3)      # 角度オフセット [rad]
  ANGLE_OFFSET_FAST = slice(3, 4) # 高速角度オフセット [rad]
  VELOCITY = slice(4, 6)          # 速度 (vx, vy) [m/s]
  YAW_RATE = slice(6, 7)          # ヨーレート [rad/s]
  STEER_ANGLE = slice(7, 8)       # ステアリング角 [rad]
  ROAD_ROLL = slice(8, 9)         # 路面ロール角 [rad]
```

**推定する理由**:
- **STIFFNESS**: 車種やタイヤによって異なる、オンライン学習
- **STEER_RATIO**: 車種固有、キャリブレーション
- **ANGLE_OFFSET**: センサーの取り付け誤差
- **VELOCITY**: 車両の運動状態
- **ROAD_ROLL**: 路面の傾斜（横方向）

---

### 車両モデル（2輪モデル）

```python
# 状態方程式（横速度・ヨーレートの微分）
A = [
  [-(cF + cR) / (m * u), -(cF * aF - cR * aR) / (m * u) - u],
  [-(cF * aF - cR * aR) / (j * u), -(cF * aF² + cR * aR²) / (j * u)]
]

B = [[cF / m / sR], [(cF * aF) / j / sR]]

x_dot = A * [v, r] + B * (δ - offset)
```

**変数の意味**:
- $u$: 前後方向速度（縦速度）
- $v$: 横方向速度（横滑り）
- $r$: ヨーレート（旋回速度）
- $\delta$: ステアリング角
- $m$: 車両質量
- $j$: ヨー慣性モーメント
- $a_F, a_R$: 重心から前後軸までの距離
- $c_F, c_R$: 前後タイヤのコーナリング剛性
- $sR$: ステアリング比

**物理的意味**:
- ステアリング入力 → 前輪が向きを変える
- 前輪の横力 → 車両が旋回
- 旋回による横速度・ヨーレートの変化

---

### 初期化とパラメータ設定

```python
# locationd.py

from openpilot.selfdrive.locationd.models.car_kf import CarKalman

class Localizer:
  def __init__(self):
    self.car_kf = CarKalman(GENERATED_DIR)
    
  def set_vehicle_params(self, vehicle_params):
    # 車両パラメータの設定
    self.car_kf.set_globals(
      mass=vehicle_params.mass,
      rotational_inertia=vehicle_params.rotational_inertia,
      center_to_front=vehicle_params.center_to_front,
      center_to_rear=vehicle_params.center_to_rear,
      stiffness_front=vehicle_params.stiffness_front,
      stiffness_rear=vehicle_params.stiffness_rear
    )
```

**車両パラメータの取得**:
- `opendbc/car/{make}/values.py` から車種ごとのパラメータ
- 例: `opendbc/car/toyota/values.py` に Prius のパラメータ

---

### センサー入力の処理

```python
def handle_car_state(self, t, car_state):
  # ステアリング角
  steer_angle = car_state.steeringAngleDeg * DEG_TO_RAD
  self.car_kf.predict_and_observe(
    t,
    ObservationKind.STEER_ANGLE,
    np.array([steer_angle])
  )
  
  # 速度（車輪速センサー）
  velocity_x = car_state.vEgo
  self.car_kf.predict_and_observe(
    t,
    ObservationKind.ROAD_FRAME_X_SPEED,
    np.array([velocity_x])
  )
  
  # ヨーレート（IMU）
  yaw_rate = car_state.yawRate
  self.car_kf.predict_and_observe(
    t,
    ObservationKind.ROAD_FRAME_YAW_RATE,
    np.array([yaw_rate])
  )
```

---

### 推定値の利用

#### 1. ステアリング制御

```python
# controlsd.py

# CarKalman から推定値を取得
state = car_kf.x
steer_ratio = state[States.STEER_RATIO][0]
angle_offset = state[States.ANGLE_OFFSET][0]

# ステアリングコマンドの計算
desired_curvature = ...  # 目標曲率
desired_steer_angle = desired_curvature * wheelbase * steer_ratio + angle_offset

# 車両に送信
send_steering_command(desired_steer_angle)
```

**効果**:
- 正確なステアリング比で制御精度向上
- 角度オフセットを補正して真っ直ぐ走行

---

#### 2. 車両モデル補正

```python
# 推定されたタイヤ剛性
stiffness = state[States.STIFFNESS][0]

# 車両モデルの更新
updated_stiffness_front = nominal_stiffness_front * stiffness
updated_stiffness_rear = nominal_stiffness_rear * stiffness

# MPC（Model Predictive Control）に反映
mpc.set_vehicle_params(..., stiffness_front=updated_stiffness_front, ...)
```

**効果**:
- 路面状況（濡れた路面など）に応じた剛性変化を学習
- より正確な車両挙動予測

---

## センサーフュージョンの詳細

### 複数センサーの統合

| センサー | 測定対象 | 利点 | 欠点 |
|---------|---------|------|------|
| **IMU（ジャイロ）** | 角速度 | 高速・正確 | ドリフト（累積誤差） |
| **IMU（加速度計）** | 加速度 | 高速 | ノイズ多い、重力と分離困難 |
| **カメラオドメトリ** | 相対移動 | ドリフト小 | 計算重い、スケール不定 |
| **車輪速センサー** | 前後速度 | 正確 | 横速度不明、スリップ時に不正確 |
| **GPS** | 絶対位置 | ドリフトなし | 更新遅い、精度低い |

**カルマンフィルタの役割**:
- 各センサーの長所を活かし、短所を補完
- 不確実性を定量的に管理
- リアルタイムで最適な推定

---

### データフロー

```
1. センサーデータ受信
   ↓
2. 時刻 t まで予測（predict）
   x(t) = f(x(t-1), dt)
   P(t) = F P(t-1) F^T + Q
   ↓
3. 観測で更新（update）
   K = P H^T (H P H^T + R)^{-1}
   x(t) = x(t) + K (z - h(x(t)))
   P(t) = (I - K H) P(t)
   ↓
4. 状態を出力
   position, velocity, orientation
   ↓
5. 制御システムに供給
   controlsd, plannerd
```

---

## リワインド機能

### 遅延センサーへの対応

**問題**: センサーによって遅延が異なる
- IMU: 遅延小（数ミリ秒）
- カメラ: 遅延大（数十〜百ミリ秒）

**解決策**: リワインド機能
```python
# EKF_sym クラス
self.rewind_t = []         # 過去の時刻リスト
self.rewind_states = []    # 過去の状態リスト
self.rewind_obscache = []  # 過去の観測キャッシュ
```

**動作**:
1. 毎ステップ、現在の状態を履歴に保存
2. 遅延センサーデータ受信時、該当時刻まで巻き戻し
3. 観測を適用
4. 現在時刻まで再度予測

```python
# 疑似コード
if sensor_time < current_time:
  # 過去に戻る
  rewind_to(sensor_time)
  
  # 観測を適用
  update(sensor_data)
  
  # 現在まで再予測
  predict_to(current_time)
```

**効果**: 遅延の大きいセンサーも正確に統合可能

---

## パラメータチューニング

### PoseKalman のチューニング

#### プロセスノイズ Q

```python
Q = np.diag([
  0.001**2, 0.001**2, 0.001**2,   # 姿勢: ゆっくり変化
  0.01**2, 0.01**2, 0.01**2,      # 速度: やや速く変化
  0.1**2, 0.1**2, 0.1**2,         # 角速度: 速く変化
  (0.005/100)**2, (0.005/100)**2, (0.005/100)**2,  # ジャイロバイアス: 非常にゆっくり
  3**2, 3**2, 3**2,               # 加速度: 急激に変化
  0.005**2, 0.005**2, 0.005**2,   # 加速度計バイアス: ゆっくり変化
])
```

**調整方針**:
- 姿勢（ロール・ピッチ・ヨー）: 小 → カメラオドメトリを重視
- 加速度: 大 → 急なアクセル・ブレーキに追従
- バイアス: 極小 → 安定した値を維持

---

#### 観測ノイズ R

```python
obs_noise = {
  ObservationKind.PHONE_GYRO: np.diag([0.025**2, 0.025**2, 0.025**2]),
  ObservationKind.PHONE_ACCEL: np.diag([.5**2, .5**2, .5**2]),
  ObservationKind.CAMERA_ODO_TRANSLATION: np.diag([0.5**2, 0.5**2, 0.5**2]),
  ObservationKind.CAMERA_ODO_ROTATION: np.diag([0.05**2, 0.05**2, 0.05**2]),
}
```

**調整方針**:
- ジャイロ: 小 → 高精度センサー、信頼
- 加速度計: 大 → ノイズ多い、慎重に
- カメラ回転: 小 → 視覚的には回転が正確
- カメラ並進: 大 → スケールの不確実性

---

### CarKalman のチューニング

#### プロセスノイズ Q

```python
Q = np.diag([
  (.05/100)**2,                # STIFFNESS: 非常にゆっくり
  .01**2,                       # STEER_RATIO: ゆっくり
  math.radians(0.02)**2,       # ANGLE_OFFSET: ゆっくり
  math.radians(0.25)**2,       # ANGLE_OFFSET_FAST: やや速く
  .1**2, .01**2,               # VELOCITY (x, y): 速く変化
  math.radians(0.1)**2,        # YAW_RATE: 速く変化
  math.radians(0.1)**2,        # STEER_ANGLE: 速く変化
  math.radians(1)**2,          # ROAD_ROLL: 急激に変化可能
])
```

**調整方針**:
- パラメータ（剛性、ステアリング比など）: 極小 → ゆっくり学習
- 運動状態（速度、ヨーレートなど）: 大 → 運転操作に追従
- 路面ロール: 大 → 傾斜路に即座に対応

---

## コード生成とビルド

### ビルドプロセス

```bash
# 1. シンボリックコード生成
python selfdrive/locationd/models/pose_kf.py generate generated/

# 2. C++コンパイル
scons -j8

# 3. 共有ライブラリが生成される
# generated/libpose.so
# generated/libcar.so
```

**SConstruct での設定**:
```python
# openpilot/SConstruct
env = Environment(...)

# rednose フィルタのビルド
pose_kf = env.SharedLibrary(
  'generated/pose',
  ['generated/pose.cpp']
)

car_kf = env.SharedLibrary(
  'generated/car',
  ['generated/car.cpp']
)
```

---

## デバッグとログ

### ログ出力

```python
# locationd.py

from openpilot.common.swaglog import cloudlog

# フィルタの状態をログ
cloudlog.debug(f"Pose: roll={roll:.3f}, pitch={pitch:.3f}, yaw={yaw:.3f}")
cloudlog.debug(f"Velocity: vx={vx:.2f}, vy={vy:.2f}, vz={vz:.2f}")
cloudlog.debug(f"Steer Ratio: {steer_ratio:.2f}")
```

### cereal でのメッセージ送信

```python
# 推定結果を cereal メッセージで送信
msg = messaging.new_message('liveLocationKalman')
msg.liveLocationKalman = {
  'positionGeodetic': {...},
  'orientationNED': [roll, pitch, yaw],
  'velocityNED': [vn, ve, vd],
  'positionAccuracy': position_std,
  'velocityAccuracy': velocity_std,
  ...
}
messaging.send('liveLocationKalman', msg)
```

**受信側**:
- `controlsd`: 制御に使用
- `plannerd`: 経路計画に使用
- `ui`: 表示に使用

---

## まとめ

### rednose の openpilot での役割

1. **PoseKalman**: デバイスの姿勢・速度・加速度を推定
   - IMU（ジャイロ・加速度計）とカメラオドメトリを統合
   - 高精度な位置・姿勢情報を提供
   
2. **CarKalman**: 車両の動特性パラメータと運動状態を推定
   - ステアリング比、タイヤ剛性などをオンライン学習
   - 制御精度の向上に貢献

3. **センサーフュージョン**: 複数センサーの最適統合
   - 各センサーの長所を活かし、短所を補完
   - 不確実性の定量的管理

### 技術的利点

1. **自動コード生成**: シンボリック定義 → C++コード、ミスなし
2. **高速実行**: ネイティブコード、リアルタイム処理可能
3. **柔軟性**: 車両モデル変更が容易
4. **保守性**: パラメータ調整が明示的

### openpilot への貢献

1. **高精度な状態推定**: 制御・計画の基盤
2. **ロバスト性**: 外れ値除去、リワインド機能
3. **適応性**: 車両パラメータのオンライン学習
4. **効率性**: 計算コストが低い

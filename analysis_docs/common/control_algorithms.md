# Control Algorithms - 制御アルゴリズム

## 概要

openpilot/commonには、制御とフィルタリングのための**基本的なアルゴリズム**が実装されています。

- **PIDコントローラ**: `pid.py`
- **カルマンフィルタ**: `simple_kalman.py`
- **1次フィルタ**: `filter_simple.py`

---

## 1. PIDコントローラ

**ファイル**: `pid.py`

### 概要

**PID（Proportional-Integral-Derivative）コントローラ**:
- **P（比例）**: 現在の誤差に比例
- **I（積分）**: 過去の誤差の累積
- **D（微分）**: 誤差の変化率
- **F（フィードフォワード）**: 目標値への先行入力

### PIDControllerクラス

#### コンストラクタ

```python
class PIDController:
  def __init__(self, k_p, k_i, k_f=0., k_d=0., 
               pos_limit=1e308, neg_limit=-1e308, rate=100):
```

**パラメータ**:
- `k_p`: 比例ゲイン（速度依存可）
- `k_i`: 積分ゲイン（速度依存可）
- `k_d`: 微分ゲイン（速度依存可）
- `k_f`: フィードフォワードゲイン
- `pos_limit`: 出力上限
- `neg_limit`: 出力下限
- `rate`: 更新周波数（Hz）

#### 基本的な使い方

```python
from openpilot.common.pid import PIDController

# 定数ゲイン
pid = PIDController(
  k_p=0.8,       # 比例
  k_i=0.12,      # 積分
  k_d=0.01,      # 微分
  k_f=0.5,       # フィードフォワード
  pos_limit=3.0, # 最大出力 m/s²
  neg_limit=-4.0,# 最小出力
  rate=100       # 100Hz
)

# 制御ループ
while True:
  error = target_speed - current_speed
  error_rate = (error - prev_error) / dt
  
  # 制御計算
  accel = pid.update(
    error=error,
    error_rate=error_rate,
    speed=current_speed,
    feedforward=ff_accel
  )
  
  prev_error = error
```

#### 速度依存ゲイン

**ゲインを速度に応じて変化させる**:

```python
# 速度に応じたゲイン設定
kp_bp = [0., 20., 40.]   # breakpoints (m/s)
kp_v = [1.0, 0.8, 0.5]   # values

ki_bp = [0., 40.]
ki_v = [0.15, 0.10]

pid = PIDController(
  k_p=[kp_bp, kp_v],  # 低速で高ゲイン
  k_i=[ki_bp, ki_v],
  rate=100
)
```

**ゲインの補間**:
```
速度 0 m/s  → kp = 1.0
速度 10 m/s → kp = 0.9 (補間)
速度 20 m/s → kp = 0.8
速度 30 m/s → kp = 0.65 (補間)
速度 40 m/s → kp = 0.5
```

#### update() メソッド

```python
def update(self, error, error_rate=0.0, speed=0.0, 
           override=False, feedforward=0., freeze_integrator=False):
```

**パラメータ**:
- `error`: 誤差（target - current）
- `error_rate`: 誤差の変化率
- `speed`: 現在速度（ゲイン補間用）
- `override`: ユーザー操作中（積分をアンワインド）
- `feedforward`: フィードフォワード入力
- `freeze_integrator`: 積分を凍結

**戻り値**: 制御出力（リミット適用済み）

#### アンチワインドアップ

**問題**: 積分が溜まりすぎて制御が不安定になる

**解決策**: override時に積分を減らす

```python
# ユーザーがブレーキを踏んでいる時
accel = pid.update(error, override=user_brake_pressed)

# 内部で積分を減らす
if override:
  self.i -= self.i_unwind_rate * sign(self.i)
```

#### 積分リミット

**問題**: 積分が大きくなりすぎて出力がリミットに張り付く

**解決策**: 積分を自動的にリミット

```python
# 内部実装
control_no_i = self.p + self.d + self.f
control_no_i = clip(control_no_i, neg_limit, pos_limit)

# 積分のリミット
self.i = clip(self.i, 
              neg_limit - control_no_i,  # 下限
              pos_limit - control_no_i)  # 上限
```

---

### 実際の使用例

#### 例1：速度制御（縦制御）

```python
from openpilot.common.pid import PIDController

def setup_longitudinal_pid():
  # Prius の縦制御PID
  kp_bp = [0., 5., 35.]
  kp_v = [1.2, 0.8, 0.5]
  
  ki_bp = [0., 35.]
  ki_v = [0.18, 0.12]
  
  return PIDController(
    k_p=[kp_bp, kp_v],
    k_i=[ki_bp, ki_v],
    k_f=1.0,  # フィードフォワード
    pos_limit=2.0,   # 最大加速 2 m/s²
    neg_limit=-3.5,  # 最大減速 -3.5 m/s²
    rate=100
  )

# 制御ループ
long_pid = setup_longitudinal_pid()

while True:
  # 目標速度と現在速度
  v_target = get_target_speed()
  v_ego = get_current_speed()
  
  # 誤差
  error = v_target - v_ego
  
  # フィードフォワード（先行車の加速度等）
  ff_accel = get_lead_accel()
  
  # ユーザー操作チェック
  user_override = gas_pressed or brake_pressed
  
  # PID計算
  accel_cmd = long_pid.update(
    error=error,
    speed=v_ego,
    feedforward=ff_accel,
    override=user_override
  )
  
  # アクチュエーターに送信
  send_accel_command(accel_cmd)
```

#### 例2：ステアリング制御（横制御）

```python
from openpilot.common.pid import PIDController

def setup_lateral_pid():
  # ステアリング角度制御PID
  return PIDController(
    k_p=0.6,
    k_i=0.1,
    k_f=0.00006,  # 小さなフィードフォワード
    pos_limit=1.0,   # -1 to 1 の範囲
    neg_limit=-1.0,
    rate=100
  )

# 制御ループ
lat_pid = setup_lateral_pid()

while True:
  # 目標角度と現在角度
  angle_target = get_desired_steer_angle()
  angle_current = get_current_steer_angle()
  
  # 誤差
  error = angle_target - angle_current
  error_rate = (error - prev_error) / 0.01  # 100Hz
  
  # ユーザーがハンドルを操作中
  user_override = steering_torque > threshold
  
  # PID計算
  steer_cmd = lat_pid.update(
    error=error,
    error_rate=error_rate,
    override=user_override
  )
  
  # ステアリングコマンド送信
  send_steer_command(steer_cmd)
  
  prev_error = error
```

---

## 2. カルマンフィルタ

**ファイル**: `simple_kalman.py`

### 概要

**カルマンフィルタ**: ノイズを含む観測から状態を推定

**openpilotの実装**:
- **KF1D**: 1次元カルマンフィルタ（高速化版）
- 状態: `[位置, 速度]` または `[値, 変化率]`

### KF1Dクラス

#### 理論

**状態方程式**:
```
x(k+1) = A * x(k) + w    # w: プロセスノイズ
y(k) = C * x(k) + v      # v: 観測ノイズ
```

**カルマンフィルタ**:
```
予測: x_pred = A * x
更新: x = x_pred + K * (y - C * x_pred)
```

**K（カルマンゲイン）**は事前計算済み。

#### コンストラクタ

```python
class KF1D:
  def __init__(self, x0, A, C, K):
```

**パラメータ**:
- `x0`: 初期状態 `[[x0], [x1]]`
- `A`: 状態遷移行列 `[[A00, A01], [A10, A11]]`
- `C`: 観測行列 `[C0, C1]`
- `K`: カルマンゲイン `[[K0], [K1]]`（事前計算）

#### カルマンゲインの計算

```python
from openpilot.common.simple_kalman import get_kalman_gain
import numpy as np

dt = 0.01  # 10ms

# 状態遷移行列
A = np.array([[1.0, dt],
              [0.0, 1.0]])

# 観測行列（位置のみ観測）
C = np.array([[1.0, 0.0]])

# プロセスノイズ共分散
Q = np.array([[0.1, 0.0],
              [0.0, 0.1]])

# 観測ノイズ共分散
R = np.array([[1.0]])

# カルマンゲイン計算
K = get_kalman_gain(dt, A, C, Q, R, iterations=100)
```

#### 使用例

```python
from openpilot.common.simple_kalman import KF1D, get_kalman_gain
import numpy as np

# カルマンゲイン計算（事前に1回）
dt = 0.01
A = np.array([[1.0, dt], [0.0, 1.0]])
C = np.array([[1.0, 0.0]])
Q = np.array([[0.1, 0.0], [0.0, 0.1]])
R = np.array([[1.0]])
K = get_kalman_gain(dt, A, C, Q, R)

# フィルタ初期化
kf = KF1D(
  x0=[[0.0], [0.0]],  # 初期状態 [位置, 速度]
  A=A,
  C=C,
  K=K
)

# 観測更新ループ
while True:
  # ノイズを含む観測値
  measurement = get_gps_position()  # 例: 50.5 m
  
  # 更新
  state = kf.update(measurement)
  
  # 推定値取得
  position = state[0]  # 位置
  velocity = state[1]  # 速度
  
  print(f"Position: {position:.2f} m, Velocity: {velocity:.2f} m/s")
```

---

### 実際の使用例

#### 例1：GPS位置のフィルタリング

```python
from openpilot.common.simple_kalman import KF1D, get_kalman_gain
import numpy as np

def setup_gps_filter():
  dt = 0.1  # GPS更新 10Hz
  
  A = np.array([[1.0, dt],
                [0.0, 1.0]])
  C = np.array([[1.0, 0.0]])
  Q = np.array([[0.5, 0.0],
                [0.0, 0.5]])  # プロセスノイズ
  R = np.array([[5.0]])       # GPS観測ノイズ
  
  K = get_kalman_gain(dt, A, C, Q, R)
  
  return KF1D(x0=[[0.0], [0.0]], A=A, C=C, K=K)

# 使用
gps_filter = setup_gps_filter()

while True:
  raw_gps = get_raw_gps_position()  # ノイズあり
  
  # フィルタリング
  state = gps_filter.update(raw_gps)
  
  filtered_position = state[0]
  estimated_velocity = state[1]
```

#### 例2：速度推定

```python
def setup_speed_filter():
  dt = 0.01  # 100Hz
  
  A = np.array([[1.0, dt],
                [0.0, 1.0]])
  C = np.array([[1.0, 0.0]])
  Q = np.array([[0.1, 0.0],
                [0.0, 0.1]])
  R = np.array([[0.5]])
  
  K = get_kalman_gain(dt, A, C, Q, R)
  
  return KF1D(x0=[[0.0], [0.0]], A=A, C=C, K=K)

# 速度フィルタ
speed_filter = setup_speed_filter()

while True:
  # 車輪速度（ノイズあり）
  wheel_speed = get_wheel_speed()
  
  # 更新
  state = speed_filter.update(wheel_speed)
  
  filtered_speed = state[0]
  acceleration = state[1]
```

---

## 3. 1次フィルタ（ローパスフィルタ）

**ファイル**: `filter_simple.py`

### FirstOrderFilterクラス

**目的**: 高周波ノイズを除去

#### アルゴリズム

```
x_filtered = (1 - alpha) * x_prev + alpha * x_new

alpha = dt / (rc + dt)
```

- `rc`: 時定数（秒）
- `dt`: サンプリング周期（秒）

#### コンストラクタ

```python
class FirstOrderFilter:
  def __init__(self, x0, rc, dt, initialized=True):
```

**パラメータ**:
- `x0`: 初期値
- `rc`: 時定数（秒）
- `dt`: サンプリング周期（秒）
- `initialized`: 初期化済みフラグ

#### 使用例

```python
from openpilot.common.filter_simple import FirstOrderFilter

# 時定数0.5秒、サンプリング0.01秒（100Hz）
lpf = FirstOrderFilter(
  x0=0.0,    # 初期値
  rc=0.5,    # 時定数（秒）
  dt=0.01,   # サンプリング周期
  initialized=True
)

while True:
  # ノイズを含むセンサー値
  raw_value = get_sensor_reading()
  
  # フィルタリング
  filtered_value = lpf.update(raw_value)
  
  print(f"Raw: {raw_value:.2f}, Filtered: {filtered_value:.2f}")
```

#### 時定数の選択

**時定数 rc の意味**:
- **小さい rc**: 速い応答、ノイズ除去弱い
- **大きい rc**: 遅い応答、ノイズ除去強い

**例**:
```python
# 速い応答（rc=0.1秒）
fast_filter = FirstOrderFilter(x0=0.0, rc=0.1, dt=0.01)

# 遅い応答（rc=1.0秒）
slow_filter = FirstOrderFilter(x0=0.0, rc=1.0, dt=0.01)
```

---

### 実際の使用例

#### 例1：ステアリング角度のフィルタリング

```python
from openpilot.common.filter_simple import FirstOrderFilter

# ステアリング角度フィルタ（時定数0.1秒）
steer_filter = FirstOrderFilter(
  x0=0.0,
  rc=0.1,
  dt=0.01  # 100Hz
)

while True:
  # 生のステアリング角度（ノイズあり）
  raw_angle = get_raw_steer_angle()
  
  # フィルタリング
  filtered_angle = steer_filter.update(raw_angle)
  
  # 制御計算に使用
  control_output = pid.update(target_angle - filtered_angle)
```

#### 例2：加速度のフィルタリング

```python
from openpilot.common.filter_simple import FirstOrderFilter

# 加速度フィルタ（時定数0.2秒）
accel_filter = FirstOrderFilter(
  x0=0.0,
  rc=0.2,
  dt=0.01
)

while True:
  # IMU加速度（ノイズあり）
  raw_accel = get_imu_accel()
  
  # フィルタリング
  filtered_accel = accel_filter.update(raw_accel)
  
  # 速度積分
  velocity += filtered_accel * 0.01
```

---

## アルゴリズムの選択ガイド

| 用途 | 推奨アルゴリズム | 理由 |
|------|----------------|------|
| 速度・位置制御 | PIDController | 定常偏差ゼロ、調整容易 |
| ノイズ除去（単純） | FirstOrderFilter | 軽量、実装簡単 |
| 状態推定（位置+速度） | KF1D | 観測ノイズ・プロセスノイズ考慮 |
| ステアリング制御 | PID + FirstOrderFilter | PID制御 + センサーフィルタ |

---

## パフォーマンス

| アルゴリズム | 計算時間 | メモリ |
|------------|---------|--------|
| PIDController.update() | ~0.01ms | ~200 bytes |
| KF1D.update() | ~0.02ms | ~100 bytes |
| FirstOrderFilter.update() | ~0.001ms | ~50 bytes |

**結論**: 全て100Hzループで使用可能

---

## まとめ

制御アルゴリズムのポイント:

1. **PIDController**: 速度依存ゲイン、アンチワインドアップ、積分リミット
2. **KF1D**: 状態推定（位置+速度）、カルマンゲイン事前計算
3. **FirstOrderFilter**: 単純ローパス、時定数で応答速度調整

---

## 関連ドキュメント

- [README.md](README.md) - common全体の概要
- [realtime.md](realtime.md) - リアルタイム制御

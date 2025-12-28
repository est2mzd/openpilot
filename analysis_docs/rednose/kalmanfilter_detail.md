# KalmanFilter 基底クラス詳細

## 概要

`rednose/helpers/kalmanfilter.py` は、すべてのカルマンフィルタ実装の基底クラスを提供します。

- **ファイル**: `rednose/helpers/kalmanfilter.py`
- **行数**: 60行
- **主要クラス**: `KalmanFilter`
- **役割**: カルマンフィルタの共通インターフェース定義

---

## KalmanFilter クラス

### クラス定義

```python
from typing import Any
import numpy as np

class KalmanFilter:
  name = "<name>"
  initial_x = np.zeros((0, 0))          # 初期状態ベクトル
  initial_P_diag = np.zeros((0, 0))     # 初期共分散対角成分
  Q = np.zeros((0, 0))                   # プロセスノイズ共分散
  obs_noise: dict[int, Any] = {}         # 観測ノイズ共分散（観測種類ごと）

  # サブクラスで初期化される
  filter = None
```

### 設計パターン

**テンプレートメソッドパターン**:
- 基底クラスが共通インターフェースを定義
- サブクラスが具体的なパラメータを実装
- `generate_code()` で C++ コードを生成
- `__init__()` で EKF インスタンスを初期化

---

## クラス変数

### 1. name

```python
name = "<name>"  # フィルタ名（例: "car", "pose", "kinematic"）
```

**用途**:
- 生成される C++ ファイル名: `{name}.cpp`, `{name}.h`
- C++ 関数名のプレフィックス: `{name}_f_fun`, `{name}_update_1` など

---

### 2. initial_x

```python
initial_x = np.array([...])  # 初期状態ベクトル
```

**例（CarKalman）**:
```python
initial_x = np.array([
    1.0,          # STIFFNESS（タイヤ剛性係数）
    15.0,         # STEER_RATIO（ステアリング比）
    0.0,          # ANGLE_OFFSET（角度オフセット）
    0.0,          # ANGLE_OFFSET_FAST
    10.0, 0.0,    # VELOCITY (vx, vy)
    0.0,          # YAW_RATE
    0.0,          # STEER_ANGLE
    0.0           # ROAD_ROLL
])
```

**意味**: フィルタ起動時の初期推定値

---

### 3. initial_P_diag または P_initial

```python
initial_P_diag = np.array([...])  # 初期共分散の対角成分
# または
P_initial = np.diag([...])         # 初期共分散行列（フル）
```

**例（PoseKalman）**:
```python
initial_P = np.diag([
    0.01**2, 0.01**2, 0.01**2,   # 姿勢の不確実性（ラジアン^2）
    10**2, 10**2, 10**2,          # 速度の不確実性（m/s）^2
    1**2, 1**2, 1**2,             # 角速度の不確実性
    ...
])
```

**意味**: 初期推定値がどれだけ不確実か（分散の大きさ）

**大きい値**: 不確実性が高い → センサー観測を重視
**小さい値**: 不確実性が低い → 初期値を信頼

---

### 4. Q（プロセスノイズ）

```python
Q = np.diag([...])  # プロセスノイズ共分散
```

**例（CarKalman）**:
```python
Q = np.diag([
    (.05 / 100)**2,              # STIFFNESS の変動
    .01**2,                       # STEER_RATIO の変動
    math.radians(0.02)**2,       # ANGLE_OFFSET の変動
    math.radians(0.25)**2,       # ANGLE_OFFSET_FAST の変動
    .1**2, .01**2,               # VELOCITY の変動
    math.radians(0.1)**2,        # YAW_RATE の変動
    math.radians(0.1)**2,        # STEER_ANGLE の変動
    math.radians(1)**2,          # ROAD_ROLL の変動
])
```

**意味**: 状態がどれだけランダムに変化するか

**大きい値**: 状態が急激に変化する可能性がある → センサー観測を重視
**小さい値**: 状態が安定 → モデル予測を信頼

**物理的解釈**:
- ステアリング比は徐々にしか変わらない → 小さいプロセスノイズ
- 路面ロール角は急に変わりうる → 大きいプロセスノイズ

---

### 5. obs_noise（観測ノイズ）

```python
obs_noise: dict[int, Any] = {
    ObservationKind.KIND_1: np.atleast_2d(variance_1),
    ObservationKind.KIND_2: np.diag([var_x, var_y, var_z]),
    ...
}
```

**例（PoseKalman）**:
```python
obs_noise = {
    ObservationKind.PHONE_GYRO: np.diag([0.025**2, 0.025**2, 0.025**2]),
    ObservationKind.PHONE_ACCEL: np.diag([.5**2, .5**2, .5**2]),
    ObservationKind.CAMERA_ODO_TRANSLATION: np.diag([0.5**2, 0.5**2, 0.5**2]),
    ObservationKind.CAMERA_ODO_ROTATION: np.diag([0.05**2, 0.05**2, 0.05**2]),
}
```

**意味**: センサー測定値の信頼性

**大きい値**: センサーが不正確 → 観測を軽視、モデル予測を重視
**小さい値**: センサーが正確 → 観測を重視、状態を大きく修正

**センサーごとの違い**:
- ジャイロ: 比較的正確 → 小さいノイズ（0.025 rad/s）
- 加速度計: 振動の影響あり → 大きいノイズ（0.5 m/s²）
- カメラオドメトリ回転: 正確 → 小さいノイズ（0.05 rad）
- カメラオドメトリ並進: やや不正確 → 大きいノイズ（0.5 m）

---

## プロパティ

### 1. x（現在の状態）

```python
@property
def x(self):
    return self.filter.state()
```

**用途**: 現在の推定状態を取得

**例**:
```python
car_kf = CarKalman(generated_dir)
state = car_kf.x

stiffness = state[States.STIFFNESS][0]
velocity_x = state[States.VELOCITY][0]
velocity_y = state[States.VELOCITY][1]
```

---

### 2. t（現在の時刻）

```python
@property
def t(self):
    return self.filter.get_filter_time()
```

**用途**: フィルタの現在時刻を取得

**時刻の管理**:
- 初期化時に `filter_time=None` または指定値
- `predict_and_observe()` で時刻を更新

---

### 3. P（共分散行列）

```python
@property
def P(self):
    return self.filter.covs()
```

**用途**: 状態推定の不確実性を取得

**例**:
```python
cov_matrix = car_kf.P

# ステアリング比の標準偏差
steer_ratio_std = np.sqrt(cov_matrix[States.STEER_RATIO, States.STEER_RATIO])

# 速度 x と速度 y の相関
vel_correlation = cov_matrix[States.VELOCITY.start, States.VELOCITY.start + 1]
```

---

## メソッド

### 1. init_state()

```python
def init_state(self, state, covs_diag=None, covs=None, filter_time=None):
    if covs_diag is not None:
        P = np.diag(covs_diag)
    elif covs is not None:
        P = covs
    else:
        P = self.filter.covs()
    
    self.filter.init_state(state, P, filter_time)
```

**用途**: フィルタの状態を（再）初期化

**パラメータ**:
- `state`: 新しい状態ベクトル
- `covs_diag`: 共分散の対角成分（オプション）
- `covs`: 共分散行列（オプション）
- `filter_time`: フィルタ時刻（オプション）

**使用例**:
```python
# 対角共分散で初期化
car_kf.init_state(
    state=np.array([1.0, 15.0, 0.0, ...]),
    covs_diag=np.array([0.1**2, 1.0**2, 0.01**2, ...])
)

# フル共分散行列で初期化
car_kf.init_state(
    state=initial_state,
    covs=custom_covariance_matrix,
    filter_time=current_time
)
```

---

### 2. get_R()

```python
def get_R(self, kind, n):
    obs_noise = self.obs_noise[kind]
    dim = obs_noise.shape[0]
    R = np.zeros((n, dim, dim))
    for i in range(n):
        R[i, :, :] = obs_noise
    return R
```

**用途**: 観測ノイズ共分散行列を取得（バッチ観測用）

**パラメータ**:
- `kind`: 観測の種類（ObservationKind）
- `n`: 観測の数

**戻り値**: `(n, dim, dim)` の3次元配列

**使用例**:
```python
# 3個のジャイロ観測
R = pose_kf.get_R(ObservationKind.PHONE_GYRO, n=3)
# R.shape = (3, 3, 3)  （3個の観測 × 3次元 × 3次元）
```

---

### 3. predict_and_observe()

```python
def predict_and_observe(self, t, kind, data, R=None):
    if len(data) > 0:
        data = np.atleast_2d(data)
    
    if R is None:
        R = self.get_R(kind, len(data))
    
    self.filter.predict_and_update_batch(t, kind, data, R)
```

**用途**: 予測ステップと観測更新を実行

**パラメータ**:
- `t`: 観測時刻
- `kind`: 観測の種類
- `data`: 観測データ（1つまたは複数）
- `R`: 観測ノイズ共分散（オプション）

**処理の流れ**:
1. 前回の時刻から `t` まで状態を予測
2. 観測データ `data` で状態を更新

**使用例**:
```python
# 単一観測
car_kf.predict_and_observe(
    t=current_time,
    kind=ObservationKind.STEER_ANGLE,
    data=np.array([0.1])  # 0.1 rad
)

# バッチ観測
pose_kf.predict_and_observe(
    t=current_time,
    kind=ObservationKind.PHONE_GYRO,
    data=np.array([
        [0.01, 0.02, 0.03],  # 観測1
        [0.01, 0.02, 0.03],  # 観測2
        [0.01, 0.02, 0.03],  # 観測3
    ])
)
```

---

## サブクラスの実装パターン

### 1. 最小限の実装

```python
class MyKalman(KalmanFilter):
    name = "my_filter"
    
    initial_x = np.array([0.0, 0.0])
    initial_P_diag = np.array([1.0, 1.0])
    Q = np.diag([0.1**2, 0.1**2])
    obs_noise = {
        ObservationKind.MEASUREMENT: np.atleast_2d(0.5**2)
    }
    
    @staticmethod
    def generate_code(generated_dir):
        # シンボリック定義
        # gen_code() 呼び出し
        pass
    
    def __init__(self, generated_dir):
        self.filter = EKF_sym_pyx(
            generated_dir,
            MyKalman.name,
            MyKalman.Q,
            MyKalman.initial_x,
            np.diag(MyKalman.initial_P_diag),
            dim_state=2,
            dim_state_err=2
        )
```

---

### 2. カスタムメソッド追加

```python
class CarKalman(KalmanFilter):
    # ... 基本定義 ...
    
    global_vars = [
        'mass',
        'rotational_inertia',
        'center_to_front',
        'center_to_rear',
        'stiffness_front',
        'stiffness_rear',
    ]
    
    def set_globals(self, mass, rotational_inertia, ...):
        """車両パラメータを設定"""
        self.filter.set_global("mass", mass)
        self.filter.set_global("rotational_inertia", rotational_inertia)
        # ...
```

---

## 典型的な使用フロー

### 1. コード生成（一度だけ）

```python
# スクリプトとして実行
if __name__ == "__main__":
    generated_dir = sys.argv[2]
    CarKalman.generate_code(generated_dir)
```

**コマンド**:
```bash
python car_kf.py generate /path/to/generated/
```

**生成されるファイル**:
- `/path/to/generated/car.cpp`
- `/path/to/generated/car.h`
- `/path/to/generated/libcar.so`（ビルド後）

---

### 2. フィルタの初期化

```python
car_kf = CarKalman(generated_dir="/path/to/generated/")

# 車両パラメータの設定
car_kf.set_globals(
    mass=1500.0,
    rotational_inertia=2500.0,
    center_to_front=1.2,
    center_to_rear=1.5,
    stiffness_front=100000.0,
    stiffness_rear=120000.0
)
```

---

### 3. リアルタイムループ

```python
while True:
    # センサーデータ取得
    t = get_current_time()
    steer_angle = read_steer_angle_sensor()
    velocity = read_velocity_sensor()
    
    # 観測更新
    car_kf.predict_and_observe(
        t=t,
        kind=ObservationKind.STEER_ANGLE,
        data=np.array([steer_angle])
    )
    
    car_kf.predict_and_observe(
        t=t,
        kind=ObservationKind.ROAD_FRAME_X_SPEED,
        data=np.array([velocity])
    )
    
    # 推定状態の取得
    state = car_kf.x
    steer_ratio = state[States.STEER_RATIO][0]
    velocity_x = state[States.VELOCITY][0]
    
    # 制御などに使用
    print(f"Steer Ratio: {steer_ratio:.2f}, Velocity: {velocity_x:.2f} m/s")
```

---

## Q, R, P のチューニング

### カルマンゲインの関係

カルマンフィルタの更新式:
```
K = P H^T (H P H^T + R)^{-1}
x = x + K (z - h(x))
P = (I - K H) P
```

**カルマンゲイン K の意味**:
- K が大きい → 観測を重視（状態を大きく修正）
- K が小さい → モデルを重視（状態をあまり修正しない）

### パラメータの影響

| パラメータ | 大きくすると | K への影響 | 結果 |
|-----------|-------------|-----------|------|
| **Q（プロセスノイズ）** | 状態が不安定と仮定 | K ↑ | 観測を重視 |
| **R（観測ノイズ）** | センサーが不正確と仮定 | K ↓ | モデルを重視 |
| **P（共分散）** | 推定が不確実 | K ↑ | 観測を重視 |

### チューニングの指針

1. **Q の選択**:
   - 物理的変化速度を考慮
   - パラメータ（ステアリング比など）: 小さい Q（ゆっくり変化）
   - 外乱（路面ロールなど）: 大きい Q（急に変化）

2. **R の選択**:
   - センサーの仕様・ノイズレベルを参考
   - 実測データの標準偏差を計算
   - 保守的に大きめに設定（外れ値の影響を軽減）

3. **P の初期値**:
   - 初期推定が不確実 → 大きい P
   - 初期推定が正確 → 小さい P
   - 通常は Q の 10〜100 倍程度

4. **実験的調整**:
   - ログデータでシミュレーション
   - 推定値と真値の誤差を評価
   - Q, R を調整して誤差を最小化

---

## まとめ

### KalmanFilter クラスの役割

1. **共通インターフェース**: すべてのフィルタが同じメソッドで使える
2. **パラメータ管理**: Q, R, P などのパラメータを一元管理
3. **シンプルな API**: `predict_and_observe()` だけで予測・更新が完結

### サブクラスの責務

1. **パラメータ定義**: initial_x, Q, obs_noise を設定
2. **コード生成**: `generate_code()` でシンボリック定義
3. **初期化**: `__init__()` で EKF インスタンス作成
4. **カスタム機能**: 必要に応じて追加メソッド実装

### 利点

1. **再利用性**: 共通ロジックを基底クラスに集約
2. **一貫性**: すべてのフィルタが同じインターフェース
3. **保守性**: パラメータ変更が容易
4. **拡張性**: 新しいフィルタの追加が簡単

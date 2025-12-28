# rednose - カルマンフィルタフレームワーク

## 概要

**rednose**は、comma.ai が開発した**カルマンフィルタフレームワーク**であり、openpilot の位置推定・姿勢推定・車両状態推定を担当するライブラリです。

- **Git Submodule**: 独立したリポジトリとして管理
- **主要機能**: 拡張カルマンフィルタ（EKF）、誤差状態カルマンフィルタ（ESKF）、マルチステート制約カルマンフィルタ（MSCKF）
- **実装言語**: Python（定義）、C++（実行時）、Cython（バインディング）
- **特徴**: シンボリック微分によるヤコビアン自動計算、高速なネイティブコード生成

### 用語解説

#### カルマンフィルタとは？
**カルマンフィルタ**は、ノイズを含むセンサーデータから対象の状態を最適に推定する統計的アルゴリズムです。

- **なぜ必要？**: センサー（GPS、加速度計、ジャイロなど）はノイズや誤差を含むため、それらを統合して正確な状態を推定する必要がある
- **rednose の役割**: 複数のセンサーデータを統合し、車両の位置・速度・姿勢・パラメータを正確に推定
- **例え**: 複数の証言（センサー）から真実（真の状態）を推測する探偵のようなもの

#### 拡張カルマンフィルタ（EKF）とは？
**EKF（Extended Kalman Filter）**は、非線形システムに対応したカルマンフィルタの拡張版です。

- **なぜ必要？**: 車両の運動や回転は非線形（速度×角速度など）なので、標準的な線形カルマンフィルタでは対応できない
- **仕組み**: 非線形関数を線形化（微分してヤコビアンを計算）して、線形カルマンフィルタを適用
- **rednose の特徴**: ヤコビアンを**自動計算**（sympy使用）し、手動計算のミスを排除

#### 誤差状態カルマンフィルタ（ESKF）とは？
**ESKF（Error-State Kalman Filter）**は、状態と誤差を異なる空間で表現するカルマンフィルタです。

- **なぜ必要？**: 3D姿勢（回転）はクォータニオン（4次元）で表現するが、自由度は3なので冗長次元がある
- **解決策**: 状態はクォータニオン（4次元）で表現、誤差はオイラー角（3次元）で表現
- **メリット**: 冗長次元の問題を回避しつつ、クォータニオンの利点（ジンバルロック回避）を維持
- **用途**: openpilot の姿勢推定（PoseKalman）

#### マルチステート制約カルマンフィルタ（MSCKF）とは？
**MSCKF（Multi-State Constraint Kalman Filter）**は、視覚的特徴追跡をカルマンフィルタに統合する手法です。

- **課題**: カメラで見た特徴点（ランドマーク）の深度は、複数フレームで追跡して推定するが、その深度推定はカルマンフィルタの状態に依存する（循環依存）
- **解決策**: 過去の複数の状態を保持し、制約条件として特徴追跡を扱う
- **効果**: 視覚オドメトリとカルマンフィルタの正のフィードバックループを回避
- **用途**: 視覚ベースの位置推定

**根拠**:
```bash
$ cat .gitmodules | grep -A 2 rednose
[submodule "rednose_repo"]
  path = rednose_repo
  url = https://github.com/est2mzd/rednose.git

$ git submodule status | grep rednose
 98ad80ec306ec2fd895239e672d7cddce82e7d0c rednose_repo (remotes/origin/annotation)

$ ls -la rednose
lrwxrwxrwx 1 takuya takuya 20  6月 27 14:51 rednose -> rednose_repo/rednose
```

---

## rednose とは

### README.md より

> The kalman filter framework described here is an incredibly powerful tool for any optimization problem,
> but particularly for visual odometry, sensor fusion localization or SLAM.

**rednose の役割**:
- **センサーフュージョン**: GPS、IMU（加速度計・ジャイロ）、カメラなどの複数センサーデータを統合
  - 各センサーの長所を活かし、短所を補完
  - 例: GPSは絶対位置が分かるが更新が遅い、IMUは高速だが誤差が累積する → 統合して最適な推定
- **状態推定**: 車両の位置、速度、姿勢、パラメータをリアルタイムで推定
  - PoseKalman: デバイスの姿勢（ロール・ピッチ・ヨー）と速度
  - CarKalman: 車両パラメータ（ステアリング比、剛性など）と車両状態
- **シンボリック微分**: sympy で自動的にヤコビアンを計算、Cコード生成
  - **メリット**: 手動計算のミスがゼロ、複雑な数式でも正確
  - **効率**: 一度コード生成すれば高速なネイティブコードで実行
- **高度な手法**: ESKF、MSCKF、RTSスムージング、外れ値除去

**根拠**: `rednose_repo/README.md` の Introduction セクション

---

## ディレクトリ構造

```
rednose_repo/
├── .git                    # Git サブモジュール管理
├── README.md               # プロジェクト説明
├── LICENSE                 # MIT ライセンス
├── setup.py                # Python パッケージ設定
├── pyproject.toml          # ruff/pytest 設定
├── requirements.txt        # 依存パッケージ
├── SConstruct              # ビルド設定（SCons）
├── Dockerfile              # Docker イメージ
├── .github/                # GitHub Actions CI
├── site_scons/             # カスタム SCons ツール
├── examples/               # サンプルコード
│   ├── kinematic_kf.py         # 運動学カルマンフィルタ例
│   ├── live_kf.py              # リアルタイム例
│   ├── test_kinematic_kf.py    # テストコード
│   └── kinematic_kf.png        # 図解
└── rednose/                # ライブラリ本体
    ├── __init__.py             # 空ファイル
    ├── helpers/                # コアモジュール
    │   ├── __init__.py             # FFI/コードローダー
    │   ├── kalmanfilter.py         # KalmanFilter 基底クラス
    │   ├── ekf_sym.py              # シンボリック EKF（691行）
    │   ├── sympy_helpers.py        # Sympy 補助関数
    │   ├── chi2_lookup.py          # カイ二乗分布
    │   ├── chi2_lookup_table.npy   # カイ二乗テーブル
    │   ├── ekf_sym_pyx.pyx         # Cython 拡張
    │   ├── ekf_sym_pyx.cpp         # コンパイル済み Cython
    │   ├── ekf_sym_pyx.so          # 共有ライブラリ
    │   ├── ekf_load.cc/h           # C++ EKF ローダー
    │   ├── ekf_sym.cc/h            # C++ EKF 実装
    │   └── libekf_sym.a            # スタティックライブラリ
    ├── logger/                 # ロギング
    │   └── logger.h                # C ヘッダー
    └── templates/              # C コード生成テンプレート
        ├── ekf_c.c                 # EKF コア実装
        ├── compute_pos.c           # 位置計算
        └── feature_handler.c       # 特徴量ハンドラ
```

**根拠**: `ls -R rednose_repo/` の出力

---

## コード規模

### Python コード

```bash
$ find rednose -name "*.py" | wc -l
6

$ wc -l rednose/helpers/*.py
  691 rednose/helpers/ekf_sym.py          # シンボリック EKF コード生成
  276 rednose/helpers/sympy_helpers.py    # Sympy ヘルパー関数
   60 rednose/helpers/kalmanfilter.py     # KalmanFilter 基底クラス
   36 rednose/helpers/__init__.py         # FFI ローダー
   24 rednose/helpers/chi2_lookup.py      # カイ二乗分布
 1087 total
```

**主要ファイル**:
- `ekf_sym.py`: シンボリック微分とCコード生成の核心（691行）
- `sympy_helpers.py`: 回転行列、オイラー角、クォータニオン変換
- `kalmanfilter.py`: すべてのカルマンフィルタの基底クラス

**根拠**: `wc -l` コマンドの出力

### C++ / Cython コード

```bash
$ find rednose -name "*.cc" -o -name "*.cpp" -o -name "*.pyx" | wc -l
4

$ ls -lh rednose/helpers/*.cc rednose/helpers/*.cpp rednose/helpers/*.pyx
-rw-rw-r-- 1 takuya takuya 157K  6月 27 14:51 ekf_sym_pyx.cpp
-rw-rw-r-- 1 takuya takuya 4.4K  6月 27 14:51 ekf_sym_pyx.pyx
-rw-rw-r-- 1 takuya takuya 3.5K  6月 27 14:51 ekf_load.cc
-rw-rw-r-- 1 takuya takuya 1.3K  6月 27 14:51 ekf_sym.cc
```

**役割**:
- `ekf_sym_pyx.pyx`: Cython ラッパー（Python から C++ を呼び出す）
- `ekf_load.cc`: 生成された C++ コードをロード
- `ekf_sym.cc`: EKF のコア C++ 実装

**根拠**: `ls -lh` の出力

### C テンプレート

```bash
$ ls -lh rednose/templates/*.c
-rw-rw-r-- 1 takuya takuya  885  6月 27 14:51 compute_pos.c
-rw-rw-r-- 1 takuya takuya  11K  6月 27 14:51 ekf_c.c
-rw-rw-r-- 1 takuya takuya 1.9K  6月 27 14:51 feature_handler.c
```

**役割**: 自動生成される C++ コードのベーステンプレート

**根拠**: `ls -lh` の出力

---

## openpilot での使用例

### 1. PoseKalman（姿勢推定）

**ファイル**: `selfdrive/locationd/models/pose_kf.py`

**用途**: デバイス（comma 3X）の姿勢と動きをリアルタイムで推定

```python
from rednose.helpers.kalmanfilter import KalmanFilter
from rednose.helpers.ekf_sym import gen_code
from rednose.helpers.sympy_helpers import euler_rotate, rot_to_euler
from rednose.helpers.ekf_sym_pyx import EKF_sym_pyx

class PoseKalman(KalmanFilter):
  name = "pose"
  
  # 状態ベクトル（18次元）
  initial_x = np.array([
    0.0, 0.0, 0.0,  # NED_ORIENTATION (roll, pitch, yaw)
    0.0, 0.0, 0.0,  # DEVICE_VELOCITY (vx, vy, vz)
    0.0, 0.0, 0.0,  # ANGULAR_VELOCITY (ωx, ωy, ωz)
    0.0, 0.0, 0.0,  # GYRO_BIAS (ジャイロバイアス)
    0.0, 0.0, 0.0,  # ACCELERATION (ax, ay, az)
    0.0, 0.0, 0.0   # ACCEL_BIAS (加速度計バイアス)
  ])
  
  # 観測ノイズ
  obs_noise = {
    ObservationKind.PHONE_GYRO: np.diag([0.025**2, 0.025**2, 0.025**2]),
    ObservationKind.PHONE_ACCEL: np.diag([.5**2, .5**2, .5**2]),
    ObservationKind.CAMERA_ODO_TRANSLATION: np.diag([0.5**2, 0.5**2, 0.5**2]),
    ObservationKind.CAMERA_ODO_ROTATION: np.diag([0.05**2, 0.05**2, 0.05**2]),
  }
```

**推定する状態**:
- **姿勢**: ロール・ピッチ・ヨー角（デバイスの向き）
- **速度**: 3軸方向の速度
- **角速度**: 3軸周りの回転速度
- **バイアス**: ジャイロと加速度計のオフセット誤差

**センサー入力**:
- **IMU**: ジャイロと加速度計データ
- **カメラオドメトリ**: 視覚的な移動・回転推定

**根拠**: `selfdrive/locationd/models/pose_kf.py`

---

### 2. CarKalman（車両状態推定）

**ファイル**: `selfdrive/locationd/models/car_kf.py`

**用途**: 車両の動特性パラメータと現在の状態を推定

```python
from rednose.helpers.kalmanfilter import KalmanFilter
from rednose.helpers.ekf_sym import gen_code
from rednose.helpers.ekf_sym_pyx import EKF_sym_pyx

class CarKalman(KalmanFilter):
  name = 'car'
  
  # 状態ベクトル（9次元）
  initial_x = np.array([
    1.0,          # STIFFNESS（タイヤ剛性）
    15.0,         # STEER_RATIO（ステアリング比）
    0.0,          # ANGLE_OFFSET（角度オフセット）
    0.0,          # ANGLE_OFFSET_FAST（高速角度オフセット）
    10.0, 0.0,    # VELOCITY (vx, vy)
    0.0,          # YAW_RATE（ヨーレート）
    0.0,          # STEER_ANGLE（ステアリング角）
    0.0           # ROAD_ROLL（路面ロール角）
  ])
  
  # 観測ノイズ
  obs_noise = {
    ObservationKind.STEER_ANGLE: np.atleast_2d(math.radians(0.05)**2),
    ObservationKind.ROAD_FRAME_X_SPEED: np.atleast_2d(0.1**2),
    ObservationKind.STEER_RATIO: np.atleast_2d(5.0**2),
    ObservationKind.STIFFNESS: np.atleast_2d(0.5**2),
  }
```

**推定する状態**:
- **車両パラメータ**: ステアリング比、タイヤ剛性（車種ごとに異なる）
- **運動状態**: 速度（前後・横）、ヨーレート、ステアリング角
- **環境**: 路面のロール角（傾斜）

**センサー入力**:
- **ステアリング角センサー**
- **速度センサー**
- **ヨーレートセンサー**

**根拠**: `selfdrive/locationd/models/car_kf.py`

---

## 主要機能

### 1. シンボリック微分によるヤコビアン自動計算

**従来の問題**:
- EKF は非線形関数のヤコビアン（偏微分行列）が必要
- 手動で微分を計算 → 複雑で間違いやすい
- コードの保守が困難

**rednose の解決策**:
```python
import sympy as sp

# 状態ベクトルをシンボルとして定義
state_sym = sp.MatrixSymbol('state', dim_state, 1)

# 状態遷移関数を定義
f_sym = state + dt * state_dot

# ヤコビアンを自動計算
F_sym = f_sym.jacobian(state_sym)

# C++ コードを自動生成
gen_code(folder, name, f_sym, dt, state_sym, obs_eqs, dim_state, dim_err)
```

**メリット**:
- **正確**: 手動計算のミスがゼロ
- **保守性**: 状態遷移関数を変更すれば、ヤコビアンも自動更新
- **高速**: 生成された C++ コードで実行

**根拠**: `rednose/helpers/ekf_sym.py` の `gen_code` 関数

---

### 2. 誤差状態カルマンフィルタ（ESKF）

**クォータニオンの問題**:
- 3D 回転は 3 自由度だが、クォータニオンは 4 次元（冗長）
- カルマンフィルタで冗長次元を扱うのは困難

**ESKF の解決策**:
- **状態**: クォータニオン（4 次元）で表現
- **誤差**: オイラー角（3 次元）で表現
- **更新**: 誤差を推定 → クォータニオンに反映

```python
# 誤差関数定義（ESKF パラメータ）
eskf_params = [
  err_eqs,           # 誤差関数: true_state = nominal_state + error
  inv_err_eqs,       # 逆誤差関数: error = true_state - nominal_state
  H_mod_sym,         # 観測行列の修正
  f_err_sym,         # 誤差の状態遷移
  x_err_sym          # 誤差状態ベクトル
]

gen_code(..., eskf_params=eskf_params)
```

**使用例**: PoseKalman で姿勢推定時にクォータニオンを使用

**根拠**: README.md の "Error State Kalman Filter" セクション、`ekf_sym.py` の eskf_params

---

### 3. マルチステート制約カルマンフィルタ（MSCKF）

**視覚オドメトリの問題**:
- カメラで見た特徴点の深度を推定したい
- 深度推定には複数フレームでの追跡が必要
- フレーム位置はカルマンフィルタの状態に依存 → 循環依存

**MSCKF の解決策**:
- 過去 N フレームの状態を保持（マルチステート）
- 特徴点観測を制約条件として扱う
- 正のフィードバックループを回避

```python
msckf_params = [
  dim_main,          # メイン状態のサイズ
  dim_augment,       # 1つの拡張状態のサイズ
  dim_main_err,      # メイン誤差のサイズ
  dim_augment_err,   # 拡張誤差のサイズ
  N,                 # 保持するフレーム数
  feature_track_kinds # 特徴追跡の種類
]

gen_code(..., msckf_params=msckf_params)
```

**根拠**: README.md の "Multi-State Constraint Kalman Filter" セクション、`ekf_sym.py` の msckf_params

---

### 4. Rauch-Tung-Striebel スムージング

**オフライン推定の問題**:
- フィルタの前向き処理のみだと、初期化期間の推定精度が低い
- リアルタイムでは過去に戻れない

**RTS スムージングの解決策**:
- データを逆方向（後ろ→前）にも処理
- 前向き・後ろ向きを複数回繰り返し
- グローバル最適化に近い精度を達成

**用途**: オフラインでのログ解析、高精度な事後推定

**根拠**: README.md の "Rauch–Tung–Striebel smoothing" セクション

---

### 5. マハラノビス距離による外れ値除去

**センサーデータの問題**:
- すべてのデータがガウス分布とは限らない
- 外れ値（異常値）が含まれる

**外れ値除去の仕組み**:
```python
# カイ二乗分布の閾値
maha_thresh = chi2_ppf(0.95, observation_dim)

# マハラノビス距離が閾値以上なら棄却
if mahalanobis_distance > maha_thresh:
    reject_observation()
```

**効果**: 異常なセンサーデータを自動的に除外、推定精度向上

**根拠**: README.md の "Mahalanobis distance outlier rejector" セクション、`ekf_sym.py` の maha_test

---

## コード生成フロー

### 1. シンボリック定義（Python）

ユーザー（openpilot 開発者）が Pythonで状態遷移関数と観測方程式を定義:

```python
import sympy as sp

# 状態ベクトル定義
state = sp.Matrix([position, velocity])

# 時間微分
dt = sp.Symbol('dt')
state_dot = sp.Matrix([velocity, 0])

# 状態遷移関数（運動方程式）
f_sym = state + dt * state_dot

# 観測方程式（位置のみ観測）
h_sym = sp.Matrix([position])
```

---

### 2. ヤコビアン自動計算

rednose が sympy でヤコビアンを自動計算:

```python
# 状態遷移のヤコビアン
F_sym = f_sym.jacobian(state_sym)

# 観測のヤコビアン
H_sym = h_sym.jacobian(state_sym)
```

---

### 3. C++ コード生成

sympy から C++ コードを生成:

```python
from rednose.helpers.ekf_sym import gen_code

gen_code(
  folder='generated/',
  name='kinematic',
  f_sym=f_sym,
  dt_sym=dt,
  x_sym=state_sym,
  obs_eqs=[[h_sym, ObservationKind.POSITION, None]],
  dim_x=2,
  dim_err=2
)
```

**生成されるファイル**:
- `generated/kinematic.cpp`: C++ 実装
- `generated/kinematic.h`: ヘッダーファイル
- `generated/libkinematic.so`: 共有ライブラリ

---

### 4. Cython 経由で Python から使用

生成されたコードを Python から呼び出し:

```python
from rednose.helpers.ekf_sym_pyx import EKF_sym_pyx

# EKF インスタンス生成
filter = EKF_sym_pyx(
  generated_dir='generated/',
  name='kinematic',
  Q=process_noise,
  initial_x=initial_state,
  initial_P=initial_covariance,
  dim_state=2,
  dim_state_err=2
)

# 予測ステップ
filter.predict(dt)

# 更新ステップ
filter.update(ObservationKind.POSITION, measurement, R)
```

---

## 依存パッケージ

### requirements.txt

```
ruff          # Linter/Formatter
sympy         # シンボリック数学（微分計算の核心）
numpy         # 数値計算
scipy         # 科学計算（カイ二乗分布など）
tqdm          # プログレスバー
cffi          # C FFI（Python と C の連携）
scons         # ビルドシステム
pre-commit    # Git hooks
Cython        # Python-C 拡張（高速化）
pytest        # テストフレームワーク
pytest-xdist  # 並列テスト
```

**重要な依存**:
- **sympy**: シンボリック微分の核心
- **cffi**: 生成された C++ コードを Python から呼び出す
- **Cython**: 高速なバインディング層

**根拠**: `rednose_repo/requirements.txt`

---

## ビルドシステム

### SConstruct

rednose は **SCons** を使用して C++/Cython コードをビルドします。

```python
env = Environment(
  CC='clang',
  CXX='clang++',
  CCFLAGS=["-g", "-fPIC", "-O2", ...],
  CFLAGS="-std=gnu11",
  CXXFLAGS="-std=c++1z",
  tools=["default", "cython", "rednose_filter"],
)
```

**カスタムツール**: `rednose_filter` ツールでフィルタコードのビルドを自動化

**根拠**: `rednose_repo/SConstruct`

---

## まとめ

### rednose の位置づけ

1. **汎用ライブラリ**: openpilot 専用ではなく、広範なカルマンフィルタ用途に対応
2. **Submodule 管理**: 独立したリポジトリとして開発・保守
3. **高度な機能**: EKF、ESKF、MSCKF、RTS スムージング、外れ値除去
4. **自動化**: シンボリック微分でヤコビアン自動計算、C++ コード自動生成

---

### openpilot での役割

1. **姿勢推定**: PoseKalman でデバイスの姿勢・速度・加速度を推定
2. **車両状態推定**: CarKalman で車両パラメータと運動状態を推定
3. **センサーフュージョン**: GPS、IMU、カメラなどの複数センサーを統合
4. **高精度**: シンボリック微分とネイティブコードで正確かつ高速

---

### 技術的強み

1. **自動微分**: 手動計算不要、計算ミスゼロ
2. **高速実行**: Cython/C++ でネイティブ実装
3. **柔軟性**: ESKF、MSCKF など高度な手法をサポート
4. **保守性**: Python で定義、自動コード生成で保守が容易

---

## 詳細ドキュメント

- **[EKF シンボリックコード生成詳細](ekf_sym_detail.md)**: `ekf_sym.py` の詳細解説
- **[KalmanFilter 基底クラス詳細](kalmanfilter_detail.md)**: `kalmanfilter.py` と使用方法
- **[Sympy ヘルパー関数詳細](sympy_helpers_detail.md)**: 回転・変換の数学的実装
- **[openpilot 統合詳細](integration_detail.md)**: PoseKalman と CarKalman の詳細

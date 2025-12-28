# rednose と rednose_repo の関係性

## 概要

openpilotには**rednose**と**rednose_repo**という2つのディレクトリが存在します。

- **rednose_repo**: Git submodule（独立したリポジトリ）
- **rednose**: rednose_repo/rednose へのシンボリックリンク

```
openpilot/
├── rednose -> rednose_repo/rednose   (シンボリックリンク)
└── rednose_repo/                     (Git submodule)
    ├── .git/
    ├── README.md
    ├── LICENSE
    ├── SConstruct
    ├── setup.py
    ├── pyproject.toml
    ├── requirements.txt
    ├── Dockerfile
    ├── examples/
    └── rednose/                      (実体)
        ├── __init__.py
        ├── helpers/
        ├── logger/
        └── templates/
```

**根拠**:
```bash
$ ls -la openpilot_202505/rednose
lrwxrwxrwx 1 takuya takuya 20  6月 27 14:51 rednose -> rednose_repo/rednose
```

---

## rednose_repo の正体

### Git Submodule

**rednose_repo**は、openpilotに組み込まれた**外部リポジトリ**です。

```bash
$ cat .gitmodules
[submodule "rednose_repo"]
  path = rednose_repo
  url = https://github.com/est2mzd/rednose.git

$ git submodule status | grep rednose
 98ad80ec306ec2fd895239e672d7cddce82e7d0c rednose_repo (remotes/origin/annotation)
```

**リポジトリ情報**:
- **URL**: https://github.com/est2mzd/rednose.git
- **コミット**: 98ad80ec306ec2fd895239e672d7cddce82e7d0c
- **ブランチ**: remotes/origin/annotation

**根拠**: `.gitmodules` ファイルと `git submodule status` コマンドの出力

---

### 独立したライブラリ

rednose_repoは**スタンドアロンライブラリ**として開発されています。

**README.md**より:
```markdown
## Introduction
The kalman filter framework described here is an incredibly powerful tool for any optimization problem,
but particularly for visual odometry, sensor fusion localization or SLAM.
```

**特徴**:
- 独自のビルドシステム（SConstruct）
- 独自のパッケージ設定（setup.py, pyproject.toml）
- サンプルコード（examples/）
- テストコード（test_*.py）
- 汎用的な設計（openpilot非依存）

**根拠**: `rednose_repo/README.md`, `rednose_repo/SConstruct`, `rednose_repo/setup.py` の存在

---

## rednose シンボリックリンク

### なぜシンボリックリンクか？

**msgq や opendbc と同様の構造**:
1. 当初、rednoseは`openpilot/rednose/`に直接存在（推測）
2. 汎用ライブラリとして分離（rednoseリポジトリ作成）
3. 既存コードの互換性のため、`rednose -> rednose_repo/rednose`にリンク

```bash
$ readlink -f rednose
/home/takuya/work/comma/openpilot_202505/rednose_repo/rednose
```

**メリット**:
- 既存のimport文が変更不要: `from rednose.helpers import ...`
- ビルドシステム（SConstruct）も変更最小限

---

## ディレクトリ構造比較

### rednose_repo/（リポジトリルート）

```
rednose_repo/
├── README.md           # ライブラリの説明
├── LICENSE             # MITライセンス
├── setup.py            # Pythonパッケージ設定
├── pyproject.toml      # ruffやpytestの設定
├── requirements.txt    # 依存パッケージ
├── SConstruct          # ビルド設定（SCons）
├── Dockerfile          # Dockerイメージ定義
├── .github/            # GitHub Actions設定
├── site_scons/         # カスタムSConsツール
├── examples/           # サンプルコード
│   ├── kinematic_kf.py      # 運動学的カルマンフィルタの例
│   ├── live_kf.py           # リアルタイム例
│   ├── test_kinematic_kf.py # テストコード
│   └── test_compare.py      # 比較テスト
└── rednose/            # ライブラリ本体（実体）
    ├── __init__.py
    ├── helpers/        # ヘルパーモジュール
    ├── logger/         # ロギング機能
    └── templates/      # Cコード生成テンプレート
```

**根拠**: `ls -la rednose_repo/` の出力

---

### rednose/（実体ディレクトリ）

```
rednose/
├── __init__.py         # 空ファイル
├── helpers/            # コアヘルパーモジュール
│   ├── __init__.py           # FFI/コードローダー
│   ├── kalmanfilter.py       # KalmanFilterベースクラス
│   ├── ekf_sym.py            # シンボリック拡張カルマンフィルタ
│   ├── sympy_helpers.py      # Sympy補助関数
│   ├── chi2_lookup.py        # カイ二乗分布テーブル
│   ├── chi2_lookup_table.npy # カイ二乗分布データ
│   ├── ekf_sym_pyx.pyx       # Cython拡張モジュール
│   ├── ekf_sym_pyx.cpp       # コンパイル済みCython
│   ├── ekf_sym_pyx.so        # 共有ライブラリ
│   ├── ekf_load.cc/h         # C++実装
│   ├── ekf_sym.cc/h          # C++実装
│   └── libekf_sym.a          # スタティックライブラリ
├── logger/             # ロギング
│   └── logger.h            # Cヘッダー
└── templates/          # コード生成テンプレート
    ├── compute_pos.c       # 位置計算
    ├── ekf_c.c             # EKFコア実装
    └── feature_handler.c   # 特徴量処理
```

**根拠**: `ls -la rednose_repo/rednose/` および `ls -la rednose_repo/rednose/helpers/` の出力

---

## パッケージ情報

### setup.py

```python
setup(
  name='rednose',
  version='0.0.1',
  url='https://github.com/commaai/rednose',
  author='comma.ai',
  author_email='harald@comma.ai',
  packages=find_packages(),
  platforms='any',
  license='MIT',
  package_data={'': ['helpers/chi2_lookup_table.npy', 'templates/*']},
  install_requires=[
    'sympy',
    'numpy',
    'scipy',
    'tqdm',
    'cffi',
  ],
  description="Kalman filter library",
)
```

**重要ポイント**:
- **オリジナル**: https://github.com/commaai/rednose
- **作者**: comma.ai (harald@comma.ai)
- **ライセンス**: MIT
- **主要依存**: sympy, numpy, scipy, cffi

**根拠**: `rednose_repo/setup.py`

---

### requirements.txt

```
ruff          # Linter/Formatter
sympy         # シンボリック数学
numpy         # 数値計算
scipy         # 科学計算
tqdm          # プログレスバー
cffi          # C FFI
scons         # ビルドシステム
pre-commit    # Git hooks
Cython        # Python-C拡張
pytest        # テストフレームワーク
pytest-xdist  # 並列テスト
```

**根拠**: `rednose_repo/requirements.txt`

---

## ビルドシステム

### SConstruct

rednoseは**SCons**を使用した独自のビルドシステムを持っています。

```python
env = Environment(
  CC='clang',
  CXX='clang++',
  CCFLAGS=["-g", "-fPIC", "-O2", ...],
  CFLAGS="-std=gnu11",
  CXXFLAGS="-std=c++1z",
  CPPPATH=[...],
  tools=["default", "cython", "rednose_filter"],
)
```

**主要機能**:
- **Cythonビルド**: Pythonコードをネイティブコードに変換
- **C/C++コンパイル**: clang/clang++使用
- **カスタムツール**: `rednose_filter` ツール

**根拠**: `rednose_repo/SConstruct`

---

## 主要コンポーネント

### 1. KalmanFilter ベースクラス

**ファイル**: `rednose/helpers/kalmanfilter.py`

```python
class KalmanFilter:
  name = "<name>"
  initial_x = np.zeros((0, 0))     # 初期状態
  initial_P_diag = np.zeros((0, 0)) # 初期共分散対角成分
  Q = np.zeros((0, 0))              # プロセスノイズ
  obs_noise: dict[int, Any] = {}    # 観測ノイズ
  
  @property
  def x(self):
    return self.filter.state()     # 現在の状態
  
  @property
  def P(self):
    return self.filter.covs()      # 現在の共分散行列
```

**役割**: すべてのカルマンフィルタ実装の基底クラス

**根拠**: `rednose_repo/rednose/helpers/kalmanfilter.py`

---

### 2. EKF シンボリックコード生成

**ファイル**: `rednose/helpers/ekf_sym.py`

**主要機能**:
- **シンボリック微分**: sympyで自動的にヤコビアンを計算
- **Cコード生成**: 最適化されたC++コードを生成
- **ESKF対応**: Error-State Kalman Filter実装
- **MSCKF対応**: Multi-State Constraint Kalman Filter実装

```python
def gen_code(folder, name, f_sym, dt_sym, x_sym, obs_eqs, dim_x, dim_err, 
             eskf_params=None, msckf_params=None, ...):
  # シンボリックな状態遷移関数からヤコビアンを計算
  F_sym = f_err_sym.jacobian(x_err_sym)
  
  # 観測方程式のヤコビアン計算
  for obs_eq in obs_eqs:
    obs_eq.append(obs_eq[0].jacobian(x_sym))
  
  # Cコード生成
  ...
```

**根拠**: `rednose_repo/rednose/helpers/ekf_sym.py`

---

### 3. Sympy ヘルパー関数

**ファイル**: `rednose/helpers/sympy_helpers.py`

**主要機能**:
- **回転行列計算**: `euler_rotate`, `quat_rotate`
- **オイラー角変換**: `rot_to_euler`
- **Cコード変換**: `sympy_into_c`

**根拠**: インポート文およびファイル存在から推測

---

### 4. カイ二乗検定（外れ値除去）

**ファイル**: 
- `rednose/helpers/chi2_lookup.py`
- `rednose/helpers/chi2_lookup_table.npy`

**目的**: マハラノビス距離による外れ値検出

**根拠**: README.mdの記述およびファイル存在

---

## openpilot での使用例

### 1. locationd/models/car_kf.py

**用途**: 車両状態推定（車両モデルパラメータ、速度、ヨーレートなど）

```python
from rednose.helpers.kalmanfilter import KalmanFilter
from rednose.helpers.ekf_sym import gen_code
from rednose.helpers.ekf_sym_pyx import EKF_sym_pyx

class CarKalman(KalmanFilter):
  name = 'car'
  
  initial_x = np.array([
    1.0,      # STIFFNESS
    15.0,     # STEER_RATIO
    0.0,      # ANGLE_OFFSET
    0.0,      # ANGLE_OFFSET_FAST
    10.0, 0.0, # VELOCITY (x, y)
    0.0,      # YAW_RATE
    0.0,      # STEER_ANGLE
    0.0       # ROAD_ROLL
  ])
```

**根拠**: `selfdrive/locationd/models/car_kf.py`

---

### 2. locationd/models/pose_kf.py

**用途**: デバイス姿勢推定（ロール、ピッチ、ヨー、速度、角速度など）

```python
from rednose.helpers.kalmanfilter import KalmanFilter
from rednose.helpers.ekf_sym import gen_code
from rednose.helpers.sympy_helpers import euler_rotate, rot_to_euler
from rednose.helpers.ekf_sym_pyx import EKF_sym_pyx

class PoseKalman(KalmanFilter):
  name = "pose"
  
  initial_x = np.array([
    0.0, 0.0, 0.0,  # NED_ORIENTATION (roll, pitch, yaw)
    0.0, 0.0, 0.0,  # DEVICE_VELOCITY
    0.0, 0.0, 0.0,  # ANGULAR_VELOCITY
    0.0, 0.0, 0.0,  # GYRO_BIAS
    0.0, 0.0, 0.0,  # ACCELERATION
    0.0, 0.0, 0.0   # ACCEL_BIAS
  ])
```

**根拠**: `selfdrive/locationd/models/pose_kf.py`

---

## 技術的特徴

### 1. Extended Kalman Filter (EKF)

**特徴**:
- 非線形システムを線形化して推定
- シンボリック微分でヤコビアン自動計算
- 手動計算のエラーを排除

**根拠**: README.md の "Extended Kalman Filter with symbolic Jacobian computation" セクション

---

### 2. Error-State Kalman Filter (ESKF)

**特徴**:
- 状態と誤差を異なる空間で表現
- クォータニオンで姿勢表現、オイラー角で誤差表現
- 冗長次元問題を回避

**根拠**: README.md の "Error State Kalman Filter" セクション

---

### 3. Multi-State Constraint Kalman Filter (MSCKF)

**特徴**:
- 視覚的オドメトリとカルマンフィルタの統合
- 特徴追跡による深度推定
- 正のフィードバックループ回避

**根拠**: README.md の "Multi-State Constraint Kalman Filter" セクション

---

### 4. Rauch-Tung-Striebel スムージング

**特徴**:
- オフライン推定の品質向上
- 前後方向の処理でグローバル最適化に近づける
- 初期化期間の推定精度改善

**根拠**: README.md の "Rauch–Tung–Striebel smoothing" セクション

---

### 5. マハラノビス距離による外れ値除去

**特徴**:
- ガウス分布でない測定値への対処
- 統計的テストで外れ値検出
- 良好な初期化が必要

**根拠**: README.md の "Mahalanobis distance outlier rejector" セクション

---

## コード生成フロー

### 1. シンボリック定義（Python）

```python
# 状態ベクトル定義
state_sym = sp.MatrixSymbol('state', dim_state, 1)

# 状態遷移関数定義
f_sym = state + dt * state_dot

# 観測方程式定義
h_sym = sp.Matrix([position])
```

---

### 2. ヤコビアン自動計算

```python
# ヤコビアン計算（自動微分）
F_sym = f_sym.jacobian(state_sym)
H_sym = h_sym.jacobian(state_sym)
```

---

### 3. Cコード生成

```python
gen_code(generated_dir, name, f_sym, dt, state_sym, obs_eqs, 
         dim_state, dim_state_err)
```

生成されるファイル:
- `{name}.cpp` - C++実装
- `{name}.h` - ヘッダーファイル
- `lib{name}.so` - 共有ライブラリ

---

### 4. Cythonラッパー経由で使用

```python
from rednose.helpers.ekf_sym_pyx import EKF_sym_pyx

self.filter = EKF_sym_pyx(generated_dir, name, Q, 
                          initial_x, initial_P, 
                          dim_state, dim_state_err)
```

---

## まとめ

### rednoseの位置づけ

1. **独立したライブラリ**: openpilot専用ではなく、汎用的なカルマンフィルタフレームワーク
2. **Submodule管理**: Git submoduleとして外部リポジトリを参照
3. **シンボリックリンク**: 既存コードの互換性維持のため
4. **高度な機能**: EKF、ESKF、MSCKF、RTSスムージング、外れ値除去

---

### openpilotでの役割

1. **車両状態推定**: CarKalmanで車両パラメータと動作状態を推定
2. **姿勢推定**: PoseKalmanでデバイスの姿勢と動きを推定
3. **センサーフュージョン**: 複数センサーデータの統合
4. **ロバストな推定**: 外れ値除去とスムージングで高精度化

---

### 技術的強み

1. **自動微分**: 手動計算不要、エラーレス
2. **高速実行**: Cython/C++でネイティブ実装
3. **柔軟性**: ESKF、MSCKFなど高度な手法をサポート
4. **メンテナンス性**: Pythonでの定義、自動コード生成

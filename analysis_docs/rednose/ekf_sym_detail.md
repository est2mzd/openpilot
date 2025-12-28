# EKF シンボリックコード生成詳細

## 概要

`rednose/helpers/ekf_sym.py` は、rednose の核心となるモジュールであり、シンボリック微分によるヤコビアン自動計算とC++コード生成を担当します。

- **ファイル**: `rednose/helpers/ekf_sym.py`
- **行数**: 691行
- **主要関数**: `gen_code()`, `EKF_sym` クラス
- **技術**: sympy（シンボリック数学）、cffi（C FFI）、コード生成

---

## gen_code() 関数

### 関数シグネチャ

```python
def gen_code(folder, name, f_sym, dt_sym, x_sym, obs_eqs, dim_x, dim_err, 
             eskf_params=None, msckf_params=None, 
             maha_test_kinds=[], quaternion_idxs=[], 
             global_vars=None, extra_routines=[]):
```

### パラメータ解説

| パラメータ | 型 | 説明 |
|-----------|-----|------|
| `folder` | str | 生成コードの出力先ディレクトリ |
| `name` | str | フィルタ名（例: "car", "pose", "kinematic"） |
| `f_sym` | sp.Matrix | 状態遷移関数（シンボリック） |
| `dt_sym` | sp.Symbol | 時間刻み幅のシンボル |
| `x_sym` | sp.MatrixSymbol | 状態ベクトルのシンボル |
| `obs_eqs` | list | 観測方程式のリスト `[[h_sym, kind, ea_sym], ...]` |
| `dim_x` | int | 状態ベクトルの次元 |
| `dim_err` | int | 誤差ベクトルの次元（ESKF の場合は dim_x と異なる） |
| `eskf_params` | tuple | ESKF パラメータ（オプション） |
| `msckf_params` | tuple | MSCKF パラメータ（オプション） |
| `maha_test_kinds` | list | マハラノビス距離テストを行う観測の種類 |
| `quaternion_idxs` | list | クォータニオンのインデックス（正規化が必要） |
| `global_vars` | list | グローバル変数（車両パラメータなど） |
| `extra_routines` | list | 追加の関数定義 |

---

## コード生成の流れ

### 1. ESKF/MSCKF パラメータの処理

```python
if eskf_params:
    err_eqs = eskf_params[0]           # 誤差関数
    inv_err_eqs = eskf_params[1]       # 逆誤差関数
    H_mod_sym = eskf_params[2]         # 観測行列修正
    f_err_sym = eskf_params[3]         # 誤差状態遷移
    x_err_sym = eskf_params[4]         # 誤差状態ベクトル
else:
    # 通常の EKF（誤差 = 状態の差）
    err_function_sym = sp.Matrix(nom_x + delta_x)
    inv_err_function_sym = sp.Matrix(true_x - nom_x)
    H_mod_sym = sp.Matrix(np.eye(dim_x))
    f_err_sym = f_sym
    x_err_sym = x_sym
```

**ESKF の場合**:
- 状態と誤差を異なる空間で表現
- 例: 状態はクォータニオン（4次元）、誤差はオイラー角（3次元）

**通常の EKF の場合**:
- 誤差 = 真の状態 - 推定状態（同じ空間）

---

### 2. ヤコビアン自動計算

```python
# 状態遷移関数のヤコビアン F = ∂f/∂x
F_sym = f_err_sym.jacobian(x_err_sym)

# ESKF の場合、誤差=0 の点で線形化
if eskf_params:
    for sym in x_err_sym:
        F_sym = F_sym.subs(sym, 0)

# 観測方程式のヤコビアン H = ∂h/∂x
for i in range(len(obs_eqs)):
    obs_eqs[i].append(obs_eqs[i][0].jacobian(x_sym))
    
    # MSCKF の場合、特徴追跡のヤコビアンも計算
    if msckf and obs_eqs[i][1] in feature_track_kinds:
        obs_eqs[i].append(obs_eqs[i][0].jacobian(obs_eqs[i][2]))
    else:
        obs_eqs[i].append(None)
```

**自動微分の威力**:
- 複雑な非線形関数でも、sympy が正確に偏微分を計算
- 手動計算のミスがゼロ
- 状態遷移関数を変更すれば、ヤコビアンも自動更新

---

### 3. シンボリック関数のリスト化

```python
sympy_functions = []

# 誤差関数
sympy_functions.append(('err_fun', err_eqs[0], [err_eqs[1], err_eqs[2]]))
sympy_functions.append(('inv_err_fun', inv_err_eqs[0], [inv_err_eqs[1], inv_err_eqs[2]]))

# 観測行列修正（ESKF 用）
sympy_functions.append(('H_mod_fun', H_mod_sym, [x_sym]))

# 状態遷移関数とヤコビアン
sympy_functions.append(('f_fun', f_sym, [x_sym, dt_sym]))
sympy_functions.append(('F_fun', F_sym, [x_sym, dt_sym]))

# 観測関数とヤコビアン
for h_sym, kind, ea_sym, H_sym, He_sym in obs_eqs:
    sympy_functions.append(('h_%d' % kind, h_sym, [x_sym, ea_sym]))
    sympy_functions.append(('H_%d' % kind, H_sym, [x_sym, ea_sym]))
    if msckf and kind in feature_track_kinds:
        sympy_functions.append(('He_%d' % kind, He_sym, [x_sym, ea_sym]))
```

**生成される関数**:
- `f_fun`: 状態遷移関数 $x_{t+1} = f(x_t, dt)$
- `F_fun`: 状態遷移のヤコビアン $F = \frac{\partial f}{\partial x}$
- `h_{kind}`: 観測関数 $z = h(x)$
- `H_{kind}`: 観測のヤコビアン $H = \frac{\partial h}{\partial x}$
- `err_fun`, `inv_err_fun`: ESKF の誤差関数
- `H_mod_fun`: ESKF の観測行列修正

---

### 4. C++ コード生成

```python
from rednose.helpers.sympy_helpers import sympy_into_c

# sympy 関数を C コードに変換
sympy_header, code = sympy_into_c(sympy_functions, global_vars)
```

**sympy_into_c の役割**:
- sympy の数式を C99 コードに変換
- 関数プロトタイプ（ヘッダー）と実装（コード）を生成

**例**:
```python
# sympy 定義
f_sym = sp.Matrix([x[0] + dt * x[1], x[1]])

# 生成される C コード
void f_fun(double *state, double dt, double *out) {
    out[0] = state[0] + dt * state[1];
    out[1] = state[1];
}
```

---

### 5. 更新関数の生成

各観測の種類ごとに更新関数を生成:

```python
for h_sym, kind, ea_sym, H_sym, He_sym in obs_eqs:
    # マハラノビス距離の閾値（95%信頼区間）
    maha_thresh = chi2_ppf(0.95, int(h_sym.shape[0]))
    maha_test = kind in maha_test_kinds
    
    # 更新関数の生成
    header += f"void {name}_update_{kind}(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea);\n"
    
    post_code += f"void {name}_update_{kind}(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea) {{\n"
    post_code += f"  update<{h_sym.shape[0]}, 3, {int(maha_test)}>(in_x, in_P, h_{kind}, H_{kind}, {He_str}, in_z, in_R, in_ea, MAHA_THRESH_{kind});\n"
    post_code += "}\n"
```

**更新関数のパラメータ**:
- `in_x`: 状態ベクトル（入出力）
- `in_P`: 共分散行列（入出力）
- `in_z`: 観測値
- `in_R`: 観測ノイズ共分散
- `in_ea`: 追加パラメータ（MSCKF 用）

---

### 6. テンプレートとの結合

```python
# EKF コアテンプレートの読み込み
with open(os.path.join(TEMPLATE_DIR, "ekf_c.c"), encoding='utf-8') as f:
    template_code = f.read()

# コードブロックの結合
code = "\n".join([pre_code, code, template_code, post_code])
```

**ekf_c.c テンプレート**:
- カルマンフィルタの予測・更新アルゴリズムの実装
- 行列演算（共分散伝播など）
- マハラノビス距離計算

---

### 7. ファイル出力

```python
# ヘッダーファイル
with open(os.path.join(folder, f"{name}.h"), 'w', encoding='utf-8') as f:
    f.write(header)

# C++ ファイル
with open(os.path.join(folder, f"{name}.cpp"), 'w', encoding='utf-8') as f:
    f.write(code)
```

**生成されるファイル**:
- `{name}.h`: 関数プロトタイプ（FFI でロード用）
- `{name}.cpp`: フル実装

---

## EKF_sym クラス

### クラス定義

```python
class EKF_sym():
  def __init__(self, folder, name, Q, x_initial, P_initial, 
               dim_main, dim_main_err, 
               N=0, dim_augment=0, dim_augment_err=0, 
               maha_test_kinds=[], quaternion_idxs=[], 
               global_vars=None, max_rewind_age=1.0, logger=logging):
```

### 主要な機能

#### 1. 状態管理

```python
# 初期状態
self.x = x_initial.reshape((-1, 1))      # 状態ベクトル
self.P = P_initial                        # 共分散行列
self.filter_time = None                   # フィルタ時刻
```

#### 2. MSCKF サポート

```python
self.msckf = N > 0
self.N = N                               # 保持するフレーム数
self.dim_augment = dim_augment           # 拡張状態のサイズ
self.dim_main = dim_main                 # メイン状態のサイズ

# 状態ベクトル構造（MSCKF の場合）
# x = [main_state, augment_0, augment_1, ..., augment_{N-1}]
assert dim_main + dim_augment * N == self.dim_x
```

#### 3. C++ コードのロードとラップ

```python
from rednose.helpers import load_code

ffi, lib = load_code(folder, name)

# C 関数を Python から呼び出せるようにラップ
def wrap_1list_1float(func_name):
    func = eval(f"lib.{name}_{func_name}", {"lib": lib})
    
    def ret(lst1, fl, out):
        func(ffi.cast("double *", lst1.ctypes.data),
             ffi.cast("double", fl),
             ffi.cast("double *", out.ctypes.data))
    return ret

self.f = wrap_1list_1float("f_fun")
self.F = wrap_1list_1float("F_fun")
```

**cffi の役割**:
- 生成された C++ 関数を Python から呼び出す
- numpy 配列を C の double* にキャスト

---

## 使用例: CarKalman

### 1. コード生成

```python
@staticmethod
def generate_code(generated_dir):
    dim_state = CarKalman.initial_x.shape[0]
    name = CarKalman.name
    
    # グローバル変数（車両パラメータ）
    global_vars = [sp.Symbol(name) for name in CarKalman.global_vars]
    m, j, aF, aR, cF_orig, cR_orig = global_vars  # 質量、慣性、重心位置、剛性
    
    # 状態ベクトル
    state_sym = sp.MatrixSymbol('state', dim_state, 1)
    state = sp.Matrix(state_sym)
    
    # 車両モデル（2輪モデル）
    sf = state[States.STIFFNESS, :][0, 0]  # タイヤ剛性係数
    cF, cR = sf * cF_orig, sf * cR_orig      # 前後タイヤ剛性
    
    # 状態方程式（横速度・ヨーレートの微分）
    A = sp.Matrix([
        [-(cF + cR) / (m * u), -(cF * aF - cR * aR) / (m * u) - u],
        [-(cF * aF - cR * aR) / (j * u), -(cF * aF**2 + cR * aR**2) / (j * u)]
    ])
    
    B = sp.Matrix([[cF / m / sR], [(cF * aF) / j / sR]])
    
    x = sp.Matrix([v, r])  # 横速度、ヨーレート
    x_dot = A * x + B * (sa - angle_offset - angle_offset_fast)
    
    # 状態遷移関数（1次離散化）
    f_sym = state + dt * state_dot
    
    # 観測方程式
    obs_eqs = [
        [sp.Matrix([r]), ObservationKind.ROAD_FRAME_YAW_RATE, None],
        [sp.Matrix([u, v]), ObservationKind.ROAD_FRAME_XY_SPEED, None],
        [sp.Matrix([sa]), ObservationKind.STEER_ANGLE, None],
        ...
    ]
    
    # コード生成
    gen_code(generated_dir, name, f_sym, dt, state_sym, obs_eqs, 
             dim_state, dim_state, global_vars=global_vars)
```

**車両モデルの物理**:
- **2輪モデル**: 前輪・後輪を1つずつのタイヤで近似
- **状態**: 横速度 $v$、ヨーレート $r$
- **入力**: ステアリング角 $\delta$
- **パラメータ**: 質量 $m$、ヨー慣性 $J$、重心位置 $a_F, a_R$、タイヤ剛性 $c_F, c_R$

**参考文献**: "The Science of Vehicle Dynamics: Handling, Braking, and Ride of Road and Race Cars"

---

### 2. フィルタの初期化と使用

```python
def __init__(self, generated_dir):
    dim_state = CarKalman.initial_x.shape[0]
    dim_state_err = CarKalman.P_initial.shape[0]
    
    self.filter = EKF_sym_pyx(
        generated_dir, 
        CarKalman.name, 
        CarKalman.Q, 
        CarKalman.initial_x, 
        CarKalman.P_initial,
        dim_state, 
        dim_state_err, 
        global_vars=CarKalman.global_vars
    )

def set_globals(self, mass, rotational_inertia, center_to_front, ...):
    self.filter.set_global("mass", mass)
    self.filter.set_global("rotational_inertia", rotational_inertia)
    ...
```

---

## ESKF の詳細

### 誤差状態の定義

**問題**: クォータニオン $q$ は4次元だが、3D回転は3自由度

**解決策**: 
- **Nominal State**: $x_{\text{nom}} = [q, v, p, ...]$ （クォータニオン使用）
- **Error State**: $\delta x = [\delta\theta, \delta v, \delta p, ...]$ （オイラー角使用）
- **True State**: $x_{\text{true}} = x_{\text{nom}} \boxplus \delta x$

### 誤差関数

```python
# err_fun: True State = Nominal State ⊕ Error
true_state = err_fun(nominal_state, error)

# inv_err_fun: Error = True State ⊖ Nominal State  
error = inv_err_fun(nominal_state, true_state)
```

**クォータニオンの場合**:
```python
# Error → True の変換
q_true = q_nominal * exp(0.5 * δθ)  # クォータニオン積

# True → Error の逆変換
δθ = 2 * log(q_nominal^{-1} * q_true)
```

### 観測行列の修正

```python
# 通常の EKF
K = P H^T (H P H^T + R)^{-1}
x = x + K (z - h(x))

# ESKF
K = P H_mod^T H^T (H H_mod P H_mod^T H^T + R)^{-1}
δx = δx + K (z - h(x))
x = x ⊕ δx
```

**H_mod の役割**: 誤差空間と状態空間の次元ギャップを埋める

---

## MSCKF の詳細

### 状態拡張（Augmentation）

新しいカメラフレームごとに、現在の状態を履歴に追加:

```python
# メイン状態（現在の姿勢・速度など）
x_main = [position, velocity, orientation, ...]

# 拡張状態（過去のカメラ姿勢）
x_augment = [position_t0, orientation_t0, 
             position_t1, orientation_t1,
             ...,
             position_tN, orientation_tN]

# フル状態
x = [x_main, x_augment]
```

### 特徴追跡の観測

複数フレームで観測された特徴点:

```python
# フレーム i での観測
z_i = [u_i, v_i]  # 画像座標

# 観測方程式
z_i = h(x_main, x_augment_i, feature_position)

# ヤコビアン
H = ∂h/∂x_main         # メイン状態に対する偏微分
He = ∂h/∂feature_pos   # 特徴位置に対する偏微分
```

### 制約による更新

特徴位置を消去（nullspace trick）:

```python
# He の nullspace を計算
N = null(He)

# プロジェクション
z_proj = N^T z
H_proj = N^T H

# 更新（特徴位置を含まない）
K = P H_proj^T (H_proj P H_proj^T + R_proj)^{-1}
x = x + K (z_proj - h_proj(x))
```

**効果**: 
- 特徴位置の不確実性を状態推定に伝播させない
- 正のフィードバックループを回避

---

## マハラノビス距離による外れ値除去

### マハラノビス距離の計算

```python
# 観測残差
y = z - h(x)

# 残差の共分散
S = H P H^T + R

# マハラノビス距離
d = sqrt(y^T S^{-1} y)
```

### 閾値判定

```python
# カイ二乗分布の95パーセンタイル
threshold = chi2_ppf(0.95, observation_dim)

if d > threshold:
    # 外れ値として棄却
    reject_observation()
else:
    # 正常な観測として更新
    update_state()
```

**統計的意味**:
- 観測が真の分布から来ている確率 < 5% なら棄却
- 誤検出率: 5%

---

## グローバル変数のサポート

### 設定

車両パラメータなど、実行時に変更可能な定数:

```python
global_vars = [
    sp.Symbol('mass'),
    sp.Symbol('rotational_inertia'),
    sp.Symbol('center_to_front'),
    ...
]

gen_code(..., global_vars=global_vars)
```

### 生成される C++ コード

```cpp
// グローバル変数の宣言
double mass;
double rotational_inertia;
...

// setter 関数
void set_mass(double x) { mass = x; }
void set_rotational_inertia(double x) { rotational_inertia = x; }
...
```

### Python から使用

```python
filter.set_global("mass", 1500.0)
filter.set_global("rotational_inertia", 2500.0)
```

---

## まとめ

### ekf_sym.py の役割

1. **シンボリック微分**: sympy でヤコビアンを自動計算
2. **C++ コード生成**: 高速なネイティブコードを自動生成
3. **高度な手法**: ESKF、MSCKF、外れ値除去をサポート
4. **柔軟性**: グローバル変数、カスタム関数をサポート

### 利点

1. **正確性**: 手動計算のミスがゼロ
2. **保守性**: 数式を変更すれば、コードも自動更新
3. **高速性**: Cython/C++ で実行
4. **汎用性**: 任意のカルマンフィルタに対応

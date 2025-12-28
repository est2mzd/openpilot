# Sympy ヘルパー関数詳細

## 概要

`rednose/helpers/sympy_helpers.py` は、3D 回転・変換のシンボリック計算を提供します。

- **ファイル**: `rednose/helpers/sympy_helpers.py`
- **行数**: 276行
- **主要機能**: オイラー角↔クォータニオン↔回転行列の変換、Cコード生成
- **用途**: 姿勢推定（PoseKalman）における回転の数学的処理

---

## 3D 回転の表現方法

### 1. オイラー角（Euler Angles）

**定義**: 3つの角度で3D回転を表現
- **roll** ($\gamma$): X軸周りの回転
- **pitch** ($\theta$): Y軸周りの回転  
- **yaw** ($\psi$): Z軸周りの回転

**範囲**:
- roll, yaw: $-\pi$ 〜 $\pi$
- pitch: $-\pi/2$ 〜 $\pi/2$

**利点**:
- 直感的（人間に理解しやすい）
- 3つのパラメータで3自由度を表現（冗長性なし）

**欠点**:
- **ジンバルロック**: pitch = ±90° で1自由度を失う
- **非線形性**: 大きな角度で挙動が複雑

---

### 2. クォータニオン（Quaternion）

**定義**: 4つの数値で3D回転を表現
$$q = q_0 + q_1 i + q_2 j + q_3 k$$

**制約**: 
$$q_0^2 + q_1^2 + q_2^2 + q_3^2 = 1$$ 
（ノルム = 1）

**利点**:
- **ジンバルロックなし**
- **数値的に安定**
- **補間が滑らか**（SLERP）
- **計算効率が良い**（回転の合成が積算）

**欠点**:
- **直感的でない**（人間に理解しにくい）
- **冗長**: 4つのパラメータで3自由度（1次元余分）

---

### 3. 回転行列（Rotation Matrix）

**定義**: 3×3 の直交行列
$$R \in \mathbb{R}^{3 \times 3}, \quad R^T R = I, \quad \det(R) = 1$$

**利点**:
- **線形代数で扱いやすい**
- **ベクトル回転が簡単**: $v' = R v$

**欠点**:
- **冗長**: 9つのパラメータで3自由度
- **制約が複雑**: 直交性、行列式=1

---

## 主要関数

### 1. euler_rotate()

**シンボリック**: オイラー角から回転行列を生成

```python
def euler_rotate(roll, pitch, yaw):
    # roll 行列（X軸周り）
    matrix_roll = sp.Matrix([
        [1, 0, 0],
        [0, sp.cos(roll), -sp.sin(roll)],
        [0, sp.sin(roll), sp.cos(roll)]
    ])
    
    # pitch 行列（Y軸周り）
    matrix_pitch = sp.Matrix([
        [sp.cos(pitch), 0, sp.sin(pitch)],
        [0, 1, 0],
        [-sp.sin(pitch), 0, sp.cos(pitch)]
    ])
    
    # yaw 行列（Z軸周り）
    matrix_yaw = sp.Matrix([
        [sp.cos(yaw), -sp.sin(yaw), 0],
        [sp.sin(yaw), sp.cos(yaw), 0],
        [0, 0, 1]
    ])
    
    # 合成: R = R_yaw * R_pitch * R_roll
    return matrix_yaw * matrix_pitch * matrix_roll
```

**回転の順序**: Roll → Pitch → Yaw（内因性）

**使用例（PoseKalman）**:
```python
# 状態ベクトルから姿勢を取得
roll, pitch, yaw = state[States.NED_ORIENTATION, :]

# NED座標系からデバイス座標系への回転行列
ned_from_device = euler_rotate(roll, pitch, yaw)

# デバイス座標系の加速度をNED座標系に変換
accel_ned = ned_from_device * accel_device
```

---

### 2. quat_rotate()

**シンボリック**: クォータニオンから回転行列を生成

```python
def quat_rotate(q0, q1, q2, q3):
    return sp.Matrix([
        [q0**2 + q1**2 - q2**2 - q3**2, 2*(q1*q2 + q0*q3), 2*(q1*q3 - q0*q2)],
        [2*(q1*q2 - q0*q3), q0**2 - q1**2 + q2**2 - q3**2, 2*(q2*q3 + q0*q1)],
        [2*(q1*q3 + q0*q2), 2*(q2*q3 - q0*q1), q0**2 - q1**2 - q2**2 + q3**2]
    ]).T
```

**導出**: クォータニオンの回転公式から
$$v' = q \cdot v \cdot q^*$$
を行列形式で表現

**使用例**:
```python
# クォータニオン状態
q0, q1, q2, q3 = state[States.QUATERNION, :]

# 回転行列
R = quat_rotate(q0, q1, q2, q3)

# ベクトル回転
v_rotated = R * v
```

---

### 3. rot_to_euler()

**シンボリック**: 回転行列からオイラー角を抽出

```python
def rot_to_euler(R):
    gamma = sp.atan2(R[2, 1], R[2, 2])  # roll
    theta = sp.asin(-R[2, 0])            # pitch
    psi = sp.atan2(R[1, 0], R[0, 0])    # yaw
    return sp.Matrix([gamma, theta, psi])
```

**導出**: 回転行列の要素から逆算

**数値版** (numpy):
```python
def rot_to_euler_numpy(R):
    roll = np.arctan2(R[2, 1], R[2, 2])
    pitch = np.arcsin(-R[2, 0])
    yaw = np.arctan2(R[1, 0], R[0, 0])
    return np.array([roll, pitch, yaw])
```

**使用例（PoseKalman）**:
```python
# 時刻 t+1 の回転行列
ned_from_device_t1 = ned_from_device * device_from_device_t1

# オイラー角に戻す
f_sym[States.NED_ORIENTATION, :] = rot_to_euler(ned_from_device_t1)
```

**注意**: ジンバルロック時（pitch = ±90°）は不定

---

### 4. cross()

**シンボリック**: ベクトルの外積を行列で表現

```python
def cross(x):
    ret = sp.Matrix(np.zeros((3, 3)))
    ret[0, 1], ret[0, 2] = -x[2], x[1]
    ret[1, 0], ret[1, 2] = x[2], -x[0]
    ret[2, 0], ret[2, 1] = -x[1], x[0]
    return ret
```

**数学的定義**: 
$$[x]_\times = \begin{bmatrix} 0 & -x_3 & x_2 \\ x_3 & 0 & -x_1 \\ -x_2 & x_1 & 0 \end{bmatrix}$$

**性質**: 
$$[x]_\times y = x \times y$$

**使用例（PoseKalman）**:
```python
angular_velocity = state[States.ANGULAR_VELOCITY, :]
velocity = state[States.DEVICE_VELOCITY, :]

# 遠心加速度 = ω × v
centripetal_acceleration = cross(angular_velocity) * velocity
```

---

### 5. euler2quat()

**数値版**: オイラー角からクォータニオンへ変換

```python
def euler2quat(eulers):
    gamma, theta, psi = eulers[:,0], eulers[:,1], eulers[:,2]
    
    q0 = np.cos(gamma/2) * np.cos(theta/2) * np.cos(psi/2) + \
         np.sin(gamma/2) * np.sin(theta/2) * np.sin(psi/2)
    q1 = np.sin(gamma/2) * np.cos(theta/2) * np.cos(psi/2) - \
         np.cos(gamma/2) * np.sin(theta/2) * np.sin(psi/2)
    q2 = np.cos(gamma/2) * np.sin(theta/2) * np.cos(psi/2) + \
         np.sin(gamma/2) * np.cos(theta/2) * np.sin(psi/2)
    q3 = np.cos(gamma/2) * np.cos(theta/2) * np.sin(psi/2) - \
         np.sin(gamma/2) * np.sin(theta/2) * np.cos(psi/2)
    
    quats = np.array([q0, q1, q2, q3]).T
    
    # q0 を正に保つ（符号の曖昧性を解消）
    for i in range(len(quats)):
        if quats[i, 0] < 0:
            quats[i] = -quats[i]
    
    return quats
```

**半角公式**: 数値的に安定

**正規化**: 自動的にノルム=1になる（半角の性質）

---

### 6. quat2rot()

**数値版**: クォータニオンから回転行列へ変換

```python
def quat2rot(quats):
    q0, q1, q2, q3 = quats[:, 0], quats[:, 1], quats[:, 2], quats[:, 3]
    
    Rs = np.zeros((quats.shape[0], 3, 3))
    Rs[:, 0, 0] = q0*q0 + q1*q1 - q2*q2 - q3*q3
    Rs[:, 0, 1] = 2 * (q1*q2 - q0*q3)
    Rs[:, 0, 2] = 2 * (q0*q2 + q1*q3)
    Rs[:, 1, 0] = 2 * (q1*q2 + q0*q3)
    Rs[:, 1, 1] = q0*q0 - q1*q1 + q2*q2 - q3*q3
    Rs[:, 1, 2] = 2 * (q2*q3 - q0*q1)
    Rs[:, 2, 0] = 2 * (q1*q3 - q0*q2)
    Rs[:, 2, 1] = 2 * (q0*q1 + q2*q3)
    Rs[:, 2, 2] = q0*q0 - q1*q1 - q2*q2 + q3*q3
    
    return Rs
```

**バッチ処理**: 複数のクォータニオンを一度に変換

---

### 7. euler2rot()

**数値版**: オイラー角から回転行列へ直接変換

```python
def euler2rot(eulers):
    return quat2rot(euler2quat(eulers))
```

**実装**: euler → quat → rot の合成

**別名**: `rotations_from_quats = quat2rot`

---

### 8. rot_matrix()

**数値版**: numpy で回転行列を直接計算

```python
def rot_matrix(roll, pitch, yaw):
    cr, sr = np.cos(roll), np.sin(roll)
    cp, sp = np.cos(pitch), np.sin(pitch)
    cy, sy = np.cos(yaw), np.sin(yaw)
    
    rr = np.array([[1,0,0], [0, cr,-sr], [0, sr, cr]])       # Roll
    rp = np.array([[cp,0,sp], [0, 1,0], [-sp, 0, cp]])       # Pitch
    ry = np.array([[cy,-sy,0], [sy, cy,0], [0, 0, 1]])       # Yaw
    
    return ry.dot(rp.dot(rr))
```

**用途**: 数値計算（非シンボリック）

---

### 9. quat_matrix_l(), quat_matrix_r()

**シンボリック**: クォータニオン積を行列で表現

```python
def quat_matrix_l(p):
    return sp.Matrix([
        [p[0], -p[1], -p[2], -p[3]],
        [p[1],  p[0], -p[3],  p[2]],
        [p[2],  p[3],  p[0], -p[1]],
        [p[3], -p[2],  p[1],  p[0]]
    ])

def quat_matrix_r(p):
    return sp.Matrix([
        [p[0], -p[1], -p[2], -p[3]],
        [p[1],  p[0],  p[3], -p[2]],
        [p[2], -p[3],  p[0],  p[1]],
        [p[3],  p[2], -p[1],  p[0]]
    ])
```

**クォータニオン積**:
$$q_1 \otimes q_2 = L(q_1) q_2 = R(q_2) q_1$$

**用途**: ESKF でのクォータニオン更新

---

### 10. sympy_into_c()

**Cコード生成**: sympy 式を C99 コードに変換

```python
def sympy_into_c(sympy_functions, global_vars=None):
    from sympy.utilities import codegen
    
    routines = []
    for name, expr, args in sympy_functions:
        r = codegen.make_routine(name, expr, language="C99", global_vars=global_vars)
        
        # 引数の順序を調整（入力→出力）
        nargs = []
        for aa in args:
            # 入力引数を追加
            ...
        for a in r.arguments:
            if type(a) == codegen.OutputArgument:
                # 出力引数を追加
                nargs.append(a)
        
        r.arguments = nargs
        routines.append(r)
    
    # Cコード生成
    [(_, c_code), (_, c_header)] = codegen.get_code_generator('C', 'ekf', 'C99').write(routines, "ekf")
    
    # プリプロセッサ指令を除去
    c_header = '\n'.join(x for x in c_header.split("\n") if len(x) > 0 and x[0] != '#')
    c_code = '\n'.join(x for x in c_code.split("\n") if len(x) > 0 and x[0] != '#')
    
    return c_header, c_code
```

**生成例**:
```python
# sympy 定義
f_sym = sp.Matrix([x + dt * v, v])

# 生成される C コード
void f_fun(double *state, double dt, double *out) {
    out[0] = state[0] + dt * state[1];
    out[1] = state[1];
}
```

---

## PoseKalman での使用例

### 状態遷移関数

```python
# 状態ベクトル
roll, pitch, yaw = state[States.NED_ORIENTATION, :]
velocity = state[States.DEVICE_VELOCITY, :]
angular_velocity = state[States.ANGULAR_VELOCITY, :]
vroll, vpitch, vyaw = angular_velocity
acceleration = state[States.ACCELERATION, :]

# 時刻 t の回転行列
ned_from_device = euler_rotate(roll, pitch, yaw)
device_from_ned = ned_from_device.T

# 速度の時間微分（デバイス座標系）
state_dot[States.DEVICE_VELOCITY, :] = acceleration

# 時刻 t から t+dt の回転（デバイス座標系での変化）
device_from_device_t1 = euler_rotate(dt*vroll, dt*vpitch, dt*vyaw)

# 時刻 t+dt の回転行列（NED座標系）
ned_from_device_t1 = ned_from_device * device_from_device_t1

# オイラー角に戻す
f_sym[States.NED_ORIENTATION, :] = rot_to_euler(ned_from_device_t1)
```

**物理的解釈**:
1. 現在の姿勢 (roll, pitch, yaw) から回転行列を計算
2. 角速度で dt 時間回転した変化量を計算
3. 回転を合成（現在 × 変化）
4. 新しい姿勢をオイラー角で表現

---

### 観測方程式（加速度計）

```python
# 重力（NED座標系）
gravity = sp.Matrix([0, 0, -EARTH_G])  # 下向きに 9.81 m/s²

# 加速度計の観測値（デバイス座標系）
h_acc_sym = device_from_ned * gravity + acceleration + centripetal_acceleration + acc_bias
```

**物理的解釈**:
1. 重力をデバイス座標系に変換: `device_from_ned * gravity`
2. 真の加速度を加算: `+ acceleration`
3. 遠心加速度を加算: `+ centripetal_acceleration`
4. センサーバイアスを加算: `+ acc_bias`

**結果**: 加速度計が観測する値を予測

---

## ESKF でのクォータニオン使用

### 誤差関数

```python
# Nominal state: クォータニオン
q_nom = state[States.QUATERNION, :]  # [q0, q1, q2, q3]

# Error state: オイラー角（小さい角度）
delta_theta = error[States.ORIENTATION_ERROR, :]  # [δγ, δθ, δψ]

# True state の計算
delta_q = sp.Matrix([
    sp.cos(|δθ|/2),
    sp.sin(|δθ|/2) * δθ_x / |δθ|,
    sp.sin(|δθ|/2) * δθ_y / |δθ|,
    sp.sin(|δθ|/2) * δθ_z / |δθ|
])

q_true = quat_matrix_l(q_nom) * delta_q
```

**小角度近似**:
$$\delta q \approx [1, \frac{\delta\theta_x}{2}, \frac{\delta\theta_y}{2}, \frac{\delta\theta_z}{2}]^T$$

---

## まとめ

### sympy_helpers.py の役割

1. **3D回転の変換**: オイラー角 ↔ クォータニオン ↔ 回転行列
2. **シンボリック計算**: sympy でヤコビアン自動計算に対応
3. **数値計算**: numpy での高速な実装
4. **Cコード生成**: 最適化されたネイティブコード生成

### 主要な変換

| From → To | シンボリック | 数値版 |
|-----------|------------|-------|
| Euler → Rotation | `euler_rotate()` | `rot_matrix()`, `euler2rot()` |
| Quat → Rotation | `quat_rotate()` | `quat2rot()` |
| Rotation → Euler | `rot_to_euler()` | （同じ、numpy化） |
| Euler → Quat | ― | `euler2quat()` |

### 利点

1. **正確性**: 数学的に正しい変換
2. **効率性**: 最適化された実装
3. **汎用性**: シンボリック・数値の両対応
4. **保守性**: 一箇所で回転演算を管理

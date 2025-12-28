# controlsd 詳細

このドキュメントでは、openpilotの車両制御システム「controlsd」の詳細を説明します。

> **📖 関連ドキュメント**
> - [README](README.md) - openpilot分析ドキュメントの全体構成
> - [ml-models-details.md](ml-models-details.md) - Vision/Policy Modelの入出力
> - [manager-details.md](manager-details.md) - プロセス管理
> - [../msgq_analysis/runtime_details.md](../msgq_analysis/runtime_details.md) - プロセス間通信

---

## 1. controlsdの概要

**ファイル**: [selfdrive/controls/controlsd.py](../../selfdrive/controls/controlsd.py)

`controlsd`は、openpilotの**車両制御の中核プロセス**であり、AIモデルの出力や経路計画を元に、実際の車両へのアクチュエーター指令（ステアリング・アクセル・ブレーキ）を生成します。

### 1.1 主要な役割

```
┌─────────────────────────────────────────┐
│           controlsd                     │
│                                         │
│  ┌──────────────┐  ┌─────────────────┐ │
│  │ LatControl   │  │  LongControl    │ │
│  │ (横制御)      │  │  (縦制御)       │ │
│  └──────────────┘  └─────────────────┘ │
│         │                   │           │
│         └───────┬───────────┘           │
│                 │                       │
│         ┌───────▼───────┐               │
│         │  carControl   │               │
│         │  (CAN出力)    │               │
│         └───────────────┘               │
└─────────────────────────────────────────┘
```

1. **入力処理**:
   - AIモデル出力（modelV2: desired_curvature, plan）
   - 経路計画（longitudinalPlan: aTarget, shouldStop）
   - 車両状態（carState: vEgo, steeringAngleDeg等）

2. **制御計算**:
   - **横制御（Lateral）**: 目標曲率に追従するステアリング制御
   - **縦制御（Longitudinal）**: 目標加速度に追従する速度制御

3. **出力生成**:
   - **carControl**: CAN経由で車両に送信（ステアリング・アクセル・ブレーキ）
   - **controlsState**: 制御状態の配信（デバッグ・UI用）

### 1.2 実行周期

```python
# controlsd.py
def run(self):
    rk = Ratekeeper(100, print_delay_threshold=None)
    while True:
        self.update()           # メッセージ受信
        CC, lac_log = self.state_control()  # 制御計算
        self.publish(CC, lac_log)           # 結果配信
        rk.monitor_time()
```

- **実行周期**: 100Hz（10ms間隔）
- **優先度**: `Priority.CTRL_HIGH`（リアルタイム性確保）

---

## 2. アーキテクチャ

### 2.1 全体構成

```
┌──────────────────────────────────────────────────────────┐
│                      Controls Class                       │
├──────────────────────────────────────────────────────────┤
│                                                           │
│  初期化（__init__）:                                       │
│    ├─ CarParams読み込み                                   │
│    ├─ CarInterface初期化                                  │
│    ├─ SubMaster/PubMaster設定                            │
│    ├─ LatControl選択（Angle/PID/Torque）                 │
│    └─ LongControl初期化                                   │
│                                                           │
│  メインループ（run）:                                      │
│    ├─ update()        : メッセージ受信・状態更新          │
│    ├─ state_control() : 制御計算                          │
│    └─ publish()       : 結果配信                          │
│                                                           │
└──────────────────────────────────────────────────────────┘
                    │                    │
        ┌───────────┴────────┐  ┌────────┴─────────┐
        │   LatControl       │  │  LongControl     │
        │   (横制御)          │  │  (縦制御)        │
        └────────────────────┘  └──────────────────┘
```

### 2.2 入力メッセージ

| メッセージ名 | 説明 | 主要フィールド |
|------------|------|---------------|
| **modelV2** | AIモデル出力 | `action.desiredCurvature`, `meta.laneChangeState` |
| **longitudinalPlan** | 縦制御計画 | `aTarget`, `shouldStop`, `speeds` |
| **carState** | 車両状態 | `vEgo`, `steeringAngleDeg`, `aEgo`, `brakePressed` |
| **selfdriveState** | 自動運転状態 | `enabled`, `active`, `alertHudVisual` |
| **liveParameters** | 車両パラメータ | `steerRatio`, `angleOffsetDeg`, `roll` |
| **liveTorqueParameters** | トルクパラメータ | `latAccelFactor`, `frictionCoefficient` |
| **liveCalibration** | カメラキャリブレーション | `rpyCalib`, `extrinsicMatrix` |
| **livePose** | 車両姿勢 | `orientation`, `angularVelocity` |
| **carOutput** | 車両応答 | `actuatorsOutput.torque`, `steeringAngleDeg` |
| **driverMonitoringState** | ドライバー監視 | `awarenessStatus` |
| **onroadEvents** | イベント | `overrideLongitudinal` |
| **driverAssistance** | 運転支援 | `leftLaneDeparture`, `rightLaneDeparture` |

### 2.3 出力メッセージ

| メッセージ名 | 説明 | 主要フィールド |
|------------|------|---------------|
| **carControl** | 車両制御指令 | `actuators.torque`, `actuators.accel`, `actuators.curvature` |
| **controlsState** | 制御状態 | `curvature`, `desiredCurvature`, `longControlState`, `lateralControlState` |

---

## 3. 横制御（Lateral Control）

### 3.1 制御フロー

```
AIモデル → desired_curvature → LatControl → steering_torque/angle → 車両EPS
              [1/m]                             [Nm] or [deg]
```

### 3.2 3種類の制御方式

openpilotは車種に応じて3つの横制御方式を切り替えます：

#### **方式1: Angle Control（角度制御）**

**ファイル**: [latcontrol_angle.py](../../selfdrive/controls/lib/latcontrol_angle.py)

```python
# 使用条件
if CP.steerControlType == car.CarParams.SteerControlType.angle:
    self.LaC = LatControlAngle(self.CP, self.CI)
```

**特徴**:
- **直接角度指令**: 目標ステアリング角度を車両に送信
- **車両側でトルク制御**: EPS（電動パワステ）が角度追従を実行
- **対象車種**: トヨタ、ホンダ等（angle指令をサポートする車両）

**計算フロー**:
```
desired_curvature [1/m]
    ↓
desired_lateral_accel = curvature × vEgo² [m/s²]
    ↓
desired_angle = atan(lateral_accel × wheelbase / vEgo²) [rad]
    ↓
PID(desired_angle - actual_angle)
    ↓
steering_angle_command [deg]
```

#### **方式2: Torque Control（トルク制御）**

**ファイル**: [latcontrol_torque.py](../../selfdrive/controls/lib/latcontrol_torque.py)

```python
# 使用条件
elif CP.lateralTuning.which() == 'torque':
    self.LaC = LatControlTorque(self.CP, self.CI)
```

**特徴**:
- **トルク直接制御**: ステアリングトルクを直接計算
- **物理モデルベース**: 横加速度とトルクの関係を学習（`torque_from_lateral_accel`）
- **低速補正**: 低速域では別のファクター適用（`LOW_SPEED_FACTOR`）
- **対象車種**: GM、Hyundai等

**計算フロー**:
```
desired_curvature [1/m]
    ↓
desired_lateral_accel = curvature × vEgo² [m/s²]
    ↓
torque_setpoint = torque_from_lateral_accel(desired_lateral_accel)
    ↓ (物理モデル: latAccelFactor, friction等)
actual_lateral_accel = actual_curvature × vEgo²
    ↓
torque_measurement = torque_from_lateral_accel(actual_lateral_accel)
    ↓
error = torque_setpoint - torque_measurement
    ↓
PID(error) + feedforward
    ↓
steering_torque [Nm]
```

**低速補正**:
```python
LOW_SPEED_X = [0, 10, 20, 30]  # [mph]
LOW_SPEED_Y = [15, 13, 10, 5]   # factor

low_speed_factor = np.interp(vEgo, LOW_SPEED_X, LOW_SPEED_Y)**2
setpoint = desired_lateral_accel + low_speed_factor * desired_curvature
```

**根拠**: 低速域では車両の慣性が小さく、曲率の変化に対する応答が異なるため補正が必要。

#### **方式3: PID Control（PID制御）**

**ファイル**: [latcontrol_pid.py](../../selfdrive/controls/lib/latcontrol_pid.py)

```python
# 使用条件
elif CP.lateralTuning.which() == 'pid':
    self.LaC = LatControlPID(self.CP, self.CI)
```

**特徴**:
- **シンプルなPID**: 曲率誤差に対するPID制御
- **レガシー方式**: 古い車種向け
- **対象車種**: 一部の古いサポート車種

### 3.3 曲率の制限（Safety Limiter）

```python
# controlsd.py
self.desired_curvature, curvature_limited = clip_curvature(
    CS.vEgo,
    self.desired_curvature,  # 前回値
    new_desired_curvature,   # AIモデル出力
    lp.roll
)
```

**制限理由**:
1. **急激な変化を防ぐ**: 10ms間での曲率変化を制限
2. **速度依存**: 高速では小さい曲率まで制限（物理的限界）
3. **横滑り防止**: ロール角を考慮

**ファイル**: [drive_helpers.py](../../selfdrive/controls/lib/drive_helpers.py)

```python
def clip_curvature(v_ego, prev_curvature, new_curvature, roll):
    # 速度に応じた最大曲率
    max_curvature_for_speed = get_max_curvature(v_ego)

    # 変化率制限
    max_delta_curvature = MAX_CURVATURE_RATE * DT_CTRL

    # クリップ
    clipped = np.clip(
        new_curvature,
        prev_curvature - max_delta_curvature,
        prev_curvature + max_delta_curvature
    )

    # 絶対値制限
    clipped = np.clip(clipped, -max_curvature_for_speed, max_curvature_for_speed)

    return clipped, (clipped != new_curvature)
```

### 3.4 VehicleModel（車両モデル）

**ファイル**: [vehicle_model.py](../../opendbc/car/vehicle_model.py)

```python
class VehicleModel:
    def __init__(self, CP):
        self.wheelbase = CP.wheelbase          # ホイールベース [m]
        self.steer_ratio = CP.steerRatio       # ステアリングレシオ
        self.tire_stiffness_factor = CP.tireStiffnessFactor

    def calc_curvature(self, steering_angle, v_ego, roll):
        """ステアリング角度から曲率を計算"""
        # Ackermann steering geometry
        curvature = tan(steering_angle) / self.wheelbase

        # ロール補正
        curvature_roll = roll / self.wheelbase

        return curvature - curvature_roll
```

**使用箇所**:
```python
# 現在の曲率を計算（フィードバック用）
steer_angle_without_offset = math.radians(CS.steeringAngleDeg - lp.angleOffsetDeg)
self.curvature = -self.VM.calc_curvature(steer_angle_without_offset, CS.vEgo, lp.roll)
```

---

## 4. 縦制御（Longitudinal Control）

### 4.1 2段階制御

openpilotの縦制御は**計画（Planning）**と**追従（Control）**の2段階構成：

```
┌──────────────────────────────────────┐
│  Stage 1: plannerd                   │
│  (LongitudinalPlanner + MPC)         │
│                                      │
│  入力: modelV2, radarState, vCruise  │
│  処理: MPC最適化                      │
│  出力: aTarget (目標加速度軌道)       │
└─────────────┬────────────────────────┘
              │
              ▼
┌──────────────────────────────────────┐
│  Stage 2: controlsd                  │
│  (LongControl + PID)                 │
│                                      │
│  入力: aTarget, carState.aEgo        │
│  処理: PID追従制御                    │
│  出力: accel (アクチュエーター指令)   │
└──────────────────────────────────────┘
```

### 4.2 Stage 1: plannerd（経路計画）

**ファイル**: [plannerd.py](../../selfdrive/controls/plannerd.py)

```python
def plannerd_thread():
    CP = messaging.log_from_bytes(params.get("CarParams", block=True), car.CarParams)
    longitudinal_planner = LongitudinalPlanner(CP)
    pm = messaging.PubMaster(['longitudinalPlan', 'driverAssistance'])

    while True:
        sm.update(timeout)

        if sm.updated['modelV2']:
            longitudinal_planner.update(sm)
            longitudinal_planner.publish(sm, pm)
```

**LongitudinalPlanner**:

**ファイル**: [longitudinal_planner.py](../../selfdrive/controls/lib/longitudinal_planner.py)

```python
class LongitudinalPlanner:
    def __init__(self, CP, init_v=0.0, init_a=0.0):
        self.mpc = LongitudinalMpc()  # Model Predictive Control
        self.v_desired_filter = FirstOrderFilter(init_v, 2.0, dt)
        self.a_desired = init_a

    def update(self, sm):
        # AIモデルから軌道を取得
        x, v, a, j, throttle_prob = self.parse_model(sm['modelV2'], self.v_model_error)

        # 加速度制限の計算
        if self.mpc.mode == 'acc':
            accel_clip = [ACCEL_MIN, get_max_accel(v_ego)]
            # コーナリング時の加速制限
            accel_clip = limit_accel_in_turns(v_ego, steer_angle, accel_clip, self.CP)
        else:
            accel_clip = [ACCEL_MIN, ACCEL_MAX]

        # throttle_probが低い場合は減速方向に制限
        if not self.allow_throttle:
            accel_clip[1] = min(accel_clip[1], accel_coast)

        # MPC最適化実行
        self.mpc.set_cur_state(self.v_desired_filter.x, self.a_desired)
        self.mpc.update(sm['radarState'], v_cruise, x, v, a, j)

        # 制御点への補間
        self.v_desired_trajectory = np.interp(CONTROL_N_T_IDX, T_IDXS_MPC, self.mpc.v_solution)
        self.a_desired_trajectory = np.interp(CONTROL_N_T_IDX, T_IDXS_MPC, self.mpc.a_solution)
```

**MPC（Model Predictive Control）**:

**ファイル**: [long_mpc.py](../../selfdrive/controls/lib/longitudinal_mpc_lib/long_mpc.py)

```python
class LongitudinalMpc:
    def __init__(self, dt=DT_MDL):
        self.solver = acados_solver  # ACADOS非線形最適化ソルバー

    def update(self, radarState, v_cruise, x, v, a, j):
        # コスト関数設定
        #   - 速度追従: (v - v_target)²
        #   - 加速度滑らかさ: a²
        #   - ジャーク最小化: j²
        #   - 安全距離: (x - x_lead - safe_distance)²

        # 制約条件
        #   - 加速度制限: a_min ≤ a ≤ a_max
        #   - 速度制限: 0 ≤ v ≤ v_cruise
        #   - 前車追従: x < x_lead - safe_distance

        # 最適化実行
        self.solver.solve()

        # 解を取得
        self.v_solution = [self.solver.get(i, 'v') for i in range(N)]
        self.a_solution = [self.solver.get(i, 'a') for i in range(N)]
```

**MPCの目的**:
- **速度追従**: クルーズコントロール速度に追従
- **安全距離維持**: 前車との安全距離確保
- **快適性**: 加速度・ジャークを最小化
- **燃費**: 不要な加減速を避ける

### 4.3 Stage 2: controlsd（PID追従）

**ファイル**: [longcontrol.py](../../selfdrive/controls/lib/longcontrol.py)

```python
class LongControl:
    def __init__(self, CP):
        self.pid = PIDController(
            (CP.longitudinalTuning.kpBP, CP.longitudinalTuning.kpV),
            (CP.longitudinalTuning.kiBP, CP.longitudinalTuning.kiV),
            k_f=CP.longitudinalTuning.kf,
            rate=1 / DT_CTRL
        )
        self.long_control_state = LongCtrlState.off

    def update(self, active, CS, a_target, should_stop, accel_limits):
        # ステートマシン遷移
        self.long_control_state = long_control_state_trans(
            self.CP, active, self.long_control_state,
            CS.vEgo, should_stop, CS.brakePressed, CS.cruiseState.standstill
        )

        if self.long_control_state == LongCtrlState.off:
            output_accel = 0.

        elif self.long_control_state == LongCtrlState.stopping:
            # 停止中: 固定減速度
            output_accel = self.last_output_accel
            if output_accel > self.CP.stopAccel:
                output_accel -= self.CP.stoppingDecelRate * DT_CTRL

        elif self.long_control_state == LongCtrlState.starting:
            # 発進時: 固定加速度
            output_accel = self.CP.startAccel

        else:  # LongCtrlState.pid
            # PID制御
            error = a_target - CS.aEgo
            output_accel = self.pid.update(
                error,
                speed=CS.vEgo,
                feedforward=a_target
            )

        return np.clip(output_accel, accel_limits[0], accel_limits[1])
```

### 4.4 状態遷移図

```
┌─────────────────────────────────────────────────────┐
│            LongControlState                         │
└─────────────────────────────────────────────────────┘

        ┌───────────┐
        │    off    │ (制御無効)
        └─────┬─────┘
              │ active=True
              ▼
     should_stop? ─YES→ ┌───────────┐
              │          │ stopping  │ (停止中)
              NO         └─────┬─────┘
              │                │ starting_condition
              ▼                ▼
        ┌───────────┐    ┌───────────┐
        │    pid    │◄───│ starting  │ (発進中)
        └───────────┘    └───────────┘
              │
              │ should_stop
              ▼
        ┌───────────┐
        │ stopping  │
        └───────────┘
```

**状態の説明**:
- **off**: 非アクティブ（エンジン停止・手動運転等）
- **stopping**: 停止指令を受けて減速中
- **starting**: 停止状態から発進中（クリープ対応）
- **pid**: 通常のPID追従制御

### 4.5 コーナーでの加速制限

```python
def limit_accel_in_turns(v_ego, angle_steers, a_target, CP):
    """
    横加速度を考慮した縦加速度の制限
    """
    # 最大合成加速度
    a_total_max = np.interp(v_ego, [20., 40.], [1.7, 3.2])

    # 現在の横加速度
    a_y = v_ego ** 2 * angle_steers * CV.DEG_TO_RAD / (CP.steerRatio * CP.wheelbase)

    # 許容される縦加速度
    a_x_allowed = sqrt(max(a_total_max ** 2 - a_y ** 2, 0.))

    return [a_target[0], min(a_target[1], a_x_allowed)]
```

**根拠**: タイヤのグリップ限界を超えないよう、横加速度と縦加速度の合成を制限。

**物理的背景**:
```
a_total² = a_x² + a_y²  (合成加速度)

タイヤのμ限界:
a_total_max ≈ μ × g  (μ: 摩擦係数, g: 重力加速度)

→ コーナリング中（a_yが大きい）は、a_xを制限
```

---

## 5. 制御パラメータのチューニング

### 5.1 横制御パラメータ（Torque方式の例）

**ファイル**: CarParamsの`lateralTuning.torque`

```python
ret.lateralTuning.torque.kp = 1.0        # 比例ゲイン
ret.lateralTuning.torque.ki = 0.1        # 積分ゲイン
ret.lateralTuning.torque.kf = 1.0        # フィードフォワードゲイン
ret.lateralTuning.torque.friction = 0.01  # 摩擦補償
ret.lateralTuning.torque.latAccelFactor = 2.0     # トルク→横加速度変換係数
ret.lateralTuning.torque.latAccelOffset = 0.0     # オフセット
ret.lateralTuning.torque.useSteeringAngle = False # 角度ベースか姿勢ベースか
```

**オンライン学習**:
```python
# liveTorqueParametersから動的更新
if torque_params.useParams:
    self.LaC.update_live_torque_params(
        torque_params.latAccelFactorFiltered,
        torque_params.latAccelOffsetFiltered,
        torque_params.frictionCoefficientFiltered
    )
```

### 5.2 縦制御パラメータ

**ファイル**: CarParamsの`longitudinalTuning`

```python
ret.longitudinalTuning.kpBP = [0., 5., 35.]     # 速度ブレークポイント
ret.longitudinalTuning.kpV = [1.2, 0.8, 0.5]   # 速度別の比例ゲイン
ret.longitudinalTuning.kiBP = [0., 35.]
ret.longitudinalTuning.kiV = [0.18, 0.12]
ret.longitudinalTuning.kf = 1.0                 # フィードフォワード

ret.stoppingDecelRate = 0.8  # [m/s²/s] 停止時の減速率
ret.startAccel = 1.0         # [m/s²] 発進時の加速度
ret.stopAccel = -2.0         # [m/s²] 停止時の目標減速度
```

**ゲインスケジューリング**:
```python
# 速度に応じてPIDゲインを変更
kp = np.interp(v_ego, kpBP, kpV)
ki = np.interp(v_ego, kiBP, kiV)
```

---

## 6. 安全機構

### 6.1 アクティブ条件のチェック

```python
# 横制御がアクティブになる条件
standstill = abs(CS.vEgo) <= max(self.CP.minSteerSpeed, 0.3) or CS.standstill
CC.latActive = (
    self.sm['selfdriveState'].active and          # openpilot有効
    not CS.steerFaultTemporary and                # 一時的な故障なし
    not CS.steerFaultPermanent and                # 恒久的な故障なし
    (not standstill or self.CP.steerAtStandstill) # 停止時制御可能
)

# 縦制御がアクティブになる条件
CC.longActive = (
    CC.enabled and                                # 全体が有効
    not any(e.overrideLongitudinal for e in events) and  # オーバーライドなし
    self.CP.openpilotLongitudinalControl          # 縦制御サポート車種
)
```

### 6.2 NaN/Inf検出

```python
# アクチュエーター出力の異常値検出
for p in ACTUATOR_FIELDS:
    attr = getattr(actuators, p)
    if not isinstance(attr, SupportsFloat):
        continue

    if not math.isfinite(attr):
        cloudlog.error(f"actuators.{p} not finite {actuators.to_dict()}")
        setattr(actuators, p, 0.0)  # 安全値に置き換え
```

### 6.3 飽和検出（Saturation）

```python
# ステアリングトルク飽和の検出
if self.CP.steerControlType == car.CarParams.SteerControlType.angle:
    self.steer_limited_by_controls = (
        abs(CC.actuators.steeringAngleDeg - CO.actuatorsOutput.steeringAngleDeg) >
        STEER_ANGLE_SATURATION_THRESHOLD
    )
else:
    self.steer_limited_by_controls = (
        abs(CC.actuators.torque - CO.actuatorsOutput.torque) > 1e-2
    )
```

**用途**: 飽和時にPIDの積分項を凍結（Integrator Windup防止）

### 6.4 強制減速

```python
# ドライバーの注意力低下時
cs.forceDecel = bool(
    (self.sm['driverMonitoringState'].awarenessStatus < 0.) or
    (self.sm['selfdriveState'].state == State.softDisabling)
)
```

---

## 7. 車両インターフェース（CarInterface）

### 7.1 車種別の抽象化

```python
# controlsd.py
from opendbc.car.car_helpers import interfaces

self.CI = interfaces[self.CP.carFingerprint](self.CP)
```

**CarInterface**の役割:
```python
class CarInterface:
    def get_pid_accel_limits(self, CP, current_speed, cruise_speed):
        """車種別のPID加速度制限"""
        return [accel_min, accel_max]

    def torque_from_lateral_accel(self):
        """横加速度→トルク変換関数"""
        return lambda inputs, params, ...: torque
```

### 7.2 主要メソッド

| メソッド | 説明 |
|---------|------|
| `get_pid_accel_limits()` | 速度別の加速度制限取得 |
| `torque_from_lateral_accel()` | 横加速度→トルク変換 |
| `get_steer_feedforward()` | ステアリングFF項計算 |

---

## 8. データフロー詳細

### 8.1 時系列フロー（1サイクル: 10ms）

```
Time: 0ms
├─ SubMaster.update(timeout=15ms)
│   ├─ carState       (100Hz) ✓ 最新
│   ├─ modelV2        (20Hz)  △ 50msごと
│   ├─ longitudinalPlan (20Hz) △ 50msごと
│   └─ その他メッセージ受信
│
├─ update()
│   ├─ liveCalibration更新 → pose_calibrator
│   └─ livePose更新 → calibrated_pose計算
│
├─ state_control()
│   ├─ VehicleModel更新（steerRatio, stiffnessFactor）
│   ├─ 現在曲率計算: curvature = f(steeringAngleDeg, vEgo)
│   ├─ 目標曲率クリップ: clip_curvature()
│   ├─ 横制御: LaC.update() → steering_torque
│   └─ 縦制御: LoC.update() → accel
│
└─ publish()
    ├─ carControl配信 → pandad → CAN → 車両
    └─ controlsState配信 → UI/ログ

Time: 10ms (次のサイクルへ)
```

### 8.2 メッセージの遅延と同期

```python
# controlsState に各planのタイムスタンプを記録
cs.longitudinalPlanMonoTime = self.sm.logMonoTime['longitudinalPlan']
cs.lateralPlanMonoTime = self.sm.logMonoTime['modelV2']
```

**遅延の影響**:
- **modelV2**: 20Hz（50ms周期） → 最大50msの遅延
- **longitudinalPlan**: 20Hz → 最大50msの遅延
- **controlsd**: 100Hz → 遅延を補間で吸収

**補間処理**:
```python
# modelV2が更新されない場合、前回の値を継続使用
if not sm.updated['modelV2']:
    # 前回のdesired_curvatureを継続
    new_desired_curvature = self.curvature  # 現在値に戻す
```

---

## 9. デバッグとモニタリング

### 9.1 controlsStateの出力内容

```python
# controlsState
cs.curvature = self.curvature                    # 現在の曲率
cs.desiredCurvature = self.desired_curvature    # 目標曲率
cs.longControlState = self.LoC.long_control_state  # 縦制御状態

# PIDの内部状態
cs.upAccelCmd = float(self.LoC.pid.p)  # 比例項
cs.uiAccelCmd = float(self.LoC.pid.i)  # 積分項
cs.ufAccelCmd = float(self.LoC.pid.f)  # フィードフォワード項

# 横制御状態（方式別）
if self.CP.steerControlType == car.CarParams.SteerControlType.angle:
    cs.lateralControlState.angleState = lac_log
elif lat_tuning == 'pid':
    cs.lateralControlState.pidState = lac_log
elif lat_tuning == 'torque':
    cs.lateralControlState.torqueState = lac_log
```

### 9.2 ログ項目

**TorqueState**の例:
```python
pid_log.error = float(torque_setpoint - torque_measurement)
pid_log.p = float(self.pid.p)
pid_log.i = float(self.pid.i)
pid_log.d = float(self.pid.d)
pid_log.f = float(self.pid.f)
pid_log.output = float(-output_torque)
pid_log.actualLateralAccel = float(actual_lateral_accel)
pid_log.desiredLateralAccel = float(desired_lateral_accel)
pid_log.saturated = bool(飽和フラグ)
```

### 9.3 可視化ツール

```bash
# controlsStateをリアルタイム表示
tools/plotjuggler/juggle.py

# ログファイルから再生
tools/replay/replay --demo
```

---

## 10. Jetson Nano移植のポイント

### 10.1 必要な変更箇所

| コンポーネント | 変更内容 |
|--------------|---------|
| **CarInterface** | SocketCAN対応の実装 |
| **pandad** | CAN送受信をSocketCANに置き換え |
| **CarParams** | ターゲット車種のパラメータ調整 |

### 10.2 CarInterfaceの実装例

```python
# selfdrive/car/jetson/interface.py
class CarInterface(CarInterfaceBase):
    def __init__(self, CP):
        super().__init__(CP)
        # SocketCANインターフェース初期化
        self.can_sock = socket.socket(socket.AF_CAN, socket.SOCK_RAW, socket.CAN_RAW)
        self.can_sock.bind(('can0',))

    def update(self, c, can_strings):
        # CANメッセージのパース
        ret = car.CarState.new_message()

        # vEgo, steeringAngleDeg等を取得
        ret.vEgo = self.parse_speed(can_strings)
        ret.steeringAngleDeg = self.parse_steering_angle(can_strings)
        # ...

        return ret

    def apply(self, c, now_nanos):
        # carControlをCANメッセージに変換
        can_sends = []

        if c.actuators.torque != 0:
            # ステアリングトルクコマンド
            can_sends.append(make_can_msg(
                addr=0x300,
                dat=[torque_to_bytes(c.actuators.torque)]
            ))

        if c.actuators.accel != 0:
            # アクセル/ブレーキコマンド
            can_sends.append(make_can_msg(
                addr=0x200,
                dat=[accel_to_bytes(c.actuators.accel)]
            ))

        return can_sends
```

### 10.3 チューニング手順

1. **CarParamsの設定**:
   ```python
   ret.wheelbase = 2.7  # [m] 実測値
   ret.steerRatio = 15.0  # 実測or推定
   ret.mass = 1500  # [kg]
   ```

2. **横制御チューニング**:
   - まずAngle方式を試す（車両がサポートしていれば）
   - Torque方式の場合、`latAccelFactor`を調整
   - PIDゲインを現場で微調整

3. **縦制御チューニング**:
   - `kpV`, `kiV`を速度別に調整
   - `startAccel`, `stopAccel`を車両特性に合わせる

4. **安全テスト**:
   - 低速での動作確認
   - 緊急停止機能のテスト
   - 飽和・リミット動作の確認

---

## 11. まとめ

### 11.1 controlsdの設計思想

1. **責任の分離**:
   - **plannerd**: 「何をするか」（目標軌道生成）
   - **controlsd**: 「どう実現するか」（アクチュエーター制御）

2. **車種依存の抽象化**:
   - **CarInterface**: 車種別の違いを吸収
   - **制御ロジック**: 汎用的なアルゴリズム

3. **安全性重視**:
   - 多重チェック（飽和・NaN・制約）
   - 段階的な制御（状態遷移）
   - 保守的なリミット

### 11.2 主要な技術要素

| 要素 | 技術 | 目的 |
|------|------|------|
| **横制御** | PID/Torque/Angle制御 | 目標曲率に追従 |
| **縦制御** | PID + State Machine | 目標加速度に追従 |
| **経路計画** | MPC（非線形最適化） | 最適な軌道生成 |
| **安全機構** | Limiter + Saturation検出 | 物理的制約の遵守 |
| **車種対応** | CarInterface抽象化 | 多様な車種サポート |

### 11.3 性能チューニングの要点

```
良い制御 = 適切なモデル × 適切なゲイン × 適切なリミット

1. VehicleModel: wheelbase, steerRatio, mass等を正確に
2. PIDゲイン: 速度別・状況別に最適化
3. Safety Limiter: 保守的すぎず、危険でない範囲に
```

---

**関連ファイル一覧**:
- [controlsd.py](../../selfdrive/controls/controlsd.py) - メインループ
- [longcontrol.py](../../selfdrive/controls/lib/longcontrol.py) - 縦制御PID
- [latcontrol_torque.py](../../selfdrive/controls/lib/latcontrol_torque.py) - 横制御（Torque）
- [latcontrol_angle.py](../../selfdrive/controls/lib/latcontrol_angle.py) - 横制御（Angle）
- [latcontrol_pid.py](../../selfdrive/controls/lib/latcontrol_pid.py) - 横制御（PID）
- [longitudinal_planner.py](../../selfdrive/controls/lib/longitudinal_planner.py) - 縦制御計画
- [drive_helpers.py](../../selfdrive/controls/lib/drive_helpers.py) - 制御ヘルパー関数

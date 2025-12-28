# 縦制御（Longitudinal Control）の詳細比較

## 概要
ichiroブランチ（Prius縦制御カスタム版）と本家openpilotの縦制御処理方法の違いを詳細に解説します。

## 1. 基本アーキテクチャの違い

### 本家openpilot（最新版）
- **制御方式**: 加速度ターゲット（a_target）ベースの制御
- **PIDコントローラ**: 加速度誤差（a_target - a_ego）をフィードバック
- **状態遷移**: `off` → `stopping/starting` → `pid` の3状態
- **インターフェース**: `LongControl.update(active, CS, a_target, should_stop, accel_limits)`

### ichiroブランチ（Prius カスタム版）
- **制御方式**: 速度ターゲット（v_target）ベースの制御
- **PIDコントローラ**: 速度誤差（v_pid - v_ego）をフィードバック + 加速度フィードフォワード
- **状態遷移**: `off` → `pid` → `stopping` の2状態（startingなし）
- **インターフェース**: `LongControl.update(active, CS, CP, long_plan, accel_limits)`

## 2. ファイル構造の比較

### longcontrol.py の違い

#### 本家 (/home/takuya/work/comma/openpilot/selfdrive/controls/lib/longcontrol.py)
```python
class LongControl:
    def __init__(self, CP):
        self.CP = CP
        self.long_control_state = LongCtrlState.off
        self.pid = PIDController(
            (CP.longitudinalTuning.kpBP, CP.longitudinalTuning.kpV),
            (CP.longitudinalTuning.kiBP, CP.longitudinalTuning.kiV),
            k_f=CP.longitudinalTuning.kf,  # フィードフォワードゲインあり
            rate=1 / DT_CTRL
        )
        self.last_output_accel = 0.0

    def reset(self):
        self.pid.reset()
        # v_pidの概念なし

    def update(self, active, CS, a_target, should_stop, accel_limits):
        # 加速度誤差ベースのPID制御
        error = a_target - CS.aEgo
        output_accel = self.pid.update(error, speed=CS.vEgo, feedforward=a_target)
```

#### ichiro (/home/takuya/work/comma/other_repo/ichiro/selfdrive/controls/lib/longcontrol.py)
```python
class LongControl():
    def __init__(self, CP):
        self.long_control_state = LongCtrlState.off
        self.pid = PIController(  # PIDControllerではなくPIController
            (CP.longitudinalTuning.kpBP, CP.longitudinalTuning.kpV),
            (CP.longitudinalTuning.kiBP, CP.longitudinalTuning.kiV),
            rate=1 / DT_CTRL
        )
        self.v_pid = 0.0  # 速度セットポイントを保持
        self.last_output_accel = 0.0

    def reset(self, v_pid):
        self.pid.reset()
        self.v_pid = v_pid  # 速度セットポイントをリセット

    def update(self, active, CS, CP, long_plan, accel_limits):
        # long_planから速度と加速度を計算
        # アクチュエータ遅延を考慮したターゲット計算
        v_target_lower = interp(CP.longitudinalActuatorDelayLowerBound, ...)
        a_target_lower = ...
        # 速度誤差ベースのPID + 加速度フィードフォワード
        output_accel = self.pid.update(self.v_pid, CS.vEgo, 
                                       speed=CS.vEgo, 
                                       deadzone=deadzone, 
                                       feedforward=a_target,
                                       freeze_integrator=freeze_integrator)
```

## 3. 制御ロジックの詳細な違い

### 3.1 状態マシンの違い

#### 本家の状態遷移
```python
def long_control_state_trans(CP, active, long_control_state, v_ego,
                             should_stop, brake_pressed, cruise_standstill):
    stopping_condition = should_stop
    starting_condition = (not should_stop and not cruise_standstill and not brake_pressed)
    started_condition = v_ego > CP.vEgoStarting

    if not active:
        long_control_state = LongCtrlState.off
    else:
        if long_control_state == LongCtrlState.off:
            if not starting_condition:
                long_control_state = LongCtrlState.stopping
            else:
                if CP.startingState:
                    long_control_state = LongCtrlState.starting
                else:
                    long_control_state = LongCtrlState.pid
        # ... その他の状態遷移
```

**特徴**:
- `starting` 状態が存在（発進時の専用処理）
- `should_stop` フラグで停止判定
- 速度閾値 `CP.vEgoStarting` で発進判定

#### ichiroの状態遷移
```python
def long_control_state_trans(CP, active, long_control_state, v_ego, v_target_future,
                             brake_pressed, cruise_standstill):
    stopping_condition = (v_ego < 2.0 and cruise_standstill) or \
                         (v_ego < CP.vEgoStopping and
                          (v_target_future < CP.vEgoStopping or brake_pressed))
    
    starting_condition = v_target_future > CP.vEgoStarting and not cruise_standstill

    if not active:
        long_control_state = LongCtrlState.off
    else:
        if long_control_state == LongCtrlState.off:
            long_control_state = LongCtrlState.pid  # 常にpidから開始
        elif long_control_state == LongCtrlState.pid:
            if stopping_condition:
                long_control_state = LongCtrlState.stopping
        elif long_control_state == LongCtrlState.stopping:
            if starting_condition:
                long_control_state = LongCtrlState.pid
```

**特徴**:
- `starting` 状態なし（シンプルな2状態）
- `v_target_future`（将来の目標速度）で停止/発進判定
- より細かい停止条件（2つの条件のOR）

### 3.2 PID制御の実装の違い

#### 本家: 加速度誤差ベース
```python
# state_control.pyで呼び出し
actuators.accel = float(self.LoC.update(
    CC.longActive, CS, 
    long_plan.aTarget,      # 加速度ターゲット
    long_plan.shouldStop,   # 停止フラグ
    pid_accel_limits
))

# longcontrol.pyでの処理
elif self.long_control_state == LongCtrlState.pid:
    error = a_target - CS.aEgo  # 加速度誤差
    output_accel = self.pid.update(error, speed=CS.vEgo, feedforward=a_target)
```

**利点**:
- 直接的な加速度制御で応答が速い
- モデルプレディクティブコントロール（MPC）の出力と相性が良い
- 現代的なアプローチ

#### ichiro: 速度誤差ベース + 加速度フィードフォワード
```python
# state_control.pyで呼び出し
actuators.accel = self.LoC.update(
    self.active, CS, self.CP, 
    long_plan,              # プラン全体を渡す
    pid_accel_limits
)

# longcontrol.pyでの処理
# アクチュエータ遅延補償
v_target_lower = interp(CP.longitudinalActuatorDelayLowerBound, T_IDXS[:CONTROL_N], speeds)
a_target_lower = 2 * (v_target_lower - speeds[0])/CP.longitudinalActuatorDelayLowerBound - long_plan.accels[0]
# ... upper boundも同様に計算
a_target = min(a_target_lower, a_target_upper)  # 保守的な加速度選択

# PID制御
self.v_pid = v_target
prevent_overshoot = not CP.stoppingControl and CS.vEgo < 1.5 and v_target_future < 0.7
deadzone = interp(CS.vEgo, CP.longitudinalTuning.deadzoneBP, CP.longitudinalTuning.deadzoneV)

output_accel = self.pid.update(
    self.v_pid, CS.vEgo,           # 速度誤差
    speed=CS.vEgo, 
    deadzone=deadzone,             # デッドゾーン
    feedforward=a_target,          # 加速度フィードフォワード
    freeze_integrator=freeze_integrator  # 積分器の凍結
)

if prevent_overshoot:
    output_accel = min(output_accel, 0.0)  # オーバーシュート防止
```

**特徴**:
- アクチュエータ遅延を明示的に補償（upper/lower bound）
- デッドゾーンで微小誤差を無視（チャタリング防止）
- オーバーシュート防止ロジック（低速停止時）
- 積分器の凍結機能（停止時の積分巻き上がり防止）

## 4. 停止制御の違い

### 本家
```python
elif self.long_control_state == LongCtrlState.stopping:
    output_accel = self.last_output_accel
    if output_accel > self.CP.stopAccel:
        output_accel = min(output_accel, 0.0)
        output_accel -= self.CP.stoppingDecelRate * DT_CTRL  # 一定減速率
    self.reset()
```
- シンプルな一定減速率
- 毎サイクルPIDリセット

### ichiro
```python
elif self.long_control_state == LongCtrlState.stopping:
    if not CS.standstill or output_accel > CP.stopAccel:
        output_accel -= CP.stoppingDecelRate * DT_CTRL
    output_accel = clip(output_accel, accel_limits[0], accel_limits[1])
    self.reset(CS.vEgo)  # 現在速度でリセット
```
- `standstill` フラグで完全停止を検出
- 加速度リミットを適用
- 現在速度でPIDをリセット（再発進に備える）

## 5. 加速度制限の違い

### 本家
```python
# ISO規格に厳密に準拠
from opendbc.car.interfaces import ACCEL_MIN, ACCEL_MAX

# 最終出力
self.last_output_accel = np.clip(output_accel, accel_limits[0], accel_limits[1])
```

### ichiro
```python
# ISO 15622:2018 準拠
ACCEL_MIN_ISO = -3.5  # m/s^2
ACCEL_MAX_ISO = 2.0   # m/s^2

# ターゲット加速度を事前にクリップ
a_target = clip(a_target, ACCEL_MIN_ISO, ACCEL_MAX_ISO)
# ...
# 最終出力もクリップ
final_accel = clip(output_accel, accel_limits[0], accel_limits[1])
```

## 6. openpilotLongitudinalControl フラグ

### ichiroでのToyota実装
```python
# selfdrive/car/toyota/interface.py (ichiro)
if candidate == CAR.PRIUS:
    # ... 車両パラメータ設定
    # Priusでは縦制御を無効化（車両の純正ACCを使用）
    # openpilotLongitudinalControl = False がデフォルト
```

ichiroブランチは、Priusの縦制御を**openpilotの制御に切り替えた**実装であり、以下の特徴があります：
- 車両固有のチューニングパラメータ
- アクチュエータ遅延の補償
- Prius特有の停止制御ロジック

## 7. 統合（controlsd.py）での呼び出しの違い

### 本家
```python
# selfdrive/controls/controlsd.py
# accel PID loop
pid_accel_limits = self.CI.get_pid_accel_limits(self.CP, CS.vEgo, CS.vCruise * CV.KPH_TO_MS)
actuators.accel = float(self.LoC.update(
    CC.longActive, CS, 
    long_plan.aTarget,      # ← 加速度ターゲット
    long_plan.shouldStop,   # ← 停止フラグ
    pid_accel_limits
))
```

### ichiro
```python
# selfdrive/controls/controlsd.py (ichiro)
pid_accel_limits = self.CI.get_pid_accel_limits(self.CP, CS.vEgo, self.v_cruise_kph * CV.KPH_TO_MS)
actuators.accel = self.LoC.update(
    self.active, CS, self.CP, 
    long_plan,              # ← プラン全体
    pid_accel_limits
)
```

## まとめ

| 項目 | 本家openpilot | ichiro (Prius縦制御版) |
|------|---------------|----------------------|
| 制御方式 | 加速度誤差ベース | 速度誤差 + 加速度FF |
| 状態数 | 4状態（off/stopping/starting/pid） | 3状態（off/stopping/pid） |
| アクチュエータ遅延補償 | なし | あり（upper/lower bound） |
| デッドゾーン | なし | あり（速度依存） |
| オーバーシュート防止 | なし | あり（低速時） |
| 積分器凍結 | なし | あり（停止時） |
| Priusサポート | 標準サポート | 縦制御カスタマイズ |

ichiroブランチは、Priusの特性に合わせた、より細かい制御チューニングが施されています。特にアクチュエータ遅延補償とオーバーシュート防止により、滑らかな加減速と安定した停止制御を実現しています。

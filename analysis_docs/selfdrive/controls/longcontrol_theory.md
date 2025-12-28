# 縦方向制御（LongControl）理論・実装・背景解説

---

## 1. 背景と意図
- 縦方向制御（Longitudinal Control）は、車両の加速・減速（アクセル・ブレーキ）を自動で調整し、目標速度や車間距離を維持するための制御です。
- openpilot では、前方車両への追従や停止・発進、滑らかな加減速を実現するために、PID制御を中心としたアルゴリズムを採用しています。

---

## 2. 制御理論（PID制御）
- **PID制御**とは、目標値（ここでは目標加速度や速度）と現在値（実際の加速度や速度）の誤差をもとに、比例（P）、積分（I）、微分（D）の3要素で制御量を決定する方法です。
- openpilot では主にP（比例）とI（積分）を使い、滑らかで安定した加減速を実現しています。
- 状態遷移（停止・発進・巡航など）も組み合わせ、実車両の挙動に近づけています。

---

## 3. コード構造と主なクラス
- 実装ファイル: `selfdrive/controls/lib/longcontrol.py`
- 主なクラス: `LongControl`
  - `__init__`: パラメータやPIDコントローラの初期化
  - `update(active, CS, a_target, should_stop, accel_limits)`: 状態遷移とPID計算を実行し、最終的な加速度コマンドを返す
  - 状態遷移関数: `long_control_state_trans`（停止・発進・巡航などを自動で切り替え）

---

## 4. 代表的なコード例
```python
class LongControl:
    def __init__(self, CP):
        self.CP = CP
        self.long_control_state = LongCtrlState.off
        self.pid = PIDController(...)

    def update(self, active, CS, a_target, should_stop, accel_limits):
        # 状態遷移
        self.long_control_state = long_control_state_trans(...)
        # PID制御
        error = a_target - CS.aEgo
        output_accel = self.pid.update(error, speed=CS.vEgo, feedforward=a_target)
        ...
        return output_accel
```

---

## 5. ポイント・工夫
- 停止・発進・巡航などの状態を自動で切り替え、実車両の自然な挙動を再現
- PIDゲインや制限値は車種ごとに最適化
- 安全性のため、急激な加減速や不自然な動作を抑制

---

## 6. まとめ
- 縦方向制御は、車両の快適性・安全性・追従性能を左右する重要な要素
- openpilot では、シンプルかつ堅牢なPID制御と状態遷移ロジックで実現

---

## longcontrol.py コード詳細解説

### LongControl クラス
- 車両の縦方向制御（加減速）を担当。
- **主なメンバ**:
  - `CP`: 車両パラメータ
  - `long_control_state`: 制御状態（off, stopping, starting, pid）
  - `pid`: PIDControllerインスタンス。車種ごとのゲイン・制限値で初期化。
  - `last_output_accel`: 前回の出力加速度（停止・発進時の滑らかさ確保）

### 主なメソッド
- `__init__()`
  - パラメータ・PIDコントローラの初期化。
- `update(active, CS, a_target, should_stop, accel_limits)`
  - 状態遷移（停止・発進・巡航）を自動で切り替え。
  - PID制御で目標加速度と実際の加速度の誤差を補正。
  - 状態ごとに出力加速度を決定（停止時は減速、発進時は加速、巡航時はPID）。
  - 最終的な加速度コマンドを返す。
- `reset()`
  - PIDコントローラのリセット。

### 状態遷移ロジック
- `long_control_state_trans()` 関数で、
  - 停止条件（should_stop, brake_pressed, cruise_standstill）
  - 発進条件（v_ego > vEgoStarting, ブレーキ解除）
  - 巡航条件（PID制御）
  を判定し、状態を自動で切り替え。

### 代表的なコード例
```python
class LongControl:
    def update(self, active, CS, a_target, should_stop, accel_limits):
        self.long_control_state = long_control_state_trans(...)
        if self.long_control_state == LongCtrlState.off:
            self.reset()
            output_accel = 0.
        elif self.long_control_state == LongCtrlState.stopping:
            output_accel = self.last_output_accel
            ...
        elif self.long_control_state == LongCtrlState.starting:
            output_accel = self.CP.startAccel
            ...
        else:  # PID制御
            error = a_target - CS.aEgo
            output_accel = self.pid.update(error, speed=CS.vEgo, feedforward=a_target)
        self.last_output_accel = np.clip(output_accel, accel_limits[0], accel_limits[1])
        return self.last_output_accel
```

### 工夫・ポイント
- 状態遷移で実車両の自然な挙動を再現（急停止・急発進を防止）
- PIDゲインや制限値は車種ごとに最適化
- 安全性のため、急激な加減速や不自然な動作を抑制

# 横方向制御（LatControl）理論・実装・背景解説

---

## 1. 背景と意図
- 横方向制御（Lateral Control）は、車両の進行方向（操舵・舵角・トルク）を自動で調整し、車線維持やカーブ走行を実現するための制御です。
- openpilot では、車両モデルやPID制御、トルク制御など複数の方式を車種や状況に応じて使い分けています。

---

## 2. 制御理論
- **角度制御（Angle）**: 目標曲率や車線情報から必要な操舵角を計算し、その角度にステアリングを合わせる方式。
- **PID制御**: 目標操舵角と実際の操舵角の誤差をPIDコントローラで補正。
- **トルク制御**: 目標横加速度や車両モデルから必要な操舵トルクを計算し、ステアリングモーターに指示。
- いずれも、車両の速度やロール、ドライバー操作などを考慮し、安全かつ滑らかな操舵を目指す。

---

## 3. コード構造と主なクラス
- 実装ファイル: `selfdrive/controls/lib/latcontrol_angle.py`, `latcontrol_pid.py`, `latcontrol_torque.py`
- 主なクラス:
  - `LatControlAngle`: 角度ベースの横方向制御
  - `LatControlPID`: PID制御による横方向制御
  - `LatControlTorque`: トルクベースの横方向制御
- いずれも `update(active, CS, VM, params, ...)` メソッドで、車両状態やモデル情報から舵角・トルクを計算

---

## 4. 代表的なコード例
```python
class LatControlPID(LatControl):
    def update(self, active, CS, VM, params, ...):
        angle_steers_des = ... # 目標操舵角
        error = angle_steers_des - CS.steeringAngleDeg
        output_steer = self.pid.update(error, override=CS.steeringPressed, ...)
        ...
        return output_steer, angle_steers_des, pid_log
```

---

## 5. ポイント・工夫
- 車両モデル（VehicleModel）を活用し、速度やロールに応じた最適な舵角・トルクを算出
- ドライバー操作や安全制限（例: ステアリング故障時の無効化）も考慮
- 車種ごとに最適な制御方式・パラメータを選択

---

## 6. まとめ
- 横方向制御は、車線維持やカーブ走行の安定性・安全性を左右する重要な要素
- openpilot では、複数の制御方式を柔軟に使い分け、実車両の特性に合わせて最適化

---

## latcontrol_angle.py コード詳細解説

### LatControlAngle クラス
- `LatControl` を継承し、角度ベースで横方向制御を行う。
- **主なメンバ**:
  - `sat_check_min_speed`: サチュレーション（制御限界）判定の最低速度。
  - `use_steer_limited_by_controls`: 車種ごとに制御限界判定方法を切り替え。
- **update() メソッド**:
  - active（制御有効）時は、車両モデル（VM）から目標曲率→操舵角を計算。
  - 実際の操舵角との差分やサチュレーション判定を行い、HUD表示用ログも生成。
  - サチュレーション判定は、車種ごとに「制御限界」か「目標と実際の差分」で判定。

---

## latcontrol_pid.py コード詳細解説

### LatControlPID クラス
- `LatControl` を継承し、PID制御で横方向制御を行う。
- **主なメンバ**:
  - `pid`: PIDControllerインスタンス。車種ごとのゲイン・制限値で初期化。
  - `get_steer_feedforward`: 車両インターフェースからフィードフォワード関数取得。
- **update() メソッド**:
  - 目標操舵角（angle_steers_des）と実際の操舵角の誤差（error）を計算。
  - PID制御で出力操舵量（output_steer）を算出。
  - フィードフォワード（車速や目標角に応じた補助量）も加味。
  - サチュレーション判定やHUD表示用ログも生成。
  - active（制御有効）でない場合は出力ゼロ＆リセット。

---

## latcontrol_torque.py コード詳細解説

### LatControlTorque クラス
- `LatControl` を継承し、トルクベースで横方向制御を行う。
- **主なメンバ**:
  - `torque_params`: 車種ごとのトルク制御パラメータ。
  - `pid`: PIDControllerインスタンス。トルク制御用ゲイン・制限値で初期化。
  - `torque_from_lateral_accel`: 車両インターフェースから横加速度→トルク変換関数取得。
  - `use_steering_angle`: 制御方式切り替え（角度ベース or 車両姿勢ベース）。
- **update() メソッド**:
  - 目標横加速度・実際の横加速度・車両ロール等から必要トルクを計算。
  - 低速補正やデッドゾーン（微小角度無視）も考慮。
  - PID制御で出力トルクを算出。フィードフォワードも加味。
  - サチュレーション判定やHUD表示用ログも生成。
  - active（制御有効）でない場合は出力ゼロ。

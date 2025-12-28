# selfdrive/controls 詳細解説

このドキュメントは、selfdrive/controls ディレクトリ内の各主要ファイル・クラス・関数について、初心者にも分かりやすく技術的に解説します。

---

## controlsd.py
- **役割**: openpilot の車両制御メインプロセス。
- **主なクラス・関数**:
  - `main()`：全体の初期化と制御ループの開始。
  - `ControlSDThread`（存在する場合）：制御ループのスレッド管理。
  - 各種メッセージ受信・送信、状態管理、制御コマンド生成。

---

## plannerd.py
- **役割**: 経路計画（プランニング）を担当。
- **主なクラス・関数**:
  - `main()`：プランナーの初期化とループ。
  - 経路・速度目標の計算ロジック。

---

## radard.py
- **役割**: レーダー情報の処理・管理。
- **主なクラス・関数**:
  - `main()`：レーダー処理の初期化とループ。
  - 前方車両の検出・追従ロジック。

---

## lib/ 配下
- **latcontrol.py / latcontrol_angle.py / latcontrol_pid.py / latcontrol_torque.py**
  - 横方向制御（操舵）の各種アルゴリズムをクラスで実装。
  - 例: `LatControlAngle`, `LatControlPID`, `LatControlTorque` など。
- **longcontrol.py**
  - 縦方向制御（加減速）のロジック。`LongControl` クラス。
- **drive_helpers.py, desire_helper.py**
  - 制御補助関数や目標生成ロジック。
- **lateral_mpc_lib/, longitudinal_mpc_lib/**
  - モデル予測制御（MPC）関連のクラス・関数。
- **ldw.py**
  - 車線逸脱警報（LDW）ロジック。

---

## tests/ 配下
- **test_following_distance.py, test_lateral_mpc.py, test_leads.py, test_longcontrol.py**
  - 各種制御ロジックの自動テスト。テストクラス・関数ごとに、制御アルゴリズムの妥当性や安全性を検証。

---

# ファイルごとのクラス・関数詳細

## controlsd.py
- **Controls クラス**
  - openpilotの車両制御メインクラス。初期化で各種サブモジュール（LongControl, LatControl, VehicleModel等）を生成。
  - `update()`：各種センサ・状態の受信と更新。
  - `state_control()`：車両状態・計画に基づき制御コマンドを生成。
  - `publish()`：制御コマンドや状態をpublish。
  - `run()`：制御ループ本体。
- **main()**
  - プロセス初期化とControlsクラスの起動。

## plannerd.py
- **main()**
  - 経路計画プロセスのエントリポイント。LaneDepartureWarning, LongitudinalPlannerを生成し、各種計画・警報をループで更新。

## radard.py
- **Track クラス**
  - レーダーで検出した物体（前方車両等）の追跡・状態推定。
- **RadarD クラス**
  - レーダー全体の管理・物体追跡・状態publish。
- **main()**
  - レーダープロセスのエントリポイント。

## lib/ 配下
### latcontrol_angle.py
- **LatControlAngle クラス**
  - 角度ベースの横方向制御。目標操舵角を計算。
### latcontrol_pid.py
- **LatControlPID クラス**
  - PID制御による横方向制御。目標操舵角と実際の差分をPIDで補正。
### latcontrol_torque.py
- **LatControlTorque クラス**
  - トルクベースの横方向制御。目標横加速度から必要トルクを計算。
### longcontrol.py
- **LongControl クラス**
  - 縦方向制御（加減速）。状態遷移とPID制御で加速度を決定。
### drive_helpers.py
- **clip_curvature()**
  - カーブ時の横加速度・ジャーク制限を考慮した目標曲率計算。
- **その他**: clamp, smooth_value など補助関数。
### desire_helper.py
- **DesireHelper クラス**
  - 車線変更意図や状態管理。
### ldw.py
- **LaneDepartureWarning クラス**
  - 車線逸脱警報ロジック。
### longitudinal_planner.py
- **LongitudinalPlanner クラス**
  - 速度・加速度目標の計画、MPCによる最適化。

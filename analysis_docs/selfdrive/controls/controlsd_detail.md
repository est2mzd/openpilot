# controlsd.py コード詳細解説

このドキュメントは、openpilot の selfdrive/controls/controlsd.py の全体構造と各クラス・関数・主要ロジックについて、初心者にも分かりやすく技術的に解説します。

---

## 概要
controlsd.py は、openpilot の車両制御メインプロセスです。車両状態や各種センサ情報を受信し、ロング（縦方向）・ラテラル（横方向）制御を統合して車両にコマンドを送信します。

---

## クラス・関数構成

### Controls クラス
- openpilot の制御中枢クラス。
- 初期化時に各種サブモジュール（LongControl, LatControl, VehicleModel など）を生成。
- 主なメンバ:
  - `self.CP` : 車両パラメータ
  - `self.CI` : 車両インターフェース
  - `self.LoC` : 縦方向制御（LongControl）
  - `self.LaC` : 横方向制御（LatControlAngle/PID/Torque）
  - `self.VM` : 車両モデル
  - `self.sm` : 各種サブシステムからの受信データ管理
  - `self.pm` : 各種トピックへのpublish

#### 主なメソッド
- `__init__()`
  - 各種パラメータ・サブモジュールの初期化。
- `update()`
  - 各種センサ・状態の受信と更新。
  - キャリブレーションや車両姿勢の補正もここで実施。
- `state_control()`
  - 車両状態・計画に基づき制御コマンド（加速度・舵角等）を生成。
  - ロング制御（LoC）・ラテラル制御（LaC）を呼び出し、アクチュエータ値を決定。
  - 各種安全判定や有効/無効切り替えもここで実施。
- `publish(CC, lac_log)`
  - 生成した制御コマンドや状態を carControl, controlsState などのトピックにpublish。
  - HUD表示用の情報もここで設定。
- `run()`
  - 制御ループ本体。update→state_control→publish を繰り返し実行。

### main()
- プロセス初期化と Controls クラスの起動。
- リアルタイム優先度設定もここで行う。

---

## 主要ロジックの流れ
1. CarParams 取得・初期化
2. 各種サブモジュール（LongControl, LatControl, VehicleModel等）生成
3. 受信データ（車両状態・計画・センサ等）の更新
4. 制御コマンド（加速度・舵角等）の計算
5. コマンド・状態のpublish
6. ループ継続

---

## 制御アルゴリズム詳細

### 縦方向制御（LongControl）
- **役割**: 車両の加減速（アクセル・ブレーキ）を制御。
- **実装**: `LongControl` クラス（lib/longcontrol.py）
  - **PID制御**: 目標加速度（a_target）と実際の加速度（aEgo）の誤差をPIDコントローラで補正。
  - **状態遷移**: 停止・発進・巡航などの状態を自動で切り替え。
  - **主なメソッド**:
    - `update(active, CS, a_target, should_stop, accel_limits)`：状態遷移とPID計算を実行し、最終的な加速度コマンドを返す。

### 横方向制御（LatControlAngle/PID/Torque）
- **役割**: 車両の操舵（舵角・トルク）を制御。
- **実装**:
  - `LatControlAngle`（lib/latcontrol_angle.py）：目標曲率から操舵角を計算。
  - `LatControlPID`（lib/latcontrol_pid.py）：目標操舵角と実際の操舵角の誤差をPIDで補正。
  - `LatControlTorque`（lib/latcontrol_torque.py）：目標横加速度から必要な操舵トルクを計算。
- **主なメソッド**:
  - `update(active, CS, VM, params, ...)`：各方式ごとに、車両状態やモデル情報から舵角・トルクを計算し、アクチュエータ値を返す。

### 安全判定
- **役割**: 制御の有効/無効や警告の発生を判断。
- **実装例**:
  - `state_control()`（controlsd.py）内で、
    - ステアリング故障（steerFaultTemporary, steerFaultPermanent）
    - 停止中や低速時の制御無効化
    - 各種 onroadEvents による制御禁止
    - ドライバー監視状態による強制減速
  - **HUD表示**や警告もここで設定
- **主な判定ポイント**:
  - `latActive`/`longActive` の有効・無効切り替え
  - `forceDecel`（ドライバー監視低下時の強制減速）
  - `visualAlert` などの警告表示

---

## コードリーディングのポイント
- 制御の全体像を掴むには Controls クラスの `run()` → `update()` → `state_control()` → `publish()` の流れを追うのが有効
- 各サブモジュール（LoC, LaC, VM など）は lib/ 配下の個別ファイルで詳細実装

---

（このファイルは自動生成されており、今後のバージョンアップで内容が変わる可能性があります）

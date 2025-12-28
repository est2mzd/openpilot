# selfdrive/car 詳細解説

このドキュメントでは、`selfdrive/car` ディレクトリ内の各ファイル・サブディレクトリについて、初心者向けに技術的な背景や使い方、各行・各ファイルの意味を丁寧に解説します。

---

## car_specific.py
- 各車種ごとの個別制御ロジックやパラメータを定義するPythonファイル
- 例：CAN信号の解釈、車速・舵角・アクセル/ブレーキ制御の個別実装
- 新しい車種を追加する場合は、このファイルにクラスや関数を追加

## card.py
- 車両情報カードやUI表示用のデータを管理
- 例：車種名、年式、特徴、サポート状況などをまとめて管理

## cruise.py
- クルーズコントロール（ACC等）の共通処理を実装
- 例：速度維持、前走車追従、加減速ロジックなど
- 車種ごとの差分は`car_specific.py`で吸収し、共通部分はここで一元管理

## docs.py
- 車両ごとの仕様や解説をまとめたドキュメント生成用スクリプト
- 例：サポート車種一覧や仕様表を自動生成

## tests/
- 車両制御ロジックの自動テストをまとめたディレクトリ
- 例：各車種のCAN信号解釈や制御ロジックの単体テスト・統合テスト

## CARS_template.md
- 新規車種追加時のテンプレートMarkdown
- 追加時はこのテンプレートをコピーして記入

## __init__.py
- Pythonパッケージとして認識させるための初期化ファイル

---

# selfdrive/car クラス・関数詳細解説

---

## car_specific.py の主なクラス・関数

### MockCarState クラス
- **目的**: テストやシミュレーション用の仮想的な車両状態（CarState）を生成。
- **主なメソッド**:
  - `__init__`: GPSデータの受信準備。
  - `update(CS)`: GPS速度をCarStateに反映。

### CarSpecificEvents クラス
- **目的**: 車種ごとのイベント（警告や状態変化）を生成・管理。
- **主なメソッド**:
  - `__init__`: 車両パラメータを受け取り初期化。
  - `update(CS, CS_prev, CC)`: 車種ごとにイベントを生成。速度やギア、クルーズ状態などを判定し、必要なイベント（例: ドア開放、シートベルト未装着、低速警告など）を追加。
  - `create_common_events(...)`: 多くの車種で共通するイベントをまとめて生成。

---

## card.py の主なクラス・関数

### Car クラス
- **目的**: 実車両インターフェース、レーダー、パラメータ管理、CAN通信など、車両制御の中核を担うクラス。
- **主なメンバ**:
  - `CI`: 車両インターフェース（CarInterfaceBase）
  - `RI`: レーダーインターフェース（RadarInterfaceBase）
  - `CP`: 車両パラメータ
- **主なメソッド**:
  - `__init__`: 各種ソケットやパラメータの初期化、車両インターフェースの自動検出。
  - `state_update()`: CANメッセージから車両状態・レーダーデータを更新。
  - `state_publish(CS, RD)`: 車両状態やパラメータを各種トピックにpublish。
  - `controls_update(CS, CC)`: 車両制御コマンドを適用。
  - `step()`: 1サイクル分の状態更新・制御・publishをまとめて実行。
  - `params_thread(evt)`, `card_thread()`: パラメータ監視やメインループのスレッド管理。

---

## cruise.py の主なクラス・関数

### VCruiseHelper クラス
- **目的**: クルーズコントロールの目標速度やボタン操作の管理。
- **主なメンバ**:
  - `v_cruise_kph`: 現在のクルーズ設定速度（kph）
  - `button_timers`: クルーズボタンの押下時間管理
- **主なメソッド**:
  - `update_v_cruise(CS, enabled, is_metric)`: 車両状態やボタン操作からクルーズ速度を更新。
  - `_update_v_cruise_non_pcm(...)`: 純正クルーズ非搭載車向けの速度更新ロジック。
  - `update_button_timers(CS, enabled)`: ボタン押下状態の管理。
  - `initialize_v_cruise(CS, experimental_mode)`: クルーズ初期化。

---

## docs.py の主な関数
- `generate_cars_md`：サポート車種一覧Markdown生成
- `get_all_car_docs`：全車種のドキュメント情報取得

---

## tests/ の主なテストクラス・関数

### test_models.py
- `TestCarModelBase`：ログデータから車両状態を再現し、各種安全・制御テストを実施
- `TestCarModel`：各車種・ルートごとにTestCarModelBaseを継承しテスト

### test_car_interfaces.py
- 各車種インターフェースのパラメータや制御ロジックの妥当性テスト

### test_cruise_speed.py
- クルーズコントロールの速度設定・ボタン操作ロジックのテスト

### test_docs.py
- ドキュメント生成スクリプトのテスト

---


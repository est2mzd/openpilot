# selfdrive/debug 詳細解説

このドキュメントは、selfdrive/debug ディレクトリ内の各主要ファイル・クラス・関数について、初心者にも分かりやすく技術的に解説します。

---

## コードレベル詳細解説

### can_print_changes.py
- CANバス上の信号（addressごとのdat）のビット変化を検出・表示。
- `update()`関数で、CANメッセージを走査し、各addressごとに新しいbitの立ち上がり/下がりを判定。
- 変化があった場合、時刻・address・変化内容（+/-）・データを表示。
- `can_printer()`は、初期状態と比較対象状態のCANメッセージを受け取り、差分を表示。liveモードではリアルタイムで変化を監視。
- コマンドライン引数でbusや比較対象ルート、テーブル表示も選択可能。

### can_printer.py
- 指定したCAN busのメッセージをリアルタイムで受信・表示。
- addressごとに最新データ・受信回数・周波数（Hz）を表示。
- asciiデコードも可能。
- コマンドライン引数でbusや最大address、ascii表示を選択。

### can_table.py
- 1バイトごとのbit列をテーブル形式で表示。
- `can_table(dat)`関数で、各バイトを2進数・16進数で整形し、pandasでmarkdownテーブル化。
- コマンドライン引数でaddress・busを指定し、該当CANデータをテーブル表示。

### check_can_parser_performance.py
- CANパーサの処理速度・性能を計測。
- `CarModelTestCase`（TestCarModelBase継承）でCANメッセージを取得。
- N_RUNS回ループし、各CANメッセージを各パーサでupdate_strings。
- 実行時間（ms）を計測し、平均・最大・最小・標準偏差を表示。

---

## CAN関連ツール
### can_print_changes.py
- CAN信号の変化を検出・表示。主に差分解析用。
### can_printer.py
- CANメッセージをリアルタイムで表示。デバッグや信号確認に便利。
### can_table.py
- CAN信号の一覧・テーブル表示。

---

## パフォーマンス・タイミング系
### check_can_parser_performance.py
- CANパーサの処理速度・性能を計測。
### check_freq.py
- 各種信号やイベントの発生頻度を計測。
### check_lag.py
- 信号や処理の遅延（ラグ）を計測。
### check_timings.py
- 各種処理のタイミングを詳細に記録。

---

## イベント・警告系
### count_events.py
- ログや信号からイベント発生回数を集計。
### cycle_alerts.py
- 警告やアラートの周期・発生タイミングを解析。

---

## ログ・ドキュメント系
### dump.py
- 各種ログやデータのダンプ（出力）。
### dump_car_docs.py
- 車両ドキュメント情報のダンプ・保存。
### print_docs_diff.py
- ドキュメントの差分比較・表示。

---

## 車両・ECU関連（car/ 配下）
- clear_dtc.py：DTC（故障コード）のクリア
- disable_ecu.py：ECUの無効化
- ecu_addrs.py：ECUアドレス情報取得
- fw_versions.py：ファームウェアバージョン取得
- hyundai_enable_radar_points.py：Hyundai車のレーダーポイント有効化
- toyota_eps_factor.py：Toyota車のEPS補正
- vin.py：車両識別番号（VIN）取得
- vw_mqb_config.py：VW MQB車両の設定

---

## その他
- filter_log_message.py：ログメッセージのフィルタ・整形
- format_fingerprints.py：車両識別情報の整形
- fuzz_fw_fingerprint.py：ファームウェア識別情報のファジング
- get_fingerprint.py：車両識別情報の取得
- live_cpu_and_temp.py：CPU温度・使用率のリアルタイム表示
- read_dtc_status.py：DTC（故障コード）取得
- run_process_on_route.py：指定ルートでプロセス実行
- set_car_params.py：車両パラメータ設定
- test_fw_query_on_routes.py：ファームウェアクエリのテスト
- touch_replay.py：リプレイ用ファイル操作
- uiview.py：UI表示補助

---

（各ファイルのクラス・関数詳細は必要に応じて追記可能です）

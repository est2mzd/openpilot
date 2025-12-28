# selfdrive/debug 概要

このドキュメントは、openpilot の selfdrive/debug ディレクトリの全体像と役割を初心者向けに解説します。

---

## ディレクトリ構成と主な役割

- **can_print_changes.py / can_printer.py / can_table.py**
  - CAN信号の解析・表示・変化検出用ツール。
- **check_can_parser_performance.py / check_freq.py / check_lag.py / check_timings.py**
  - 各種パフォーマンス・タイミング・遅延の計測・検証。
- **count_events.py / cycle_alerts.py**
  - イベントや警告の発生頻度・周期の解析。
- **dump.py / dump_car_docs.py / print_docs_diff.py**
  - ログや車両ドキュメントのダンプ・差分表示。
- **filter_log_message.py / format_fingerprints.py / fuzz_fw_fingerprint.py**
  - ログメッセージや車両識別情報の整形・検証。
- **car/**
  - ECUや車両固有情報の取得・設定・解析ツール群。
- **その他**
  - CPU温度・使用率計測、DTC（故障コード）取得、プロセス実行補助など、デバッグ・解析に役立つスクリプト多数。

---

## 使い方・カスタマイズのポイント
- 車両やCAN信号の解析・デバッグを行いたい場合は、目的に応じたスクリプトを選択
- ECUや車両固有情報の操作は car/ 配下のツールを利用
- パフォーマンスやタイミング検証は check_ 系スクリプトが便利

---

（詳細なクラス・関数解説は debug_detail.md を参照してください）

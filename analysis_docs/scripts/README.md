

```text
scripts/
├── apply-pr.sh           # PRパッチ適用
├── cell.sh               # GPUテスト用ループ
├── checkout-pr.sh        # PRブランチのチェックアウト
├── disable-powersave.py  # デバイス省電力無効化
├── jenkins_loop_test.sh  # Jenkinsループテスト
├── launch_corolla.sh     # Corolla用起動スクリプト
├── lint/                 # Lint関連
│   ├── check_nomerge_comments.sh
│   ├── check_raylib_includes.sh
│   ├── check_shebang_format.sh
│   └── lint.sh
├── post-commit           # コミット後フック
├── reporter.py           # モデルチェックポイント比較
├── retry.sh              # コマンド自動リトライ
├── stop_updater.sh       # アップデータ停止
├── update_now.sh         # アップデータ即時実行
├── usb.sh                # USB関連
├── waste.c               # CPU/RAM負荷テスト（C）
└── waste.py              # CPU負荷テスト（Python）
```


# scripts ディレクトリ詳細ドキュメント

このドキュメントは `/scripts` ディレクトリの全スクリプトについて、カテゴリごとに「背景・意図・目的」→「概要」→「詳細（初心者向け解説）」→「根拠（ファイル・行番号）」の順でまとめます。

---


## 1. PR管理

### 背景・意図・目的
オープンソース開発では、GitHubのPull Request（PR）を使って他の開発者の変更を取り込むことが一般的です。openpilotでは、PRの内容を素早くローカルに適用・検証できるよう、専用のスクリプトを用意しています。これにより、初心者でも複雑なgitコマンドを覚えずにPRのテストができます。

### 概要
PRのパッチ適用やブランチのチェックアウトを自動化し、レビューや動作確認を効率化します。

### 詳細
- **apply-pr.sh**: 指定したPR番号の変更内容（パッチ）をダウンロードし、現在の作業ツリーに適用します。
  - 使い方: `./apply-pr.sh 1234` のようにPR番号を指定して実行します。
  - 何が起きるか: GitHubからパッチファイルを取得し、`git apply`で自動的に変更を反映します。手動でパッチをダウンロード・適用する手間が省けます。
  - 失敗時はusage（使い方）を表示します。
  - 根拠: [scripts/apply-pr.sh](../../scripts/apply-pr.sh#L1-L13)
- **checkout-pr.sh**: 指定したPR番号のリモートブランチをローカルに作成し、そのブランチに切り替えます。
  - 使い方: `./checkout-pr.sh 1234` のようにPR番号を指定して実行します。
  - 何が起きるか: PRの内容を反映した一時的なブランチを自動で作成し、すぐにその状態で作業できます。元のブランチに戻すのも簡単です。
  - 既存の同名ブランチは自動で削除されます。
  - 根拠: [scripts/checkout-pr.sh](../../scripts/checkout-pr.sh#L1-L15)

---


## 2. デバイス制御

### 背景・意図・目的
openpilotは車載デバイス上で動作するため、電源管理やアップデート、車種ごとの起動設定など、ハードウェア制御が重要です。これらの操作を手作業で行うのは難しいため、初心者でも安全・確実に操作できるスクリプトを用意しています。

### 概要
デバイスの省電力設定やアップデータ制御、特定車種向けの起動など、ハードウェアに関わる操作を自動化します。

### 詳細
- **disable-powersave.py**: デバイスの省電力モードを無効化します。
  - 使い方: `python3 disable-powersave.py` で実行。
  - 何が起きるか: openpilotのHARDWARE APIを使い、スリープや省電力状態を解除します。デバッグや長時間動作時に便利です。
  - 根拠: [scripts/disable-powersave.py](../../scripts/disable-powersave.py#L1-L5)
- **stop_updater.sh**: アップデート用プロセスを安全に停止し、未完了のアップデート情報を削除します。
  - 使い方: `./stop_updater.sh` で実行。
  - 何が起きるか: system.updated.updatedプロセスにSIGINTを送り、アップデートの一時ファイルを消します。アップデートの不具合時に有効です。
  - 根拠: [scripts/stop_updater.sh](../../scripts/stop_updater.sh#L1-L8)
- **update_now.sh**: アップデート用プロセスにSIGHUPを送り、即時アップデートを促します。
  - 使い方: `./update_now.sh` で実行。
  - 何が起きるか: system.updatedプロセスに信号を送り、アップデートを即座に開始させます。
  - 根拠: [scripts/update_now.sh](../../scripts/update_now.sh#L1-L6)
- **launch_corolla.sh**: トヨタCorolla用のopenpilotを起動します。
  - 使い方: `./launch_corolla.sh` で実行。
  - 何が起きるか: 必要な環境変数をセットし、Corolla用の設定でopenpilotを起動します。車種ごとの違いを意識せずに使えます。
  - 根拠: [scripts/launch_corolla.sh](../../scripts/launch_corolla.sh#L1-L8)

---


## 3. Jenkins/CI

### 背景・意図・目的
openpilotの品質を保つためには、継続的インテグレーション（CI）による自動テストや、コミット時の自動チェックが不可欠です。これらを手作業で行うのは非効率なので、CIサーバーやGitフックで使えるスクリプトを整備しています。

### 概要
Jenkins等のCI環境での自動ビルド・テストや、コミット時の自動リント実行をサポートします。

### 詳細
- **jenkins_loop_test.sh**: Jenkinsで同じビルド・テストを何度も繰り返し実行し、安定性や再現性を検証します。
  - 使い方: Jenkinsサーバー上で自動実行。
  - 何が起きるか: 一時的なテスト用ブランチを作成し、Jenkins APIで複数回ビルド・テストを実行、進捗を監視します。
  - 色付きの進捗表示や自動クリーンアップも行います。
  - 根拠: [scripts/jenkins_loop_test.sh](../../scripts/jenkins_loop_test.sh#L1-L60)
- **post-commit**: Gitのコミット直後に自動でリント（コードチェック）を実行します。
  - 使い方: `.git/hooks/post-commit`に設置。
  - 何が起きるか: コミット後にtools/op.sh lint --fastを呼び出し、コード品質を自動で担保します。
  - 根拠: [scripts/post-commit](../../scripts/post-commit#L1-L6)

---


## 4. Lint/コードチェック

### 背景・意図・目的
大規模なプロジェクトでは、複数人が同時に開発するため、コードの書き方や品質を統一することが重要です。openpilotでは、リント（静的解析）スクリプトを使って、ミスやスタイルの不統一を自動で検出・修正します。初心者でも安心して開発できる環境を目指しています。

### 概要
PythonやBashのコード品質・一貫性を保つための自動チェックツール群です。

### 詳細
- **lint/lint.sh**: プロジェクト全体のリント・静的解析を一括で実行します。
  - 使い方: `./lint.sh` で実行。
  - 何が起きるか: 除外パターンを定義し、ruff（Python linter）やmypy、codespellなど複数の外部ツールとサブスクリプトを順番に呼び出します。エラーや警告があれば色付きで表示します。
  - 根拠: [scripts/lint/lint.sh](../../scripts/lint/lint.sh#L1-L60)
- **lint/check_nomerge_comments.sh**: `NOMERGE`コメントが残っていないかをチェックします。
  - 使い方: ファイルリストを引数で指定。
  - 何が起きるか: `#`や`//`に続くNOMERGEコメントをgrepで検出し、見つかれば警告を出します。PRマージ前の最終チェックに便利です。
  - 根拠: [scripts/lint/check_nomerge_comments.sh](../../scripts/lint/check_nomerge_comments.sh#L1-L10)
- **lint/check_raylib_includes.sh**: raylibヘッダの不正なincludeを検出します。
  - 使い方: ファイルリストを引数で指定。
  - 何が起きるか: `#include "third_party/raylib/include/raylib.h"`の記述を探し、正しいパス以外で使われていれば警告します。
  - 根拠: [scripts/lint/check_raylib_includes.sh](../../scripts/lint/check_raylib_includes.sh#L1-L10)
- **lint/check_shebang_format.sh**: Python/Bashスクリプトのシバン行（#!）の形式をチェックします。
  - 使い方: ファイルリストを引数で指定。
  - 何が起きるか: Pythonは`#!/usr/bin/env python3`、Bashは`#!/usr/bin/env bash`以外のシバンを検出し、誤りがあれば警告します。
  - 根拠: [scripts/lint/check_shebang_format.sh](../../scripts/lint/check_shebang_format.sh#L1-L15)

---


## 5. ユーティリティ

### 背景・意図・目的
openpilotの開発・運用現場では、デバッグやベンチマーク、ネットワーク設定など多様な作業が発生します。これらを手作業で行うのは非効率なので、よく使う操作をスクリプト化し、初心者でも簡単に使えるようにしています。

### 概要
開発・デバッグ・ベンチマーク・ネットワーク設定等の補助スクリプト群です。

### 詳細
- **reporter.py**: モデルONNXファイルのチェックポイント情報を比較し、Markdown表で出力します。
  - 使い方: `python3 reporter.py` で実行。
  - 何が起きるか: モデルファイルのメタデータを自動で抽出し、master/PRブランチの違いを表で比較できます。
  - 根拠: [scripts/reporter.py](../../scripts/reporter.py#L1-L32)
- **retry.sh**: コマンド失敗時に自動で再試行します。
  - 使い方: `./retry.sh <コマンド>` のように使います。
  - 何が起きるか: コマンドが失敗しても最大3回まで自動でリトライし、一時的な失敗に強くなります。
  - 根拠: [scripts/retry.sh](../../scripts/retry.sh#L1-L20)
- **usb.sh**: nmcliコマンドでesimのネットワーク設定を一時的に変更し、接続を有効化します。
  - 使い方: `./usb.sh` で実行。
  - 何が起きるか: ルートメトリックを調整し、ネットワークの優先度を変更できます。
  - 根拠: [scripts/usb.sh](../../scripts/usb.sh#L1-L4)
- **waste.c**: CPUやメモリに高負荷をかけるCプログラムです。
  - 使い方: `gcc -O2 waste.c -lpthread -owaste` でビルドし、`./waste`で実行。
  - 何が起きるか: 各CPUコアにプロセスを割り当て、128MBのメモリを使ってベクトル演算を繰り返します。ハードウェアのベンチマークやストレステストに使えます。
  - 根拠: [scripts/waste.c](../../scripts/waste.c#L1-L60)
- **waste.py**: PythonでCPUに高負荷をかけるプログラムです。
  - 使い方: `python3 waste.py` で実行。
  - 何が起きるか: 各CPUコアごとにプロセスを立ち上げ、numpyで行列積を繰り返します。Python環境で手軽にベンチマークできます。
  - 根拠: [scripts/waste.py](../../scripts/waste.py#L1-L34)
- **cell.sh**: tinygradのGPUテストループを自動で回します。
  - 使い方: `./cell.sh` で実行。
  - 何が起きるか: tinygradのサンプルプログラム（beautiful_cartpole.py）を無限ループで実行し、GPUの動作確認やデバッグに使えます。
  - 根拠: [scripts/cell.sh](../../scripts/cell.sh#L1-L11)

---

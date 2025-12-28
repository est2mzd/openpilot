
# PR管理スクリプト詳細（初心者向け解説）

このドキュメントでは、openpilotの `scripts/` ディレクトリにある「PR管理」用スクリプトについて、初心者向けに技術的な背景も含めて丁寧に解説します。

---

## 背景・目的

---

## apply-pr.sh
- **用途**: 指定したGitHub PR番号のパッチ（差分）をダウンロードし、現在の作業ツリーに適用します。
- **技術解説**:
  - PRの内容はGitHub上で「.patch」形式で取得できます。
  - `curl`コマンドでパッチをダウンロードし、`git apply -3`で自動的にマージします。
- **使い方**:
  ```bash
  ```

### 【全行解説】apply-pr.sh

```bash
#!/usr/bin/env bash
# このスクリプトがbashで実行されることを指定

if [ $# -eq 0 ]; then
  echo "usage: $0 <pull-request-number>"
  exit 1
fi
# 引数がなければ使い方を表示して終了

BASE="https://github.com/commaai/openpilot/pull/"
# GitHubのPRページのベースURLを変数に格納
PR_NUM="$(echo $1 | grep -o -E '[0-9]+')"
# 引数から数字だけを抽出してPR番号とする

curl -L $BASE/$PR_NUM.patch | git apply -3
# 指定したPRのパッチをダウンロードし、git applyで3-wayマージを試みて適用
```

---

  これでPR #1234の内容が現在のブランチに適用されます。
- **注意点**:
- **根拠**: [scripts/apply-pr.sh](../../scripts/apply-pr.sh#L1-L13)

---
## checkout-pr.sh

- **用途**: 指定したGitHub PR番号のリモートブランチをローカルに作成し、そのブランチに切り替えます。
- **技術解説**:
  - `git fetch`でPRの内容を一時的なローカルブランチ（tmp-pr番号）として取得し、`git switch`でそのブランチに切り替えます。
  - 既存の同名ブランチは自動で削除されるため、何度でもやり直せます。
  ```bash

### 【全行解説】checkout-pr.sh

```bash
#!/usr/bin/env bash
set -e
# エラーが発生したら即終了

if [ $# -eq 0 ]; then
  echo "usage: $0 <pull-request-number>"
  exit 1
fi
# 引数がなければ使い方を表示して終了

BASE="https://github.com/commaai/openpilot/pull/"
# GitHubのPRページのベースURLを変数に格納
PR_NUM="$(echo $1 | grep -o -E '[0-9]+')"
# 引数から数字だけを抽出してPR番号とする
BRANCH=tmp-pr${PR_NUM}
# 一時的なブランチ名を作成

git branch -D -f $BRANCH || true
# 既存の同名ブランチがあれば削除（なければ無視）
git fetch -u -f origin pull/$PR_NUM/head:$BRANCH
# PRの内容をfetchして一時ブランチに格納
git switch $BRANCH
# そのブランチに切り替え
git reset --hard FETCH_HEAD
# 強制的にHEADをPRの内容に合わせる
```

---
  ./checkout-pr.sh 1234
  ```
  これでPR #1234の内容を反映したブランチに自動で切り替わります。
- **注意点**:
  - 作業中の変更がある場合は、事前にコミットやstashをしておきましょう。
  - チェックアウトしたブランチで十分にテストし、不要になったら削除できます。
- **根拠**: [scripts/checkout-pr.sh](../../scripts/checkout-pr.sh#L1-L15)

---


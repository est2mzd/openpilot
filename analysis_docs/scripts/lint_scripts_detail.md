# Lint/コードチェックスクリプト詳細

---



このドキュメントでは、openpilotの `scripts/` ディレクトリにある「Lint」用スクリプトについて、初心者向けに技術的な背景も含めて丁寧に解説します。


### 【全行解説】codespell.sh
```bash
#!/usr/bin/env bash
# このスクリプトがbashで実行されることを指定

codespell "$@"
# 引数で指定されたファイルやディレクトリに対してcodespellを実行
```
## 背景・目的

---

## codespell.sh

- **技術解説**:
	- `codespell`は、英単語のスペルミスを検出するオープンソースツールです。
	- スクリプトは単純に`codespell`コマンドを実行し、全ファイルをチェックします。

### 【全行解説】lint.sh
```bash
#!/usr/bin/env bash
set -e
# エラーが発生したら即終了

RED='\033[0;31m'
GREEN='\033[0;32m'
UNDERLINE='\033[4m'
BOLD='\033[1m'
NC='\033[0m'
# 色付き出力用のエスケープシーケンスを定義

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"
ROOT="$DIR/../../"
cd $ROOT
# スクリプトのルートディレクトリに移動

FAILED=0
# 失敗フラグ

IGNORED_FILES="uv\.lock|docs\/CARS.md"
IGNORED_DIRS="^third_party.*|^msgq.*|^msgq_repo.*|^opendbc.*|^opendbc_repo.*|^cereal.*|^panda.*|^rednose.*|^rednose_repo.*|^tinygrad.*|^tinygrad_repo.*|^teleoprtc.*|^teleoprtc_repo.*"
# 無視するファイル・ディレクトリのパターン

function run() {
	shopt -s extglob
	case $1 in
		$SKIP | $RUN ) return 0 ;;
	esac
	echo -en "$1"
	for ((i=0; i<$((50 - ${#1})); i++)); do
		echo -n "."
	done
	shift 1;
	CMD="$@"
	set +e
	log="$((eval "$CMD" ) 2>&1)"
	if [[ $? -eq 0 ]]; then
		echo -e "[${GREEN}\u2714${NC}]"
	else
		echo -e "[${RED}\u2717${NC}]"
		echo "$log"
		FAILED=1
	fi
	set -e
}

function run_tests() {
	ALL_FILES=$1
	PYTHON_FILES=$2
	run "ruff" ruff check $ROOT --quiet
	run "check_added_large_files" python3 -m pre_commit_hooks.check_added_large_files --enforce-all $ALL_FILES --maxkb=120
	run "check_shebang_scripts_are_executable" python3 -m pre_commit_hooks.check_shebang_scripts_are_executable $ALL_FILES
	run "check_shebang_format" $DIR/check_shebang_format.sh $ALL_FILES
	run "check_nomerge_comments" $DIR/check_nomerge_comments.sh $ALL_FILES
	if [[ -z "$FAST" ]]; then
		run "mypy" mypy $PYTHON_FILES
		run "codespell" codespell $ALL_FILES
	fi
	return $FAILED
}

function help() {
	echo "A fast linter"
	echo ""
	echo -e "${BOLD}${UNDERLINE}Usage:${NC} op lint [TESTS] [OPTIONS]"
	echo ""
	echo -e "${BOLD}${UNDERLINE}Tests:${NC}"
	echo -e "  ${BOLD}ruff${NC}"
	echo -e "  ${BOLD}mypy${NC}"
	echo -e "  ${BOLD}codespell${NC}"
	echo -e "  ${BOLD}check_added_large_files${NC}"
	echo -e "  ${BOLD}check_shebang_scripts_are_executable${NC}"
	echo ""
	echo -e "${BOLD}${UNDERLINE}Options:${NC}"
	echo -e "  ${BOLD}-f, --fast${NC}"
	echo "          Skip slow tests"
	echo -e "  ${BOLD}-s, --skip${NC}"
	echo "          Specify tests to skip separated by spaces"
	echo ""
	echo -e "${BOLD}${UNDERLINE}Examples:${NC}"
	echo "  op lint mypy ruff"
	echo "          Only run the mypy and ruff tests"
	echo ""
	echo "  op lint --skip mypy ruff"
	echo "          Skip the mypy and ruff tests"
	echo ""
	echo "  op lint"
	echo "          Run all the tests"
}

SKIP=""
RUN=""
while [[ $# -gt 0 ]]; do
	case $1 in
		-f | --fast ) shift 1; FAST="1" ;;
		-s | --skip ) shift 1; SKIP=" " ;;
		-h | --help | -help | --h ) help; exit 0 ;;
		* ) if [[ -n $SKIP ]]; then SKIP+="$1 "; else RUN+="$1 "; fi; shift 1 ;;
	esac
done

RUN=$([ -z "$RUN" ] && echo "" || echo "!($(echo $RUN | sed 's/ /|/g'))")
SKIP="@($(echo $SKIP | sed 's/ /|/g'))"

GIT_FILES="$(git ls-files | sed -E "s/$IGNORED_FILES|$IGNORED_DIRS//g")"
ALL_FILES=""
for f in $GIT_FILES; do
	if [[ -f $f ]]; then
		ALL_FILES+="$f"$'\n'
	fi
done
PYTHON_FILES=$(echo "$ALL_FILES" | grep --color=never '.py$' || true)

run_tests "$ALL_FILES" "$PYTHON_FILES"
```
	```bash
	./codespell.sh
- **根拠**: [scripts/codespell.sh](../../scripts/codespell.sh#L1-L5)


## lint.sh


### 【全行解説】check_nomerge_comments.sh
```bash
#!/usr/bin/env bash
# このスクリプトがbashで実行されることを指定

FAIL=0
# 失敗フラグ

if grep -n '\(#\|//\)\([[:space:]]*\)NOMERGE' $@; then
	echo -e "NOMERGE comments found! Remove them before merging\n"
	FAIL=1
fi
# NOMERGEコメントがあれば警告を出してFAIL=1に

exit $FAIL
# 失敗時は1で終了
```
- **技術解説**:
	- `ruff`はPythonコードのスタイルやバグを検出する高速Lintツールです。
	- これらをまとめて実行することで、1コマンドで多角的なコードチェックができます。
- **使い方**:
	./lint.sh
	```
- **出力**: 各ツールのLint結果が順に表示されます。

### 【全行解説】check_raylib_includes.sh
```bash
#!/usr/bin/env bash
# このスクリプトがbashで実行されることを指定

FAIL=0
# 失敗フラグ

if grep -n '#include "third_party/raylib/include/raylib\.h"' $@ | grep -v '^system/ui/raylib/raylib\.h'; then
	echo -e "Bad raylib include found! Use '#include \"system/ui/raylib/raylib.h\"' instead\n"
	FAIL=1
fi
# 不正なraylib.hのインクルードがあれば警告を出してFAIL=1に

exit $FAIL
# 失敗時は1で終了
```

---
- **言語**: Bash
- **機能**: 不正なraylibヘッダのincludeを検出
- **処理内容**:
	1. `#include "third_party/raylib/include/raylib.h"` を検出
	2. `system/ui/raylib/raylib.h` 以外で使われていれば警告

### 【全行解説】check_shebang_format.sh
```bash
#!/usr/bin/env bash
# このスクリプトがbashで実行されることを指定

FAIL=0
# 失敗フラグ

if grep '^#!.*python' $@ | grep -v '#!/usr/bin/env python3$'; then
	echo -e "Invalid shebang! Must use '#!/usr/bin/env python3'\n"
	FAIL=1
fi
# pythonスクリプトのシェバンが正しいかチェック

if grep '^#!.*bash' $@ | grep -v '#!/usr/bin/env bash$'; then
	echo -e "Invalid shebang! Must use '#!/usr/bin/env bash'"
	FAIL=1
fi
# bashスクリプトのシェバンが正しいかチェック

exit $FAIL
# 失敗時は1で終了
```

---

## check_shebang_format.sh

- **パス**: scripts/lint/check_shebang_format.sh
- **言語**: Bash
- **機能**: Python/Bashスクリプトのシバン行が正しいか検査
- **使い方**: ファイルリストを引数で指定
- **処理内容**:
	1. Python: `#!/usr/bin/env python3` 以外のシバンを検出
	2. Bash:   `#!/usr/bin/env bash` 以外のシバンを検出
	3. 不正な場合はエラー
- **出力**: 検出時は警告メッセージ

---


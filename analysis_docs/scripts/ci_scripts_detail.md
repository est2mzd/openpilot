
# Jenkins/CIスクリプト詳細（初心者向け解説）

このドキュメントでは、openpilotの `scripts/` ディレクトリにある「Jenkins/CI」用スクリプトについて、初心者向けに技術的な背景も含めて丁寧に解説します。

---

## 背景・目的

CI（継続的インテグレーション）は、ソフトウェア開発で自動ビルドや自動テストを行う仕組みです。openpilotではJenkinsというCIツールを使い、コードの品質を保っています。これらのスクリプトは、JenkinsやGitの自動化フックから呼び出され、ビルドやテスト、リントを自動実行します。

---


## jenkins_loop_test.sh

- **用途**: Jenkins上で複数回のビルド・テストを自動で繰り返し実行し、安定性を検証します。
- **技術解説**:
	- JenkinsのAPIを使い、テスト用の一時ブランチを作成・pushし、ビルドを複数回自動実行します。
	- 色付き出力や進捗監視、Jenkinsのcrumb/token取得など、実運用に即した工夫がされています。
	- Jenkinsの内部構成やAPI仕様に依存するため、CI管理者向けの高度なスクリプトです。
- **使い方**:
	```bash
	./jenkins_loop_test.sh
	```
- **出力**: Jenkinsビルドの進捗・結果が標準出力に表示されます。
- **根拠**: [scripts/jenkins_loop_test.sh](../../scripts/jenkins_loop_test.sh#L1-L50)

### 【全行解説】jenkins_loop_test.sh
```bash
#!/usr/bin/env bash
set -e
# エラーが発生したら即終了
YELLOW='\033[0;33m'
GREEN='\033[0;32m'
UNDERLINE='\033[4m'
BOLD='\033[1m'
NC='\033[0m'
# 色付き出力用のエスケープシーケンスを定義
BRANCH="master"
RUNS="20"
# デフォルトのブランチとループ回数を設定
COOKIE_JAR=/tmp/cookies
CRUMB=$(curl -s --cookie-jar $COOKIE_JAR 'https://jenkins.comma.life/crumbIssuer/api/xml?xpath=concat(//crumbRequestField,":",//crumb)')
# JenkinsのCSRF対策用トークンを取得
FIRST_LOOP=1
# 最初のループかどうかのフラグ
function loop() {
	JENKINS_BRANCH="__jenkins_loop_${BRANCH}_$(date +%s)"
	API_ROUTE="https://jenkins.comma.life/job/openpilot/job/$JENKINS_BRANCH"
	# 一時的なJenkins用ブランチ名とAPIエンドポイントを作成
	for run in $(seq 1 $((RUNS / 2))); do
		N=2
		if [[ $FIRST_LOOP ]]; then
			TEMP_DIR=$(mktemp -d)
			GIT_LFS_SKIP_SMUDGE=1 git clone --quiet -b $BRANCH --depth=1 --no-tags git@github.com:commaai/openpilot $TEMP_DIR
			git -C $TEMP_DIR checkout --quiet -b $JENKINS_BRANCH
			echo "TESTING: $(date)" >> $TEMP_DIR/testing_jenkins
			git -C $TEMP_DIR add testing_jenkins
			git -C $TEMP_DIR commit --quiet -m "testing"
			git -C $TEMP_DIR push --quiet -f origin $JENKINS_BRANCH
			rm -rf $TEMP_DIR
			FIRST_BUILD=1
			echo ''
			echo 'waiting on Jenkins...'
			echo ''
			sleep 90
			FIRST_LOOP=""
		fi
		FIRST_BUILD=$(curl -s $API_ROUTE/api/json | jq .nextBuildNumber)
		LAST_BUILD=$((FIRST_BUILD+N-1))
		TEST_BUILDS=( $(seq $FIRST_BUILD $LAST_BUILD) )
		# N回分のビルド番号を取得
		# Start N new builds
		for i in ${TEST_BUILDS[@]}; do
			echo "Starting build $i"
			curl -s --output /dev/null --cookie $COOKIE_JAR -H "$CRUMB" -X POST $API_ROUTE/build?delay=0sec
			sleep 5
		done
		echo ""
		# Wait for all builds to end
		while true; do
			sleep 30
			count=0
			for i in ${TEST_BUILDS[@]}; do
				RES=$(curl -s -w "\n%{http_code}" --cookie $COOKIE_JAR -H "$CRUMB" $API_ROUTE/$i/api/json)
				HTTP_CODE=$(tail -n1 <<< "$RES")
				JSON=$(sed '$ d' <<< "$RES")
				if [[ $HTTP_CODE == "200" ]]; then
					STILL_RUNNING=$(echo $JSON | jq .inProgress)
					if [[ $STILL_RUNNING == "true" ]]; then
						echo -e "Build $i: ${YELLOW}still running${NC}"
						continue
					else
						count=$((count+1))
						echo -e "Build $i: ${GREEN}done${NC}"
					fi
				else
					echo "No status for build $i"
				fi
			done
			echo "See live results: ${API_ROUTE}/buildTimeTrend"
			echo ""
			if [[ $count -ge $N ]]; then
				break
			fi
		done
	done
}
function usage() {
	echo ""
	echo "Run the Jenkins tests multiple times on a specific branch"
	echo ""
	echo -e "${BOLD}${UNDERLINE}Options:${NC}"
	echo -e "  ${BOLD}-n, --n${NC}"
	echo -e "          Specify how many runs to do (default to ${BOLD}20${NC})"
	echo -e "  ${BOLD}-b, --branch${NC}"
	echo -e "          Specify which branch to run the tests against (default to ${BOLD}master${NC})"
	echo ""
}
function _looper() {
	if [[ $# -eq 0 ]]; then
		usage
		exit 0
	fi
	# parse Options
	while [[ $# -gt 0 ]]; do
		case $1 in
			-n | --n ) shift 1; RUNS="$1"; shift 1 ;;
			-b | --b | --branch | -branch ) shift 1; BRANCH="$1"; shift 1 ;;
			* ) usage; exit 0 ;;
		esac
	done
	echo ""
	echo -e "You are about to start $RUNS Jenkins builds against the $BRANCH branch."
	echo -e "If you expect this to run overnight, ${UNDERLINE}${BOLD}unplug the cold reboot power switch${NC} from the testing closet before."
	echo ""
	read -p "Press (y/Y) to confirm: " choice
	if [[ "$choice" == "y" || "$choice" == "Y" ]]; then
		loop
	fi
}
_looper $@
```

---


## post-commit

- **用途**: Gitでコミットした直後に自動でリント（コードチェック）を実行します。
- **技術解説**:
	- Gitのpost-commitフックとして設置し、コミット時に`tools/op.sh lint --fast`を自動実行します。
	- `.git/hooks/post-commit.d/post-commit`が存在すればそれも実行し、柔軟な拡張が可能です。
	- コード品質を保つため、コミットごとに自動でチェックできる仕組みです。
- **使い方**:
	- `.git/hooks/post-commit`としてこのスクリプトを設置します。
- **出力**: リント結果が標準出力に表示されます。
- **根拠**: [scripts/post-commit](../../scripts/post-commit#L1-L15)

### 【全行解説】post-commit
```bash
#!/usr/bin/env bash
set -e
# エラーが発生したら即終了
if [[ -f .git/hooks/post-commit.d/post-commit ]]; then
	.git/hooks/post-commit.d/post-commit
fi
# post-commit.dディレクトリに追加フックがあれば実行
tools/op.sh lint --fast
# コミット後に高速リントを実行
echo ""
# 空行を出力
```

---


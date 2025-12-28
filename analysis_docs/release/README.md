# release - リリースビルドシステム

## 概要

**release** ディレクトリは、openpilot の公式リリースを作成・管理するためのスクリプト群です。

- **役割**: ソースコードから配布用リリースビルドを生成
- **主要スクリプト**: `build_release.sh`, `build_devel.sh`, `pack.py`
- **リリース種別**: 正式リリース（release3）、開発版（devel）、ナイトリービルド（__nightly）
- **配布先**: GitHub releases、openpilot.comma.ai、comma 3X デバイス

### 用語解説

#### リリースビルドとは？
**リリースビルド**は、ユーザーに配布するために最適化・整理されたバージョンです。

- **なぜ必要？**: 開発環境には不要なファイル（テスト、ドキュメント、ビルド中間ファイル）が含まれる
- **目的**: 
  - ダウンロードサイズを削減
  - 不要なファイルを除外（セキュリティ向上）
  - ビルド済みバイナリを含める（ユーザーはビルド不要）
- **例え**: 製品版ソフトウェアのパッケージング（開発ツールは含まない）

#### ブランチ戦略
openpilot は複数のブランチでバージョンを管理：

- **master**: 最新の開発コード（CI テスト済み）
- **devel-staging**: 次期リリース候補（テスト中）
- **devel**: 開発版リリース（ユーザーテスト可能）
- **release3-staging**: 正式リリース候補
- **release3**: 正式リリース（安定版）
- **__nightly**: 毎日自動ビルド

#### Orphan Branch とは？
**Orphan Branch**は、履歴を持たない独立したブランチです。

```bash
git checkout --orphan release3
```

- **通常のブランチ**: 親コミットを持つ（履歴が連なる）
- **Orphan Branch**: 親コミットなし（履歴がゼロから始まる）
- **なぜ使う？**: リリースブランチは開発履歴を含まない軽量なブランチにするため
- **効果**: `git clone` が高速（履歴をダウンロードしない）

**根拠**: `build_release.sh` の `git checkout --orphan $RELEASE_BRANCH`

---

## ディレクトリ構造

```
release/
├── README.md              # リリースチェックリスト
├── build_release.sh       # 正式リリースビルドスクリプト
├── build_devel.sh         # 開発版ビルドスクリプト
├── release_files.py       # リリースに含めるファイルのフィルタ
├── pack.py                # 単一実行ファイル化ツール
├── check-dirty.sh         # ビルド後の変更検出
├── check-submodules.sh    # サブモジュールの master 確認
└── identity.sh            # Git ユーザー情報設定
```

---

## 主要スクリプト

### 1. build_release.sh

**用途**: 正式リリース（release3）のビルド

**ファイル**: `release/build_release.sh` (103行)

**処理フロー**:

#### ステップ1: リポジトリのセットアップ

```bash
BUILD_DIR=/data/openpilot
SOURCE_DIR="$(git rev-parse --show-toplevel)"

# クリーンな環境を作成
rm -rf $BUILD_DIR
mkdir -p $BUILD_DIR
cd $BUILD_DIR

# 新しい Git リポジトリを初期化
git init
git remote add origin git@github.com:commaai/openpilot.git

# Orphan ブランチを作成（履歴なし）
git checkout --orphan $RELEASE_BRANCH
```

**Orphan Branch の効果**:
- 開発履歴を含まない軽量なブランチ
- ユーザーは `git clone` で全履歴をダウンロードする必要なし
- ストレージ・帯域の節約

**根拠**: `build_release.sh` 20-27行目

---

#### ステップ2: ファイルのコピー

```bash
# リリースファイルのみコピー
cd $SOURCE_DIR
cp -pR --parents $(./release/release_files.py) $BUILD_DIR/
```

**release_files.py の役割**:
- リリースに含めるファイルをフィルタリング
- ブラックリスト方式（除外リスト）
- ホワイトリストで例外を許可

**除外されるファイル**:
```python
blacklist = [
  ".git/",                    # Git 内部ファイル
  ".github/workflows/",       # CI/CD 設定
  "matlab.*.md",              # MATLAB ドキュメント
  ".lfsconfig",               # Git LFS 設定
  ".gitattributes",           # Git 属性
  ".gitmodules",              # サブモジュール定義
]
```

**結果**: ソースコードとリソースファイルのみコピー

**根拠**: `build_release.sh` 33-34行目、`release_files.py`

---

#### ステップ3: コミット

```bash
# panda の署名済みバイナリを削除（再ビルドするため）
rm -f panda/board/obj/panda.bin.signed
rm -f panda/board/obj/panda_h7.bin.signed

# バージョン番号を取得
VERSION=$(cat common/version.h | awk -F[\"-]  '{print $2}')

# コミット
git add -f .
git commit -a -m "openpilot v$VERSION release"
```

**バージョン番号の取得**:
```c
// common/version.h
#define COMMA_VERSION "0.9.10"
```

**根拠**: `build_release.sh` 39-43行目

---

#### ステップ4: ビルド

```bash
# Python パスを設定
export PYTHONPATH="$BUILD_DIR"

# メインビルド（最小限の依存）
scons -j$(nproc) --minimal

# Panda ファームウェアのビルド
if [ -z "$PANDA_DEBUG_BUILD" ]; then
  # リリースビルド（署名あり）
  CERT=/data/pandaextra/certs/release RELEASE=1 scons -j$(nproc) panda/
else
  # デバッグビルド（実験的機能有効）
  scons -j$(nproc) panda/
fi
```

**--minimal オプション**:
- 必要最小限の依存のみビルド
- テストコード、開発ツールなどをスキップ
- ビルド時間短縮

**Panda リリースビルド**:
- `CERT=/data/pandaextra/certs/release`: 証明書で署名
- `RELEASE=1`: リリースモードフラグ
- **効果**: デバッグ機能無効化、最適化有効

**根拠**: `build_release.sh` 46-54行目

---

#### ステップ5: サブモジュールチェック

```bash
# リリースにサブモジュールがないことを確認
if test "$(git submodule--helper list | wc -l)" -gt "0"; then
  echo "submodules found:"
  git submodule--helper list
  exit 1
fi
```

**なぜサブモジュール禁止？**:
- ユーザー環境で `git submodule update` が必要になる
- ネットワークアクセスが必要（オフライン不可）
- リリースは自己完結すべき

**根拠**: `build_release.sh` 57-62行目

---

#### ステップ6: クリーンアップ

```bash
# 中間ファイルの削除
find . -name '*.a' -delete      # スタティックライブラリ
find . -name '*.o' -delete      # オブジェクトファイル
find . -name '*.os' -delete     # SCons オブジェクト
find . -name '*.pyc' -delete    # Python バイトコード
find . -name 'moc_*' -delete    # Qt メタオブジェクト
find . -name '__pycache__' -delete

# 不要なファイル削除
rm -rf .sconsign.dblite Jenkinsfile release/
rm selfdrive/modeld/models/driving_vision.onnx     # 巨大モデル（ビルド済みを使う）
rm selfdrive/modeld/models/driving_policy.onnx

# x86、macOS 用バイナリを削除（ARM デバイス専用）
find third_party/ -name '*x86*' -exec rm -r {} +
find third_party/ -name '*Darwin*' -exec rm -r {} +
```

**削減効果**:
- ダウンロードサイズを大幅削減（数百MB）
- ストレージ節約
- デバイスでのインストール高速化

**根拠**: `build_release.sh` 65-75行目

---

#### ステップ7: third_party の復元

```bash
# third_party を元に戻す（削除しすぎた場合の保護）
git checkout third_party/
```

**理由**: クリーンアップで必要なファイルまで削除されないように

**根拠**: `build_release.sh` 78行目

---

#### ステップ8: プリビルドマーカー

```bash
# プリビルド済みフラグを作成
touch prebuilt
```

**prebuilt ファイルの役割**:
- デバイスがビルド済みリリースを検出
- ビルドプロセスをスキップ
- ユーザーは即座に openpilot を使用可能

**根拠**: `build_release.sh` 81行目

---

#### ステップ9: 最終コミット

```bash
# ビルド済みファイルをコミット
git add -f .
git commit --amend -m "openpilot v$VERSION"
```

**--amend の理由**:
- 複数コミットではなく、1つのリリースコミットにまとめる
- クリーンな履歴

**根拠**: `build_release.sh` 84-85行目

---

#### ステップ10: テスト

```bash
# リリーステストを実行
cd $BUILD_DIR
RELEASE=1 pytest -n0 -s selfdrive/test/test_onroad.py
```

**test_onroad.py**:
- 実車走行のシミュレーション
- 主要な制御ループをテスト
- リリース前の最終確認

**根拠**: `build_release.sh` 88-89行目

---

#### ステップ11: プッシュ

```bash
if [ ! -z "$RELEASE_BRANCH" ]; then
  echo "[-] pushing release T=$SECONDS"
  git push -f origin $RELEASE_BRANCH:$RELEASE_BRANCH
fi
```

**-f (force) の理由**:
- Orphan ブランチは履歴を共有しない
- 毎回完全に上書き
- リリースブランチは常に最新の単一コミット

**根拠**: `build_release.sh` 92-95行目

---

### 2. build_devel.sh

**用途**: 開発版（__nightly、devel）のビルド

**ファイル**: `release/build_devel.sh` (76行)

**主な違い（build_release.sh との比較）**:

#### 差分1: 既存ブランチを利用

```bash
# build_release.sh: 新規 Orphan ブランチ
git checkout --orphan $RELEASE_BRANCH

# build_devel.sh: 既存ブランチを更新
git fetch --depth 1 origin __nightly
git fetch --depth 1 origin devel
git checkout -f --track origin/__nightly
git reset --hard origin/devel
```

**理由**: 開発版は継続的に更新されるため、既存ブランチを上書き

---

#### 差分2: ソースコミット情報を記録

```bash
# ソースのコミットハッシュと日時を記録
GIT_HASH=$(git --git-dir=$SOURCE_DIR/.git rev-parse HEAD)
GIT_COMMIT_DATE=$(git --git-dir=$SOURCE_DIR/.git show --no-patch --format='%ct %ci' HEAD)
DATETIME=$(date '+%Y-%m-%dT%H:%M:%S')
VERSION=$(cat $SOURCE_DIR/common/version.h | awk -F\" '{print $2}')

echo -n "$GIT_HASH" > git_src_commit
echo -n "$GIT_COMMIT_DATE" > git_src_commit_date
```

**用途**:
- デバッグ時にどのソースからビルドされたか追跡
- 問題発生時に再現可能性を確保

**根拠**: `build_devel.sh` 47-53行目

---

#### 差分3: コミットメッセージに詳細情報

```bash
git commit -a -m "openpilot v$VERSION release

date: $DATETIME
master commit: $GIT_HASH
"
```

**効果**: リリースの由来が明確

**根拠**: `build_devel.sh` 56-60行目

---

#### 差分4: LFS チェック

```bash
# Git LFS ファイルが含まれていないことを確認
if [ ! -z "$(git lfs ls-files)" ]; then
  echo "LFS files detected!"
  exit 1
fi
```

**Git LFS とは？**:
- **Git Large File Storage**: 大きなバイナリファイルを外部保存
- **問題**: ユーザーが `git lfs pull` を実行する必要がある
- **リリースの方針**: すべてのファイルを含める（LFS 不使用）

**根拠**: `build_devel.sh` 66-70行目

---

#### 差分5: ファイルサイズチェック

```bash
# GitHub の 100MB 制限を超えるファイルを検出
BIG_FILES="$(find . -type f -not -path './.git/*' -size +95M)"
if [ ! -z "$BIG_FILES" ]; then
  printf '\n\n\n'
  echo "Found files exceeding GitHub's 100MB limit:"
  echo "$BIG_FILES"
  exit 1
fi
```

**GitHub の制限**:
- 100MB 以上のファイルは push できない
- リリースは 95MB を上限として余裕を持たせる

**根拠**: `build_devel.sh` 73-79行目

---

### 3. release_files.py

**用途**: リリースに含めるファイルのフィルタリング

**ファイル**: `release/release_files.py` (38行)

**処理ロジック**:

```python
# ブラックリスト（除外するファイル）
blacklist = [
  ".git/",                    # Git 管理ファイル
  ".github/workflows/",       # CI/CD 設定
  "matlab.*.md",              # MATLAB ドキュメント
  ".lfsconfig",               # Git LFS 設定
  ".gitattributes",           # Git 属性ファイル
  ".git$",                    # .git ディレクトリ
  ".gitmodules",              # サブモジュール定義
]

# ホワイトリスト（例外的に含める）
whitelist: list[str] = []

# 全ファイルをスキャン
for f in Path(ROOT).rglob("**/*"):
    if not (f.is_file() or f.is_symlink()):
        continue
    
    rf = str(f.relative_to(ROOT))
    blacklisted = any(re.search(p, rf) for p in blacklist)
    whitelisted = any(re.search(p, rf) for p in whitelist)
    
    # ブラックリストだがホワイトリストでない → 除外
    if blacklisted and not whitelisted:
        continue
    
    # それ以外 → 含める
    print(rf)
```

**使用方法**:
```bash
cp -pR --parents $(./release/release_files.py) $BUILD_DIR/
```

**効果**: Git 関連ファイル、CI 設定などを除外

**根拠**: `release_files.py`

---

### 4. pack.py

**用途**: Python モジュールを単一実行ファイルにパッケージング

**ファイル**: `release/pack.py` (52行)

**機能**:

```python
# 使用例
python release/pack.py -o spinner openpilot.system.ui.spinner
```

**処理**:

1. **モジュールのインポート**:
```python
mod = importlib.import_module(args.module)  # 'openpilot.system.ui.spinner'
```

2. **エントリーポイントの確認**:
```python
if not hasattr(mod, args.entrypoint):
    print(f'{args.module} does not have a {args.entrypoint}() function')
    sys.exit(1)
```

3. **必要なディレクトリをコピー**:
```python
DIRS = ['cereal', 'openpilot']
EXTS = ['.png', '.py', '.ttf', '.capnp']  # 含めるファイル拡張子

for directory in DIRS:
    shutil.copytree(BASEDIR + '/' + directory, tmp + '/' + directory, 
                    symlinks=False, copy_function=copy)
```

4. **ZIP アーカイブ作成**:
```python
INTERPRETER = '/usr/bin/env python3'
entry = f'{args.module}:{args.entrypoint}'
zipapp.create_archive(tmp, target=args.output, 
                      interpreter=INTERPRETER, main=entry)
```

**生成されるファイル**:
- 単一の実行ファイル
- 先頭行: `#!/usr/bin/env python3`
- 実体: ZIP アーカイブ（Python ファイル含む）
- 実行: `./spinner` で直接起動可能

**用途**:
- デバッグツールの配布
- 単機能ユーティリティの提供

**根拠**: `pack.py`

---

### 5. check-dirty.sh

**用途**: ビルド後の変更検出

**ファイル**: `release/check-dirty.sh` (10行)

```bash
if [ ! -z "$(git status --porcelain)" ]; then
  echo "Dirty working tree after build:"
  git status --porcelain
  exit 1
fi
```

**目的**: ビルドプロセスが意図しないファイル変更を起こしていないか確認

**使用タイミング**: CI/CD でのビルド検証

**根拠**: `check-dirty.sh`

---

### 6. check-submodules.sh

**用途**: サブモジュールが master ブランチを指しているか確認

**ファイル**: `release/check-submodules.sh` (18行)

```bash
while read hash submodule ref; do
  # tinygrad_repo はスキップ（開発中）
  if [ "$submodule" = "tinygrad_repo" ]; then
    echo "Skipping $submodule"
    continue
  fi
  
  # サブモジュールの master をフェッチ
  git -C $submodule fetch --depth 100 origin master
  
  # 現在のハッシュが master に含まれるか確認
  git -C $submodule branch -r --contains $hash | grep "origin/master"
  if [ "$?" -eq 0 ]; then
    echo "$submodule ok"
  else
    echo "$submodule: $hash is not on master"
    exit 1
  fi
done <<< $(git submodule status --recursive)
```

**目的**: 
- サブモジュールが開発ブランチを指していないか確認
- リリースには安定版（master）のみを含める

**根拠**: `check-submodules.sh`

---

### 7. identity.sh

**用途**: Git のコミッター情報を設定

**ファイル**: `release/identity.sh` (5行)

```bash
export GIT_COMMITTER_NAME="Vehicle Researcher"
export GIT_COMMITTER_EMAIL="user@comma.ai"
export GIT_AUTHOR_NAME="Vehicle Researcher"
export GIT_AUTHOR_EMAIL="user@comma.ai"
```

**目的**:
- リリースコミットを統一されたユーザー名で作成
- 個人情報を含めない
- 自動化されたビルドであることを明示

**根拠**: `identity.sh`

---

## リリースプロセス

### リリースチェックリスト

**ファイル**: `release/README.md`

#### フェーズ1: devel-staging → devel

```markdown
**Go to `devel-staging`**
- [ ] update `devel-staging`: `git reset --hard origin/master-ci`
- [ ] open a pull request from `devel-staging` to `devel`

**Go to `devel`**
- [ ] update RELEASES.md
- [ ] close out milestone
- [ ] post on Discord dev channel
- [ ] bump version on master: `common/version.h` and `RELEASES.md`
- [ ] merge the pull request
```

**目的**: 開発版リリースの準備

---

#### フェーズ2: テスト

```markdown
tests:
- [ ] update from previous release -> new release
- [ ] update from new release -> previous release
- [ ] fresh install with `openpilot-test.comma.ai`
- [ ] drive on fresh install
- [ ] comma body test
- [ ] no submodules or LFS
- [ ] check sentry, MTBF, etc.
```

**テスト項目**:
- **アップグレード**: 前バージョン → 新バージョン
- **ダウングレード**: 新バージョン → 前バージョン
- **新規インストール**: クリーンな状態から
- **実車テスト**: 実際の運転でテスト
- **comma body テスト**: ロボット制御のテスト
- **サブモジュール確認**: サブモジュールが含まれないこと
- **エラー監視**: Sentry でのエラー率、MTBF（平均故障間隔）

---

#### フェーズ3: release3 公開

```markdown
**Go to `release3`**
- [ ] publish the blog post
- [ ] `git reset --hard origin/release3-staging`
- [ ] tag the release
```
git tag v0.X.X <commit-hash>
git push origin v0.X.X
```
- [ ] create GitHub release
- [ ] final test install on `openpilot.comma.ai`
- [ ] update production
- [ ] Post on Discord, X, etc.
```

**公開ステップ**:
1. ブログ投稿を公開
2. release3 ブランチを release3-staging から更新
3. Git タグを作成（例: v0.9.10）
4. GitHub Release ページを作成
5. 本番環境でのテストインストール
6. 本番環境を更新
7. ソーシャルメディアで告知

**根拠**: `release/README.md`

---

## ブランチ戦略

### ブランチの種類と役割

| ブランチ | 用途 | 更新頻度 | 対象ユーザー |
|---------|------|---------|------------|
| **master** | 開発の最新コード | 毎日複数回 | 開発者 |
| **master-ci** | CI テスト済み master | 毎日 | 開発者 |
| **devel-staging** | 次期開発版候補 | 週次 | 内部テスター |
| **devel** | 開発版リリース | 月次 | アーリーアダプター |
| **release3-staging** | 次期正式版候補 | 月次 | ベータテスター |
| **release3** | 正式リリース | 2〜3ヶ月 | 一般ユーザー |
| **__nightly** | 毎日自動ビルド | 毎日 | 開発者・テスター |

---

### ブランチフロー

```
master (開発)
  ↓ CI テスト
master-ci
  ↓ 定期更新
devel-staging (テスト)
  ↓ PR・レビュー
devel (開発版リリース)
  ↓ 安定化・テスト
release3-staging (RC)
  ↓ 最終確認
release3 (正式リリース)
```

**並行ビルド**:
```
master → __nightly (毎日自動)
```

---

## バージョン管理

### バージョン番号

**フォーマット**: `0.MAJOR.MINOR`

例: `0.9.10`
- **MAJOR**: 9（大きな機能変更）
- **MINOR**: 10（小さな改善・バグフィックス）

**定義場所**: `common/version.h`

```c
#define COMMA_VERSION "0.9.10"
```

---

### RELEASES.md

**最新リリース** (2025-06-30):
```markdown
Version 0.9.10 (2025-06-30)
========================
（詳細はまだ記載なし、将来のリリース）

Version 0.9.9 (2025-05-23)
========================
* New driving model
  * New training architecture using parts from MLSIM
* Steering actuation delay is now learned online
* Ford Escape 2023-24 support
* Ford Kuga 2024 support
* Hyundai Nexo 2021 support
* Tesla Model 3 and Y support
```

**記載内容**:
- リリース日
- 主要な新機能
- 新規対応車種
- 重要なバグフィックス

**根拠**: `RELEASES.md`

---

## ビルド環境

### 必要な環境

1. **ビルドマシン**:
   - Linux（Ubuntu 推奨）
   - `/data/openpilot` へのアクセス権
   - 十分なディスクスペース（10GB以上）

2. **ツール**:
   - Git
   - SCons（ビルドシステム）
   - Python 3.11+
   - clang/clang++（コンパイラ）

3. **認証情報**:
   - GitHub SSH キー（push 用）
   - Panda 署名証明書（リリースビルド用）

---

### 環境変数

| 変数 | 用途 | 例 |
|-----|------|-----|
| `RELEASE_BRANCH` | リリース先ブランチ名 | `release3`, `release3-staging` |
| `PANDA_DEBUG_BUILD` | デバッグビルドフラグ | 設定すると実験的機能有効 |
| `CERT` | Panda 署名証明書パス | `/data/pandaextra/certs/release` |
| `TARGET_DIR` | ビルド出力先 | デフォルト: 一時ディレクトリ |
| `BRANCH` | push 先ブランチ | `__nightly`, `devel` |

---

## ビルド成果物

### 含まれるファイル

```
openpilot/
├── cereal/               # メッセージ定義（capnp）
├── common/               # 共通ユーティリティ
├── opendbc/             # 車両データベース
├── panda/               # CAN インターフェース
│   └── board/obj/       # ビルド済みファームウェア
├── selfdrive/           # メインコード
│   ├── controls/        # 制御システム
│   ├── locationd/       # 位置推定
│   ├── modeld/          # AI モデル
│   └── ui/              # ユーザーインターフェース
├── system/              # システムサービス
├── third_party/         # 外部ライブラリ（ARM用のみ）
├── prebuilt             # ビルド済みマーカー
└── common/version.h     # バージョン情報
```

---

### 除外されるファイル

- **開発ツール**: `.git/`, `.github/`, `Jenkinsfile`
- **ビルド中間ファイル**: `*.o`, `*.os`, `*.a`, `*.pyc`
- **ドキュメント**: `matlab.*.md`
- **巨大モデル**: `driving_vision.onnx`, `driving_policy.onnx`（ビルド済みを使用）
- **他プラットフォーム用**: `*x86*`, `*Darwin*`
- **Git 関連**: `.lfsconfig`, `.gitattributes`, `.gitmodules`

---

## 配布方法

### 1. GitHub Releases

**URL**: https://github.com/commaai/openpilot/releases

**内容**:
- タグ: `v0.9.10`
- リリースノート: `RELEASES.md` から抜粋
- ソースコード ZIP（自動生成）

---

### 2. openpilot.comma.ai

**URL**: https://openpilot.comma.ai

**仕組み**:
- comma 3X デバイスからアクセス
- デバイスが自動的に最新版をダウンロード
- インストール・アップデート

---

### 3. openpilot-test.comma.ai

**URL**: https://openpilot-test.comma.ai

**用途**: テスト版の配布（内部テスター向け）

---

## まとめ

### release ディレクトリの役割

1. **ビルド自動化**: スクリプトで一貫したリリースビルドを生成
2. **品質保証**: テスト、チェックで品質を確保
3. **配布準備**: 軽量化、最適化された配布パッケージ
4. **バージョン管理**: ブランチ戦略とリリースプロセスの標準化

---

### 主要なスクリプト

1. **build_release.sh**: 正式リリースビルド（Orphan ブランチ、署名あり）
2. **build_devel.sh**: 開発版ビルド（履歴保持、ソース追跡）
3. **release_files.py**: ファイルフィルタリング
4. **pack.py**: 単一実行ファイル化
5. **check-*.sh**: 検証スクリプト

---

### リリースフロー

```
開発 (master)
  ↓
テスト (devel-staging)
  ↓
開発版リリース (devel)
  ↓
安定化 (release3-staging)
  ↓
正式リリース (release3)
  ↓
ユーザーに配布
```

---

### 技術的特徴

1. **Orphan Branch**: 軽量なリリースブランチ
2. **ファイルフィルタリング**: 不要なファイルを除外
3. **プリビルド**: ユーザーはビルド不要
4. **署名**: Panda ファームウェアの安全性確保
5. **自動化**: CI/CD との統合

---

## 詳細ドキュメント

- **[ビルドスクリプト詳細](build_scripts_detail.md)**: build_release.sh と build_devel.sh の詳細解説
- **[リリースプロセス詳細](release_process_detail.md)**: チェックリスト、テスト、公開手順
- **[ブランチ戦略詳細](branch_strategy_detail.md)**: ブランチの種類、フロー、運用

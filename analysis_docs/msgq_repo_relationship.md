# msgq と msgq_repo の関係性

## 概要

openpilotには**msgq**と**msgq_repo**という2つのディレクトリが存在します。

- **msgq_repo**: Git submodule（独立したリポジトリ）
- **msgq**: msgq_repo/msgq へのシンボリックリンク

```
openpilot/
├── msgq -> msgq_repo/msgq   (シンボリックリンク)
└── msgq_repo/               (Git submodule)
    ├── .git/
    ├── README.md
    ├── SConscript
    ├── SConstruct
    └── msgq/                (実体)
        ├── __init__.py
        ├── ipc.h
        ├── ipc_pyx.pyx
        ├── impl_zmq.cc/h
        ├── impl_msgq.cc/h
        ├── impl_fake.cc/h
        └── visionipc/
```

---

## msgq_repo の正体

### Git Submodule

**msgq_repo**は、openpilotに組み込まれた**外部リポジトリ**です。

```bash
$ cat .gitmodules
[submodule "msgq"]
  path = msgq_repo
  url = https://github.com/est2mzd/msgq.git

$ git submodule status
bcfcd158d629555525ae4a5665c6a0bf8a03e5cd msgq_repo (remotes/origin/annotation)
```

**リポジトリ情報**:
- **URL**: https://github.com/est2mzd/msgq.git
- **コミット**: bcfcd158d629555525ae4a5665c6a0bf8a03e5cd

---

### 独立したライブラリ

msgq_repoは**スタンドアロンライブラリ**として開発されています。

**README.md**より:
```
# MSGQ: A lock free single producer multi consumer message queue

MSGQ is a generic high performance IPC pub sub system with a single 
publisher and multiple subscribers. MSGQ is designed to be a high 
performance replacement for ZMQ-like SUB/PUB patterns.
```

**特徴**:
- 独自のビルドシステム（SConstruct）
- テストコード（msgq_tests.cc）
- 汎用的な設計（openpilot非依存）

---

## msgq シンボリックリンク

### なぜシンボリックリンクか？

**歴史的経緯**:
1. 当初、msgqは`openpilot/msgq/`に直接存在
2. 汎用ライブラリとして分離（msgqリポジトリ作成）
3. 既存コードの互換性のため、`msgq -> msgq_repo/msgq`にリンク

```bash
$ ls -la openpilot/msgq
lrwxrwxrwx 1 takuya takuya 14 6月 27 14:51 msgq -> msgq_repo/msgq

$ readlink -f msgq
/home/takuya/work/comma/openpilot/msgq_repo/msgq
```

**メリット**:
- 既存のimport文が変更不要: `from openpilot.msgq import ...`
- ビルドシステム（SConstruct）も変更最小限

---

## ディレクトリ構造比較

### msgq_repo/（リポジトリルート）

```
msgq_repo/
├── .git/                  # Git管理
├── .github/               # GitHub Actions
├── README.md              # ライブラリ説明
├── SConstruct             # ビルド設定（スタンドアロン）
├── SConscript             # ビルドスクリプト
├── pyproject.toml         # Python設定
├── codecov.yml            # カバレッジ設定
└── msgq/                  # 実際のソースコード
    ├── __init__.py
    ├── ipc.h/cc           # IPC API
    ├── ipc_pyx.pyx        # Python binding
    ├── impl_*.cc/h        # 実装（ZMQ/msgq/fake）
    ├── msgq.cc/h          # msgqコア実装
    └── visionipc/         # VisionIPC
```

### msgq/（シンボリックリンク先）

```
msgq/  -> msgq_repo/msgq/
├── __init__.py
├── ipc.h
├── ipc_pyx.pyx
├── impl_zmq.cc/h
├── impl_msgq.cc/h
├── impl_fake.cc/h
├── msgq.cc/h
└── visionipc/
```

**実体は同じ**: `msgq/`と`msgq_repo/msgq/`は同一ディレクトリ

---

## openpilotビルドシステムとの統合

### openpilot側のSConstruct

```python
# openpilot/SConstruct
cpppath = [
    "#msgq_repo",  # msgq_repoをインクルードパスに追加
    # ...
]

# msgq_repoをビルド
SConscript(['msgq_repo/SConscript'], exports={'env': env_swaglog})
```

**動作**:
1. `msgq_repo/SConscript`を実行
2. `libmsgq.a`、`libvisionipc.a`をビルド
3. openpilot本体にリンク

---

### インクルードパスの使い分け

**C++コード内**:

```cpp
// msgq_repo/msgq/ を指定（submoduleルートから）
#include "msgq/ipc.h"
#include "msgq/visionipc/visionipc.h"
```

**Pythonコード内**:

```python
# シンボリックリンク msgq/ 経由
from openpilot.msgq import pub_sock, sub_sock
from openpilot.msgq.visionipc import VisionIpcClient
```

---

## 更新の流れ

### msgqライブラリの更新

```bash
# msgq_repo内で開発
cd openpilot/msgq_repo
git checkout -b new-feature

# コード変更
vim msgq/ipc.cc

# コミット
git add msgq/ipc.cc
git commit -m "Add new IPC feature"
git push origin new-feature

# msgqリポジトリでマージ
# （GitHubでPull Request → merge）
```

### openpilotでのmsgq更新

```bash
# openpilotリポジトリで
cd openpilot

# submodule更新（新しいコミットを指定）
cd msgq_repo
git pull origin main
cd ..

# submoduleの変更をコミット
git add msgq_repo
git commit -m "bump msgq (#34445)"
git push
```

**Git履歴**に残る:
```
* eabee8c73 bump msgq (#35345)
* 475c9ba49 bump msgq (#34445)
* 4dc95f606 bump msgq (#34410)
* 7ffad1935 bump msgq (#34278)
```

---

## 他のsubmoduleとの比較

openpilotには複数のsubmoduleが存在します:

```bash
$ cat .gitmodules
[submodule "panda"]
  path = panda
  url = https://github.com/est2mzd/panda.git

[submodule "opendbc"]
  path = opendbc_repo
  url = https://github.com/est2mzd/opendbc.git

[submodule "msgq"]
  path = msgq_repo                         # ← msgq
  url = https://github.com/est2mzd/msgq.git

[submodule "rednose_repo"]
  path = rednose_repo
  url = https://github.com/est2mzd/rednose.git

[submodule "tinygrad"]
  path = tinygrad_repo
  url = https://github.com/est2mzd/tinygrad.git
```

**パターン**:
- **panda**: `panda/`（シンボリックリンクなし）
- **opendbc**: `opendbc_repo/`、`opendbc -> opendbc_repo/opendbc`
- **msgq**: `msgq_repo/`、`msgq -> msgq_repo/msgq`
- **rednose**: `rednose_repo/`、`rednose -> rednose_repo/rednose`
- **tinygrad**: `tinygrad_repo/`、`tinygrad -> tinygrad_repo/tinygrad`

**命名規則**: `<name>_repo/`（submodule）、`<name> -> <name>_repo/<name>`（リンク）

---

## なぜ分離したのか？

### 1. 再利用性

msgqは**openpilot以外でも使える**汎用ライブラリ:

- 高性能IPC（Pub/Sub）
- ロックフリーリングバッファ
- VisionIPC（画像共有）

**他のプロジェクトでも利用可能**:
```bash
git clone https://github.com/est2mzd/msgq.git
cd msgq
scons  # スタンドアロンでビルド可能
```

---

### 2. 独立した開発サイクル

**msgqリポジトリ**:
- 独自のバージョン管理
- 独自のCI/CD（GitHub Actions）
- 独自のテスト（msgq_tests.cc）

**openpilot**:
- 必要な時にmsgqを更新（`bump msgq`）
- 特定のコミットに固定（安定性）

---

### 3. 明確な責任分離

**msgq**:
- IPC通信の実装
- パフォーマンス最適化
- 汎用性

**openpilot**:
- msgqの利用
- cerealとの統合（SubMaster/PubMaster）
- 自動運転ロジック

---

## 開発者向けガイド

### msgqソースコード編集

**方法1: シンボリックリンク経由**
```bash
# openpilot/msgq/ 経由で編集
vim openpilot/msgq/ipc.cc

# 実際は msgq_repo/msgq/ipc.cc が編集される
cd openpilot/msgq_repo
git status
# modified:   msgq/ipc.cc
```

**方法2: 直接編集**
```bash
# msgq_repo直接編集
vim openpilot/msgq_repo/msgq/ipc.cc
```

**どちらも同じファイル**（シンボリックリンク）

---

### ビルド

**openpilot全体のビルド**:
```bash
cd openpilot
scons -j8
# msgq_repo/SConscript が実行される
# libmsgq.a, libvisionipc.a が生成される
```

**msgqのみビルド**:
```bash
cd openpilot/msgq_repo
scons -j8
# スタンドアロンビルド
```

---

### テスト

**msgq単体テスト**:
```bash
cd openpilot/msgq_repo
scons -j8
./msgq/test_runner
./msgq/visionipc/test_runner
```

**openpilot統合テスト**:
```bash
cd openpilot
pytest selfdrive/test/test_*.py
```

---

## トラブルシューティング

### 問題1: シンボリックリンクが壊れている

**症状**:
```bash
$ ls -la msgq
lrwxrwxrwx 1 user user 14 msgq -> msgq_repo/msgq (broken)
```

**原因**: msgq_repo submoduleが初期化されていない

**解決策**:
```bash
git submodule update --init --recursive
```

---

### 問題2: msgq_repoの変更が反映されない

**症状**: msgq_repo内のコード変更が無視される

**原因**: openpilotが古いコミットを指している

**解決策**:
```bash
cd openpilot/msgq_repo
git pull origin main  # 最新に更新

cd ..
git add msgq_repo
git commit -m "Update msgq"
```

---

### 問題3: インクルードパスエラー

**症状**:
```
fatal error: msgq/ipc.h: No such file or directory
```

**原因**: `#msgq_repo`がインクルードパスにない

**解決策**: SConscriptの確認
```python
cpppath = [
    "#msgq_repo",  # これが必要
]
```

---

## まとめ

### 構造

```
msgq_repo/               → Git submodule（独立リポジトリ）
  └── msgq/              → 実際のソースコード
       ↑
       │ (シンボリックリンク)
       │
msgq/                    → 互換性のためのリンク
```

### 役割

| 項目 | msgq_repo | msgq |
|------|-----------|------|
| 実体 | Git submodule | シンボリックリンク |
| ビルド | 独立可能 | リンク先を参照 |
| 開発 | msgqリポジトリ | openpilot経由 |
| 更新 | `git pull` | submodule更新 |
| 目的 | 汎用ライブラリ | openpilot統合 |

### ポイント

1. **msgq = msgq_repo/msgq**（同一ディレクトリ）
2. **msgq_repo**: 独立した汎用IPCライブラリ
3. **msgq**: openpilot互換性のためのリンク
4. **更新**: msgqリポジトリ → openpilot submodule更新

---

## 関連ドキュメント

- [msgq_analysis/README.md](msgq_analysis/README.md) - msgqライブラリ全体の概要
- [msgq_analysis/ipc.md](msgq_analysis/ipc.md) - IPC API詳細
- [msgq_analysis/implementations.md](msgq_analysis/implementations.md) - ZMQ/msgq/Fake実装

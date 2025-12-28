# openpilot/ パッケージディレクトリの構造

## 概要

openpilot リポジトリには、**openpilot/** という特殊なディレクトリが存在し、その中身はすべて**シンボリックリンク**になっています。

この設計により、openpilotは**後方互換性を保ちながら**、標準的なPythonパッケージ構造へ移行しています。

---

## ディレクトリ構造

```
openpilot_202505/
├── common/          (実体ディレクトリ)
├── selfdrive/       (実体ディレクトリ)
├── system/          (実体ディレクトリ)
├── tools/           (実体ディレクトリ)
├── third_party/     (実体ディレクトリ)
└── openpilot/       (Pythonパッケージディレクトリ)
    ├── __init__.py  (空ファイル)
    ├── common -> ../common           (シンボリックリンク)
    ├── selfdrive -> ../selfdrive/    (シンボリックリンク)
    ├── system -> ../system/          (シンボリックリンク)
    ├── tools -> ../tools             (シンボリックリンク)
    └── third_party -> ../third_party (シンボリックリンク)
```

**根拠**:
```bash
$ ls -la openpilot/
total 12
drwxrwxr-x  3 takuya takuya 4096  6月 27 14:55 .
drwxrwxr-x 28 takuya takuya 4096 12月 21 17:32 ..
lrwxrwxrwx  1 takuya takuya    9  6月 27 14:51 common -> ../common
-rw-rw-r--  1 takuya takuya    0  6月 27 14:54 __init__.py
drwxr-xr-x  2 takuya takuya 4096  6月 27 14:55 __pycache__
lrwxrwxrwx  1 takuya takuya   13  6月 27 14:51 selfdrive -> ../selfdrive/
lrwxrwxrwx  1 takuya takuya   10  6月 27 14:51 system -> ../system/
lrwxrwxrwx  1 takuya takuya   14  6月 27 14:51 third_party -> ../third_party
lrwxrwxrwx  1 takuya takuya    8  6月 27 14:51 tools -> ../tools

$ readlink openpilot/common
../common
```

---

## なぜこのような構造にするのか？

### 1. Python パッケージの名前空間統一

#### 新しいimport方式（推奨）

```python
from openpilot.common.basedir import BASEDIR
from openpilot.common.params import Params
from openpilot.selfdrive.test.helpers import set_params_enabled
from openpilot.tools.lib.logreader import LogReader
from openpilot.system.version import training_version
```

**根拠**: openpilot_latest のコード

```python
# openpilot_latest/openpilot/selfdrive/test/test_onroad.py
from openpilot.common.basedir import BASEDIR
from openpilot.common.timeout import Timeout
from openpilot.common.params import Params
from openpilot.selfdrive.selfdrived.events import EVENTS, ET
from openpilot.selfdrive.test.helpers import set_params_enabled, release_only
from openpilot.system.hardware.hw import Paths
from openpilot.tools.lib.logreader import LogReader
```

#### 古いimport方式（後方互換性）

```python
from common.basedir import BASEDIR
from common.params import Params
from selfdrive.test.helpers import set_params_enabled
from tools.lib.logreader import LogReader
```

シンボリックリンクにより、**どちらのimport方式も動作します**。

---

### 2. 明示的なパッケージスコープ

#### 問題: 一般的な名前の衝突リスク

`common`, `tools`, `system` などの名前は非常に一般的で、他のPythonパッケージと衝突する可能性があります。

**例**:
```python
# どちらのcommonか不明確
from common import params

# 他のライブラリにもcommonがあるかもしれない
import some_library.common
```

#### 解決: openpilot 名前空間

```python
# openpilotのモジュールであることが明確
from openpilot.common import params

# 他のライブラリと区別できる
from openpilot.common.params import Params
from other_lib.common.config import Config  # 衝突しない
```

---

### 3. 開発の移行期間対応

シンボリックリンクを使うことで、**新旧両方のimport方式を同時にサポート**できます。

```python
# 新しいコード
from openpilot.common.params import Params

# 古いコード（まだリポジトリ内に存在）
from common.params import Params

# どちらも同じファイルを参照
```

**メリット**:
- 段階的な移行が可能
- 既存コードを一度に書き換える必要がない
- 新規コードは新しい方式で書ける

**根拠**: openpilot_latest では両方のimport形式が混在しています

---

### 4. PyPI パッケージ化の準備

将来的に `pip install openpilot` でインストールできるようにするための標準的な構造です。

#### 標準的なPythonパッケージの構造

```
package_name/
├── setup.py
├── README.md
└── package_name/        # パッケージ名と同じディレクトリ
    ├── __init__.py
    ├── module1/
    ├── module2/
    └── module3/
```

#### openpilotの構造

```
openpilot/               # リポジトリルート
├── pyproject.toml      # パッケージ設定
├── README.md
└── openpilot/          # パッケージディレクトリ
    ├── __init__.py
    ├── common/         (→ ../common/)
    ├── selfdrive/      (→ ../selfdrive/)
    └── system/         (→ ../system/)
```

**将来的な使用イメージ**:
```bash
# PyPIからインストール
pip install openpilot

# Pythonコード内で
from openpilot.car import CarInterface
from openpilot.common.params import Params
```

**根拠**: `pyproject.toml` の存在と、標準的なPythonパッケージングのベストプラクティス

---

## 技術的な仕組み

### Pythonのモジュール解決

1. **import文**:
   ```python
   from openpilot.common.params import Params
   ```

2. **Pythonがモジュールを探す**:
   ```
   /home/takuya/work/comma/openpilot_202505/openpilot/common/params.py
   ```

3. **シンボリックリンクを辿る**:
   ```
   openpilot/common → ../common
   ```

4. **実際のファイルを読み込む**:
   ```
   /home/takuya/work/comma/openpilot_202505/common/params.py
   ```

**検証**:
```bash
$ python3 -c "import openpilot.common; print(openpilot.common.__file__)"
/home/takuya/work/comma/openpilot_202505/openpilot/common/__init__.py
```

Pythonは `openpilot/common/__init__.py` を見つけますが、実際にはシンボリックリンクを通じて `../common/__init__.py` を読み込んでいます。

**根拠**: `python3 -c` コマンドの実行結果

---

## 利点と欠点

### 利点

| 利点 | 説明 |
|------|------|
| **後方互換性** | 既存のコードを壊さずに新しい構造に移行できる |
| **段階的移行** | すべてのimportを一度に変更する必要がない |
| **名前空間の明確化** | `openpilot.*` で統一された名前空間 |
| **衝突回避** | 他のパッケージとの名前衝突を防ぐ |
| **標準準拠** | Pythonパッケージングのベストプラクティスに従う |
| **実体は1箇所** | コードの重複がない（シンボリックリンクのみ） |

### 欠点・注意点

| 欠点 | 説明 |
|------|------|
| **複雑性** | ディレクトリ構造が初見では分かりにくい |
| **Windows互換性** | Windowsではシンボリックリンクのサポートが限定的 |
| **ツール依存** | 一部のツールがシンボリックリンクを正しく扱えない可能性 |
| **混在期間** | 新旧両方のimportスタイルが混在し、統一されていない |

---

## 実例: 新旧import方式の比較

### 旧方式

```python
# openpilot_202505/selfdrive/controls/controlsd.py (古いコード)
from common.params import Params
from common.realtime import DT_CTRL
from selfdrive.car.interfaces import CarInterfaceBase
from selfdrive.controls.lib.lateral_planner import LateralPlanner
```

### 新方式

```python
# openpilot_latest/openpilot/selfdrive/test/test_onroad.py (新しいコード)
from openpilot.common.basedir import BASEDIR
from openpilot.common.params import Params
from openpilot.selfdrive.test.helpers import set_params_enabled
from openpilot.tools.lib.logreader import LogReader
```

**根拠**: 実際のソースファイルの比較

---

## 移行のタイムライン（推測）

### フェーズ1: 準備（完了）
- `openpilot/` ディレクトリを作成
- シンボリックリンクを設定
- `__init__.py` を配置

### フェーズ2: 移行期（現在）
- 新規コードは `from openpilot.*` を使用
- 既存コードは段階的に更新
- 両方のimportスタイルが共存

### フェーズ3: 完了（将来）
- すべてのimportを `from openpilot.*` に統一
- PyPI公開の準備
- 古いimportスタイルを非推奨化

---

## 類似の設計パターン

### 他のプロジェクトでの例

openpilotと同様のシンボリックリンク方式は、他のプロジェクトでも使われています:

1. **ROS (Robot Operating System)**
   - パッケージ名前空間の整理に使用

2. **TensorFlow**
   - 内部モジュールの整理に使用

3. **Django**
   - 一部のバージョンでアプリケーション構造に使用

---

## まとめ

| 項目 | 内容 |
|------|------|
| **目的** | 標準的なPythonパッケージ構造への移行 |
| **手法** | シンボリックリンクによる実体の共有 |
| **メリット** | 後方互換性を保ちながら段階的移行 |
| **対象** | common, selfdrive, system, tools, third_party |
| **import方式** | 新: `from openpilot.*`、旧: `from common.*` |
| **移行状況** | 両方のスタイルが混在（移行期） |
| **将来展望** | PyPI公開、完全な名前空間統一 |

openpilot/ ディレクトリのシンボリックリンク構造は、**大規模プロジェクトをモダンなPythonパッケージ構造に段階的に移行**するための巧妙な設計です。コードの実体を移動せずに、import パスを整理できる優れた手法と言えます。

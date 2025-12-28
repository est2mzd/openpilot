# opendbc と opendbc_repo の関係性

## 概要

openpilotには**opendbc**と**opendbc_repo**という2つのディレクトリが存在します。

- **opendbc_repo**: Git submodule（独立したリポジトリ）
- **opendbc**: opendbc_repo/opendbc へのシンボリックリンク

```
openpilot/
├── opendbc -> opendbc_repo/opendbc   (シンボリックリンク)
└── opendbc_repo/                     (Git submodule)
    ├── .git/
    ├── README.md
    ├── LICENSE
    ├── SConstruct
    ├── pyproject.toml
    └── opendbc/                      (実体)
        ├── __init__.py
        ├── can/
        ├── car/
        ├── dbc/
        └── safety/
```

**根拠**:
```bash
$ ls -la openpilot_202505/opendbc
lrwxrwxrwx 1 takuya takuya 20  6月 27 14:51 opendbc -> opendbc_repo/opendbc
```

---

## opendbc_repo の正体

### Git Submodule

**opendbc_repo**は、openpilotに組み込まれた**外部リポジトリ**です。

```bash
$ cat .gitmodules
[submodule "opendbc"]
  path = opendbc_repo
  url = https://github.com/est2mzd/opendbc.git

$ git submodule status | grep opendbc
 e6096794026bbd32d7f932f6e73e8537812032f7 opendbc_repo (v0.2.1-492-ge609679)
```

**リポジトリ情報**:
- **URL**: https://github.com/est2mzd/opendbc.git
- **コミット**: e6096794026bbd32d7f932f6e73e8537812032f7
- **バージョンタグ**: v0.2.1-492-ge609679

**根拠**: `.gitmodules` ファイルと `git submodule status` コマンドの出力

---

### 独立したライブラリ

opendbc_repoは**スタンドアロンライブラリ**として開発されています。

**README.md**より:
```markdown
# opendbc

opendbc is a Python API for your car.
Control the gas, brake, steering, and more. Read the speed, steering angle, and more.

Most cars since 2016 have electronically-actuatable steering, gas, and brakes 
thanks to LKAS and ACC. The goal of this project is to support controlling the 
steering, gas, and brakes on every single one of those cars.
```

**特徴**:
- 独自のビルドシステム（SConstruct）
- 独自のパッケージ設定（pyproject.toml）
- テストスクリプト（test.sh）
- 汎用的な設計（openpilot非依存）

**根拠**: `opendbc_repo/README.md`, `opendbc_repo/SConstruct`, `opendbc_repo/pyproject.toml` の存在

---

## opendbc シンボリックリンク

### なぜシンボリックリンクか？

**msgq と同様の構造**:
1. 当初、opendbcは`openpilot/opendbc/`に直接存在（推測）
2. 汎用ライブラリとして分離（opendbcリポジトリ作成）
3. 既存コードの互換性のため、`opendbc -> opendbc_repo/opendbc`にリンク

```bash
$ readlink -f openpilot_202505/opendbc
/home/takuya/work/comma/openpilot_202505/opendbc_repo/opendbc
```

**メリット**:
- 既存のimport文が変更不要: `from opendbc.can import ...`
- ビルドシステム（SConstruct）も変更最小限
- openpilotの他の部分からは透過的にアクセス可能

**根拠**: `ls -la` コマンドと `readlink` コマンドの出力

---

## ディレクトリ構造比較

### opendbc_repo/（リポジトリルート）

```
opendbc_repo/
├── .git/                    # Gitリポジトリ管理
├── .github/                 # GitHub Actions CI設定
├── .gitignore
├── LICENSE                  # MITライセンス
├── README.md                # プロジェクト説明
├── RELEASES.md              # リリースノート
├── SConstruct               # ビルド設定
├── SConscript
├── pyproject.toml           # Pythonパッケージ設定
├── setup.sh                 # セットアップスクリプト
├── test.sh                  # テストスクリプト
├── conftest.py              # pytestの設定
├── lefthook.yml             # Gitフック設定
├── docs/                    # ドキュメント
├── examples/                # サンプルコード
├── site_scons/              # SCons拡張
└── opendbc/                 # 実際のライブラリコード
    ├── __init__.py
    ├── can/                 # CANパーサー/パッカー
    ├── car/                 # 車両インターフェース
    ├── dbc/                 # DBCファイル集
    └── safety/              # セーフティファームウェア
```

**根拠**: `ls -la opendbc_repo/` の出力

### opendbc/（シンボリックリンク先）

```
opendbc/
├── __init__.py              # DBC_PATH, INCLUDE_PATH定義
├── can/                     # CANメッセージングライブラリ
│   ├── __init__.py
│   ├── parser.py           # CANParser (wrapper)
│   ├── parser_pyx.pyx      # CANParser (Cython実装)
│   ├── packer.py           # CANPacker (wrapper)
│   ├── packer_pyx.pyx      # CANPacker (Cython実装)
│   ├── common.pxd          # C++型定義
│   ├── common.cc/h         # C++実装
│   └── dbc.cc              # DBCパーサー実装
├── car/                     # 車両固有実装（19ブランド）
│   ├── __init__.py
│   ├── interfaces.py       # 基底クラス
│   ├── body/               # comma body
│   ├── chrysler/           # Chrysler
│   ├── ford/               # Ford
│   ├── gm/                 # GM
│   ├── honda/              # Honda
│   ├── hyundai/            # Hyundai/Kia
│   ├── mazda/              # Mazda
│   ├── nissan/             # Nissan
│   ├── rivian/             # Rivian
│   ├── subaru/             # Subaru
│   ├── tesla/              # Tesla
│   ├── toyota/             # Toyota/Lexus
│   ├── volkswagen/         # VW/Audi
│   └── ...
├── dbc/                     # DBCファイル（96個）
│   ├── README.md
│   ├── toyota_*.dbc        # Toyota車両のDBC
│   ├── honda_*.dbc         # Honda車両のDBC
│   ├── hyundai_*.dbc       # Hyundai車両のDBC
│   └── ...
└── safety/                  # Panda用セーフティコード
    ├── safety_declarations.h  # セーフティモード定義
    ├── main.c
    ├── helpers.h
    ├── lateral.h
    ├── longitudinal.h
    ├── modes/              # 各ブランドのセーフティ実装
    └── tests/              # セーフティユニットテスト
```

**根拠**: 
- `ls opendbc_repo/opendbc/` の出力
- `ls opendbc_repo/opendbc/car/*/` で19個のディレクトリを確認
- `ls opendbc_repo/opendbc/dbc/*.dbc | wc -l` で96個のDBCファイルを確認

---

## opendbc の役割

### 1. Python API for Cars

openpilotが多様な車種をサポートするための**統一的なインターフェース**を提供します。

**主要機能**:
- CANメッセージの解析（Parser）
- CANメッセージの生成（Packer）
- 車両状態の取得（CarState）
- 車両制御コマンドの送信（CarController）
- セーフティチェック（Safety）

**根拠**: `README.md` の説明

### 2. マルチブランド対応

**サポートブランド数**: 19ブランド

```bash
$ ls -d opendbc/car/*/
opendbc/car/body/      opendbc/car/mazda/      opendbc/car/toyota/
opendbc/car/chrysler/  opendbc/car/nissan/     opendbc/car/volkswagen/
opendbc/car/ford/      opendbc/car/rivian/
opendbc/car/gm/        opendbc/car/subaru/
opendbc/car/honda/     opendbc/car/tesla/
opendbc/car/hyundai/   ...
```

**根拠**: `ls -d opendbc/car/*/` の出力（19個のディレクトリ）

各ブランドは以下のファイル構成を持ちます:
- `interface.py`: 車両固有のパラメータと設定
- `carstate.py`: CAN信号から車両状態を解析
- `carcontroller.py`: 制御コマンドをCANメッセージに変換
- `values.py`: サポート車種の定義
- `fingerprints.py`: 車種識別用のECUファームウェア情報
- `<brand>can.py`: DBC定義をラップするヘルパー
- `radar_interface.py`: レーダーデータの解析（該当車種のみ）

**根拠**: `opendbc_repo/opendbc/car/toyota/` 内のファイル確認

---

## コード規模

### 全体

```bash
# CAN解析/生成ライブラリ
opendbc/can/*.{py,cc,h,pyx}    : 約2,078行

# セーフティコード
opendbc/safety/*.{c,h}         : 36ファイル

# DBCファイル
opendbc/dbc/*.dbc              : 96ファイル、約79,795行
```

**根拠**:
- `wc -l opendbc/can/*.{py,cc,h,pyx}` の出力: 2078 total
- `find opendbc/safety -name "*.c" -o -name "*.h" | wc -l` の出力: 36
- `ls opendbc/dbc/*.dbc | wc -l` の出力: 96
- `wc -l opendbc/dbc/*.dbc | tail -1` の出力: 79795 total

### 車両固有コード（例: Toyota）

```
toyota/
├── interface.py       # 162行 - 車種別パラメータ設定
├── carstate.py        # 271行 - CAN信号解析
├── carcontroller.py   # 制御コマンド生成
├── values.py          # サポート車種定義
├── fingerprints.py    # ECUファームウェアDB
└── toyotacan.py       # DBC定義ヘルパー
```

**根拠**: `wc -l` コマンドでの各ファイル行数確認

---

## 技術スタック

### 実装言語

| 言語 | 用途 |
|------|------|
| **Python** | 高レベルAPI、車両インターフェース |
| **Cython** | Python-C++ブリッジ（parser_pyx.pyx, packer_pyx.pyx） |
| **C++** | CANパーサー/パッカーの実装（common.cc, dbc.cc） |
| **C** | Panda用セーフティコード（safety/*.c） |

**根拠**: 
- `parser_pyx.pyx` の先頭: `# distutils: language = c++`
- `common.pxd` の定義: `cdef extern from "common_dbc.h"`
- `safety/main.c` の存在

### ビルドシステム

- **SCons**: ネイティブコードのビルド
- **Cython**: .pyx → .so の変換
- **pytest**: テスト実行
- **lefthook**: Git pre-commit フック

**根拠**: `SConstruct`, `pyproject.toml`, `lefthook.yml` の存在

---

## openpilot との統合

### import パス

openpilot内のコードは、シンボリックリンクを通じて透過的にopendbcにアクセスします:

```python
# どちらも同じファイルを参照
from opendbc.can.parser import CANParser
from openpilot.opendbc.can.parser import CANParser  # 完全修飾パス
```

### 実際の使用例

```python
# openpilot_202505/opendbc_repo/opendbc/can/parser.py
from opendbc.can.parser_pyx import CANParser, CANDefine
```

```python
# openpilot_202505/opendbc_repo/opendbc/car/toyota/interface.py
from opendbc.car import Bus, structs, get_safety_config, uds
from opendbc.car.toyota.carstate import CarState
from opendbc.car.toyota.carcontroller import CarController
```

**根拠**: 実際のソースコードの import 文

---

## セーフティモデル

### 定義されているセーフティモード

opendbc/safety には、各ブランド専用のセーフティチェックコードが含まれています。

```c
// opendbc/safety/safety_declarations.h
#define SAFETY_SILENT 0U
#define SAFETY_HONDA_NIDEC 1U
#define SAFETY_TOYOTA 2U
#define SAFETY_GM 4U
#define SAFETY_FORD 6U
#define SAFETY_HYUNDAI 8U
#define SAFETY_CHRYSLER 9U
#define SAFETY_TESLA 10U
#define SAFETY_SUBARU 11U
#define SAFETY_MAZDA 13U
#define SAFETY_NISSAN 14U
#define SAFETY_VOLKSWAGEN_MQB 15U
#define SAFETY_VOLKSWAGEN_PQ 21U
#define SAFETY_HYUNDAI_CANFD 28U
#define SAFETY_RIVIAN 33U
#define SAFETY_VOLKSWAGEN_MEB 34U
// ... 合計約30種類
```

**根拠**: `opendbc/safety/safety_declarations.h` の内容（1-100行目）

### 役割

Panda（CANインターフェースデバイス）上で動作し、以下をチェックします:
- 送信メッセージの妥当性検証
- ステアリングトルク/角度の制限
- 縦方向制御（アクセル/ブレーキ）の制限
- ドライバーオーバーライドの監視

**根拠**: `README.md` の "Safety Model" セクション

---

## DBC ファイル

### 概要

**DBC (Database CAN)**: CANメッセージの定義ファイル

```
96個のDBCファイル、約79,795行
```

**根拠**: 
- ファイル数: `ls opendbc/dbc/*.dbc | wc -l` → 96
- 総行数: `wc -l opendbc/dbc/*.dbc | tail -1` → 79795 total

### 内容

DBCファイルは以下を定義します:
- メッセージID（アドレス）
- 信号名
- 信号のビット位置、長さ
- 物理単位への変換係数
- コメント

**例**: `toyota_adas.dbc`, `honda_civic_touring_2016_can_generated.dbc`

**根拠**: `opendbc/dbc/README.md` の説明

### プリプロセッサ

ブランド共通のDBCと車種固有のDBCを組み合わせて、最終的なDBCファイルを生成する仕組みがあります。

```
opendbc/dbc/
├── generator/           # DBCジェネレータスクリプト
└── *_generated.dbc      # 生成されたDBCファイル
```

**根拠**: `opendbc/dbc/README.md` の "DBC file preprocessor" セクション

---

## まとめ

| 項目 | 内容 |
|------|------|
| **リポジトリ** | https://github.com/est2mzd/opendbc.git |
| **コミット** | e6096794026bbd32d7f932f6e73e8537812032f7 |
| **バージョン** | v0.2.1-492-ge609679 |
| **統合方式** | Git submodule + シンボリックリンク |
| **主要機能** | CANパーサー/パッカー、車両インターフェース、セーフティコード、DBCファイル |
| **サポートブランド** | 19ブランド |
| **DBCファイル数** | 96ファイル（約79,795行） |
| **実装言語** | Python, Cython, C++, C |
| **ビルドシステム** | SCons |
| **ライセンス** | MIT |

opendbcは、openpilotが多様な車種をサポートするための**基盤ライブラリ**として機能しており、msgqと同様に独立したリポジトリとして管理されつつ、シンボリックリンクを通じてopenpilot本体から透過的にアクセスできる構造になっています。

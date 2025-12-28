# opendbc 分析ドキュメント

## 概要

この`opendbc_analysis`ディレクトリには、**opendbc**（openpilotの車両インターフェースライブラリ）の詳細な分析ドキュメントが含まれています。

分析は`msgq_analysis`と同様の形式で、**推測を避け、根拠を明示**して作成されています。

---

## ドキュメント一覧

### 1. [can.md](can.md) - CANメッセージングライブラリ

**内容**:
- CANParser: CANメッセージの解析
- CANPacker: CANメッセージの生成
- CANDefine: DBC定義辞書
- Cython + C++ によるネイティブ実装
- ブランド別チェックサム計算

**コード規模**: 約2,078行（Python/Cython/C++）

### 2. [car.md](car.md) - 車両インターフェースライブラリ

**内容**:
- サポートブランド: 19ブランド
- 車種定義（values.py）
- 車両状態解析（carstate.py）
- 制御コマンド送信（carcontroller.py）
- 車種識別（fingerprinting）
- セーフティ制限

**サポート**: Toyota, Honda, Hyundai, GM, Ford, VW, Subaru, Mazda, Nissan, Tesla, 他

### 3. [safety.md](safety.md) - セーフティファームウェア

**内容**:
- Panda用セーフティコード（C言語）
- 約30種類のセーフティモード
- トルク/角度/加速度制限
- レート制限
- ドライバー介入検出
- MISRA C:2012 準拠
- 100%テストカバレッジ

**ファイル数**: 36ファイル（C/H）

### 4. [dbc.md](dbc.md) - DBCファイルデータベース

**内容**:
- 96個のDBCファイル
- 約79,795行の定義
- CANメッセージとシグナルの定義
- 物理値変換式
- DBCジェネレーター
- ベストプラクティス

**対応**: 主要な自動車メーカー全般

---

## opendbc の全体像

```
opendbc_repo/
├── opendbc/                      # ライブラリ本体（シンボリックリンク先）
│   ├── can/                      # CANメッセージング [2,078行]
│   │   ├── parser.py/pyx        # CANメッセージ解析
│   │   ├── packer.py/pyx        # CANメッセージ生成
│   │   ├── common.cc/h          # C++ 実装
│   │   └── dbc.cc               # DBCパーサー
│   │
│   ├── car/                      # 車両インターフェース [19ブランド]
│   │   ├── toyota/              # Toyota/Lexus
│   │   ├── honda/               # Honda/Acura
│   │   ├── hyundai/             # Hyundai/Kia
│   │   ├── gm/                  # GM
│   │   ├── ford/                # Ford
│   │   ├── volkswagen/          # VW/Audi
│   │   └── ...                  # 他13ブランド
│   │
│   ├── dbc/                      # DBCファイル [96ファイル, 79,795行]
│   │   ├── toyota_*.dbc
│   │   ├── honda_*.dbc
│   │   ├── hyundai_*.dbc
│   │   └── ...
│   │
│   └── safety/                   # セーフティコード [36ファイル]
│       ├── safety_declarations.h
│       ├── lateral.h
│       ├── longitudinal.h
│       ├── modes/               # 各ブランドのセーフティ [20ファイル]
│       └── tests/               # ユニットテスト
│
├── README.md                     # プロジェクト説明
├── SConstruct                    # ビルド設定
├── pyproject.toml                # Pythonパッケージ設定
├── test.sh                       # テストスクリプト
└── examples/                     # サンプルコード
```

---

## 主要な技術情報

### アーキテクチャ

```
┌─────────────────────────────────────────────────────┐
│                 openpilot                           │
│  ┌───────────────────────────────────────────┐     │
│  │         selfdrive/controls/               │     │
│  │  - lateral control                        │     │
│  │  - longitudinal control                   │     │
│  └───────────────┬───────────────────────────┘     │
│                  │                                  │
│                  ▼                                  │
│  ┌───────────────────────────────────────────┐     │
│  │         opendbc/car/                      │     │
│  │  - CarInterface (車両固有実装)            │     │
│  │  - CarState (状態解析)                    │     │
│  │  - CarController (制御送信)               │     │
│  └───────────────┬───────────────────────────┘     │
│                  │                                  │
│                  ▼                                  │
│  ┌───────────────────────────────────────────┐     │
│  │         opendbc/can/                      │     │
│  │  - CANParser (解析)                       │     │
│  │  - CANPacker (生成)                       │     │
│  └───────────────┬───────────────────────────┘     │
│                  │                                  │
└──────────────────┼──────────────────────────────────┘
                   │
                   ▼
         ┌─────────────────┐
         │      Panda      │ ← opendbc/safety/ (C)
         │  (セーフティ)   │
         └────────┬────────┘
                  │
                  ▼
            ┌─────────┐
            │   Car   │
            │  (CAN)  │
            └─────────┘
```

### 実装言語

| 言語 | 用途 | 場所 |
|------|------|------|
| **Python** | 高レベルAPI | opendbc/can/parser.py, opendbc/car/ |
| **Cython** | Python-C++ブリッジ | opendbc/can/*_pyx.pyx |
| **C++** | 高速処理 | opendbc/can/common.cc, dbc.cc |
| **C** | Pandaセーフティ | opendbc/safety/ |

### ビルドシステム

- **SCons**: ネイティブコードのビルド
- **Cython**: .pyx → .so の変換
- **pytest**: テスト実行

---

## 参照関係図

```
openpilot/
├── selfdrive/
│   ├── controls/
│   │   ├── controlsd.py     → uses opendbc.car.CarInterface
│   │   └── laterald.py      → uses opendbc.car.CarState
│   └── locationd/
│       └── paramsd.py       → uses opendbc.car values
│
└── opendbc -> opendbc_repo/opendbc/  (シンボリックリンク)
    ├── can/
    │   ├── parser.py        ← imported by CarState
    │   └── packer.py        ← imported by CarController
    ├── car/
    │   ├── toyota/
    │   │   ├── interface.py ← CarInterface実装
    │   │   ├── carstate.py  ← CANメッセージ解析
    │   │   └── values.py    ← 車種定義
    │   └── ...
    ├── dbc/
    │   └── toyota_*.dbc     ← CANParser/Packerで使用
    └── safety/
        └── modes/toyota.h   → Pandaにコンパイル
```

---

## 関連ドキュメント

### 上位ドキュメント

- [opendbc_repo_relationship.md](../opendbc_repo_relationship.md): opendbc と opendbc_repo の関係性

### 並列ドキュメント

- [msgq_analysis/](../msgq_analysis/): msgq ライブラリの分析

### 参考資料

- [opendbc GitHub](https://github.com/commaai/opendbc): 公式リポジトリ
- [opendbc docs](https://docs.comma.ai): 公式ドキュメント

---

## 分析方法

すべてのドキュメントは以下の方法で作成されました:

### 1. ソースコード確認

```bash
# ファイル構造
ls -la opendbc_repo/opendbc/

# 行数カウント
wc -l opendbc/can/*.py
wc -l opendbc/dbc/*.dbc

# ファイル検索
find opendbc/safety -name "*.h"
```

### 2. ファイル内容読み取り

```bash
# ファイル内容確認
cat opendbc/can/parser.py
head -100 opendbc/car/toyota/values.py
```

### 3. Git情報確認

```bash
# Submodule情報
cat .gitmodules
git submodule status

# シンボリックリンク確認
ls -la opendbc
readlink -f opendbc
```

### 4. README.md参照

```bash
# 公式ドキュメント
cat opendbc_repo/README.md
cat opendbc_repo/opendbc/dbc/README.md
```

---

## 作成日時

- **作成日**: 2025年12月22日
- **対象バージョン**: opendbc_repo コミット e6096794026bbd32d7f932f6e73e8537812032f7
- **openpilotバージョン**: 202505

---

## 注意事項

- すべての情報は**実際のソースコード**と**コマンド出力**に基づいています
- 推測や仮定は行わず、**根拠を明示**しています
- ファイルパスは `/home/takuya/work/comma/openpilot_202505/` からの相対パスです

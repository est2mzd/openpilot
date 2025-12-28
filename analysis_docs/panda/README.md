# panda - CANインターフェースデバイス

## 概要

**panda**は、comma.ai が開発した**CANバス通信デバイス**であり、openpilot と車両の間でCANメッセージを送受信するハードウェアです。

- **Git Submodule**: 独立したリポジトリとして管理
- **マイコン**: STM32F413 および STM32H725
- **主要機能**: CAN/CAN FD 通信、セーフティチェック、USB/SPI接続
- **実装言語**: C（ファームウェア）、Python（ホストライブラリ）

### 用語解説

#### CANバスとは？
**CAN（Controller Area Network）**は、車両内の電子制御ユニット（ECU）間で通信するための規格です。

- **なぜ必要？**: 現代の車両には数十個のECU（エンジン、ブレーキ、ステアリングなど）があり、それらが互いに情報を交換する必要があるため
- **panda の役割**: 車両のCANバスに接続し、openpilot が車両の状態を読み取ったり、制御コマンドを送信したりできるようにする

#### ファームウェアとは？
**ファームウェア**は、panda デバイス内のマイコン（小型コンピュータ）上で動作するプログラムです。

- **なぜ必要？**: ハードウェア（マイコン）単体では何もできないため、動作させるためのソフトウェアが必要
- **panda の場合**: STM32マイコン上で動作し、CAN通信の処理やセーフティチェックを高速・リアルタイムに実行
- **例え**: スマートフォンでいうOS（Android/iOS）のようなもの。ハードウェアを制御する基本ソフト

#### セーフティチェックとは？
**セーフティチェック**は、openpilot からの制御コマンドが安全かどうかを検証する機能です。

- **なぜ必要？**: openpilot のソフトウェアにバグがあったり、ハッキングされたりした場合でも、車両を危険な状態にしないため
- **具体例**:
  - ステアリングトルクが急激すぎる → ブロック（送信しない）
  - ドライバーがブレーキを踏んでいる → openpilot の制御を無効化
  - 異常な加速コマンド → ブロック
- **実装**: panda のファームウェアが、すべての制御コマンドをチェックしてから車両に送信
- **重要性**: **物理的な安全装置**として機能。ソフトウェアの最後の砦

**根拠**:
```bash
$ cat .gitmodules | grep -A 2 panda
[submodule "panda"]
  path = panda
  url = https://github.com/est2mzd/panda.git

$ git submodule status | grep panda
 34c974c12ab58197a78ed6e3ddcd3889a85b965e panda (remotes/origin/annotation)

$ ls -la panda
drwxrwxr-x 13 takuya takuya 4096  6月 27 15:33 .
(通常のディレクトリ、シンボリックリンクではない)
```

---

## panda とは

### README.md より

> panda speaks CAN and CAN FD, and it runs on STM32F413 and STM32H725.

**panda の役割**:
- **CANブリッジ**: 車両のCANバスとPC/openpilotの間でメッセージを中継
  - 車両のセンサー情報（速度、ステアリング角度など）を openpilot に送信
  - openpilot の制御コマンド（ステアリング、加減速）を車両に送信
- **セーフティゲートウェイ**: opendbc/safety のコードを実行し、危険な制御コマンドをブロック
  - **なぜ必要？**: openpilot が誤動作しても、panda が物理的に危険なコマンドを遮断
  - **動作例**: 急激なステアリング変更を検出 → 送信を拒否 → 車両は安全を保つ
- **高信頼性**: MISRA C準拠、包括的なテスト
  - **MISRA C**: 自動車業界の安全基準に準拠したコーディング規格
  - **目的**: バグやメモリエラーを徹底的に排除し、信頼性を確保

**根拠**: `panda/README.md` の内容

---

## ディレクトリ構造

```
panda/
├── .git                    # Git サブモジュール管理
├── README.md               # プロジェクト説明
├── LICENSE                 # MIT ライセンス
├── pyproject.toml          # Python パッケージ設定
├── SConstruct              # ビルド設定（SCons）
├── setup.sh                # セットアップスクリプト
├── test.sh                 # テストスクリプト
├── board/                  # ファームウェア（STM32）
│   ├── main.c              # メインプログラム
│   ├── boards/             # ボードバリアント定義
│   │   ├── uno.h           # panda uno
│   │   ├── dos.h           # panda dos
│   │   ├── tres.h          # panda tres
│   │   ├── cuatro.h        # panda cuatro
│   │   ├── white.h         # white panda
│   │   ├── grey.h          # grey panda
│   │   ├── black.h         # black panda
│   │   └── red.h           # red panda
│   ├── drivers/            # ハードウェアドライバ
│   ├── stm32f4/            # STM32F4 固有コード
│   └── stm32h7/            # STM32H7 固有コード
├── python/                 # Python ライブラリ
│   ├── __init__.py         # Panda クラス（876行）
│   ├── base.py             # 基底クラス
│   ├── usb.py              # USB通信
│   ├── spi.py              # SPI通信
│   ├── constants.py        # 定数定義
│   └── utils.py            # ユーティリティ
├── drivers/                # OS用ドライバ
│   └── spi/                # SPIドライバ
├── tests/                  # テスト
├── examples/               # サンプルコード
├── scripts/                # 開発ツール
├── docs/                   # ドキュメント
├── certs/                  # 証明書
└── crypto/                 # 暗号化関連
```

**根拠**: `ls -la panda/` と `ls -la panda/board/` の出力

---

## コード規模

### ファームウェア（C/H）

```bash
$ find . -name "*.c" -o -name "*.h" | wc -l
136
```

**主要ファイル**:
- `board/main.c`: メインループ（panda 起動時から常に動作する中核プログラム）
- `board/can.h`: CAN通信処理（車両とのメッセージ送受信）
- `board/health.h`: デバイス状態管理（電圧、温度、エラー監視）
- `board/faults.h`: 異常検出（ハードウェア故障の検出）

**なぜC言語？**:
- **リアルタイム性**: CANメッセージは数ミリ秒単位で処理する必要があるため、高速なC言語を使用
- **メモリ効率**: マイコンは限られたメモリ（数百KB）しかないため、効率的な言語が必要

**根拠**: `find` コマンドの出力

### Python ライブラリ

```bash
$ wc -l python/*.py | tail -1
 1824 total
```

**主要ファイル**:
- `python/__init__.py`: 876行 - Panda クラス（PC側から panda を制御するAPI）
- `python/usb.py`: USB通信実装（PCと panda の接続）
- `python/spi.py`: SPI通信実装（comma 3X 内部の高速接続）
- `python/base.py`: 基底クラス（共通機能）

**なぜPython？**:
- **開発効率**: openpilot のメインプログラムがPythonなので、同じ言語で統合しやすい
- **使いやすさ**: `panda = Panda()` のようにシンプルに使える

**根拠**: `wc -l python/*.py` の出力

---

## panda のバリアント

### ボード種類

**ファイル**: `board/boards/`

```bash
$ ls board/boards/
black.h   cuatro.h  grey.h  red.h  uno.h
dos.h     tres.h    white.h
board_declarations.h
unused_funcs.h
```

**根拠**: `ls board/boards/` の出力

| バリアント | 説明 |
|-----------|------|
| **uno** | panda uno（最新世代） |
| **dos** | panda dos |
| **tres** | panda tres |
| **cuatro** | panda cuatro |
| **white** | white panda（旧世代） |
| **grey** | grey panda（開発用） |
| **black** | black panda（旧世代） |
| **red** | red panda（旧世代） |

### マイコン

| マイコン | 使用ボード | 特徴 |
|----------|------------|------|
| **STM32F413** | white, grey, black, red, uno, dos | ARM Cortex-M4, CAN 2.0 |
| **STM32H725** | tres, cuatro | ARM Cortex-M7, CAN FD対応 |

**マイコンとは？**:
- **定義**: 小型の組み込みコンピュータ。CPU、メモリ、入出力が1つのチップに統合されている
- **panda での役割**: ファームウェアを実行し、CAN通信、USB通信、セーフティチェックを処理
- **例え**: スマートフォンのプロセッサのようなもの（ただし、より小型で省電力）

**CAN FD とは？**:
- **CAN 2.0**: 従来規格（最大1Mbps、8バイト/メッセージ）
- **CAN FD**: 新規格（最大8Mbps、64バイト/メッセージ）- より高速・大容量
- **なぜ両方必要？**: 古い車はCAN 2.0、新しい車はCAN FDを使用するため

**根拠**: 
- README.md: "runs on STM32F413 and STM32H725"
- `board/stm32f4/` と `board/stm32h7/` ディレクトリの存在

---

## panda のハードウェア世代

### 世代別特徴

| 世代 | ボード | マイコン | CAN | 特徴 |
|------|--------|----------|-----|------|
| **Gen 1** | white panda | STM32F413 | CAN 2.0 | 初期モデル |
| **Gen 2** | grey panda | STM32F413 | CAN 2.0 | 開発用 |
| **Gen 3** | black panda | STM32F413 | CAN 2.0 | 量産モデル |
| **Gen 4** | uno, dos | STM32F413 | CAN 2.0 | 現行モデル |
| **Gen 5** | tres, cuatro | STM32H725 | CAN FD | 最新（CAN FD対応） |

**根拠**: `board/boards/*.h` ファイルの存在とマイコン型番

---

## パッケージ構成

### pyproject.toml

```toml
[project]
name = "pandacan"
version = "0.0.10"
description = "Code powering the comma.ai panda"
requires-python = ">=3.11,<3.13"
license = {text = "MIT"}

dependencies = [
  "libusb1",
  "opendbc @ git+https://github.com/commaai/opendbc.git@...",
]
```

**根拠**: `pyproject.toml` の内容

**特徴**:
- PyPI パッケージ名: `pandacan`
- opendbc への依存（セーフティコード）
- Python 3.11-3.12 対応

---

## 依存関係

### panda が依存するもの

```
panda
└── opendbc/safety/  (セーフティファームウェア)
    ├── safety_declarations.h
    ├── lateral.h           (ステアリング制御の安全ルール)
    ├── longitudinal.h      (加減速制御の安全ルール)
    └── modes/*.h           (車種ごとの安全モード)
```

**なぜ opendbc/safety が必要？**:
- **車種ごとの違い**: Honda、Toyota、GMなど、メーカーごとに安全な制御の基準が異なる
- **専門知識の共有**: opendbc プロジェクトが各車種の詳細を管理し、panda がそれを利用
- **更新の容易性**: 新しい車種への対応や安全ルールの改善を、opendbc 側で一元管理

**根拠**: 
- `board/main.c`: `#include "opendbc/safety/safety.h"`
- `pyproject.toml`: `opendbc @ git+https://...`

### openpilot が panda に依存

```
openpilot
├── selfdrive/pandad/pandad.py      → panda からCANデータを受信
├── opendbc/car/panda_runner.py     → 車両との通信を抽象化
├── tools/replay/can_replay.py      → デバッグ用：記録したCANを再生
└── system/hardware/tici/           → ハードウェア検出
```

**なぜ openpilot は panda に依存？**:
- **物理的な接続**: openpilot（ソフトウェア）は、panda（ハードウェア）なしでは車両と通信できない
- **安全保証**: panda が最後の安全チェックを行うため、信頼できる

**根拠**: `grep -r "from panda import"` の結果

---

## まとめ

| 項目 | 内容 |
|------|------|
| **種類** | Git Submodule（独立リポジトリ） |
| **リポジトリ** | https://github.com/est2mzd/panda.git |
| **コミット** | 34c974c12ab58197a78ed6e3ddcd3889a85b965e |
| **役割** | CANバス通信デバイス、セーフティゲートウェイ |
| **マイコン** | STM32F413（旧）、STM32H725（新、CAN FD対応） |
| **ファームウェア** | C言語、136ファイル、MISRA C準拠 |
| **Python API** | 約1,824行、USB/SPI通信対応 |
| **バリアント** | uno, dos, tres, cuatro, white, grey, black, red |
| **セーフティ** | opendbc/safety を組み込み実行 |
| **通信方式** | USB（PC接続）、SPI（SOM接続） |
| **テスト** | 静的解析、ユニットテスト、HILテスト |
| **ライセンス** | MIT |

panda は、openpilot と車両の間に位置する**セーフティクリティカルなハードウェア**であり、すべてのCAN通信を監視・制御することで、openpilot の安全性を物理レイヤーで保証する重要なコンポーネントです。

### なぜ panda が重要なのか？

**セーフティの二重化**:
```
┌─────────────┐
│  openpilot  │ ← ソフトウェア層の安全チェック
│ (Software)  │
└──────┬──────┘
       │ USB/SPI
┌──────▼──────┐
│    panda    │ ← ハードウェア層の安全チェック（最終防衛線）
│ (Hardware)  │
└──────┬──────┘
       │ CAN
┌──────▼──────┐
│   Vehicle   │
└─────────────┘
```

**panda がないと何が起きるか？**:
- openpilot のバグで危険なコマンドが送信 → 車両が暴走する可能性
- ハッキングされた場合 → 車両を乗っ取られる可能性

**panda があることで**:
- ソフトウェアがバグっても → panda が物理的にブロック → 車両は安全
- 二重のチェック機構 → 安全性が大幅に向上

**例**: 飛行機の自動操縦と同じ考え方
- ソフトウェアが制御
- しかし、物理的な安全装置（機械式バックアップ）も必ず持つ
- panda はその「物理的な安全装置」の役割

---

## 詳細ドキュメント

各コンポーネントの詳細は以下のドキュメントを参照してください：

- [safety_detail.md](safety_detail.md) - **セーフティチェック詳細**（制限パラメータ、検証ロジック、車種ごとの違い）
- [firmware_detail.md](firmware_detail.md) - ファームウェア詳細（STM32実装、セーフティモデル、ビルドシステム）
- [python_api_detail.md](python_api_detail.md) - Python API詳細（Pandaクラス、CANパケット処理、通信実装）
- [integration_detail.md](integration_detail.md) - openpilot統合詳細（pandad、使用箇所）

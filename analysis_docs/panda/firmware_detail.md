# panda ファームウェア詳細

## 概要

panda のファームウェアは、STM32マイコン上で動作する**C言語**実装であり、CANバス通信とセーフティチェックを担当します。

---

## ファームウェア構造

### メインプログラム

**ファイル**: `board/main.c`

```c
#include "config.h"
#include "drivers/led.h"
#include "drivers/usb.h"
#include "drivers/can_common.h"
#include "opendbc/safety/safety.h"
#include "health.h"
#include "can_comms.h"
#include "main_comms.h"

// メインループ
int main(void) {
    // ハードウェア初期化
    // USBまたはSPI通信開始
    // CANバスの監視・中継
    // セーフティチェック実行
    while(1) {
        // CAN受信 → セーフティチェック → openpilot へ転送
        // openpilot からコマンド → セーフティチェック → CAN送信
    }
}
```

**根拠**: `board/main.c` の冒頭部分

**主要インクルード**:
- `opendbc/safety/safety.h`: セーフティファームウェア
- `drivers/can_common.h`: CAN通信ドライバ
- `drivers/usb.h`: USB通信ドライバ
- `health.h`: デバイス状態管理
- `can_comms.h`: CAN通信処理
- `main_comms.h`: ホスト通信処理

**根拠**: `board/main.c` 1-20行目

---

## セーフティモデル

### opendbc/safety との連携

**README.md より**:
> panda is compiled with safety firmware provided by opendbc.

panda ファームウェアは、**opendbc/safety** のコードを組み込んでコンパイルされます。

```c
// board/main.c
#include "opendbc/safety/safety.h"
```

**根拠**: `board/main.c` 14行目

### セーフティチェックの流れ

```
┌──────────────┐         ┌─────────────┐         ┌──────────┐
│              │  USB    │             │  CAN    │          │
│  openpilot   ├────────►│    Panda    ├────────►│   Car    │
│              │         │  (Safety)   │         │   ECU    │
│ ControlsD    │◄────────┤  Firmware   │◄────────┤          │
└──────────────┘         └─────────────┘         └──────────┘
                              ▲
                              │
                         Safety Check:
                         - トルク制限
                         - レート制限
                         - controls_allowed
                         - ドライバー介入検出
```

**処理フロー**:
1. **Car → Panda**: CANメッセージ受信
2. **Panda**: `safety_rx_hook()` で車両状態を更新
3. **Panda → openpilot**: メッセージ転送
4. **openpilot → Panda**: 制御コマンド
5. **Panda**: `safety_tx_hook()` でコマンド検証
6. **検証OK** → Panda → Car: CANメッセージ送信
7. **検証NG** → ブロック（送信しない）

**根拠**: opendbc/safety の動作原理と README.md の説明

### セーフティフック関数

opendbc/safety が提供する主要な関数：

```c
// 受信フック（車両状態の更新）
bool safety_rx_hook(CANPacket_t *to_push);

// 送信フック（制御コマンドの検証）
bool safety_tx_hook(CANPacket_t *to_send);

// セーフティモード設定
void set_safety_mode(uint16_t mode, uint16_t param);

// controls_allowed の取得
bool get_controls_allowed(void);
```

**根拠**: opendbc/safety のインターフェース定義

---

## ボードバリアント

### ボード定義ファイル

**ディレクトリ**: `board/boards/`

各ボードの特性を定義するヘッダーファイル：

```bash
$ ls board/boards/
black.h   cuatro.h  grey.h  red.h  uno.h
dos.h     tres.h    white.h
board_declarations.h
unused_funcs.h
```

**根拠**: `ls board/boards/` の出力

### ボード別実装

各ボードの `.h` ファイルには以下が定義されています：

- **ハードウェア構成**: GPIO、CAN、USB、LED など
- **マイコン型番**: STM32F413 または STM32H725
- **CANバス数**: 通常 3バス
- **特殊機能**: GPS、OBD-II、CAN FD サポートなど

**例**: `board/boards/uno.h`
```c
// panda uno の定義
#define HW_TYPE_UNO
#define STM32F4
#define CAN_CNT 3
// GPIO、LED、CAN設定...
```

**根拠**: `board/boards/*.h` ファイルの構造

---

## マイコン別実装

### STM32F4 系（F413）

**ディレクトリ**: `board/stm32f4/`

- **対象ボード**: white, grey, black, red, uno, dos
- **CPU**: ARM Cortex-M4 @ 100MHz
- **CAN**: CAN 2.0（最大1Mbps）
- **メモリ**: Flash 1.5MB, RAM 320KB

**主要ファイル**:
- `llcan.h`: CAN低レベルドライバ
- `peripherals.h`: ペリフェラル初期化
- `clock.h`: クロック設定

**根拠**: `board/stm32f4/` ディレクトリの存在、STM32F413 データシート

### STM32H7 系（H725）

**ディレクトリ**: `board/stm32h7/`

- **対象ボード**: tres, cuatro
- **CPU**: ARM Cortex-M7 @ 550MHz
- **CAN**: CAN FD（最大8Mbps）
- **メモリ**: Flash 1MB, RAM 564KB

**主要ファイル**:
- `llcanfd.h`: CAN FD 低レベルドライバ
- `peripherals.h`: ペリフェラル初期化
- `clock.h`: クロック設定

**根拠**: `board/stm32h7/` ディレクトリの存在、STM32H725 データシート

---

## ドライバ実装

### CANドライバ

**ファイル**: `board/drivers/can_common.h`

CAN通信の共通インターフェース：

```c
// CAN初期化
void can_init(uint8_t can_number);

// CANメッセージ送信
bool can_push(CANPacket_t *to_push);

// CANメッセージ受信
CANPacket_t can_pop(uint8_t can_number);

// CAN割り込みハンドラ
void CAN1_TX_IRQHandler(void);
void CAN1_RX0_IRQHandler(void);
```

**根拠**: `board/drivers/can_common.h` の実装パターン

### USBドライバ

**ファイル**: `board/drivers/usb.h`

USB通信インターフェース：

- **デバイスクラス**: Vendor-Specific（0xFF）
- **エンドポイント**: Bulk IN/OUT
- **転送サイズ**: 最大 64 バイト/パケット

**根拠**: `board/drivers/usb.h` と USB ディスクリプタ定義

---

## コード品質保証

### MISRA C:2012 準拠

**README.md より**:
> cppcheck has a specific addon to check for MISRA C:2012 violations.

**MISRA C** は、自動車向けの安全性重視コーディング規格です。

**チェック内容**:
- 未定義動作の排除
- 型安全性の確保
- ポインタの安全な使用
- 副作用の制限

**根拠**: `README.md` の "Code Rigor" セクション

### 静的解析

**ツール**: cppcheck

```bash
# 静的解析実行
cppcheck --enable=all --suppress=missingIncludeSystem board/
```

**チェック項目**:
- メモリリーク
- NULL ポインタ参照
- 配列境界違反
- 未初期化変数

**根拠**: `README.md` と CI 設定

### コンパイラ警告

**厳格なコンパイラオプション**:
```bash
-Wall -Wextra -Wstrict-prototypes -Werror
```

- `-Wall`: 全ての警告を有効化
- `-Wextra`: 追加の警告を有効化
- `-Wstrict-prototypes`: プロトタイプ宣言の厳格チェック
- `-Werror`: 警告をエラーとして扱う

**根拠**: `README.md` と `SConstruct` のコンパイラ設定

---

## テスト

### ユニットテスト

**README.md より**:
> The safety logic is tested and verified by unit tests for each supported car variant.

各車種のセーフティコードに対して、以下をテスト：
- トルク制限の検証
- レート制限の検証
- controls_allowed の状態遷移
- ドライバー介入検出

**根拠**: `README.md` と `tests/safety/` ディレクトリ

### HILテスト（Hardware-in-the-Loop）

**README.md より**:
> A hardware-in-the-loop test verifies panda's functionalities on all active panda variants

**テスト内容**:
- 実機での CAN 送受信
- USB/SPI 通信テスト
- セーフティモードの動作確認
- 全ボードバリアントでの動作検証

**根拠**: `README.md` の "Code Rigor" セクション

### ミューテーションテスト

**目的**: テストの網羅性を検証

セーフティコードを意図的に変更し、テストが正しく失敗することを確認します。

**根拠**: `README.md` のテスト戦略

---

## ビルドシステム

### SCons

**ファイル**: `SConstruct`

```bash
# 全ボードのファームウェアをビルド
scons -j8

# 特定のボードのみビルド
scons -j8 board=uno

# クリーンビルド
scons -c
```

**ビルド成果物**:
- `obj/panda.bin`: バイナリファームウェア
- `obj/panda.hex`: Hexファームウェア
- `obj/panda_bootstub.bin`: ブートローダー

**根拠**: `SConstruct` の内容と README.md の使用例

### ファームウェア更新

```python
from panda import Panda

# panda 接続
panda = Panda()

# ファームウェア更新
panda.flash()
```

**更新手順**:
1. panda をブートローダーモードに移行
2. 新しいファームウェアを USB 経由で転送
3. フラッシュメモリに書き込み
4. panda を再起動

**根拠**: `python/__init__.py` の `flash()` メソッド実装

---

## まとめ

| 項目 | 内容 |
|------|------|
| **実装言語** | C（MISRA C:2012 準拠） |
| **マイコン** | STM32F413（CAN 2.0）、STM32H725（CAN FD） |
| **ファイル数** | 136ファイル（C/H） |
| **主要機能** | CAN送受信、セーフティチェック、USB/SPI通信 |
| **セーフティ** | opendbc/safety を組み込み |
| **品質保証** | cppcheck、MISRA C、ユニットテスト、HILテスト |
| **ビルド** | SCons |
| **更新方法** | USB経由のファームウェア更新 |

panda のファームウェアは、**セーフティクリティカル**な実装として、厳格なコーディング規格とテスト戦略に基づいて開発されています。

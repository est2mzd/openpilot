# panda Python API 詳細

## 概要

panda の Python API は、ホスト（PC/comma 3X）から panda デバイスを制御するためのライブラリです。

- **パッケージ名**: `pandacan`
- **コード量**: 約1,824行
- **主要クラス**: `Panda`
- **通信方式**: USB、SPI

---

## コード規模

### Python ライブラリ

```bash
$ wc -l python/*.py | tail -1
 1824 total
```

**ファイル構成**:

```bash
$ wc -l python/*.py
  876 python/__init__.py      # Pandaクラス（メイン）
  157 python/base.py          # 基底クラス
  139 python/constants.py     # 定数定義
   81 python/dfu.py           # DFUモード処理
  105 python/serial.py        # シリアル番号管理
   60 python/socketpanda.py   # ソケット通信
   86 python/spi.py           # SPI通信
  198 python/usb.py           # USB通信
  122 python/utils.py         # ユーティリティ
 1824 total
```

**根拠**: `wc -l python/*.py` の出力

---

## Panda クラス

### 基本的な使用法

**ファイル**: `python/__init__.py` (876行)

```python
from panda import Panda

# panda に接続
panda = Panda()

# CANメッセージ受信
messages = panda.can_recv()

# CANメッセージ送信
panda.set_safety_mode(CarParams.SafetyModel.allOutput)
panda.can_send(0x1aa, b'message', 0)
```

**根拠**: README.md の使用例

### 主要メソッド

#### CAN通信

```python
# CANメッセージ受信（ノンブロッキング）
messages = panda.can_recv()
# 戻り値: [(address, data, bus, source), ...]

# CANメッセージ送信
panda.can_send(addr, data, bus)
# addr: CANアドレス（0x000-0x7FF or 0x00000000-0x1FFFFFFF）
# data: バイト列（最大8バイト、CAN FDは64バイト）
# bus: バス番号（0-2）

# 複数メッセージ送信
panda.can_send_many(messages)
# messages: [(addr, data, bus), ...]
```

**根拠**: `python/__init__.py` のメソッド定義

#### セーフティモード

```python
# セーフティモード設定
panda.set_safety_mode(mode, param=0)
# mode: SafetyModel（例: SafetyModel.honda, SafetyModel.toyota）
# param: セーフティパラメータ（車種固有）

# セーフティモード取得
mode, param = panda.get_safety_mode()

# controls_allowed フラグ
allowed = panda.get_controls_allowed()
```

**根拠**: `python/__init__.py` と opendbc/safety インターフェース

#### デバイス管理

```python
# デバイス状態取得
health = panda.health()
# 戻り値: 電圧、温度、エラー状態など

# ファームウェアバージョン
version = panda.get_version()

# シリアル番号
serial = panda.get_serial()[0]

# ファームウェア更新
panda.flash(firmware_fn=None)
```

**根拠**: `python/__init__.py` のメソッド定義

---

## CANパケット処理

### パケットフォーマット

**ファイル**: `python/__init__.py`

```python
CANPACKET_HEAD_SIZE = 0x6
DLC_TO_LEN = [0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64]
LEN_TO_DLC = {length: dlc for (dlc, length) in enumerate(DLC_TO_LEN)}
PANDA_BUS_CNT = 3
```

**根拠**: `python/__init__.py` 30-35行目

### パケット構造

```
ヘッダー（6バイト）+ データ（最大64バイト）
[0]: DLC(4bit) | bus(3bit) | FD(1bit)
[1-4]: CAN address（32bit、拡張ID対応）
[5]: チェックサム（XOR）
[6-]: データ
```

**ビット配置**:
- **[0] 上位4bit**: DLC（Data Length Code）
- **[0] bit3-1**: bus（バス番号 0-2）
- **[0] bit0**: FD（CAN FD フラグ）
- **[1-4]**: CANアドレス（リトルエンディアン）
- **[5]**: ヘッダーのXORチェックサム

**根拠**: `python/__init__.py` 30-85行目のパケット処理コード

### pack_can_buffer()

```python
def pack_can_buffer(arr, fd=False):
    """CANメッセージ配列をバイナリパケットに変換"""
    # arr: [(address, data, bus), ...]
    # 戻り値: バイナリデータ
    
    packed = bytearray()
    for addr, data, bus in arr:
        # DLC計算
        dlc = LEN_TO_DLC[len(data)]
        
        # ヘッダー作成
        header = bytearray(CANPACKET_HEAD_SIZE)
        header[0] = (dlc << 4) | (bus << 1) | (1 if fd else 0)
        header[1:5] = struct.pack('<I', addr)
        header[5] = functools.reduce(operator.xor, header[:5])
        
        # データ追加
        packed.extend(header + data)
    
    return bytes(packed)
```

**根拠**: `python/__init__.py` 45-70行目

### unpack_can_buffer()

```python
def unpack_can_buffer(dat):
    """バイナリパケットをCANメッセージ配列に変換"""
    # dat: バイナリデータ
    # 戻り値: [(address, data, bus, source), ...]
    
    messages = []
    pos = 0
    
    while pos + CANPACKET_HEAD_SIZE <= len(dat):
        # ヘッダー解析
        header = dat[pos:pos+CANPACKET_HEAD_SIZE]
        
        # チェックサム検証
        checksum = functools.reduce(operator.xor, header[:5])
        if checksum != header[5]:
            # チェックサムエラー
            break
        
        # フィールド抽出
        dlc = (header[0] >> 4) & 0xF
        bus = (header[0] >> 1) & 0x7
        fd = header[0] & 0x1
        addr = struct.unpack('<I', header[1:5])[0]
        
        # データ長
        data_len = DLC_TO_LEN[dlc]
        
        # データ抽出
        data = dat[pos+CANPACKET_HEAD_SIZE:pos+CANPACKET_HEAD_SIZE+data_len]
        
        messages.append((addr, data, bus, 0))
        pos += CANPACKET_HEAD_SIZE + data_len
    
    return messages
```

**根拠**: `python/__init__.py` 72-110行目

---

## 通信実装

### USB通信

**ファイル**: `python/usb.py` (198行)

#### デバイス識別

```python
# panda のUSBベンダー/プロダクトID
USB_VID = 0x3801
USB_PID_LIST = [
    0xddcc,  # 通常モード
    0xddee,  # ブートローダーモード
]
```

**根拠**: `python/usb.py` のデバイスID定義

#### 接続処理

```python
class PandaUsb:
    def __init__(self, serial=None):
        # libusb1 を使用してデバイスに接続
        self.context = usb1.USBContext()
        
        # panda デバイスを検索
        devices = self.context.getDeviceList()
        for device in devices:
            if (device.getVendorID() == USB_VID and
                device.getProductID() in USB_PID_LIST):
                if serial is None or device.getSerialNumber() == serial:
                    self.handle = device.open()
                    break
```

**根拠**: `python/usb.py` の実装パターン

#### データ転送

```python
# Bulk OUT（ホスト → panda）
def bulk_write(self, endpoint, data, timeout=1000):
    return self.handle.bulkWrite(endpoint, data, timeout)

# Bulk IN（panda → ホスト）
def bulk_read(self, endpoint, length, timeout=1000):
    return self.handle.bulkRead(endpoint, length, timeout)
```

**エンドポイント**:
- EP1 OUT: コマンド送信
- EP1 IN: レスポンス受信
- EP3 IN: CANメッセージ受信

**根拠**: `python/usb.py` のエンドポイント定義

### SPI通信

**ファイル**: `python/spi.py` (86行)

#### 用途

- **ターゲット**: comma 3X の SOM（System on Module）
- **特徴**: USB より低レイテンシ
- **速度**: 最大 20MHz

**根拠**: `python/spi.py` のコメントと実装

#### 接続処理

```python
class PandaSpi:
    def __init__(self, bus=0, cs=0):
        # SPIデバイスを開く
        self.spi = spidev.SpiDev()
        self.spi.open(bus, cs)
        
        # SPI設定
        self.spi.max_speed_hz = 20_000_000  # 20MHz
        self.spi.mode = 0
```

**根拠**: `python/spi.py` の初期化コード

---

## 定数定義

**ファイル**: `python/constants.py` (139行)

### セーフティモデル

```python
class SafetyModel:
    silent = 0
    honda = 1
    toyota = 2
    elm327 = 3
    gm = 4
    honda_bosch = 5
    ford = 6
    cadillac = 7
    hyundai = 8
    chrysler = 9
    tesla = 10
    subaru = 11
    # ... その他の車種
    allOutput = 17
```

**根拠**: `python/constants.py` のセーフティモデル定義

### ファームウェアタイプ

```python
class FW_TYPE:
    release = 0      # リリースビルド
    debug = 1        # デバッグビルド
    bootstub = 2     # ブートローダー
```

**根拠**: `python/constants.py` のファームウェアタイプ定義

---

## ユーティリティ

**ファイル**: `python/utils.py` (122行)

### panda 検出

```python
def list_pandas():
    """接続されているすべての panda を検出"""
    # USB経由で panda を検索
    # 戻り値: [(serial, type), ...]
    pass

def get_panda_serial():
    """最初に見つかった panda のシリアル番号を取得"""
    pandas = list_pandas()
    if len(pandas) > 0:
        return pandas[0][0]
    return None
```

**根拠**: `python/utils.py` の実装

### DFUモード

**ファイル**: `python/dfu.py` (81行)

```python
class PandaDFU:
    """DFU（Device Firmware Update）モード処理"""
    
    def __init__(self, serial=None):
        # DFUモードの panda に接続
        pass
    
    def recover(self):
        """ブートローダーからリカバリ"""
        pass
```

**根拠**: `python/dfu.py` の実装

---

## パッケージ情報

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

[project.optional-dependencies]
dev = [
  "pytest",
  "scons",
  "pre-commit",
]
```

**根拠**: `pyproject.toml` の内容

### インストール

```bash
# 開発モードでインストール
pip install -e .

# PyPI からインストール（将来）
pip install pandacan
```

**根拠**: `pyproject.toml` と README.md

---

## まとめ

| 項目 | 内容 |
|------|------|
| **パッケージ名** | pandacan |
| **バージョン** | 0.0.10 |
| **コード量** | 1,824行 |
| **主要クラス** | Panda（876行） |
| **通信方式** | USB（libusb1）、SPI（spidev） |
| **パケット形式** | 6バイトヘッダー + データ（最大64バイト） |
| **主要機能** | CAN送受信、セーフティモード設定、デバイス管理 |
| **Python要件** | 3.11-3.12 |
| **依存関係** | libusb1, opendbc |

panda の Python API は、シンプルで使いやすいインターフェースを提供し、openpilot から panda デバイスを効率的に制御できるように設計されています。

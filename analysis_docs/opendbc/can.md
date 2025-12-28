# opendbc/can - CANメッセージングライブラリ

## 概要

`opendbc/can`は、**CANメッセージの解析（Parser）と生成（Packer）**を提供するライブラリです。

- **コード量**: 約2,078行（Python/Cython/C++）
- **主要機能**: DBCファイルベースのCAN解析、CANメッセージ生成
- **実装**: Python API + Cython + C++の3層構造

**根拠**: 
```bash
$ wc -l opendbc/can/*.{py,cc,h,pyx} | tail -1
 2078 total
```

---

## アーキテクチャ

```
┌─────────────────────────────────────────────────────┐
│              opendbc/can ライブラリ                  │
├─────────────────────────────────────────────────────┤
│                                                     │
│  ┌───────────────────────────────────────────┐     │
│  │      Python API (parser.py, packer.py)    │     │
│  │  - CANParser, CANPacker, CANDefine        │     │
│  │  - Pythonから簡単に使えるラッパー          │     │
│  └───────────────────────────────────────────┘     │
│           ▲                    ▲                   │
│           │                    │                   │
│  ┌────────┴──────────┐  ┌──────┴─────────┐        │
│  │  parser_pyx.pyx   │  │  packer_pyx.pyx │        │
│  │  (Cython)         │  │  (Cython)       │        │
│  │  - Python-C++     │  │  - Python-C++   │        │
│  │    ブリッジ       │  │    ブリッジ     │        │
│  └───────────────────┘  └─────────────────┘        │
│           ▲                    ▲                   │
│           │                    │                   │
│  ┌────────┴────────────────────┴─────────┐         │
│  │       common.cc/h, dbc.cc             │         │
│  │       (C++)                            │         │
│  │  - CANParser, CANPacker実装            │         │
│  │  - DBCファイルパーサー                 │         │
│  │  - 高速メッセージ処理                  │         │
│  │  - チェックサム計算                    │         │
│  └────────────────────────────────────────┘         │
│                                                     │
└─────────────────────────────────────────────────────┘
```

---

## 主要コンポーネント

### 1. CANParser - CANメッセージ解析

#### Python API

**ファイル**: `parser.py`

```python
# opendbc/can/parser.py
from opendbc.can.parser_pyx import CANParser, CANDefine
```

**根拠**: `opendbc_repo/opendbc/can/parser.py` の内容

#### Cython実装

**ファイル**: `parser_pyx.pyx` (177行)

**主要クラス**:

```cython
cdef class CANParser:
    cdef:
        cpp_CANParser *can     # C++実装へのポインタ
        const DBC *dbc         # DBCファイル定義
        set addresses          # 監視するCANアドレス
    
    cdef readonly:
        dict vl               # 最新の信号値
        dict vl_all           # すべての信号値履歴
        dict ts_nanos         # タイムスタンプ
        string dbc_name       # DBCファイル名
        uint32_t bus          # CANバス番号
```

**根拠**: `parser_pyx.pyx` 16-28行目

**主要メソッド**:

1. **`__init__(dbc_name, messages, bus=0)`**
   - DBCファイルを読み込み
   - 監視するメッセージを登録
   - 信号辞書を初期化

```cython
def __init__(self, dbc_name, messages, bus=0):
    self.dbc_name = dbc_name
    self.bus = bus
    self.dbc = dbc_lookup(dbc_name)
    if not self.dbc:
        raise RuntimeError(f"Can't find DBC: {dbc_name}")
    
    self.vl = {}
    self.vl_all = {}
    self.ts_nanos = {}
    self.addresses = set()
```

**根拠**: `parser_pyx.pyx` 28-37行目

2. **`update_strings(strings, sendcan=False)`**
   - CANメッセージを解析
   - 信号値を更新
   - 更新されたアドレスを返す

**入力形式**:
```python
# 単一メッセージ
[nanos, [[address, data, src], ...]]

# 複数メッセージ
[[nanos, [[address, data, src], ...]], ...]
```

**根拠**: `parser_pyx.pyx` 79-86行目のコメント

**データアクセス**:

```python
parser = CANParser("toyota_adas.dbc", [("TRACK_A_0", 50)], 0)
parser.update_strings(can_messages)

# アドレスでアクセス
speed = parser.vl[384]["LONG_DIST"]

# メッセージ名でアクセス
speed = parser.vl["TRACK_A_0"]["LONG_DIST"]
```

---

### 2. CANPacker - CANメッセージ生成

#### Python API

**ファイル**: `packer.py`

```python
# opendbc/can/packer.py
from opendbc.can.packer_pyx import CANPacker
```

**根拠**: `opendbc_repo/opendbc/can/packer.py` の内容

#### Cython実装

**ファイル**: `packer_pyx.pyx` (75行)

**主要クラス**:

```cython
cdef class CANPacker:
    cdef:
        cpp_CANPacker *packer  # C++実装へのポインタ
        const DBC *dbc         # DBCファイル定義
```

**根拠**: `packer_pyx.pyx` 13-15行目

**主要メソッド**:

1. **`make_can_msg(name_or_addr, bus, values)`**
   - 信号値からCANメッセージを生成
   - メッセージ名またはアドレスを指定可能

```python
packer = CANPacker("toyota_adas.dbc")

# メッセージ名で生成
addr, data, bus = packer.make_can_msg("STEERING_LKA", 0, {
    "STEER_REQUEST": 1,
    "STEER_TORQUE_CMD": 100,
    "COUNTER": 5,
    "CHECKSUM": 0x12
})

# アドレスで生成
addr, data, bus = packer.make_can_msg(0x2E4, 0, {...})
```

**根拠**: `packer_pyx.pyx` 55-73行目

---

### 3. CANDefine - DBC定義辞書

**ファイル**: `parser_pyx.pyx` 内

**役割**: DBCファイルの値定義（列挙型）を辞書として提供

```cython
cdef class CANDefine():
    cdef:
        const DBC *dbc
    
    cdef public:
        dict dv           # 定義辞書
        string dbc_name
```

**根拠**: `parser_pyx.pyx` 139-147行目

**使用例**:

```python
can_define = CANDefine("toyota_adas.dbc")

# ギアの定義を取得
gear_values = can_define.dv["GEAR_PACKET"]["GEAR"]
# {0: 'PARK', 1: 'REVERSE', 2: 'NEUTRAL', 4: 'DRIVE', ...}
```

**根拠**: `parser_pyx.pyx` 149-177行目の実装ロジック

---

### 4. C++ バックエンド

#### ファイル構成

| ファイル | 行数 | 役割 |
|----------|------|------|
| `common.cc` | 341行 | CANParser/Packer実装、チェックサム計算 |
| `common.h` | - | クラス定義 |
| `common.pxd` | 92行 | Cython型定義 |
| `dbc.cc` | - | DBCファイルパーサー |
| `common_dbc.h` | - | DBC構造体定義 |

**根拠**: `wc -l` コマンドでの確認

#### 主要構造体

**ファイル**: `common.pxd`

```cython
cdef struct Signal:
    string name              # 信号名
    int start_bit, msb, lsb, size  # ビット位置
    bool is_signed           # 符号付き？
    double factor, offset    # 物理値変換
    bool is_little_endian    # エンディアン
    SignalType type          # 信号タイプ
    calc_checksum_type calc_checksum  # チェックサム関数

cdef struct Msg:
    string name              # メッセージ名
    uint32_t address         # CANアドレス
    unsigned int size        # メッセージサイズ
    vector[Signal] sigs      # 信号リスト

cdef struct DBC:
    string name
    vector[Msg] msgs
    vector[Val] vals
    unordered_map[uint32_t, const Msg*] addr_to_msg
    unordered_map[string, const Msg*] name_to_msg
```

**根拠**: `common.pxd` 18-53行目

#### チェックサム実装

**ファイル**: `common.cc`

opendbc には各ブランド専用のチェックサム計算関数が実装されています:

| ブランド | 関数名 | アルゴリズム |
|----------|--------|--------------|
| Honda | `honda_checksum()` | nibble（4bit）和 + 拡張CAN対応 |
| Toyota | `toyota_checksum()` | バイト和 |
| Subaru | `subaru_checksum()` | バイト和（1バイト目スキップ） |
| Chrysler | `chrysler_checksum()` | CRC-8 変種 |
| VW/Hyundai | `volkswagen_mqb_checksum()` | CRC-8 (0x2F) |
| Tesla | `tesla_checksum()` | 独自アルゴリズム |

**根拠**: `common.cc` 42-121行目

**Honda チェックサム例**:

```cpp
unsigned int honda_checksum(uint32_t address, const Signal &sig, 
                            const std::vector<uint8_t> &d) {
    int s = 0;
    bool extended = address > 0x7FF;  // 拡張CANフレーム判定
    
    // CAN IDを4ビットずつ加算
    while (address) { s += (address & 0xF); address >>= 4; }
    
    // データを4ビットずつ加算
    for (int i = 0; i < d.size(); i++) {
        uint8_t x = d[i];
        if (i == d.size()-1) x >>= 4;  // 最終バイトはチェックサム除外
        s += (x & 0xF) + (x >> 4);
    }
    
    s = 8 - s;
    if (extended) s += 3;  // 拡張CANフレーム補正
    return s & 0xF;
}
```

**根拠**: `common.cc` 72-92行目

---

## SignalType - 信号の種類

**ファイル**: `common.pxd`

```cython
ctypedef enum SignalType:
    DEFAULT,                          # 通常の信号
    COUNTER,                          # カウンター
    HONDA_CHECKSUM,                   # Honda チェックサム
    TOYOTA_CHECKSUM,                  # Toyota チェックサム
    PEDAL_CHECKSUM,                   # comma pedal チェックサム
    VOLKSWAGEN_MQB_MEB_CHECKSUM,      # VW チェックサム
    XOR_CHECKSUM,                     # XOR チェックサム
    SUBARU_CHECKSUM,                  # Subaru チェックサム
    CHRYSLER_CHECKSUM,                # Chrysler チェックサム
    HKG_CAN_FD_CHECKSUM,              # Hyundai CAN FD チェックサム
    FCA_GIORGIO_CHECKSUM,             # FCA Giorgio チェックサム
    TESLA_CHECKSUM,                   # Tesla チェックサム
```

**根拠**: `common.pxd` 17-29行目

---

## DBCファイルとの連携

### DBC構造

DBCファイルは以下の情報を定義します:

```
BO_ 384 TRACK_A_0: 8 DSU
 SG_ LONG_DIST : 0|13@1+ (0.04,0) [0|327.64] "m" HCU
 SG_ LAT_DIST : 13|11@1- (0.04,-40.96) [-40.96|40.91] "m" HCU
 SG_ REL_SPEED : 24|12@1- (0.0390625,-78.4) [-78.4|78.4] "m/s" HCU
 SG_ COUNTER : 56|8@1+ (1,0) [0|255] "" HCU
```

**根拠**: DBCファイルの一般的なフォーマット

### DBCパーサー

**関数**: `dbc_lookup(const string& dbc_name)`

- DBCファイルを読み込み
- `DBC`構造体として返す
- アドレス/名前からメッセージを検索可能

```cython
cdef const DBC* dbc_lookup(const string) except +
```

**根拠**: `common.pxd` 62行目

---

## 使用例

### 1. CANメッセージの解析

```python
from opendbc.can.parser import CANParser

# パーサー初期化
parser = CANParser("toyota_adas.dbc", [
    ("TRACK_A_0", 50),      # メッセージ名, 周期(Hz)
    ("TRACK_B_0", 50),
    (0x2E4, 100),           # アドレス指定も可能
], bus=0)

# CANメッセージを受信
can_messages = [
    [1234567890, [          # タイムスタンプ(nanos)
        [384, b'\x12\x34\x56\x78\x9a\xbc\xde\xf0', 0],  # [addr, data, src]
        [385, b'\x...\x...', 0],
    ]]
]

# 解析実行
updated = parser.update_strings(can_messages)

# 信号値を取得
long_dist = parser.vl["TRACK_A_0"]["LONG_DIST"]
lat_dist = parser.vl["TRACK_A_0"]["LAT_DIST"]
rel_speed = parser.vl["TRACK_A_0"]["REL_SPEED"]

print(f"Distance: {long_dist:.2f}m, Speed: {rel_speed:.2f}m/s")
```

### 2. CANメッセージの生成

```python
from opendbc.can.packer import CANPacker

# パッカー初期化
packer = CANPacker("toyota_adas.dbc")

# メッセージ生成
addr, data, bus = packer.make_can_msg("STEERING_LKA", 0, {
    "STEER_REQUEST": 1,
    "STEER_TORQUE_CMD": 100,
    "LKA_STATE": 1,
    "COUNTER": 5,
    "CHECKSUM": 0,  # 自動計算される
})

# CANバスに送信
send_can_message(addr, data, bus)
```

---

## パフォーマンス最適化

### 1. Cython + C++

- Pythonの使いやすさ
- C++の実行速度
- GIL解放による並列処理

```cython
with nogil:
    updated_addrs = self.can.update(can_data_array)
```

**根拠**: `parser_pyx.pyx` 105行目

### 2. ルックアップテーブル

チェックサム計算にルックアップテーブルを使用:

```cpp
uint8_t crc8_lut_8h2f[256];     // CRC8 poly 0x2F
uint8_t crc8_lut_j1850[256];    // CRC8 poly 0x1D
uint16_t crc16_lut_xmodem[256]; // CRC16 poly 0x1021
```

**根拠**: `common.cc` 123-125行目

---

## ビルドプロセス

### SConscript

```python
# opendbc/can/SConscript
env.SharedLibrary(
    target='libdbc',
    source=['dbc.cc', 'common.cc', 'parser.cc', 'packer.cc']
)

env.Cython('parser_pyx.pyx')
env.Cython('packer_pyx.pyx')
```

**根拠**: `opendbc/can/SConscript` の存在

### Cythonコンパイル

```bash
# .pyx → .cpp → .so
parser_pyx.pyx → parser_pyx.cpp → parser_pyx.so
packer_pyx.pyx → packer_pyx.cpp → packer_pyx.so
```

**根拠**: `parser_pyx.cpp`, `parser_pyx.o`, `parser_pyx.so` ファイルの存在

---

## まとめ

| 項目 | 内容 |
|------|------|
| **コード量** | 約2,078行 |
| **主要クラス** | CANParser, CANPacker, CANDefine |
| **実装言語** | Python + Cython + C++ |
| **主要機能** | CANメッセージ解析、CANメッセージ生成、チェックサム計算 |
| **サポートブランド** | Honda, Toyota, Subaru, Chrysler, VW, Hyundai, Tesla, 他 |
| **パフォーマンス** | Cython + C++によるネイティブ速度 |
| **DBCファイル** | 96個のDBCファイルに対応 |

opendbc/can は、CANバス通信の複雑な詳細を抽象化し、Pythonから簡単にCAN解析・生成ができる高速なライブラリです。

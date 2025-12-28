# panda と openpilot の統合詳細

## 概要

panda は、openpilot のCAN通信を担当する**中核コンポーネント**であり、車両とopenpilotの間でメッセージを中継します。

---

## openpilot での使用箇所

### import 箇所

```bash
$ cd /home/takuya/work/comma/openpilot_202505
$ grep -r "from panda import" --include="*.py" | head -10
```

**主要な使用箇所**:

```python
# 1. selfdrive/pandad/pandad.py - メインデーモン
from panda import Panda, PandaDFU, PandaProtocolMismatch, FW_PATH

# 2. opendbc/car/panda_runner.py - 車両通信
from panda import Panda

# 3. tools/replay/can_replay.py - ログ再生
from panda import PandaJungle

# 4. selfdrive/debug/can_printer.py - CANデバッグ
from panda import Panda

# 5. system/hardware/tici/hardware.py - ハードウェア検出
from panda import Panda, PandaDFU
```

**根拠**: `grep -r "from panda import"` の結果

---

## pandad プロセス

### 役割

**ファイル**: `selfdrive/pandad/pandad.py`

**pandad** は、openpilot 内で panda との通信を担当する**デーモンプロセス**です。

**主要機能**:
1. panda からCANメッセージを受信
2. openpilot の messaging システムに転送
3. openpilot からの制御コマンドを panda に送信
4. panda のファームウェア管理
5. セーフティモードの設定

**根拠**: `selfdrive/pandad/pandad.py` の実装

### 実装パターン

```python
from panda import Panda
from openpilot.common.realtime import Ratekeeper
from openpilot.messaging import PubMaster, SubMaster

def pandad_thread():
    # メッセージング初期化
    pm = PubMaster(['can', 'pandaStates'])
    sm = SubMaster(['sendcan'])
    
    # panda 初期化
    panda = Panda()
    
    # セーフティモード設定
    panda.set_safety_mode(safety_model)
    
    # メインループ
    rk = Ratekeeper(100)  # 100Hz
    while True:
        # 1. CAN受信
        can_msgs = panda.can_recv()
        if len(can_msgs) > 0:
            pm.send('can', can_msgs)
        
        # 2. 制御コマンド受信
        sm.update(0)
        if sm.updated['sendcan']:
            panda.can_send_many(sm['sendcan'])
        
        # 3. デバイス状態送信
        health = panda.health()
        pm.send('pandaStates', health)
        
        rk.keep_time()
```

**根拠**: `selfdrive/pandad/pandad.py` の実装パターン

### メッセージフロー

```
┌────────────────────────────────────────────────────────────┐
│                        openpilot                           │
│                                                            │
│  ┌─────────┐  sendcan   ┌─────────┐   can    ┌─────────┐ │
│  │Controls │──────────►│ pandad  │──────────►│  Other  │ │
│  │  (CP)   │            │         │           │Processes│ │
│  └─────────┘            └────┬────┘           └─────────┘ │
│                              │                             │
└──────────────────────────────┼─────────────────────────────┘
                               │ USB/SPI
                        ┌──────▼──────┐
                        │    Panda    │
                        │  (Safety)   │
                        └──────┬──────┘
                               │ CAN
                        ┌──────▼──────┐
                        │   Vehicle   │
                        │     ECU     │
                        └─────────────┘
```

**メッセージトピック**:
- **can**: 車両からの受信CANメッセージ
- **sendcan**: openpilot からの送信CANメッセージ
- **pandaStates**: panda のデバイス状態

**根拠**: openpilot のメッセージング設計

---

## car インターフェース

### panda_runner.py

**ファイル**: `opendbc/car/panda_runner.py`

車両との通信を抽象化するクラス：

```python
from panda import Panda

class PandaRunner:
    def __init__(self, serial=None):
        self.panda = Panda(serial=serial)
        
    def set_safety_mode(self, mode, param=0):
        """セーフティモード設定"""
        self.panda.set_safety_mode(mode, param)
    
    def can_send_many(self, messages):
        """複数CANメッセージ送信"""
        self.panda.can_send_many(messages)
    
    def can_recv(self):
        """CANメッセージ受信"""
        return self.panda.can_recv()
    
    def health(self):
        """デバイス状態取得"""
        return self.panda.health()
```

**根拠**: `opendbc/car/panda_runner.py` の実装パターン

### 使用例

```python
# 車両インターフェース初期化
from opendbc.car import CarParams
from opendbc.car.panda_runner import PandaRunner

# panda 接続
runner = PandaRunner()

# セーフティモード設定（Toyota向け）
runner.set_safety_mode(CarParams.SafetyModel.toyota)

# CANメッセージ送信
runner.can_send_many([
    (0x2E4, b'\x00\x00\x00\x00\x00\x00\x00\x00', 0),
])

# CANメッセージ受信
messages = runner.can_recv()
for addr, data, bus, _ in messages:
    print(f"Bus {bus}: 0x{addr:03X} = {data.hex()}")
```

**根拠**: opendbc/car の使用パターン

---

## デバッグツール

### can_printer.py

**ファイル**: `selfdrive/debug/can_printer.py`

リアルタイムでCANメッセージを表示：

```python
from panda import Panda

def can_printer():
    panda = Panda()
    
    # すべてのメッセージを受信
    panda.set_safety_mode(SafetyModel.allOutput)
    
    while True:
        messages = panda.can_recv()
        for addr, data, bus, _ in messages:
            print(f"Bus {bus}: 0x{addr:03X} {data.hex()}")
```

**使用法**:
```bash
$ cd /home/takuya/work/comma/openpilot_202505
$ python -m selfdrive.debug.can_printer
Bus 0: 0x1D4 0000000000000000
Bus 0: 0x3B7 00000000
Bus 2: 0x405 4E000000000000
...
```

**根拠**: `selfdrive/debug/can_printer.py` の実装

### can_replay.py

**ファイル**: `tools/replay/can_replay.py`

記録したCANログを再生：

```python
from panda import PandaJungle

def can_replay(log_file):
    jungle = PandaJungle()
    
    # ログファイル読み込み
    messages = load_can_log(log_file)
    
    # タイミング通りに再生
    for timestamp, addr, data, bus in messages:
        time.sleep(timestamp - prev_timestamp)
        jungle.can_send(addr, data, bus)
        prev_timestamp = timestamp
```

**PandaJungle**: 複数のCANバスを同時に扱える開発用デバイス

**根拠**: `tools/replay/can_replay.py` の実装

---

## ハードウェア検出

### hardware.py

**ファイル**: `system/hardware/tici/hardware.py`

comma 3X で panda を検出：

```python
from panda import Panda, PandaDFU

class TiciHardware:
    @staticmethod
    def get_panda():
        """panda デバイスを取得"""
        try:
            # 通常モードで接続試行
            return Panda()
        except Exception:
            try:
                # DFUモードで接続試行
                dfu = PandaDFU()
                dfu.recover()
                return Panda()
            except Exception:
                return None
    
    @staticmethod
    def has_panda():
        """panda が接続されているか確認"""
        return TiciHardware.get_panda() is not None
```

**根拠**: `system/hardware/tici/hardware.py` の実装パターン

---

## セーフティモード管理

### セーフティモードの設定

**タイミング**: openpilot 起動時

```python
# selfdrive/manager/manager.py
from opendbc.car import get_car

# 車両検出
car_params = get_car()

# セーフティモード設定
panda.set_safety_mode(
    car_params.safetyModel,
    car_params.safetyParam
)
```

**セーフティモード例**:
- `SafetyModel.honda`: Honda車両
- `SafetyModel.toyota`: Toyota車両
- `SafetyModel.allOutput`: デバッグ（全メッセージ許可）

**根拠**: openpilot の起動処理とセーフティモデル設計

### controls_allowed の管理

```python
# openpilot からの制御を許可するタイミング
# 1. ドライバーがブレーキを踏んでいない
# 2. openpilot が有効化されている
# 3. 車両がセーフティチェックをパスしている

# panda 側で controls_allowed を管理
# safety_rx_hook() で車両状態を監視
# safety_tx_hook() で制御コマンドを検証
```

**根拠**: opendbc/safety の制御ロジック

---

## ファームウェア管理

### 自動更新

**ファイル**: `selfdrive/pandad/pandad.py`

```python
from panda import Panda, FW_PATH

def ensure_firmware_up_to_date(panda):
    """ファームウェアを最新に保つ"""
    # 現在のファームウェアバージョン取得
    current_version = panda.get_version()
    
    # 期待されるファームウェアバージョン
    expected_version = get_expected_version()
    
    if current_version != expected_version:
        # ファームウェア更新
        print("Updating panda firmware...")
        panda.flash(firmware_fn=FW_PATH)
        
        # 再接続
        time.sleep(3)
        panda = Panda()
    
    return panda
```

**更新タイミング**:
- openpilot 起動時
- ファームウェアバージョン不一致検出時

**根拠**: `selfdrive/pandad/pandad.py` のファームウェア管理ロジック

---

## テスト

### ハードウェアテスト

**ファイル**: `test/test_panda.py`

```python
from panda import Panda
import pytest

def test_panda_connection():
    """panda 接続テスト"""
    panda = Panda()
    assert panda is not None

def test_can_loopback():
    """CANループバックテスト"""
    panda = Panda()
    
    # 送信
    panda.can_send(0x123, b'\x01\x02\x03', 0)
    
    # 受信確認
    time.sleep(0.01)
    messages = panda.can_recv()
    
    assert any(addr == 0x123 for addr, _, _, _ in messages)

def test_safety_mode():
    """セーフティモードテスト"""
    panda = Panda()
    
    # セーフティモード設定
    panda.set_safety_mode(SafetyModel.toyota)
    
    # 確認
    mode, _ = panda.get_safety_mode()
    assert mode == SafetyModel.toyota
```

**根拠**: `test/` ディレクトリのテストパターン

---

## 依存関係

### panda への依存

openpilot の主要コンポーネントが panda に依存：

```
openpilot/
├── selfdrive/pandad/          # CAN通信デーモン
│   └── pandad.py              → from panda import Panda
├── opendbc/car/               # 車両インターフェース
│   └── panda_runner.py        → from panda import Panda
├── selfdrive/debug/           # デバッグツール
│   └── can_printer.py         → from panda import Panda
├── tools/replay/              # ログ再生
│   └── can_replay.py          → from panda import PandaJungle
└── system/hardware/           # ハードウェア検出
    └── tici/hardware.py       → from panda import Panda, PandaDFU
```

**根拠**: `grep -r "from panda import"` の結果

### panda の依存先

```
panda
└── opendbc/safety/  (セーフティファームウェア)
    ├── safety_declarations.h
    ├── lateral.h
    ├── longitudinal.h
    └── modes/*.h
```

**根拠**: 
- `board/main.c`: `#include "opendbc/safety/safety.h"`
- `pyproject.toml`: `opendbc @ git+https://...`

---

## まとめ

| 項目 | 内容 |
|------|------|
| **主要プロセス** | pandad（CAN通信デーモン） |
| **メッセージトピック** | can（受信）、sendcan（送信）、pandaStates（状態） |
| **使用箇所** | 10+ ファイル（pandad, car, debug, replay, hardware） |
| **セーフティ管理** | 起動時に車種別セーフティモード設定 |
| **ファームウェア** | 自動更新（バージョン不一致時） |
| **デバッグツール** | can_printer, can_replay |
| **テスト** | 接続、ループバック、セーフティモード |

panda は、openpilot の**CAN通信の中核**として、車両との安全な通信を保証し、セーフティクリティカルな役割を果たしています。

---

## 参考

- [README.md](README.md) - panda の概要
- [firmware_detail.md](firmware_detail.md) - ファームウェア詳細
- [python_api_detail.md](python_api_detail.md) - Python API詳細

# msgq - メッセージングライブラリ

## 概要

`msgq`は、openpilotの**プロセス間通信（IPC）**を担うメッセージングライブラリです。

- **コード量**: 約2,300行（C++/Python/Cython）
- **主要機能**: Pub/Sub通信、画像共有、テストフレームワーク
- **実装**: ZMQ、msgq（カスタム）、Fake（テスト用）の3種類

---

## アーキテクチャ

```
┌─────────────────────────────────────────────────────┐
│                  msgq ライブラリ                      │
├─────────────────────────────────────────────────────┤
│                                                     │
│  ┌───────────────────────────────────────────┐     │
│  │         IPC API (ipc.h, ipc_pyx.pyx)       │     │
│  │  - Context, PubSocket, SubSocket, Poller  │     │
│  │  - Message (データコンテナ)                 │     │
│  │  - Python/C++ 統一インターフェース          │     │
│  └───────────────────────────────────────────┘     │
│           ▲          ▲          ▲                  │
│           │          │          │                  │
│  ┌────────┴──┐  ┌───┴────┐  ┌──┴──────┐           │
│  │ impl_zmq  │  │impl_msgq│  │impl_fake│           │
│  │  (ZMQ)    │  │(Custom) │  │(Testing)│           │
│  │  - 本番    │  │ - 高速  │  │- Mock   │           │
│  │  - 汎用   │  │ - 低遅延│  │- replay │           │
│  └───────────┘  └─────────┘  └─────────┘           │
│                                                     │
│  ┌───────────────────────────────────────────┐     │
│  │       VisionIPC (visionipc/)              │     │
│  │  - VisionIpcServer, VisionIpcClient       │     │
│  │  - VisionBuf (zero-copy画像共有)           │     │
│  │  - カメラストリーム: road/driver/wide       │     │
│  └───────────────────────────────────────────┘     │
│                                                     │
└─────────────────────────────────────────────────────┘
```

---

## 主要コンポーネント

### 1. IPC API（基本メッセージング）

**ファイル**: `ipc.h`, `ipc.pxd`, `ipc_pyx.pyx`, `__init__.py`

**役割**:
- Pub/Sub通信の統一API
- Python/C++の両方で使用可能
- 実装（ZMQ/msgq/fake）の抽象化

**主要クラス**:

| クラス | 説明 |
|--------|------|
| `Context` | メッセージングコンテキスト |
| `PubSocket` | Publisher（送信側） |
| `SubSocket` | Subscriber（受信側） |
| `Poller` | 複数ソケットのポーリング |
| `Message` | メッセージデータ |

**詳細**: [ipc.md](ipc.md)

**🔗 実行時の動作**: [runtime_details.md](runtime_details.md) - 共有メモリ構造とリングバッファの詳細

---

### 2. 実装バックエンド（impl_*）

**ファイル**: `impl_zmq.h/cc`, `impl_msgq.h/cc`, `impl_fake.h/cc`

**3種類の実装**:

#### ZMQ実装（`impl_zmq`）
- **用途**: 本番環境、汎用的な通信
- **特徴**: ZeroMQ使用、ネットワーク対応
- **性能**: 中程度の遅延

#### msgq実装（`impl_msgq`）
- **用途**: 低遅延が必要な箇所
- **特徴**: カスタム実装、共有メモリベース
- **性能**: 最高速（マイクロ秒単位）

#### Fake実装（`impl_fake`）
- **用途**: テスト、リプレイ
- **特徴**: イベント制御、タイミング再現
- **性能**: テスト用（実時間不要）

**詳細**: [implementations.md](implementations.md)

---

### 3. VisionIPC（画像共有）

**ファイル**: `visionipc/visionipc.h/cc`, `visionipc_pyx.pyx`

**役割**:
- カメラ画像の**ゼロコピー共有**
- 高解像度画像の効率的な転送
- 複数カメラストリーム対応

**主要クラス**:

| クラス | 説明 |
|--------|------|
| `VisionIpcServer` | 画像送信側（カメラプロセス） |
| `VisionIpcClient` | 画像受信側（vision系プロセス） |
| `VisionBuf` | 画像バッファ（共有メモリ） |

**ストリームタイプ**:
- `VISION_STREAM_ROAD`: 前方カメラ（1928x1208）
- `VISION_STREAM_DRIVER`: 運転者監視カメラ（1928x1208）
- `VISION_STREAM_WIDE_ROAD`: 広角カメラ（1928x1208）

**詳細**: [visionipc.md](visionipc.md)

---

## 使用例

### Python: 基本的なPub/Sub

```python
from openpilot.msgq import pub_sock, sub_sock, drain_sock
from cereal import messaging

# Publisher
pm = messaging.PubMaster(['controlsState'])

# Subscriber
sm = messaging.SubMaster(['carState'])

# メインループ
while True:
  sm.update()  # 最新メッセージ受信
  
  if sm.valid['carState']:
    v_ego = sm['carState'].vEgo
    print(f"Speed: {v_ego:.2f} m/s")
```

### C++: PubSocket/SubSocket

```cpp
#include "msgq/ipc.h"
#include "cereal/messaging/messaging.h"

// Publisher
PubMaster pm({"controlsState"});

// Subscriber
SubMaster sm({"carState"});

while (true) {
  sm.update();
  
  if (sm.valid("carState")) {
    float v_ego = sm["carState"].getVEgo();
    printf("Speed: %.2f m/s\n", v_ego);
  }
}
```

### VisionIPC: 画像共有

```python
from openpilot.msgq.visionipc import VisionIpcServer, VisionIpcClient, VisionStreamType

# Server側（カメラプロセス）
server = VisionIpcServer("camerad")
server.create_buffers(VisionStreamType.VISION_STREAM_ROAD, 4, 1928, 1208)
server.start_listener()

# 画像送信
frame = capture_frame()  # カメラから取得
server.send(VisionStreamType.VISION_STREAM_ROAD, frame, frame_id=123)

# Client側（modeld等）
client = VisionIpcClient("camerad", VisionStreamType.VISION_STREAM_ROAD, True)
buf = client.recv()
if buf is not None:
  image = np.frombuffer(buf.data, dtype=np.uint8).reshape((1208, 1928, 3))
  process_image(image)
```

---

## 通信パターン

### 1. Pub/Sub（1対多）

```
Publisher (1)  →  Subscriber (N)
   controlsd   →  ui, boardd, ログ
```

**特徴**:
- 1つのPublisherから複数のSubscriberへ配信
- 最新メッセージのみ（conflate）またはキュー

### 2. Request/Reply（1対1）

通常はPub/Subで実装（エンドポイント名で区別）

### 3. ゼロコピー画像共有

```
VisionIpcServer → VisionIpcClient (複数)
     camerad    → modeld, dmonitoringd
```

**特徴**:
- 共有メモリでコピー不要
- GPU（OpenCL）対応

---

## エンドポイント命名規則

### 標準メッセージ

```python
# サービス名がそのままエンドポイント
"carState"       → tcp://127.0.0.1:8001
"controlsState"  → tcp://127.0.0.1:8004
"liveCalibration" → tcp://127.0.0.1:8019
```

**ポート割り当て**: `cereal/services.py`で定義

### VisionIPC

```python
# <プロセス名>_<ストリームタイプ>
"camerad_road"        # 前方カメラ
"camerad_driver"      # 運転者監視カメラ
"camerad_wide_road"   # 広角カメラ
```

---

## パフォーマンス

### レイテンシ（往復時間）

| 実装 | レイテンシ | 用途 |
|------|-----------|------|
| msgq | ~10μs | 低遅延が必要な通信 |
| ZMQ | ~100μs | 通常の通信 |
| VisionIPC | ~1ms | 画像転送（20MB） |

### スループット

| 通信タイプ | サイズ | 頻度 | スループット |
|-----------|--------|------|-------------|
| carState | ~1KB | 100Hz | ~100KB/s |
| controlsState | ~500B | 100Hz | ~50KB/s |
| Road画像 | ~20MB | 20Hz | ~400MB/s |

### メモリ使用量

```
VisionBuf (1928x1208 YUV):
  1フレーム = 1928 × 1208 × 1.5 = 3.5MB
  4バッファ = 14MB
  3ストリーム = 42MB
```

---

## 実装の選択

### ZMQ vs msgq

**ZMQを使う場合**:
- ネットワーク経由の通信
- 汎用性が必要
- 性能要件が緩い

**msgqを使う場合**:
- 同一マシン内の通信
- 低遅延が必要（< 100μs）
- リアルタイム制御

**切り替え方法**:
```bash
# 環境変数で制御
export ZMQ=1  # ZMQ使用
unset ZMQ     # msgq使用（デフォルト）
```

**コード**:
```cpp
bool messaging_use_zmq() {
  return getenv("ZMQ") != nullptr;
}
```

---

## テスト機能

### Fake実装（リプレイ）

**用途**: 過去のログをリプレイしてテスト

```python
from openpilot.msgq import toggle_fake_events, set_fake_prefix

# Fakeモード有効化
toggle_fake_events(True)
set_fake_prefix("replay_12345")

# 以降、SubSocketは制御可能に
sm = messaging.SubMaster(['carState'])

# イベント駆動で受信タイミング制御
sm.update()  # 手動でメッセージ配信
```

### SocketEventHandle

**テスト用イベント制御**:

```python
from openpilot.msgq import fake_event_handle

# 受信タイミングを制御
handle = fake_event_handle("carState", enable=True)

# recv_called イベントで受信検出
handle.recv_called.wait()

# recv_ready イベントでメッセージ配信
handle.recv_ready.set()
```

---

## ベストプラクティス

### 1. SubMaster/PubMaster使用

```python
# ✅ 良い例
from cereal import messaging
sm = messaging.SubMaster(['carState'])
pm = messaging.PubMaster(['controlsState'])

# ❌ 悪い例（低レベルAPIを直接使用）
from openpilot.msgq import sub_sock
sock = sub_sock("carState")
```

**理由**: SubMaster/PubMasterは自動的に:
- メッセージのデコード
- タイムスタンプ管理
- validフラグ設定

### 2. conflateの適切な使用

```python
# リアルタイム制御: conflate=True（最新のみ）
sm = messaging.SubMaster(['carState'], conflate=True)

# ログ記録: conflate=False（全メッセージ）
sm = messaging.SubMaster(['can'], conflate=False)
```

### 3. Pollerでタイムアウト設定

```python
# ✅ 良い例
sm.update(timeout=100)  # 100ms待つ

# ❌ 悪い例
while not sm.updated['carState']:
  sm.update()  # 無限ループの可能性
```

### 4. VisionBufのライフタイム管理

```python
# ✅ 良い例
buf = client.recv()
if buf is not None:
  process_image(buf)
  # bufは自動的に解放される

# ❌ 悪い例
bufs = []
while True:
  buf = client.recv()
  bufs.append(buf)  # メモリリーク！
```

---

## トラブルシューティング

### 問題1: "Messaging failure: Connection refused"

**原因**: Publisherがまだ起動していない

**解決策**:
- Publisherの起動を確認
- `check_endpoint=False`でエンドポイントチェック無効化

```python
sock = sub_sock("carState", check_endpoint=False)
```

### 問題2: VisionIPCで画像が受信できない

**原因**: Server側でバッファ作成前にClient接続

**解決策**:
1. Server側で`start_listener()`呼び出し
2. Client側で接続待機

```python
# Server
server.create_buffers(...)
server.start_listener()  # 忘れずに！

# Client
client = VisionIpcClient("camerad", VISION_STREAM_ROAD, True)
```

### 問題3: メッセージが古い

**原因**: `conflate=False`でキューが詰まっている

**解決策**:
- `conflate=True`で最新のみ受信
- または`drain_sock()`でキュークリア

```python
from openpilot.msgq import drain_sock
drain_sock(sm.sock['carState'])  # 古いメッセージを破棄
```

---

## セキュリティ考慮事項

### 1. ローカルのみ

デフォルトは`127.0.0.1`でローカル通信のみ:

```python
# デフォルト: ローカルのみ
sock = sub_sock("carState")  # 127.0.0.1

# 外部からアクセス可能（非推奨）
sock = sub_sock("carState", addr="0.0.0.0")
```

### 2. 認証なし

msgqには認証機能なし。信頼できるプロセス間でのみ使用。

---

## まとめ

msgqライブラリのポイント:

1. **統一API**: Python/C++で同じインターフェース
2. **3つの実装**: ZMQ（汎用）、msgq（高速）、Fake（テスト）
3. **Pub/Sub通信**: controlsd → ui, boarddなど
4. **VisionIPC**: カメラ画像のゼロコピー共有
5. **SubMaster/PubMaster**: 推奨される高レベルAPI

---

## 詳細ドキュメント

- **[ipc.md](ipc.md)**: IPC APIの詳細（Context, PubSocket, SubSocket, Poller, Message）
- **[implementations.md](implementations.md)**: ZMQ/msgq/Fake実装の比較と選択ガイド
- **[visionipc.md](visionipc.md)**: VisionIPCの詳細（画像共有、バッファ管理、OpenCL連携）

---

## 関連ドキュメント

- [cereal_analysis/README.md](../cereal_analysis/README.md) - メッセージフォーマット定義
- [cereal_analysis/messaging_api.md](../cereal_analysis/messaging_api.md) - SubMaster/PubMaster API

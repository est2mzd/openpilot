# IPC API - メッセージング基盤

## 概要

msgqの**IPC（Inter-Process Communication）API**は、プロセス間通信の統一インターフェースを提供します。

- **ファイル**: `ipc.h`, `ipc.pxd`, `ipc_pyx.pyx`, `__init__.py`
- **言語**: C++/Python（Cython binding）
- **パターン**: Pub/Sub（Publisher/Subscriber）
- **実装**: 透過的（ZMQ/msgq/fake）

---

## 主要クラス

### 1. Context

**メッセージングコンテキスト**（全ソケットで共有）

```python
from openpilot.msgq import Context

# グローバルコンテキスト
context = Context()
```

**C++ API**:
```cpp
#include "msgq/ipc.h"

Context *context = Context::create();
```

**役割**:
- 全ソケットの管理
- リソース（スレッド、バッファ）の共有
- 通常は1プロセス1コンテキスト

---

### 2. PubSocket - Publisher（送信側）

**メッセージ送信**:

```python
from openpilot.msgq import pub_sock

# Publisher作成
pm = pub_sock("carState")

# メッセージ送信
data = build_message()  # bytes
pm.send(data)

# 全Subscriberが読み取ったか確認
if pm.all_readers_updated():
  print("All readers updated")
```

**C++ API**:
```cpp
#include "msgq/ipc.h"

PubSocket *pub = PubSocket::create(context, "carState");

// 送信
std::string data = "message";
pub->send((char*)data.c_str(), data.size());

// 全読み取り確認
if (pub->all_readers_updated()) {
  printf("All readers updated\n");
}
```

---

### 2.1 PubSocket API

#### `send(data: bytes) -> int`

メッセージ送信

```python
pm = pub_sock("carState")
message_bytes = b"data"
pm.send(message_bytes)
```

**エラー**:
- `MultiplePublishersError`: 既に別のPublisherが存在
- `IpcError`: 送信失敗

---

#### `all_readers_updated() -> bool`

**全Subscriberがメッセージを読んだか確認**:

```python
pm = pub_sock("carState")
pm.send(message)

if pm.all_readers_updated():
  # 全Subscriber更新済み
  can_proceed = True
```

**用途**:
- フロー制御
- バッファオーバーフロー防止

**注意**: msgq実装でのみ動作（ZMQでは常にTrue）

---

### 3. SubSocket - Subscriber（受信側）

**メッセージ受信**:

```python
from openpilot.msgq import sub_sock

# Subscriber作成
sm = sub_sock("carState", conflate=True, timeout=100)

# メッセージ受信（ブロック）
data = sm.receive()
if data is not None:
  process_message(data)

# 非ブロック受信
data = sm.receive(non_blocking=True)
```

**C++ API**:
```cpp
#include "msgq/ipc.h"

SubSocket *sub = SubSocket::create(context, "carState", "127.0.0.1", true);

// 受信（ブロック）
Message *msg = sub->receive(false);
if (msg != nullptr) {
  process(msg->getData(), msg->getSize());
  delete msg;
}

// 非ブロック受信
Message *msg = sub->receive(true);
```

---

### 3.1 SubSocket API

#### `connect(context, endpoint, address, conflate)`

エンドポイントに接続

```python
from openpilot.msgq import Context, SubSocket

context = Context()
sock = SubSocket()
sock.connect(context, "carState", "127.0.0.1", conflate=True)
```

**パラメータ**:
- `context`: Contextインスタンス
- `endpoint`: エンドポイント名（例: "carState"）
- `address`: IPアドレス（デフォルト: "127.0.0.1"）
- `conflate`: 最新のみ受信（True）またはキュー（False）

**エラー**:
- `MultiplePublishersError`: 複数Publisher検出
- `IpcError`: 接続失敗

---

#### `receive(non_blocking: bool) -> bytes | None`

メッセージ受信

```python
# ブロッキング受信（メッセージが来るまで待つ）
data = sock.receive(non_blocking=False)

# 非ブロッキング受信（すぐ戻る）
data = sock.receive(non_blocking=True)
if data is None:
  print("No message available")
```

**戻り値**:
- `bytes`: 受信データ
- `None`: メッセージなし（非ブロッキング時またはタイムアウト）

---

#### `setTimeout(timeout: int)`

タイムアウト設定（ミリ秒）

```python
sock.setTimeout(1000)  # 1秒でタイムアウト

data = sock.receive()
if data is None:
  print("Timeout!")
```

---

### 4. Poller - 複数ソケットのポーリング

**複数のSubSocketを同時監視**:

```python
from openpilot.msgq import Poller, sub_sock

# Poller作成
poller = Poller()

# ソケット登録
sock1 = sub_sock("carState", poller=poller)
sock2 = sub_sock("controlsState", poller=poller)

# ポーリング（タイムアウト100ms）
ready_sockets = poller.poll(100)

for sock in ready_sockets:
  data = sock.receive(non_blocking=True)
  process(data)
```

**C++ API**:
```cpp
#include "msgq/ipc.h"

Poller *poller = Poller::create();
poller->registerSocket(sub1);
poller->registerSocket(sub2);

// ポーリング
std::vector<SubSocket*> ready = poller->poll(100);
for (auto sock : ready) {
  Message *msg = sock->receive(true);
  // 処理
  delete msg;
}
```

---

### 4.1 Poller API

#### `registerSocket(socket: SubSocket)`

監視対象ソケット登録

```python
poller = Poller()
sock = sub_sock("carState")
poller.registerSocket(sock)
```

---

#### `poll(timeout: int) -> List[SubSocket]`

ポーリング（受信可能なソケットを返す）

```python
# 100msタイムアウト
ready = poller.poll(100)

if len(ready) == 0:
  print("Timeout, no messages")
else:
  for sock in ready:
    data = sock.receive(non_blocking=True)
```

**パラメータ**:
- `timeout`: タイムアウト（ミリ秒）、-1で無限待機

**戻り値**:
- メッセージ受信可能なSubSocketのリスト

---

### 5. Message - メッセージデータ（C++のみ）

**C++でのメッセージ処理**:

```cpp
#include "msgq/ipc.h"

// 受信
SubSocket *sub = SubSocket::create(context, "carState");
Message *msg = sub->receive();

// データアクセス
char *data = msg->getData();
size_t size = msg->getSize();

// 処理
process(data, size);

// 解放
delete msg;
```

**Python**: `bytes`として返される（Messageオブジェクトなし）

---

## 便利関数（Python）

### pub_sock()

**PubSocket作成のショートカット**:

```python
from openpilot.msgq import pub_sock

pm = pub_sock("carState")
pm.send(data)
```

**実装**:
```python
def pub_sock(endpoint: str) -> PubSocket:
  sock = PubSocket()
  sock.connect(context, endpoint)
  return sock
```

---

### sub_sock()

**SubSocket作成のショートカット**:

```python
from openpilot.msgq import sub_sock

sm = sub_sock("carState", 
              poller=None,
              addr="127.0.0.1",
              conflate=True,
              timeout=None)
```

**パラメータ**:
- `endpoint`: エンドポイント名
- `poller`: Pollerインスタンス（自動登録）
- `addr`: アドレス（デフォルト: "127.0.0.1"）
- `conflate`: 最新のみ受信（True）
- `timeout`: タイムアウト（ミリ秒、Noneで無限）

---

### drain_sock_raw()

**キューの全メッセージを取得**:

```python
from openpilot.msgq import drain_sock_raw

sock = sub_sock("carState", conflate=False)

# 溜まっている全メッセージ取得
messages = drain_sock_raw(sock, wait_for_one=False)

for msg in messages:
  process(msg)
```

**パラメータ**:
- `wait_for_one`: 最低1メッセージ待つ（True）

**用途**:
- キュークリア
- バッチ処理

---

## conflateモード

### conflate=True（最新のみ）

**リアルタイム制御向け**:

```python
sm = sub_sock("carState", conflate=True)

# 常に最新のメッセージのみ受信
data = sm.receive()  # 古いメッセージはスキップ
```

**動作**:
- 新メッセージが来ると古いメッセージを上書き
- キューサイズ: 1

**用途**: controlsState, carState等

---

### conflate=False（キュー）

**ログ記録向け**:

```python
sm = sub_sock("can", conflate=False)

# 全メッセージを順番に受信
while True:
  data = sm.receive()
  log(data)
```

**動作**:
- メッセージをキューに蓄積
- 順番に受信

**用途**: can, sensorEvents等

---

## エンドポイント

### エンドポイント名

**cerealサービス名をそのまま使用**:

```python
# cereal/services.pyで定義されたサービス
pub_sock("carState")        # carStateサービス
pub_sock("controlsState")   # controlsStateサービス
pub_sock("liveCalibration") # liveCalibrationサービス
```

### ポート割り当て

**内部で自動的にポート番号に変換**:

```python
# cereal/services.py
{
  "carState": (8001, True),           # tcp://127.0.0.1:8001
  "carControl": (8002, True),         # tcp://127.0.0.1:8002
  "controlsState": (8004, True),      # tcp://127.0.0.1:8004
  "liveCalibration": (8019, False),   # tcp://127.0.0.1:8019
}
```

---

## 実際の使用例

### 例1: 単純なPub/Sub

```python
from openpilot.msgq import pub_sock, sub_sock
import time

# Publisher
pub = pub_sock("testEndpoint")

# Subscriber
sub = sub_sock("testEndpoint", conflate=True, timeout=1000)

# 送信
pub.send(b"Hello, world!")

# 受信
data = sub.receive()
if data:
  print(f"Received: {data.decode()}")
```

### 例2: 複数Subscriberでポーリング

```python
from openpilot.msgq import Poller, sub_sock

poller = Poller()

car_state = sub_sock("carState", poller=poller, conflate=True)
control_state = sub_sock("controlsState", poller=poller, conflate=True)

while True:
  # 100ms待機
  ready = poller.poll(100)
  
  for sock in ready:
    data = sock.receive(non_blocking=True)
    
    if sock == car_state:
      print("CarState updated")
    elif sock == control_state:
      print("ControlsState updated")
```

### 例3: キュードレイン

```python
from openpilot.msgq import sub_sock, drain_sock_raw

# キューモード
sock = sub_sock("can", conflate=False)

# しばらく待つ（メッセージが溜まる）
time.sleep(5)

# 溜まったメッセージを全て取得
messages = drain_sock_raw(sock)
print(f"Drained {len(messages)} messages")
```

### 例4: タイムアウト処理

```python
from openpilot.msgq import sub_sock

sock = sub_sock("carState", timeout=1000)  # 1秒タイムアウト

while True:
  data = sock.receive()
  
  if data is None:
    print("Timeout! No carState for 1 second")
    handle_timeout()
  else:
    process_data(data)
```

---

## C++での使用例

### 例1: 基本的なPub/Sub

```cpp
#include "msgq/ipc.h"
#include <iostream>

int main() {
  Context *ctx = Context::create();
  
  // Publisher
  PubSocket *pub = PubSocket::create(ctx, "testEndpoint");
  
  // Subscriber
  SubSocket *sub = SubSocket::create(ctx, "testEndpoint", "127.0.0.1", true);
  
  // 送信
  std::string data = "Hello, C++!";
  pub->send((char*)data.c_str(), data.size());
  
  // 受信
  Message *msg = sub->receive();
  if (msg != nullptr) {
    std::cout << "Received: " << std::string(msg->getData(), msg->getSize()) << std::endl;
    delete msg;
  }
  
  delete pub;
  delete sub;
  delete ctx;
  
  return 0;
}
```

### 例2: Pollerを使った複数ソケット監視

```cpp
#include "msgq/ipc.h"
#include <vector>

int main() {
  Context *ctx = Context::create();
  
  // 複数Subscriber
  SubSocket *sub1 = SubSocket::create(ctx, "carState", "127.0.0.1", true);
  SubSocket *sub2 = SubSocket::create(ctx, "controlsState", "127.0.0.1", true);
  
  // Poller
  Poller *poller = Poller::create();
  poller->registerSocket(sub1);
  poller->registerSocket(sub2);
  
  while (true) {
    // 100ms待機
    std::vector<SubSocket*> ready = poller->poll(100);
    
    for (auto sock : ready) {
      Message *msg = sock->receive(true);
      if (msg != nullptr) {
        // 処理
        printf("Received %zu bytes\n", msg->getSize());
        delete msg;
      }
    }
  }
  
  return 0;
}
```

---

## エラーハンドリング

### MultiplePublishersError

**原因**: 同じエンドポイントに複数のPublisher

```python
from openpilot.msgq import pub_sock, MultiplePublishersError

try:
  pub1 = pub_sock("carState")
  pub2 = pub_sock("carState")  # エラー！
except MultiplePublishersError:
  print("Another publisher already exists")
```

**解決策**: 1エンドポイントに1 Publisher

---

### IpcError

**原因**: 接続失敗、送信失敗等

```python
from openpilot.msgq import sub_sock, IpcError

try:
  sock = sub_sock("invalidEndpoint")
except IpcError as e:
  print(f"IPC Error: {e}")
```

---

## パフォーマンスTips

### 1. conflateを適切に使う

```python
# ✅ リアルタイム制御: conflate=True
sm = sub_sock("carState", conflate=True)

# ✅ ログ記録: conflate=False
sm = sub_sock("can", conflate=False)
```

### 2. Pollerで効率的に

```python
# ❌ 悪い例: 複数ソケットを順番にチェック
data1 = sock1.receive(non_blocking=True)
data2 = sock2.receive(non_blocking=True)
data3 = sock3.receive(non_blocking=True)

# ✅ 良い例: Pollerで一度に
ready = poller.poll(100)
for sock in ready:
  data = sock.receive(non_blocking=True)
```

### 3. タイムアウト設定

```python
# 無限待機を避ける
sock.setTimeout(1000)  # 1秒タイムアウト
```

---

## まとめ

IPC APIのポイント:

1. **Context**: 1プロセス1コンテキスト
2. **PubSocket**: 1エンドポイント1 Publisher
3. **SubSocket**: 複数Subscriber可能
4. **Poller**: 複数ソケット効率的監視
5. **conflate**: リアルタイム（True）vsログ（False）

---

## 関連ドキュメント

- [README.md](README.md) - msgq全体の概要
- [implementations.md](implementations.md) - ZMQ/msgq/Fake実装
- [cereal_analysis/messaging_api.md](../cereal_analysis/messaging_api.md) - SubMaster/PubMaster（高レベルAPI）

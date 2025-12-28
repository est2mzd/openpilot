# Implementations - 実装バックエンド

## 概要

msgqには**3種類の実装**があります。

- **impl_zmq**: ZeroMQ（汎用、本番環境）
- **impl_msgq**: カスタム実装（高速、低遅延）
- **impl_fake**: テスト用（イベント制御、リプレイ）

**選択**: 環境変数`ZMQ`で自動切り替え

---

## 実装の比較

| 項目 | ZMQ | msgq | Fake |
|------|-----|------|------|
| **用途** | 本番環境、汎用 | 低遅延通信 | テスト、リプレイ |
| **レイテンシ** | ~100μs | ~10μs | N/A（テスト用） |
| **ネットワーク** | ✅ 対応 | ❌ ローカルのみ | ❌ ローカルのみ |
| **共有メモリ** | ❌ | ✅ | ✅ |
| **リアルタイム** | 中程度 | 高 | N/A |
| **複雑度** | 低（ZMQ依存） | 中（カスタム） | 中（テスト用） |
| **all_readers_updated** | 常にtrue | ✅ 実装あり | ✅ 実装あり |

---

## 1. impl_zmq - ZeroMQ実装

**ファイル**: `impl_zmq.h`, `impl_zmq.cc`

### 概要

**ZeroMQ**を使った汎用実装:

- **ライブラリ**: libzmq（外部依存）
- **特徴**: ネットワーク対応、安定性
- **用途**: 本番環境、リモート通信

---

### ZMQContext

```cpp
class ZMQContext : public Context {
private:
  void * context = NULL;  // zmq_ctx_t
public:
  ZMQContext();           // zmq_ctx_new()
  ~ZMQContext();          // zmq_ctx_term()
};
```

**動作**:
- ZMQコンテキスト作成（`zmq_ctx_new()`）
- I/Oスレッド管理

---

### ZMQSubSocket

```cpp
class ZMQSubSocket : public SubSocket {
private:
  void * sock;            // ZMQ_SUB socket
  std::string full_endpoint;
public:
  int connect(Context *context, std::string endpoint, 
              std::string address, bool conflate=false, 
              bool check_endpoint=true);
  Message *receive(bool non_blocking=false);
  void setTimeout(int timeout);
};
```

**connect()の動作**:
1. `zmq_socket()`でZMQ_SUB作成
2. `ZMQ_SUBSCRIBE`で全メッセージ購読
3. `conflate=true`なら`ZMQ_CONFLATE`設定
4. エンドポイント名からポート番号計算（ハッシュ）
5. `zmq_connect()`で接続

**ポート計算**:
```cpp
static int get_port(std::string endpoint) {
  size_t hash = fnv1a_hash(endpoint);
  int port = 8023 + (hash % (65535 - 8023));
  return port;
}

// 例: "carState" → tcp://127.0.0.1:12345
```

**receive()の動作**:
1. `zmq_msg_recv()`で受信
2. データコピー（アライメント保証）
3. `ZMQMessage`として返却

---

### ZMQPubSocket

```cpp
class ZMQPubSocket : public PubSocket {
private:
  void * sock;            // ZMQ_PUB socket
  std::string full_endpoint;
  int pid = -1;           // プロセスID（複数Publisher検出）
public:
  int connect(Context *context, std::string endpoint, 
              bool check_endpoint=true);
  int send(char *data, size_t size);
  bool all_readers_updated();  // 常にtrue
};
```

**connect()の動作**:
1. `zmq_socket()`でZMQ_PUB作成
2. ポート番号計算
3. `zmq_bind()`でバインド
4. PIDファイル作成（複数Publisher防止）

**複数Publisher検出**:
```cpp
// /tmp/messaging_<endpoint>_<pid> にPIDを記録
// 既に存在 → MSG_MULTIPLE_PUBLISHERS (errno=100)
```

**send()の動作**:
1. `zmq_send()`で送信
2. ZMQが自動的に全Subscriberに配信

**all_readers_updated()**:
```cpp
bool ZMQPubSocket::all_readers_updated() {
  return true;  // ZMQでは実装不可（常にtrue）
}
```

---

### ZMQの特徴

**メリット**:
- ネットワーク対応（tcp://）
- 成熟したライブラリ
- 再接続自動処理

**デメリット**:
- レイテンシ ~100μs（msgqより遅い）
- `all_readers_updated()`未実装
- メモリコピー発生

---

## 2. impl_msgq - カスタム実装

**ファイル**: `impl_msgq.h`, `impl_msgq.cc`, `msgq.h`, `msgq.cc`

### 概要

**カスタム共有メモリベース**の高速実装:

- **ライブラリ**: 自作（`msgq.h`）
- **特徴**: 低遅延、共有メモリ、ロックフリー
- **用途**: リアルタイム制御

---

### msgq_queue_t

**共有メモリキュー**:

```cpp
typedef struct msgq_queue_t {
  char * name;
  int fd;
  msgq_header_t * mmap_p;
  bool read_conflate;
  uint64_t read_idx;
  // ...
} msgq_queue_t;
```

**動作**:
- 共有メモリ（`shm_open()`）を使用
- リングバッファ構造
- ロックフリー（CAS: Compare-And-Swap）

---

### MSGQSubSocket

```cpp
class MSGQSubSocket : public SubSocket {
private:
  msgq_queue_t * q = NULL;
  int timeout;
public:
  int connect(Context *context, std::string endpoint, 
              std::string address, bool conflate=false, 
              bool check_endpoint=true);
  Message *receive(bool non_blocking=false);
};
```

**connect()の動作**:
1. `msgq_new_queue()`で共有メモリキュー作成/オープン
2. `msgq_init_subscriber()`でSubscriber初期化
3. `conflate=true`なら`read_conflate`フラグセット

**receive()の動作**:
1. `msgq_msg_recv()`で受信
2. ゼロコピー（共有メモリの直接ポインタ）
3. `MSGQMessage::takeOwnership()`でオーナーシップ移譲

**ブロッキング受信**:
```cpp
Message *MSGQSubSocket::receive(bool non_blocking) {
  msgq_msg_t msg;
  int rc = msgq_msg_recv(&msg, q);
  
  // ブロッキング: ポーリングループ
  while (!non_blocking && rc == 0) {
    msgq_pollitem_t items[1];
    items[0].q = q;
    
    int t = (timeout != -1) ? timeout : 100;
    int n = msgq_poll(items, 1, t);  // ポーリング
    rc = msgq_msg_recv(&msg, q);
    
    if (timeout != -1) break;
  }
  
  // メッセージ取得成功
  if (rc > 0) {
    MSGQMessage *r = new MSGQMessage;
    r->takeOwnership(msg.data, msg.size);  // ゼロコピー
    return r;
  }
  
  return NULL;
}
```

---

### MSGQPubSocket

```cpp
class MSGQPubSocket : public PubSocket {
private:
  msgq_queue_t * q = NULL;
public:
  int connect(Context *context, std::string endpoint, 
              bool check_endpoint=true);
  int send(char *data, size_t size);
  bool all_readers_updated();  // 実装あり
};
```

**connect()の動作**:
1. `msgq_new_queue()`で共有メモリキュー作成
2. `msgq_init_publisher()`でPublisher初期化
3. 複数Publisher検出（`EEXIST`）

**send()の動作**:
1. 共有メモリにデータ書き込み
2. `msgq_msg_send()`でメッセージキューに追加
3. カウンタインクリメント（CAS）

**all_readers_updated()**:
```cpp
bool MSGQPubSocket::all_readers_updated() {
  return msgq_all_readers_updated(q);
}

// msgq.cc内
bool msgq_all_readers_updated(msgq_queue_t *q) {
  msgq_header_t *h = q->mmap_p;
  
  // 全Subscriberが最新メッセージを読んだかチェック
  for (int i = 0; i < h->num_readers; i++) {
    if (h->read_pointers[i] < h->write_pointer - 1) {
      return false;  // 未読あり
    }
  }
  return true;  // 全て読了
}
```

---

### msgqの特徴

**メリット**:
- 超低遅延 ~10μs
- ゼロコピー（共有メモリ直接アクセス）
- `all_readers_updated()`実装あり
- ロックフリー（高スループット）

**デメリット**:
- ローカルのみ（ネットワーク不可）
- カスタム実装（保守性）
- 共有メモリ管理必要

---

## 3. impl_fake - テスト用実装

**ファイル**: `impl_fake.h`, `impl_fake.cc`, `event.h`, `event.cc`

### 概要

**テスト/リプレイ用**のイベント駆動実装:

- **ベース**: ZMQ/msgqのラッパー
- **特徴**: 受信タイミング制御、決定論的実行
- **用途**: テスト、ログリプレイ

---

### FakeSubSocket

```cpp
template<typename TSubSocket>
class FakeSubSocket: public TSubSocket {
private:
  Event *recv_called = nullptr;  // receive()呼び出し検出
  Event *recv_ready = nullptr;   // メッセージ配信制御
  EventState *state = nullptr;   // 共有メモリ状態
public:
  int connect(Context *context, std::string endpoint, 
              std::string address, bool conflate=false, 
              bool check_endpoint=true) override;
  Message *receive(bool non_blocking=false) override;
};
```

**connect()の動作**:
1. 環境変数`CEREAL_FAKE_PREFIX`取得（リプレイID）
2. `event_state_shm_mmap()`でイベント状態共有メモリ作成
3. `recv_called`/`recv_ready`イベント初期化
4. ベース実装の`connect()`呼び出し

**receive()の動作**:
```cpp
Message *FakeSubSocket::receive(bool non_blocking) {
  if (this->state->enabled) {
    this->recv_called->set();      // 「receive()呼ばれた」通知
    this->recv_ready->wait();      // 「メッセージ配信」待機
    this->recv_ready->clear();
  }
  
  return TSubSocket::receive(non_blocking);  // 実際の受信
}
```

**用途**:
```python
# リプレイ側
from openpilot.msgq import toggle_fake_events, set_fake_prefix

toggle_fake_events(True)
set_fake_prefix("replay_12345")

# SubSocketは自動的にFakeSubSocketに
sm = messaging.SubMaster(['carState'])

# テスト側でタイミング制御
handle = fake_event_handle("carState", "replay_12345")

# receive()呼び出しを待つ
handle.recv_called_event.wait()

# メッセージ配信
handle.recv_ready_event.set()
```

---

### SocketEventHandle

**イベント制御ハンドル**:

```python
from openpilot.msgq import fake_event_handle

handle = fake_event_handle("carState", 
                           identifier="replay_123", 
                           override=True, 
                           enable=True)

# receive()呼び出し検出
handle.recv_called_event.wait()
print("carState.receive() was called")

# メッセージ配信
handle.recv_ready_event.set()
print("Message delivered")

# 有効/無効切り替え
handle.enabled = False  # Fakeモード無効化
```

---

### Fakeの特徴

**メリット**:
- 決定論的実行（テスト再現性）
- タイミング完全制御
- リプレイ正確な再現

**デメリット**:
- 本番環境では不要
- オーバーヘッド増加
- 設定複雑

---

## 実装の選択

### 環境変数で切り替え

```cpp
// ipc.cc
bool messaging_use_zmq() {
  return getenv("ZMQ") != nullptr;
}

Context *Context::create() {
  if (messaging_use_zmq()) {
    return new ZMQContext();  // ZMQ実装
  } else {
    return new MSGQContext(); // msgq実装
  }
}
```

**使い方**:
```bash
# msgq使用（デフォルト）
./selfdrive/manager/manager.py

# ZMQ使用
ZMQ=1 ./selfdrive/manager/manager.py
```

---

### Fake実装の有効化

```python
from openpilot.msgq import toggle_fake_events, set_fake_prefix

# Fakeモード有効化
toggle_fake_events(True)
set_fake_prefix("replay_12345")

# 以降、SubSocketは自動的にFakeSubSocketに
```

**内部動作**:
```cpp
// impl_fake.cc
SubSocket *SubSocket::create() {
  if (SocketEventHandle::is_fake_enabled()) {
    if (messaging_use_zmq()) {
      return new FakeSubSocket<ZMQSubSocket>();
    } else {
      return new FakeSubSocket<MSGQSubSocket>();
    }
  } else {
    if (messaging_use_zmq()) {
      return new ZMQSubSocket();
    } else {
      return new MSGQSubSocket();
    }
  }
}
```

---

## 実装別の使い分け

### ZMQを使う場合

**シナリオ**:
- ネットワーク経由通信（マシン間）
- 開発環境（PC）
- 汎用性が必要

**例**:
```bash
# PC開発環境
ZMQ=1 python selfdrive/controls/controlsd.py
```

---

### msgqを使う場合

**シナリオ**:
- 本番環境（comma device）
- 低遅延が必要（controlsd, boardd）
- 同一マシン内通信

**例**:
```bash
# comma device本番環境
./selfdrive/manager/manager.py  # ZMQ環境変数なし
```

---

### Fakeを使う場合

**シナリオ**:
- テストコード
- ログリプレイ
- タイミング制御が必要

**例**:
```python
# test_controlsd.py
from openpilot.msgq import toggle_fake_events

toggle_fake_events(True)
# テストコード
```

---

## パフォーマンス比較

### レイテンシ測定

```python
import time
from openpilot.msgq import pub_sock, sub_sock

# Publisher/Subscriber作成
pub = pub_sock("perfTest")
sub = sub_sock("perfTest", conflate=True, timeout=100)

# レイテンシ測定
iterations = 10000
start = time.perf_counter()

for i in range(iterations):
  pub.send(b"test")
  data = sub.receive()

elapsed = time.perf_counter() - start
latency = (elapsed / iterations) * 1e6  # μs

print(f"Latency: {latency:.2f} μs")
```

**結果**:
- **msgq**: ~10μs
- **ZMQ**: ~100μs
- **Fake**: ~100μs（テスト用、性能不要）

---

## トラブルシューティング

### 問題1: ZMQ環境変数の影響

**症状**: `all_readers_updated()`が常にtrue

**原因**: ZMQ実装使用中

**解決策**: msgq実装に切り替え
```bash
unset ZMQ
```

### 問題2: 共有メモリ残留

**症状**: msgq起動失敗

**原因**: `/dev/shm/`に古い共有メモリ残留

**解決策**: クリーンアップ
```bash
rm -f /dev/shm/msgq_*
```

### 問題3: Fakeモードが無効化されない

**症状**: テスト後もFakeモード継続

**原因**: `toggle_fake_events(False)`忘れ

**解決策**: 明示的に無効化
```python
from openpilot.msgq import toggle_fake_events

toggle_fake_events(False)
```

---

## まとめ

実装選択のポイント:

1. **本番環境**: msgq（高速、低遅延）
2. **開発/PC**: ZMQ（汎用、ネットワーク対応）
3. **テスト**: Fake（タイミング制御、リプレイ）
4. **切り替え**: 環境変数`ZMQ`で簡単

---

## 関連ドキュメント

- [README.md](README.md) - msgq全体の概要
- [ipc.md](ipc.md) - IPC API詳細

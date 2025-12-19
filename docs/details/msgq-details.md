# msgq（メッセージキュー）詳細

このドキュメントでは、openpilotのプロセス間通信システム「msgq」の実装詳細を説明します。

> **📖 関連ドキュメント**
> - [実行フロー概要](../execution-flow-ja.md) - 全体の流れ
> - [Manager詳細](manager-details.md) - プロセス管理

## 1. msgqの概要

**msgq**（message queue）は、openpilot独自の高速プロセス間通信システムです。

### 1.1 特徴

- **共有メモリベース**: `/dev/shm`を使用（カーネル空間を経由しない）
- **リングバッファ構造**: 固定サイズの循環バッファ
- **ゼロコピー**: 大きなデータでもコピーせずにポインタ渡し
- **Pub/Subモデル**: 1対多の配信が可能
- **型安全**: Cap'n Protoによるスキーマ定義

### 1.2 アーキテクチャ

```
┌──────────┐                         ┌──────────┐
│Publisher │                         │Subscriber│
│(modeld)  │                         │(controlsd)│
└────┬─────┘                         └────┬─────┘
     │ PubMaster                          │ SubMaster
     │                                    │
     ▼                                    ▼
┌─────────────────────────────────────────────┐
│      共有メモリ (/dev/shm/)                  │
│                                             │
│  ┌──────────────────────────────┐          │
│  │ modelV2 リングバッファ        │          │
│  │  [msg0][msg1][msg2]...[msgN] │          │
│  │       ↑write    ↑read         │          │
│  └──────────────────────────────┘          │
└─────────────────────────────────────────────┘
```

## 2. 実装詳細

### 2.1 ファイル構成

| パス | 説明 |
|------|------|
| [msgq/msgq.cc](../../msgq/msgq.cc) | C++実装（メイン） |
| [msgq/msgq.h](../../msgq/msgq.h) | C++ヘッダー |
| [cereal/messaging/messaging.py](../../cereal/messaging/messaging.py) | Pythonバインディング |
| [cereal/messaging/messaging.pyx](../../cereal/messaging/messaging.pyx) | Cythonラッパー |
| [cereal/log.capnp](../../cereal/log.capnp) | メッセージスキーマ定義 |

### 2.2 共有メモリの構造

```
/dev/shm/[topic_name]
├─ メタデータ
│   ├─ write_pointer (書き込み位置)
│   ├─ read_pointer (読み取り位置)
│   ├─ num_readers (購読者数)
│   └─ buffer_size (バッファサイズ)
│
└─ リングバッファ
    ├─ [slot 0] メッセージ1
    ├─ [slot 1] メッセージ2
    ├─ [slot 2] メッセージ3
    ...
    └─ [slot N-1] メッセージN
```

**リングバッファの動作**:
```
初期状態:
write=0, read=0
[___][___][___][___]
 ↑w,r

メッセージ書き込み1:
write=1, read=0
[msg][___][___][___]
 ↑r  ↑w

メッセージ書き込み2:
write=2, read=0
[msg][msg][___][___]
 ↑r       ↑w

読み取り:
write=2, read=2
[msg][msg][___][___]
           ↑w,r

バッファフル（ループ）:
write=0, read=0
[mg4][mg1][mg2][mg3]
 ↑w,r
 （古いmsg0は上書きされる）
```

### 2.3 PubMaster（Publisher側）

```python
class PubMaster:
    def __init__(self, topics: list[str]):
        self.topics = {t: PubSocket(t) for t in topics}

    def send(self, topic: str, msg):
        self.topics[topic].send(msg.to_bytes())
```

**内部動作（C++）**:
```cpp
class PubSocket {
  int send(void* data, size_t len) {
    // 1. 共有メモリへのポインタ取得
    void* slot = get_write_slot();

    // 2. メッセージをコピー
    memcpy(slot, data, len);

    // 3. write_pointerを進める
    advance_write_pointer();

    // 4. 購読者に通知（futex等）
    notify_readers();

    return len;
  }
};
```

### 2.4 SubMaster（Subscriber側）

```python
class SubMaster:
    def __init__(self, topics: list[str], poll: str = None):
        self.topics = {t: SubSocket(t) for t in topics}
        self.poll_topic = poll  # 優先的に監視するトピック
        self.updated = {t: False for t in topics}
        self.rcv_time = {t: 0 for t in topics}
        self.data = {}

    def update(self, timeout: int = 1000):
        """新しいメッセージを取得"""
        # poll_topicを優先的に待機
        if self.poll_topic:
            self.topics[self.poll_topic].receive(timeout)

        # 全トピックの最新メッセージを取得
        for topic in self.topics:
            msg = self.topics[topic].receive(0)  # ノンブロッキング
            if msg:
                self.data[topic] = msg
                self.updated[topic] = True
                self.rcv_time[topic] = sec_since_boot()

    def __getitem__(self, topic: str):
        """メッセージへのアクセス"""
        return self.data[topic]

    def all_alive(self, topics: list[str] = None) -> bool:
        """指定したトピックが全て受信済みか"""
        topics = topics or self.topics.keys()
        return all(self.updated[t] for t in topics)
```

**内部動作（C++）**:
```cpp
class SubSocket {
  Message* receive(int timeout_ms) {
    // 1. 新しいメッセージがあるか確認
    if (read_pointer == write_pointer) {
      // メッセージなし: 待機（timeout指定）
      if (timeout_ms > 0) {
        wait_for_message(timeout_ms);
      }
      return nullptr;
    }

    // 2. read_pointerの位置からメッセージを読み取り
    Message* msg = parse_message(get_read_slot());

    // 3. read_pointerを進める
    advance_read_pointer();

    return msg;
  }
};
```

## 3. メッセージ定義（Cap'n Proto）

### 3.1 スキーマ定義

[cereal/log.capnp](../../cereal/log.capnp):

```capnp
struct CarState {
  vEgo @0 :Float32;           # 自車速度 (m/s)
  aEgo @1 :Float32;           # 自車加速度 (m/s²)
  steeringAngleDeg @2 :Float32;  # ステアリング角度 (度)
  steeringTorque @3 :Float32;    # ステアリングトルク (Nm)
  brakePressed @4 :Bool;         # ブレーキペダル
  gasPressed @5 :Bool;           # アクセルペダル
  # ... その他多数のフィールド
}

struct ModelDataV2 {
  struct Action {
    desiredCurvature @0 :Float32;      # 目標曲率
    desiredAcceleration @1 :Float32;   # 目標加速度
    shouldStop @2 :Bool;               # 停止フラグ
  }

  action @0 :Action;
  laneLines @1 :List(XYZTData);
  roadEdges @2 :List(XYZTData);
  # ... その他多数のフィールド
}
```

### 3.2 メッセージの作成と送信

```python
import cereal.messaging as messaging

# 新しいメッセージを作成
msg = messaging.new_message('carState')

# フィールドに値を設定
msg.carState.vEgo = 10.5
msg.carState.aEgo = 0.2
msg.carState.steeringAngleDeg = 5.0

# 送信
pm = messaging.PubMaster(['carState'])
pm.send('carState', msg)
```

### 3.3 メッセージの受信

```python
import cereal.messaging as messaging

# Subscriberを作成
sm = messaging.SubMaster(['carState', 'modelV2'])

while True:
    # 新しいメッセージを取得
    sm.update(1000)  # 1000msタイムアウト

    # メッセージにアクセス
    if sm.updated['carState']:
        v_ego = sm['carState'].vEgo
        print(f"速度: {v_ego} m/s")

    # 全メッセージが受信済みか確認
    if sm.all_alive(['carState', 'modelV2']):
        # 処理
        pass
```

## 4. パフォーマンス最適化

### 4.1 ゼロコピー

**通常のIPC（パイプやソケット）**:
```
送信側メモリ → カーネル空間 → 受信側メモリ
  (コピー1)      (コピー2)
```

**msgq（共有メモリ）**:
```
送信側 → 共有メモリ ← 受信側
  (書き込み)   (読み取り)
  ※コピーは1回のみ
```

### 4.2 バッチ処理

**SubMaster.update()の最適化**:
```python
def update(self, timeout: int = 1000):
    # 1回のシステムコールで複数トピックを確認
    ready_topics = poll_topics(self.topics.keys(), timeout)

    # 準備ができたトピックのみ読み取り
    for topic in ready_topics:
        msg = self.topics[topic].receive(0)
        if msg:
            self.data[topic] = msg
```

### 4.3 リングバッファサイズの調整

```python
# デフォルトサイズ（messages.py）
QUEUE_SIZE = {
    'carState': 100,        # 高頻度 → 大きめ
    'modelV2': 10,          # 低頻度 → 小さめ
    'controlsState': 100,   # 高頻度 → 大きめ
}
```

## 5. 使用例

### 5.1 基本的な使用パターン

```python
import cereal.messaging as messaging

def main():
    # Publisher/Subscriber作成
    pm = messaging.PubMaster(['myTopic'])
    sm = messaging.SubMaster(['carState'])

    while True:
        # メッセージ受信
        sm.update(100)

        # 処理
        v_ego = sm['carState'].vEgo
        result = process(v_ego)

        # メッセージ送信
        msg = messaging.new_message('myTopic')
        msg.myTopic.result = result
        pm.send('myTopic', msg)
```

### 5.2 複数トピックの監視

```python
def main():
    sm = messaging.SubMaster([
        'carState',
        'modelV2',
        'longitudinalPlan'
    ], poll='carState')  # carStateを優先的に監視

    while True:
        sm.update(15)  # 15msタイムアウト

        # 全メッセージが揃ったか確認
        if not sm.all_alive():
            continue

        # 処理
        CS = sm['carState']
        model = sm['modelV2']
        plan = sm['longitudinalPlan']

        control(CS, model, plan)
```

### 5.3 タイムスタンプの確認

```python
def main():
    sm = messaging.SubMaster(['carState', 'modelV2'])

    while True:
        sm.update()

        # 最後に受信した時刻
        car_state_time = sm.rcv_time['carState']
        model_time = sm.rcv_time['modelV2']

        # タイムスタンプの差を確認（同期チェック）
        if abs(car_state_time - model_time) > 0.1:  # 100ms以上の差
            print("WARNING: messages out of sync")
```

## 6. トラブルシューティング

### 6.1 メッセージが受信できない

**原因1**: Publisherが起動していない
```bash
# プロセスの確認
ps aux | grep modeld

# managerStateで確認
cat /dev/shm/managerState
```

**原因2**: トピック名の間違い
```python
# 正しいトピック名を確認
from cereal import log
print(log.Event.which())  # 全トピック一覧
```

**原因3**: バッファがフル（Publisherが速すぎる）
```python
# バッファサイズを増やす
QUEUE_SIZE['myTopic'] = 200  # デフォルト100から増やす
```

### 6.2 メッセージが古い

**原因**: update()の頻度が低い
```python
# 解決策: update()を高頻度で呼ぶ
while True:
    sm.update(10)  # 10msごと
    # 処理
```

### 6.3 共有メモリの確認

```bash
# 共有メモリファイルの確認
ls -lh /dev/shm/

# サイズと使用状況
df -h /dev/shm/

# 特定トピックの確認
hexdump -C /dev/shm/carState | head
```

## 7. まとめ

msgqはopenpilotの高速通信を支える基盤技術：

- **高速**: 共有メモリによるゼロコピー通信
- **型安全**: Cap'n Protoによるスキーマ定義
- **柔軟**: Pub/Subモデルで1対多配信
- **リアルタイム**: 低レイテンシ（マイクロ秒オーダー）

この設計により、openpilotは100Hzの制御ループを安定的に実行できます。

# VisionIPC - 画像共有システム

## 概要

**VisionIPC**は、カメラ画像を**ゼロコピー**で複数プロセス間共有するシステムです。

- **ファイル**: `visionipc/visionipc.h/cc`, `visionipc_pyx.pyx`
- **用途**: カメラ画像の高速配信（camerad → modeld/dmonitoringd）
- **特徴**: 共有メモリ、OpenCL対応、YUVフォーマット
- **性能**: 20MB画像を20Hz（400MB/s）

---

## アーキテクチャ

```
┌──────────────────────────────────────────────┐
│              VisionIPC                       │
├──────────────────────────────────────────────┤
│                                              │
│  ┌────────────────┐      ┌────────────────┐ │
│  │ VisionIpcServer│      │VisionIpcClient │ │
│  │  (camerad)     │      │ (modeld等)     │ │
│  │                │      │                │ │
│  │ - buffers[4]   │      │ - recv()       │ │
│  │ - send()       │      │ - VisionBuf    │ │
│  └────────┬───────┘      └───────┬────────┘ │
│           │                      │          │
│           │  共有メモリ(ion/dmabuf)│          │
│           │ ┌──────────────────┐ │          │
│           └─│  VisionBuf[0-3]  │─┘          │
│             │  (1928x1208 YUV) │            │
│             └──────────────────┘            │
│                                              │
│  ストリームタイプ:                             │
│  - VISION_STREAM_ROAD (前方カメラ)            │
│  - VISION_STREAM_DRIVER (運転者監視)          │
│  - VISION_STREAM_WIDE_ROAD (広角)            │
│                                              │
└──────────────────────────────────────────────┘
```

---

## 主要クラス

### 1. VisionBuf - 画像バッファ

**共有メモリ上の画像データ**:

```cpp
class VisionBuf {
public:
  size_t len = 0;        // バッファサイズ
  void * addr = nullptr; // メモリアドレス
  int fd = 0;            // ファイルディスクリプタ
  
  // 画像サイズ
  size_t width = 0;
  size_t height = 0;
  size_t stride = 0;
  size_t uv_offset = 0;
  
  // YUVポインタ
  uint8_t * y = nullptr;
  uint8_t * uv = nullptr;
  
  // OpenCL
  cl_mem buf_cl = nullptr;
  
  // メソッド
  void allocate(size_t len);
  void init_yuv(size_t width, size_t height, size_t stride, size_t uv_offset);
  void sync(int dir);  // CPU ↔ GPU同期
};
```

**Python API**:
```python
from openpilot.msgq.visionipc import VisionBuf

# Clientから受信
client = VisionIpcClient("camerad", VisionStreamType.VISION_STREAM_ROAD, True)
buf = client.recv()

if buf is not None:
  # 画像プロパティ
  width = buf.width        # 1928
  height = buf.height      # 1208
  stride = buf.stride      # 1928
  uv_offset = buf.uv_offset  # YUVのUV開始位置
  
  # NumPy配列として取得
  img_data = buf.data  # np.ndarray
  
  # YUV → RGB変換
  yuv = np.frombuffer(buf.data, dtype=np.uint8)
  y = yuv[:height * stride].reshape((height, stride))
  uv = yuv[uv_offset:uv_offset + height//2 * stride].reshape((height//2, stride))
```

---

### 2. VisionIpcServer - 画像送信側

**カメラプロセス（camerad）**:

```python
from openpilot.msgq.visionipc import VisionIpcServer, VisionStreamType

# Server作成
server = VisionIpcServer("camerad")

# バッファ作成（4個、1928x1208）
server.create_buffers(VisionStreamType.VISION_STREAM_ROAD, 
                      num_buffers=4, 
                      width=1928, 
                      height=1208)

# リスナー起動
server.start_listener()

# 画像送信ループ
frame_id = 0
while True:
  # カメラからキャプチャ
  frame_data = capture_frame()  # bytes（YUVデータ）
  
  # 送信
  server.send(VisionStreamType.VISION_STREAM_ROAD, 
              frame_data, 
              frame_id=frame_id,
              timestamp_sof=get_timestamp(),
              timestamp_eof=get_timestamp())
  
  frame_id += 1
  time.sleep(0.05)  # 20Hz
```

**C++ API**:
```cpp
#include "msgq/visionipc/visionipc_server.h"

VisionIpcServer server("camerad");

// バッファ作成
server.create_buffers(VISION_STREAM_ROAD, 4, 1928, 1208);
server.start_listener();

// 画像送信
VisionBuf *buf = server.get_buffer(VISION_STREAM_ROAD);
memcpy(buf->addr, frame_data, buf->len);

VisionIpcBufExtra extra;
extra.frame_id = frame_id;
extra.timestamp_sof = timestamp_sof;
extra.timestamp_eof = timestamp_eof;

server.send(buf, &extra, true);
```

---

### 2.1 VisionIpcServer API

#### `create_buffers(type, num_buffers, width, height)`

バッファ作成

```python
server.create_buffers(VisionStreamType.VISION_STREAM_ROAD, 
                      num_buffers=4,  # 4バッファ（リングバッファ）
                      width=1928, 
                      height=1208)
```

**パラメータ**:
- `type`: ストリームタイプ
- `num_buffers`: バッファ数（通常4）
- `width`, `height`: 画像サイズ

**自動計算**:
```cpp
// YUVサイズ = width * height * 1.5
size_t size = width * height * 3 / 2;
size_t stride = width;
size_t uv_offset = height * stride;
```

---

#### `create_buffers_with_sizes(type, num_buffers, width, height, size, stride, uv_offset)`

カスタムサイズでバッファ作成

```python
# カスタムストライド
server.create_buffers_with_sizes(
  VisionStreamType.VISION_STREAM_ROAD,
  num_buffers=4,
  width=1928,
  height=1208,
  size=3500000,  # カスタムサイズ
  stride=2048,   # アライメント調整
  uv_offset=2478080
)
```

---

#### `send(type, data, frame_id, timestamp_sof, timestamp_eof)`

画像送信

```python
server.send(VisionStreamType.VISION_STREAM_ROAD,
            frame_data,         # bytes（YUVデータ）
            frame_id=123,       # フレームID
            timestamp_sof=12345,  # Start of Frame
            timestamp_eof=12350)  # End of Frame
```

**内部動作**:
1. 空きバッファ取得（リングバッファ）
2. データコピー
3. `frame_id`設定
4. IPC経由で通知（PubSocket）

---

#### `start_listener()`

Client接続を待ち受け

```python
server.start_listener()
```

**役割**:
- Clientからの接続要求を監視
- バッファのファイルディスクリプタを送信
- バックグラウンドスレッドで動作

---

### 3. VisionIpcClient - 画像受信側

**Vision系プロセス（modeld, dmonitoringd）**:

```python
from openpilot.msgq.visionipc import VisionIpcClient, VisionStreamType

# Client作成
client = VisionIpcClient("camerad", 
                        VisionStreamType.VISION_STREAM_ROAD, 
                        conflate=True)  # 最新のみ

# メインループ
while True:
  # 画像受信（タイムアウト100ms）
  buf = client.recv(timeout_ms=100)
  
  if buf is None:
    print("Timeout!")
    continue
  
  # 画像処理
  process_frame(buf.data, 
                frame_id=client.frame_id,
                timestamp_sof=client.timestamp_sof)
```

**C++ API**:
```cpp
#include "msgq/visionipc/visionipc_client.h"

VisionIpcClient client("camerad", VISION_STREAM_ROAD, true);

VisionIpcBufExtra extra;
VisionBuf *buf = client.recv(&extra, 100);  // 100ms timeout

if (buf != nullptr) {
  // 画像処理
  process(buf->addr, buf->len);
  
  printf("Frame ID: %u\n", extra.frame_id);
  printf("Timestamp: %lu\n", extra.timestamp_sof);
}
```

---

### 3.1 VisionIpcClient API

#### `recv(timeout_ms: int) -> VisionBuf | None`

画像受信

```python
buf = client.recv(timeout_ms=100)

if buf is not None:
  # 画像データ
  img = np.frombuffer(buf.data, dtype=np.uint8)
  
  # メタデータ
  frame_id = client.frame_id
  timestamp_sof = client.timestamp_sof
  timestamp_eof = client.timestamp_eof
  valid = client.valid
```

**戻り値**:
- `VisionBuf`: 画像バッファ
- `None`: タイムアウトまたは切断

---

#### `connect(blocking: bool) -> bool`

Server接続

```python
# ブロッキング接続（Serverが起動するまで待つ）
if client.connect(blocking=True):
  print("Connected!")

# 非ブロッキング接続（すぐ戻る）
if not client.connect(blocking=False):
  print("Server not ready")
```

---

#### `is_connected() -> bool`

接続状態確認

```python
if client.is_connected():
  buf = client.recv()
else:
  print("Disconnected, reconnecting...")
  client.connect(blocking=True)
```

---

#### `available_streams(name: str, block: bool) -> Set[VisionStreamType]`

利用可能なストリーム取得（静的メソッド）

```python
from openpilot.msgq.visionipc import VisionIpcClient

# 利用可能なストリーム確認
streams = VisionIpcClient.available_streams("camerad", block=True)

if VisionStreamType.VISION_STREAM_ROAD in streams:
  print("Road camera available")
if VisionStreamType.VISION_STREAM_DRIVER in streams:
  print("Driver camera available")
```

---

## ストリームタイプ

### VisionStreamType

```python
from openpilot.msgq.visionipc import VisionStreamType

# 前方カメラ（メイン）
VisionStreamType.VISION_STREAM_ROAD

# 運転者監視カメラ
VisionStreamType.VISION_STREAM_DRIVER

# 広角カメラ
VisionStreamType.VISION_STREAM_WIDE_ROAD

# マップ（未使用）
VisionStreamType.VISION_STREAM_MAP
```

### 画像サイズ

| ストリーム | 解像度 | サイズ（YUV） |
|-----------|-------|-------------|
| ROAD | 1928x1208 | ~3.5MB |
| DRIVER | 1928x1208 | ~3.5MB |
| WIDE_ROAD | 1928x1208 | ~3.5MB |

---

## YUVフォーマット

### YUV420 レイアウト

```
┌─────────────────────┐
│  Y (輝度)            │  height × stride
│  1928 × 1208        │
├─────────────────────┤ ← uv_offset
│  UV (色差)           │  (height/2) × stride
│  1928 × 604         │
└─────────────────────┘

合計: 1928 × 1208 × 1.5 = 3,493,440 bytes
```

### Python: YUV → RGB変換

```python
import cv2
import numpy as np

def yuv_to_rgb(buf):
  """VisionBuf YUV → RGB変換"""
  yuv_data = np.frombuffer(buf.data, dtype=np.uint8)
  
  # YUV420 → BGR
  yuv = yuv_data.reshape((buf.height * 3 // 2, buf.stride))
  bgr = cv2.cvtColor(yuv, cv2.COLOR_YUV2BGR_I420)
  
  # BGR → RGB
  rgb = cv2.cvtColor(bgr, cv2.COLOR_BGR2RGB)
  
  return rgb

# 使用
buf = client.recv()
if buf is not None:
  rgb_img = yuv_to_rgb(buf)
  # rgb_img: (1208, 1928, 3) RGB画像
```

---

## OpenCL連携

### GPU処理

```python
from openpilot.msgq.visionipc import VisionIpcClient, CLContext

# OpenCLコンテキスト作成
cl_ctx = CLContext()

# Client作成（OpenCL対応）
client = VisionIpcClient("camerad", 
                        VisionStreamType.VISION_STREAM_ROAD, 
                        conflate=True,
                        context=cl_ctx)

# 画像受信
buf = client.recv()

# buf.buf_cl がOpenCLメモリオブジェクト
# GPUで直接処理可能（CPUコピー不要）
```

**メリット**:
- ゼロコピー（CPU ↔ GPU転送不要）
- 高速処理（modeldのGPU推論）

---

## 実際の使用例

### 例1: 基本的な画像受信

```python
from openpilot.msgq.visionipc import VisionIpcClient, VisionStreamType
import numpy as np

client = VisionIpcClient("camerad", 
                        VisionStreamType.VISION_STREAM_ROAD, 
                        True)

while True:
  buf = client.recv(timeout_ms=100)
  
  if buf is None:
    print("No frame")
    continue
  
  print(f"Frame {client.frame_id}: {buf.width}x{buf.height}")
  
  # 画像データ
  img_data = np.frombuffer(buf.data, dtype=np.uint8)
  print(f"Data size: {len(img_data)} bytes")
```

### 例2: 複数ストリーム受信

```python
from openpilot.msgq.visionipc import VisionIpcClient, VisionStreamType

# 前方カメラ
road_client = VisionIpcClient("camerad", 
                             VisionStreamType.VISION_STREAM_ROAD, 
                             True)

# 運転者監視カメラ
driver_client = VisionIpcClient("camerad", 
                               VisionStreamType.VISION_STREAM_DRIVER, 
                               True)

while True:
  # 両方受信
  road_buf = road_client.recv(timeout_ms=50)
  driver_buf = driver_client.recv(timeout_ms=50)
  
  if road_buf:
    process_road(road_buf)
  
  if driver_buf:
    process_driver(driver_buf)
```

### 例3: タイムスタンプ同期

```python
from openpilot.msgq.visionipc import VisionIpcClient, VisionStreamType

client = VisionIpcClient("camerad", 
                        VisionStreamType.VISION_STREAM_ROAD, 
                        True)

prev_timestamp = 0

while True:
  buf = client.recv()
  
  if buf is None:
    continue
  
  # フレーム間隔計算
  dt = (client.timestamp_sof - prev_timestamp) / 1e9  # 秒
  fps = 1.0 / dt if dt > 0 else 0
  
  print(f"Frame {client.frame_id}: {fps:.1f} FPS")
  
  prev_timestamp = client.timestamp_sof
```

### 例4: Server側（カメラシミュレータ）

```python
from openpilot.msgq.visionipc import VisionIpcServer, VisionStreamType
import numpy as np
import time

server = VisionIpcServer("test_camera")

# バッファ作成
server.create_buffers(VisionStreamType.VISION_STREAM_ROAD, 
                      num_buffers=4, 
                      width=1928, 
                      height=1208)

server.start_listener()

frame_id = 0

while True:
  # ダミー画像生成（グレースケール）
  y_size = 1928 * 1208
  uv_size = 1928 * 604
  
  y_data = np.full(y_size, 128, dtype=np.uint8)  # グレー
  uv_data = np.full(uv_size, 128, dtype=np.uint8)  # 色なし
  
  frame_data = np.concatenate([y_data, uv_data]).tobytes()
  
  # 送信
  timestamp = int(time.time() * 1e9)
  server.send(VisionStreamType.VISION_STREAM_ROAD,
              frame_data,
              frame_id=frame_id,
              timestamp_sof=timestamp,
              timestamp_eof=timestamp + 5000000)  # +5ms
  
  frame_id += 1
  time.sleep(0.05)  # 20Hz
```

---

## パフォーマンス

### スループット

```python
# 1フレーム: 1928 × 1208 × 1.5 = 3.5MB
# 頻度: 20Hz
# スループット: 3.5MB × 20 = 70MB/s

# 3ストリーム（road, driver, wide）
# 合計: 70MB/s × 3 = 210MB/s
```

### レイテンシ

```
camerad capture → VisionIPC send → modeld recv
    ~5ms              ~1ms             ~5ms
                  (ゼロコピー)
```

---

## ベストプラクティス

### 1. conflateの使用

```python
# ✅ リアルタイム処理: conflate=True
client = VisionIpcClient("camerad", VISION_STREAM_ROAD, conflate=True)

# ❌ ログ記録: conflate=False（非推奨、遅延増大）
```

### 2. タイムアウト設定

```python
# ✅ 適切なタイムアウト
buf = client.recv(timeout_ms=100)  # 100ms

# ❌ 無限待機
buf = client.recv(timeout_ms=-1)  # ハングの可能性
```

### 3. 接続状態確認

```python
# ✅ 再接続処理
if not client.is_connected():
  if not client.connect(blocking=False):
    time.sleep(0.1)
    continue

buf = client.recv()
```

---

## トラブルシューティング

### 問題1: "No frame" （bufがNone）

**原因**: Serverが起動していない、またはタイムアウト

**解決策**:
```python
# Server起動待ち
client.connect(blocking=True)

# タイムアウト延長
buf = client.recv(timeout_ms=1000)  # 1秒
```

### 問題2: 画像が壊れている

**原因**: サイズ不一致、YUVフォーマット誤り

**解決策**:
```python
# サイズ確認
assert len(frame_data) == client.buffer_len

# YUVレイアウト確認
print(f"Width: {client.width}, Height: {client.height}")
print(f"Stride: {client.stride}, UV offset: {client.uv_offset}")
```

### 問題3: メモリリーク

**原因**: VisionBuf参照保持

**解決策**:
```python
# ❌ 悪い例
bufs = []
while True:
  buf = client.recv()
  bufs.append(buf)  # メモリリーク！

# ✅ 良い例
while True:
  buf = client.recv()
  process(buf)
  # bufは自動的に解放
```

---

## まとめ

VisionIPCのポイント:

1. **ゼロコピー**: 共有メモリで高速転送
2. **YUV420**: カメラネイティブフォーマット
3. **複数ストリーム**: road/driver/wide
4. **OpenCL対応**: GPU処理可能
5. **20Hz配信**: 70MB/s × 3ストリーム

---

## 関連ドキュメント

- [README.md](README.md) - msgq全体の概要
- [ipc.md](ipc.md) - 基本的なIPC API（VisionIPCの基盤）

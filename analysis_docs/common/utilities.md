# Utilities - 汎用ユーティリティ

## 概要

`common/`の汎用ユーティリティ群:

- **util.py/cc/h**: 汎用ユーティリティ関数
- **file_helpers.py**: ファイル操作ヘルパー
- **retry.py**: リトライデコレータ
- **timeout.py**: タイムアウト処理
- **dict_helpers.py**: 辞書操作ヘルパー
- **time_helpers.py**: 時刻関連ユーティリティ

---

## 1. util.py - 汎用ユーティリティ

### MovingAverage

**移動平均フィルタ**:

```python
from openpilot.common.util import MovingAverage

# 10サンプルの移動平均
avg = MovingAverage(window=10)

for value in sensor_readings:
  avg.add(value)
  smoothed = avg.get()
  print(f"Raw: {value:.2f}, Smoothed: {smoothed:.2f}")
```

**内部実装**:
```python
class MovingAverage:
  def __init__(self, window):
    self.window = window
    self.values = []
  
  def add(self, value):
    self.values.append(value)
    if len(self.values) > self.window:
      self.values.pop(0)
  
  def get(self):
    return sum(self.values) / len(self.values) if self.values else 0.0
```

**特徴**:
- O(1) 追加、O(n) 平均計算
- メモリ使用量: window * sizeof(float)

---

### Ratelimiter

**頻度制限**:

```python
from openpilot.common.util import Ratelimiter

# 1秒に1回だけ実行
limiter = Ratelimiter(dt=1.0)

while True:
  if limiter.check():
    expensive_operation()  # 1秒に1回実行
  
  do_fast_work()  # 高頻度実行
  time.sleep(0.01)
```

**内部実装**:
```python
class Ratelimiter:
  def __init__(self, dt):
    self.dt = dt
    self.last_time = 0
  
  def check(self):
    now = time.monotonic()
    if now - self.last_time > self.dt:
      self.last_time = now
      return True
    return False
```

**用途**:
- ログの頻度制限
- ネットワーク通信の間引き
- UI更新の制限

---

### getenv

**環境変数取得（型変換付き）**:

```python
from openpilot.common.util import getenv

# 文字列
api_key = getenv("API_KEY", default="")

# 整数
max_workers = getenv("MAX_WORKERS", int, default=4)

# 浮動小数点数
timeout = getenv("TIMEOUT", float, default=5.0)

# ブール値
debug = getenv("DEBUG", bool, default=False)
```

**内部実装**:
```python
def getenv(key, cast=str, default=None):
  value = os.getenv(key)
  if value is None:
    return default
  
  if cast == bool:
    return value.lower() in ('true', '1', 'yes')
  else:
    return cast(value)
```

---

### clip

**値の範囲制限**:

```python
from openpilot.common.util import clip

# 範囲制限
value = clip(sensor_reading, 0, 100)

# 使用例
throttle = clip(pid_output, 0.0, 1.0)  # 0-1に制限
steer = clip(lateral_output, -1.0, 1.0)  # -1から1に制限
```

**実装**:
```python
def clip(x, lo, hi):
  return max(lo, min(hi, x))
```

---

### interp

**線形補間**:

```python
from openpilot.common.util import interp

# 1次元補間
x = [0, 10, 20, 30]
y = [0, 5, 15, 30]
result = interp(15, x, y)  # 10.0

# 速度依存ゲイン
speeds = [0, 10, 20, 30]
gains = [0.5, 0.8, 1.0, 1.2]
current_speed = 25
gain = interp(current_speed, speeds, gains)  # 1.1
```

**実装**: Numpyの`np.interp()`ラッパー

**用途**:
- 速度依存パラメータ
- ルックアップテーブル
- センサー補正

---

## 2. file_helpers.py - ファイル操作

### atomic_write_in_dir

**アトミック書き込み**:

```python
from openpilot.common.file_helpers import atomic_write_in_dir

# アトミックに書き込み（書き込み中にクラッシュしても破損しない）
atomic_write_in_dir("/data/params/d/DongleId", b"0123456789abcdef")
```

**内部動作**:
1. 一時ファイル作成: `/data/params/d/.tmp_DongleId_12345`
2. データ書き込み
3. `os.rename()`でアトミック置換

**メリット**:
- 読み取り中にファイルが壊れない
- 書き込み中にクラッシュしても古いファイルが残る

---

### mkdirs_exists_ok

**ディレクトリ作成（存在OK）**:

```python
from openpilot.common.file_helpers import mkdirs_exists_ok

# 既に存在していてもエラーにならない
mkdirs_exists_ok("/data/params/d/")
```

**実装**:
```python
def mkdirs_exists_ok(path):
  try:
    os.makedirs(path)
  except FileExistsError:
    pass
```

**Python 3.2+では不要**: `os.makedirs(path, exist_ok=True)`が推奨

---

## 3. retry.py - リトライデコレータ

### @retry

**自動リトライ**:

```python
from openpilot.common.retry import retry

@retry(attempts=3, delay=1.0)
def fetch_data_from_server():
  response = requests.get("https://api.comma.ai/v1/data")
  response.raise_for_status()
  return response.json()

# 使用（自動で3回までリトライ、1秒間隔）
try:
  data = fetch_data_from_server()
except Exception as e:
  print(f"Failed after 3 attempts: {e}")
```

**パラメータ**:
- `attempts`: リトライ回数（デフォルト: 3）
- `delay`: リトライ間隔（秒、デフォルト: 1.0）
- `exceptions`: リトライする例外タイプ（デフォルト: `Exception`）

**内部実装**:
```python
def retry(attempts=3, delay=1.0, exceptions=(Exception,)):
  def decorator(func):
    def wrapper(*args, **kwargs):
      for attempt in range(attempts):
        try:
          return func(*args, **kwargs)
        except exceptions as e:
          if attempt == attempts - 1:
            raise
          time.sleep(delay)
    return wrapper
  return decorator
```

---

## 4. timeout.py - タイムアウト処理

### timeout

**関数実行にタイムアウト設定**:

```python
from openpilot.common.timeout import timeout

@timeout(5.0)  # 5秒でタイムアウト
def slow_operation():
  time.sleep(10)  # 10秒かかる処理

try:
  slow_operation()
except TimeoutError:
  print("Operation timed out!")
```

**内部**: `signal.alarm()`使用（Unixのみ）

**注意**: マルチスレッドでは動作しない可能性

---

## 5. dict_helpers.py - 辞書操作

### strip_deprecated_keys

**非推奨キーの削除**:

```python
from openpilot.common.dict_helpers import strip_deprecated_keys

data = {
  "valid_key": 123,
  "DEPRECATED_old_key": 456,
  "another_valid": 789,
}

cleaned = strip_deprecated_keys(data)
# {"valid_key": 123, "another_valid": 789}
```

### deep_merge_dicts

**辞書の深いマージ**:

```python
from openpilot.common.dict_helpers import deep_merge_dicts

base_config = {
  "network": {"host": "0.0.0.0", "port": 8080},
  "logging": {"level": "INFO"},
}

user_config = {
  "network": {"port": 9000},  # ポートだけ上書き
  "features": {"new_feature": True},
}

merged = deep_merge_dicts(base_config, user_config)
# {
#   "network": {"host": "0.0.0.0", "port": 9000},
#   "logging": {"level": "INFO"},
#   "features": {"new_feature": True},
# }
```

---

## 6. time_helpers.py - 時刻ユーティリティ

### system_time_valid

**システム時刻が有効かチェック**:

```python
from openpilot.common.time_helpers import system_time_valid

if not system_time_valid():
  print("Waiting for NTP sync...")
  wait_for_time_sync()
```

**判定基準**: 2020年1月1日以降

**用途**: NTP同期前の処理スキップ

---

## 実際の使用例

### 例1：センサー値の平滑化

```python
from openpilot.common.util import MovingAverage

# GPS速度の移動平均（5サンプル）
gps_speed_avg = MovingAverage(5)

while True:
  raw_speed = get_gps_speed()
  gps_speed_avg.add(raw_speed)
  smoothed_speed = gps_speed_avg.get()
  
  print(f"GPS: {raw_speed:.2f} m/s, Smoothed: {smoothed_speed:.2f} m/s")
```

### 例2：ログの頻度制限

```python
from openpilot.common.util import Ratelimiter
from openpilot.common.swaglog import cloudlog

# 5秒に1回だけログ出力
log_limiter = Ratelimiter(dt=5.0)

while True:
  process_data()
  
  if log_limiter.check():
    cloudlog.info(f"Processed {count} items")
```

### 例3：パラメータのアトミック保存

```python
from openpilot.common.file_helpers import atomic_write_in_dir
import json

def save_config(config):
  """設定をアトミックに保存"""
  config_json = json.dumps(config).encode('utf-8')
  atomic_write_in_dir("/data/config.json", config_json)

# 使用
config = {"speed_limit": 25.0, "enabled": True}
save_config(config)
```

### 例4：ネットワークリトライ

```python
from openpilot.common.retry import retry
import requests

@retry(attempts=3, delay=2.0)
def upload_log(log_data):
  """ログをサーバーにアップロード（自動リトライ）"""
  response = requests.post("https://api.comma.ai/upload", json=log_data)
  response.raise_for_status()
  return response.json()

# 使用
try:
  result = upload_log({"event": "started", "time": 1234567890})
  print(f"Upload successful: {result}")
except Exception as e:
  print(f"Upload failed after 3 attempts: {e}")
```

### 例5：速度依存パラメータ

```python
from openpilot.common.util import interp

# 速度依存のステアリング比
speeds = [0, 10, 20, 30, 40]  # m/s
steer_ratios = [15.0, 14.5, 14.0, 13.5, 13.0]

def get_steer_ratio(v_ego):
  """現在速度からステアリング比を補間"""
  return interp(v_ego, speeds, steer_ratios)

# 使用
current_speed = 25.0  # m/s
ratio = get_steer_ratio(current_speed)
print(f"Steer ratio at {current_speed} m/s: {ratio:.2f}")  # 13.75
```

---

## C++ユーティリティ（util.cc/h）

### util_sleep

**精密スリープ**:

```cpp
#include "common/util.h"

// 100msスリープ
util_sleep(100);
```

### util_nanos

**ナノ秒タイムスタンプ**:

```cpp
#include "common/util.h"

uint64_t start = util_nanos();
// 処理
uint64_t elapsed = util_nanos() - start;
printf("Elapsed: %llu ns\n", elapsed);
```

### util_read_file

**ファイル全体読み込み**:

```cpp
#include "common/util.h"

std::string content = util_read_file("/proc/stat");
```

---

## ベストプラクティス

### 1. MovingAverageのウィンドウサイズ

```python
# ✅ 良い例
# 高頻度センサー（100Hz）→ 1秒分の平均
avg = MovingAverage(100)

# 低頻度センサー（1Hz）→ 10秒分の平均
avg = MovingAverage(10)

# ❌ 悪い例
avg = MovingAverage(1000)  # 大きすぎ（遅延、メモリ）
```

### 2. Ratelimiterの適切な使用

```python
# ✅ 良い例：高頻度ループで低頻度処理
limiter = Ratelimiter(dt=1.0)
while True:
  fast_work()  # 毎回実行
  if limiter.check():
    slow_work()  # 1秒に1回

# ❌ 悪い例：Ratelimiterで処理全体を制限
limiter = Ratelimiter(dt=0.01)
while True:
  if limiter.check():
    fast_work()  # time.sleep()を使うべき
```

### 3. retryの適切な設定

```python
# ✅ 良い例：ネットワーク通信
@retry(attempts=3, delay=2.0)
def network_request():
  ...

# ❌ 悪い例：ローカルファイル読み込み
@retry(attempts=10, delay=5.0)  # リトライ無意味、遅延大
def read_local_file():
  ...
```

---

## パフォーマンス

### MovingAverage

- **追加**: O(1)
- **取得**: O(n) - nはウィンドウサイズ
- **メモリ**: n * 8 bytes（float64）

**最適化例**:
```python
# 累積和を使ったO(1)取得
class FastMovingAverage:
  def __init__(self, window):
    self.window = window
    self.values = deque(maxlen=window)
    self.sum = 0.0
  
  def add(self, value):
    if len(self.values) == self.window:
      self.sum -= self.values[0]
    self.values.append(value)
    self.sum += value
  
  def get(self):
    return self.sum / len(self.values) if self.values else 0.0
```

---

## トラブルシューティング

### 問題1：MovingAverageが遅い

**症状**: 頻繁な`get()`呼び出しで遅延

**解決策**: 累積和版（上記の`FastMovingAverage`）を使用

### 問題2：atomic_write_in_dirでディスク容量不足

**原因**: 一時ファイル作成でディスクフル

**解決策**: 定期的な古いファイル削除

### 問題3：retryで永遠に待つ

**原因**: `delay`が大きすぎる

**解決策**: 適切な`delay`設定（1-3秒推奨）

---

## まとめ

ユーティリティのポイント:

1. **MovingAverage**: センサー値の平滑化
2. **Ratelimiter**: 頻度制限
3. **atomic_write**: 安全なファイル書き込み
4. **retry**: ネットワーク通信の自動リトライ
5. **interp**: 線形補間

---

## 関連ドキュメント

- [README.md](README.md) - common全体の概要
- [control_algorithms.md](control_algorithms.md) - MovingAverageの代替としてFirstOrderFilter

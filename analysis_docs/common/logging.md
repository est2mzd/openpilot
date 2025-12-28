# Logging - ロギングシステム

## 概要

openpilotのロギングシステムは、**統一されたログ管理**を提供します。

- **swaglog**: 統一ロギングシステム
- **watchdog**: プロセス生存監視
- **logging_extra**: ログフォーマッター

---

## 1. swaglog - 統一ロギング

**ファイル**: `swaglog.py`, `swaglog.cc/h`

### 機能

1. **ファイルローテーション**: 60秒または256KB毎に新ファイル
2. **ZMQ転送**: athena（comma server）にアップロード
3. **レベル別ログ**: DEBUG, INFO, WARNING, ERROR, EXCEPTION
4. **自動管理**: 最大2500ファイル保持

---

### 基本的な使い方

```python
from openpilot.common.swaglog import cloudlog

# 各レベルのログ
cloudlog.debug("Debug information")
cloudlog.info("System started")
cloudlog.warning(f"Temperature high: {temp}°C")
cloudlog.error("Critical failure!")

# 例外ログ
try:
  dangerous_operation()
except Exception:
  cloudlog.exception("Operation failed")  # トレースバック付き
```

---

### ログレベル

| レベル | 関数 | 用途 |
|--------|------|------|
| DEBUG | `cloudlog.debug()` | デバッグ情報 |
| INFO | `cloudlog.info()` | 一般情報 |
| WARNING | `cloudlog.warning()` | 警告 |
| ERROR | `cloudlog.error()` | エラー |
| EXCEPTION | `cloudlog.exception()` | 例外（トレースバック付き） |

---

### ログファイル

**保存先**:
```
/data/media/0/realdata/swaglog/
├── swaglog.0000000001
├── swaglog.0000000002
├── swaglog.0000000003
└── ...
```

**ローテーション条件**:
- 60秒経過
- ファイルサイズ256KB超過

**最大保持数**: 2500ファイル

**古いファイル削除**: 自動（2500個を超えると古いものから削除）

---

### 実際の使用例

#### 例1：プロセス起動ログ

```python
from openpilot.common.swaglog import cloudlog

def controlsd_thread():
  cloudlog.info("controlsd started")
  
  try:
    main_loop()
  except Exception:
    cloudlog.exception("controlsd crashed")
  finally:
    cloudlog.info("controlsd stopped")
```

#### 例2：警告とエラー

```python
from openpilot.common.swaglog import cloudlog

def check_temperature():
  temp = get_cpu_temp()
  
  if temp > 80:
    cloudlog.warning(f"High temperature: {temp}°C")
  
  if temp > 90:
    cloudlog.error(f"Critical temperature: {temp}°C, shutting down")
    initiate_shutdown()
```

#### 例3：デバッグログ

```python
from openpilot.common.swaglog import cloudlog

def debug_control_loop(v_ego, accel_cmd):
  cloudlog.debug(f"Control: v_ego={v_ego:.2f}, accel={accel_cmd:.2f}")
```

**注意**: DEBUGログは本番環境では無効化されることが多い

---

### SwaglogRotatingFileHandler

**内部クラス**: ファイルローテーション管理

```python
class SwaglogRotatingFileHandler:
  def __init__(self, 
               base_filename, 
               interval=60,         # 60秒毎
               max_bytes=1024*256,  # 256KB
               backup_count=2500,   # 最大2500ファイル
               encoding=None):
```

**動作**:
1. 60秒経過または256KB超過でローテーション
2. 新ファイル作成: `swaglog.0000000123`
3. 2500ファイル超過時、古いファイル削除

---

### UnixDomainSocketHandler

**ZMQ経由でathenaに送信**:

```python
class UnixDomainSocketHandler:
  def connect(self):
    self.zctx = zmq.Context()
    self.sock = self.zctx.socket(zmq.PUSH)
    self.sock.connect(Paths.swaglog_ipc())
```

**動作**:
- ログをZMQソケットに送信
- `athenad`がまとめてcomma serverにアップロード

---

## 2. watchdog - プロセス監視

**ファイル**: `watchdog.py`, `watchdog.cc/h`

### 概要

**目的**: プロセスの生存確認（heartbeat）

**仕組み**:
- プロセスが定期的に`kick_watchdog()`を呼ぶ
- 呼ばれなくなった = プロセスがハング

---

### kick_watchdog()

```python
from openpilot.common.watchdog import kick_watchdog

while True:
  # 定期的にkick（1秒に1回）
  kick_watchdog()
  
  # 処理
  do_work()
  time.sleep(0.1)
```

**内部動作**:
1. 現在時刻をファイルに書き込み: `/dev/shm/wd_{pid}`
2. 1秒に1回だけ更新（高頻度呼び出しを抑制）

---

### 監視側（manager）

```python
# manager がwatchdogファイルをチェック
def check_watchdog(pid, timeout=5.0):
  watchdog_path = f"/dev/shm/wd_{pid}"
  
  if not os.path.exists(watchdog_path):
    return False  # ファイルなし = プロセス起動していない
  
  with open(watchdog_path, 'rb') as f:
    last_kick = struct.unpack('<Q', f.read())[0] / 1e9
  
  # 最後のkickから5秒以上経過
  if time.monotonic() - last_kick > timeout:
    return False  # ハング検出
  
  return True  # 正常
```

---

### 実際の使用例

```python
from openpilot.common.watchdog import kick_watchdog
from openpilot.common.realtime import Ratekeeper

def monitored_process():
  rk = Ratekeeper(100)
  
  while True:
    # 処理
    do_control()
    
    # watchdog kick（1秒に1回自動的に）
    kick_watchdog()
    
    # 100Hz維持
    rk.keep_time()
```

---

## 3. logging_extra - ログフォーマッター

**ファイル**: `logging_extra.py`

### SwagLogger

**Pythonロギングの拡張**:

```python
class SwagLogger(logging.Logger):
  def __init__(self, name):
    logging.Logger.__init__(self, name)
```

### SwagFormatter

**ログフォーマット**:

```
2025-12-21 12:34:56,789 INFO: System started
```

---

## ログの確認

### ログファイル確認

```bash
# 最新ログ
tail -f /data/media/0/realdata/swaglog/swaglog.*

# 全ログ検索
grep "ERROR" /data/media/0/realdata/swaglog/swaglog.*

# ログ数確認
ls -l /data/media/0/realdata/swaglog/ | wc -l
```

### Pythonでログ読み込み

```python
import glob

log_files = sorted(glob.glob("/data/media/0/realdata/swaglog/swaglog.*"))

# 最新100行
with open(log_files[-1]) as f:
  lines = f.readlines()
  for line in lines[-100:]:
    print(line, end='')
```

---

## ベストプラクティス

### 1. 適切なログレベル

```python
# ✅ 良い例
cloudlog.debug("Loop iteration 12345")      # 頻繁、詳細
cloudlog.info("System started")             # 重要イベント
cloudlog.warning("Temperature high: 85°C")  # 警告
cloudlog.error("Cannot connect to device")  # エラー
cloudlog.exception("Unexpected crash")      # 例外

# ❌ 悪い例
cloudlog.info("Loop iteration 12345")  # 頻繁すぎ（INFOには不適切）
cloudlog.error("Temperature high")     # エラーではなくWARNING
```

### 2. 例外ログ

```python
# ✅ 良い例
try:
  risky_operation()
except Exception:
  cloudlog.exception("Operation failed")  # トレースバック付き

# ❌ 悪い例
try:
  risky_operation()
except Exception as e:
  cloudlog.error(f"Error: {e}")  # トレースバックなし
```

### 3. 機密情報のログ禁止

```python
# ❌ 悪い例
cloudlog.info(f"Access token: {token}")  # 機密情報をログに残さない

# ✅ 良い例
cloudlog.info("Access token acquired")
```

---

## トラブルシューティング

### 問題1：ログファイルが増えすぎる

**原因**: 頻繁なログ出力

**解決策**:
```python
# DEBUGログを減らす
# cloudlog.debug() の呼び出しを削減

# または間引き
if frame % 100 == 0:  # 100回に1回
  cloudlog.debug("Status update")
```

### 問題2：watchdog タイムアウト

**症状**: プロセスがハングとして検出される

**原因**: 長時間処理で`kick_watchdog()`が呼ばれない

**解決策**:
```python
# 長時間処理中もkick
def long_process():
  for i in range(1000):
    process_item(i)
    
    if i % 100 == 0:
      kick_watchdog()  # 定期的にkick
```

---

## まとめ

ロギングシステムのポイント:

1. **swaglog**: 統一ロギング、自動ローテーション
2. **watchdog**: プロセス生存監視
3. **適切なレベル**: DEBUG < INFO < WARNING < ERROR < EXCEPTION
4. **機密情報注意**: ログに残さない

---

## 関連ドキュメント

- [README.md](README.md) - common全体の概要

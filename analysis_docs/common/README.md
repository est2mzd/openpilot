# openpilot/common - 共通ユーティリティライブラリ

## 概要

`openpilot/common`は、openpilot全体で使用される**共通ユーティリティライブラリ**です。

- **ディレクトリ**: `/home/takuya/work/comma/openpilot/common/`
- **総行数**: 約2,889行
- **言語**: Python, C++, Cython
- **役割**: パラメータ管理、リアルタイム制御、フィルタ、ログ、変換等

---

## アーキテクチャ概要

```
common/
├── Params系 - 永続化パラメータ管理
│   ├── params.py/cc/h - Key-Value ストア
│   ├── params_keys.h - 全パラメータキー定義
│   └── params_pyx.pyx - Python/C++ブリッジ
│
├── リアルタイム制御系
│   ├── realtime.py - プロセス優先度・CPU割り当て
│   ├── ratekeeper.cc/h - 周波数維持
│   └── timing.h - 高精度タイミング
│
├── 制御アルゴリズム系
│   ├── pid.py - PIDコントローラ
│   ├── filter_simple.py - 1次フィルタ
│   └── simple_kalman.py - カルマンフィルタ
│
├── データ変換系
│   ├── conversions.py - 単位変換（mph↔m/s等）
│   ├── transformations/ - 座標変換
│   └── gps.py - GPS座標処理
│
├── ログ・デバッグ系
│   ├── swaglog.py/cc/h - 統一ロギング
│   ├── logging_extra.py - ログフォーマット
│   └── watchdog.py - プロセス監視
│
├── ユーティリティ系
│   ├── util.py/cc/h - 汎用ユーティリティ
│   ├── file_helpers.py - ファイル操作
│   ├── dict_helpers.py - 辞書操作
│   └── retry.py - リトライ処理
│
└── その他
    ├── api.py - comma API クライアント
    ├── basedir.py - ディレクトリパス
    ├── git.py - Gitリポジトリ情報
    └── version.h - バージョン定義
```

---

## 主要コンポーネント

### 1. Params - パラメータ管理システム

**目的**: 永続化されたKey-Valueストア

**主要ファイル**:
- [params.py](params.md) - Pythonインターフェース
- [params.cc/h](params.md) - C++実装
- [params_keys.h](params.md) - 全キー定義

**使用例**:
```python
from openpilot.common.params import Params

params = Params()

# 読み込み
dongle_id = params.get("DongleId")
experimental_mode = params.get_bool("ExperimentalMode")

# 書き込み
params.put("ExperimentalMode", "1")
params.put_bool("ExperimentalMode", True)
```

**主要パラメータ**:
- `DongleId`: デバイス固有ID
- `CarParams`: 車両パラメータ
- `ExperimentalMode`: 実験モード有効化
- `CalibrationParams`: キャリブレーションデータ

**詳細**: [params.md](params.md)

---

### 2. Realtime - リアルタイム制御

**目的**: プロセスのリアルタイム性確保

**主要ファイル**:
- [realtime.py](realtime.md) - プロセス優先度設定
- [ratekeeper.cc/h](realtime.md) - 周波数維持

**周波数定義**:
```python
DT_CTRL = 0.01   # 100Hz - controlsd
DT_MDL = 0.05    # 20Hz - modeld
DT_HW = 0.5      # 2Hz - hardwared
DT_DMON = 0.05   # 20Hz - drivermonitord
```

**優先度設定**:
```python
class Priority:
  CTRL_LOW = 51   # plannerd, radard
  CTRL_HIGH = 53  # controlsd
```

**使用例**:
```python
from openpilot.common.realtime import Ratekeeper, config_realtime_process, Priority

# リアルタイム設定（コア3、優先度53）
config_realtime_process(3, Priority.CTRL_HIGH)

# 100Hz維持
rk = Ratekeeper(100, print_delay_threshold=0.05)
while True:
  # 処理
  do_control()
  
  # 周波数維持（10ms待機）
  rk.keep_time()
```

**詳細**: [realtime.md](realtime.md)

---

### 3. 制御アルゴリズム

#### PIDコントローラ

**ファイル**: [pid.py](control_algorithms.md)

**特徴**:
- 速度依存ゲイン（kp, ki, kd）
- フィードフォワード対応
- アンチワインドアップ
- 積分リミット

**使用例**:
```python
from openpilot.common.pid import PIDController

# 速度依存ゲイン設定
kp_bp = [0., 20., 40.]   # 速度 breakpoints (m/s)
kp_v = [1.0, 0.8, 0.5]   # ゲイン values

pid = PIDController(
  k_p=[kp_bp, kp_v],
  k_i=0.1,
  k_d=0.01,
  k_f=0.5,
  pos_limit=3.0,   # 最大出力 m/s²
  neg_limit=-4.0,  # 最小出力
  rate=100         # 100Hz
)

# 制御計算
error = target_speed - current_speed
accel = pid.update(error, speed=current_speed, feedforward=ff_accel)
```

#### カルマンフィルタ

**ファイル**: [simple_kalman.py](control_algorithms.md)

**1次元カルマンフィルタ（KF1D）**:
```python
from openpilot.common.simple_kalman import KF1D

# 状態: [位置, 速度]
A = [[1.0, dt], [0.0, 1.0]]  # 状態遷移
C = [1.0, 0.0]                # 観測行列
K = [[0.12], [0.01]]          # カルマンゲイン

kf = KF1D(x0=[[0.0], [0.0]], A=A, C=C, K=K)

# 観測更新
measurement = 50.0  # m
kf.update(measurement)

# 状態取得
position, velocity = kf.x[0][0], kf.x[1][0]
```

#### 1次フィルタ

**ファイル**: [filter_simple.py](control_algorithms.md)

**ローパスフィルタ**:
```python
from openpilot.common.filter_simple import FirstOrderFilter

# 時定数0.5秒、サンプリング0.01秒
lpf = FirstOrderFilter(x0=0.0, rc=0.5, dt=0.01)

while True:
  raw_value = get_sensor_reading()
  filtered = lpf.update(raw_value)
```

**詳細**: [control_algorithms.md](control_algorithms.md)

---

### 4. データ変換

#### 単位変換

**ファイル**: [conversions.py](conversions.md)

```python
from openpilot.common.conversions import Conversions

# 速度変換
speed_mph = 60
speed_ms = speed_mph * Conversions.MPH_TO_MS  # 26.82 m/s
speed_kph = speed_ms * Conversions.MS_TO_KPH  # 96.56 km/h

# 角度変換
angle_deg = 45
angle_rad = angle_deg * Conversions.DEG_TO_RAD  # 0.785 rad

# 質量変換
weight_lb = 3200
weight_kg = weight_lb * Conversions.LB_TO_KG  # 1451 kg
```

**変換定数**:
| 変換 | 定数 | 値 |
|------|------|-----|
| MPH → KPH | `MPH_TO_KPH` | 1.609344 |
| KPH → m/s | `KPH_TO_MS` | 0.277778 |
| m/s → MPH | `MS_TO_MPH` | 2.23694 |
| degree → radian | `DEG_TO_RAD` | π/180 |
| lb → kg | `LB_TO_KG` | 0.453592 |

**詳細**: [conversions.md](conversions.md)

---

### 5. ロギングシステム

#### swaglog - 統一ロギング

**ファイル**: [swaglog.py/cc/h](logging.md)

**特徴**:
- ファイルローテーション（60秒または256KB毎）
- ZMQ経由でathenaにアップロード
- レベル別ログ（DEBUG, INFO, WARNING, ERROR）

**使用例**:
```python
from openpilot.common.swaglog import cloudlog

cloudlog.info("System started")
cloudlog.warning(f"Temperature high: {temp}°C")
cloudlog.error("Critical failure!")

# 例外ログ
try:
  dangerous_operation()
except Exception:
  cloudlog.exception("Operation failed")
```

**ログファイル**:
- 保存先: `/data/media/0/realdata/swaglog/`
- 形式: `swaglog.0000000001`, `swaglog.0000000002`...
- 保持数: 最大2500ファイル

#### watchdog - プロセス監視

**ファイル**: [watchdog.py](logging.md)

**目的**: プロセスの生存確認

```python
from openpilot.common.watchdog import kick_watchdog

while True:
  # 定期的にkick（1秒に1回）
  kick_watchdog()
  
  # 処理
  do_work()
  time.sleep(0.1)
```

**詳細**: [logging.md](logging.md)

---

### 6. ユーティリティ

#### util.py - 汎用ユーティリティ

**ファイル**: [util.py](utilities.md)

**主要クラス**:

**MovingAverage**:
```python
from openpilot.common.util import MovingAverage

ma = MovingAverage(window=10)
for value in sensor_readings:
  ma.add_value(value)
  avg = ma.get_average()
```

**Ratelimiter**:
```python
from openpilot.common.util import Ratelimiter

rl = Ratelimiter(dt=1.0)  # 1秒に1回
while True:
  if rl.check():
    print("1 second passed")
```

#### file_helpers.py - ファイル操作

**アトミック書き込み**:
```python
from openpilot.common.file_helpers import atomic_write_in_dir

# 途中で失敗しても破損しない
atomic_write_in_dir("/data/params/MyParam", b"value")
```

#### retry.py - リトライ処理

```python
from openpilot.common.retry import retry

@retry(attempts=3, delay=1.0)
def unstable_network_call():
  response = requests.get("https://api.comma.ai/")
  return response.json()
```

**詳細**: [utilities.md](utilities.md)

---

## プロセス間での使用パターン

### controlsd での使用例

```python
from openpilot.common.params import Params
from openpilot.common.realtime import Ratekeeper, config_realtime_process, Priority, DT_CTRL
from openpilot.common.swaglog import cloudlog
from openpilot.common.conversions import Conversions

def controlsd_thread():
  # リアルタイム設定
  config_realtime_process(3, Priority.CTRL_HIGH)
  
  # パラメータ読み込み
  params = Params()
  experimental_mode = params.get_bool("ExperimentalMode")
  
  # 100Hz維持
  rk = Ratekeeper(1 / DT_CTRL, print_delay_threshold=0.05)
  
  cloudlog.info("controlsd started")
  
  while True:
    # 制御処理
    do_control()
    
    # 周波数維持
    rk.keep_time()
```

### plannerd での使用例

```python
from openpilot.common.pid import PIDController
from openpilot.common.filter_simple import FirstOrderFilter
from openpilot.common.simple_kalman import KF1D

# 縦制御PID
long_pid = PIDController(k_p=0.8, k_i=0.12, k_f=0.5, rate=20)

# 横制御フィルタ
lat_filter = FirstOrderFilter(x0=0.0, rc=0.1, dt=0.05)

# 位置推定カルマンフィルタ
position_kf = KF1D(...)

while True:
  # 縦制御
  accel = long_pid.update(speed_error, speed=v_ego)
  
  # 横制御
  filtered_angle = lat_filter.update(raw_angle)
  
  # 位置推定
  position_kf.update(gps_measurement)
```

---

## パフォーマンス特性

### Params読み書き速度

| 操作 | 速度 | 備考 |
|------|------|------|
| `get()` | ~0.1ms | 共有メモリから読み込み |
| `put()` | ~1-5ms | ファイルシステムに書き込み |
| `get_bool()` | ~0.1ms | 文字列→bool変換 |

### リアルタイム性

| プロセス | 周波数 | CPU | 優先度 | ジッター |
|---------|-------|-----|--------|---------|
| controlsd | 100Hz | Core 3 | 53 | <1ms |
| plannerd | 20Hz | Core 2 | 51 | <5ms |
| modeld | 20Hz | Core 2 | - | GPU依存 |

### フィルタ処理速度

| フィルタ | 計算時間 | 用途 |
|---------|---------|------|
| FirstOrderFilter | ~0.001ms | 単純ローパス |
| PIDController | ~0.01ms | 制御計算 |
| KF1D | ~0.02ms | 状態推定 |

---

## ベストプラクティス

### 1. Paramsの使用

**読み込みは頻繁に、書き込みは慎重に**:
```python
# Good: 起動時に1回読み込み
params = Params()
car_params = params.get("CarParams")

# Bad: 100Hzループで毎回読み込み
while True:
  car_params = params.get("CarParams")  # 遅い！
```

**書き込みは非同期で**:
```python
# Good: 別スレッドで書き込み
threading.Thread(target=lambda: params.put("LongKey", data)).start()

# Bad: メインループで書き込み
while True:
  params.put("LongKey", data)  # ブロックする！
  do_control()  # 遅延発生
```

### 2. リアルタイム処理

**GC無効化**:
```python
import gc
gc.disable()  # リアルタイムプロセスでは必須
```

**CPU割り当て**:
```python
# 制御系: Core 3
config_realtime_process(3, Priority.CTRL_HIGH)

# モデル系: Core 2
config_realtime_process(2, Priority.CTRL_LOW)
```

### 3. フィルタの選択

| 用途 | 推奨フィルタ | 理由 |
|------|-------------|------|
| ノイズ除去 | FirstOrderFilter | 軽量、シンプル |
| 制御 | PIDController | 定常偏差ゼロ |
| 状態推定 | KF1D | ノイズ・モデル考慮 |

---

## トラブルシューティング

### 問題1：周波数が維持できない

**症状**: Ratekeeperで遅延警告

**原因**: 処理時間が長すぎる

**解決策**:
```python
rk = Ratekeeper(100, print_delay_threshold=0.05)
while True:
  start = time.time()
  do_work()
  elapsed = time.time() - start
  
  if elapsed > 0.01:
    cloudlog.warning(f"Slow processing: {elapsed*1000:.1f}ms")
  
  rk.keep_time()
```

### 問題2：Paramsが読めない

**症状**: `params.get("Key")`が`None`

**原因**: キーが存在しない、またはタイプミス

**解決策**:
```python
# キー確認
params.check_key("MyKey")  # True/False

# デフォルト値使用
value = params.get("MyKey")
if value is None:
  value = "default"
```

### 問題3：PIDが発振する

**症状**: 制御出力が振動

**原因**: ゲインが高すぎる

**解決策**:
```python
# ゲインを下げる
pid = PIDController(
  k_p=0.5,  # 1.0 → 0.5
  k_i=0.05, # 0.1 → 0.05
  k_d=0.01
)

# または積分を凍結
pid.update(error, freeze_integrator=True)
```

---

## カスタム開発ガイド

### 新しいParamsキーの追加

#### ステップ1：params_keys.hに追加

```cpp
// common/params_keys.h
inline static std::unordered_map<std::string, uint32_t> keys = {
  // ... 既存キー
  
  {"MyCustomParam", PERSISTENT},  // 永続化
};
```

#### ステップ2：使用

```python
from openpilot.common.params import Params

params = Params()
params.put("MyCustomParam", "value")
value = params.get("MyCustomParam")
```

### カスタムフィルタの実装

```python
class MyCustomFilter:
  def __init__(self, initial_value):
    self.value = initial_value
  
  def update(self, measurement):
    # カスタムロジック
    self.value = 0.9 * self.value + 0.1 * measurement
    return self.value

# 使用
filter = MyCustomFilter(0.0)
filtered = filter.update(sensor_value)
```

---

## まとめ

`openpilot/common`の主要ポイント:

1. **Params**: 永続化Key-Valueストア
   - 起動時読み込み、非同期書き込み
   
2. **Realtime**: リアルタイム制御
   - CPU割り当て、優先度設定
   - Ratekeeperで周波数維持
   
3. **制御アルゴリズム**: PID、カルマンフィルタ、1次フィルタ
   - 用途に応じて選択
   
4. **変換**: 単位変換、座標変換
   - Conversionsクラスで統一
   
5. **ロギング**: swaglog、watchdog
   - 統一ログフォーマット
   
6. **ユーティリティ**: MovingAverage、Ratelimiter等
   - 汎用的に使用可能

---

## 詳細ドキュメント

- [params.md](params.md) - Paramsシステム詳細
- [realtime.md](realtime.md) - リアルタイム制御詳細
- [control_algorithms.md](control_algorithms.md) - PID、カルマンフィルタ、1次フィルタ
- [conversions.md](conversions.md) - 単位・座標変換
- [logging.md](logging.md) - swaglog、watchdog
- [utilities.md](utilities.md) - ユーティリティ関数群

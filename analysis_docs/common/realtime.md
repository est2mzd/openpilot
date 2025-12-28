# Realtime - リアルタイム制御

## 概要

**realtime.py**は、openpilotプロセスの**リアルタイム性**を確保するためのモジュールです。

- **主要ファイル**: `realtime.py`, `ratekeeper.cc/h`
- **機能**: CPU割り当て、プロセス優先度、周波数維持
- **対象**: controlsd, plannerd, modeld等の時間制約プロセス

---

## 主要コンポーネント

### 1. 周波数定義（DT_*）

openpilotの各プロセスの標準周波数（時間ステップ）:

```python
DT_CTRL = 0.01   # 100Hz - controlsd（制御ループ）
DT_MDL = 0.05    # 20Hz - modeld（モデル推論）
DT_HW = 0.5      # 2Hz - hardwared, manager（ハードウェア監視）
DT_DMON = 0.05   # 20Hz - drivermonitord（ドライバー監視）
```

**使用例**:
```python
from openpilot.common.realtime import DT_CTRL, Ratekeeper

# 100Hz制御ループ
rk = Ratekeeper(1 / DT_CTRL)  # 1 / 0.01 = 100Hz

while True:
  do_control()
  rk.keep_time()  # 10ms周期を維持
```

---

### 2. Priority - プロセス優先度

**Linux SFIFOスケジューラの優先度**:

```python
class Priority:
  # CORE 2 (モデル処理用)
  # - modeld = 55
  # - camerad = 54
  CTRL_LOW = 51   # plannerd, radard
  
  # CORE 3 (制御用)
  # - pandad = 55 (最高優先度)
  CTRL_HIGH = 53  # controlsd
```

**優先度の意味**:
- **高い数値 = 高優先度**
- SCHED_FIFO（リアルタイムスケジューリング）
- 通常プロセスより優先的にCPU時間を獲得

---

### 3. config_realtime_process() - リアルタイム設定

**目的**: プロセスをリアルタイムモードに設定

**シグネチャ**:
```python
def config_realtime_process(cores: int | list[int], priority: int) -> None
```

**動作**:
1. **GC無効化**: `gc.disable()`でガベージコレクション停止
2. **SCHED_FIFO設定**: リアルタイムスケジューリング
3. **CPU Affinity設定**: 指定コアに固定

**使用例**:
```python
from openpilot.common.realtime import config_realtime_process, Priority

# controlsd: Core 3、優先度53
config_realtime_process(3, Priority.CTRL_HIGH)

# plannerd: Core 2、優先度51
config_realtime_process(2, Priority.CTRL_LOW)

# 複数コア指定
config_realtime_process([2, 3], Priority.CTRL_LOW)
```

**CPU割り当て例**:
```
Core 0: OS、一般プロセス
Core 1: カメラ処理（camerad）
Core 2: モデル処理（modeld=55, plannerd=51）
Core 3: 制御処理（pandad=55, controlsd=53）
```

---

### 4. Ratekeeper - 周波数維持

**目的**: ループの実行周波数を一定に保つ

#### 基本的な使い方

```python
from openpilot.common.realtime import Ratekeeper

# 100Hz（10ms周期）
rk = Ratekeeper(100)

while True:
  # 処理（例: 5ms）
  do_work()
  
  # 残り時間（5ms）sleep
  rk.keep_time()
```

**内部動作**:
1. 前回の`keep_time()`からの経過時間を測定
2. 目標周期（例: 10ms）に対する残り時間を計算
3. 残り時間だけsleep
4. ジッター（遅延）を監視

#### コンストラクタ

```python
def __init__(self, rate: float, print_delay_threshold: float | None = 0.0)
```

**パラメータ**:
- `rate`: 目標周波数（Hz）
- `print_delay_threshold`: 遅延警告の閾値（秒）

**例**:
```python
# 100Hz、50ms以上遅延で警告
rk = Ratekeeper(100, print_delay_threshold=0.05)

# 警告なし
rk = Ratekeeper(100, print_delay_threshold=None)
```

#### keep_time() - 周波数維持

**シグネチャ**:
```python
def keep_time(self) -> bool
```

**戻り値**:
- `True`: 遅延が発生した（lagged）
- `False`: 正常

**使用例**:
```python
rk = Ratekeeper(100, print_delay_threshold=0.05)

while True:
  start = time.time()
  
  # 処理
  do_control()
  
  # 周波数維持
  lagged = rk.keep_time()
  
  if lagged:
    print("Warning: Loop is lagging!")
```

**出力例（遅延時）**:
```
controlsd lagging by 15.23 ms
```

#### monitor_time() - 遅延監視のみ

**目的**: 遅延を監視するが、sleepしない

```python
def monitor_time(self) -> bool
```

**使用例**:
```python
rk = Ratekeeper(100)

while True:
  # 処理
  do_work()
  
  # 遅延監視のみ（sleepしない）
  lagged = rk.monitor_time()
  
  if lagged:
    handle_lag()
```

#### プロパティ

**frame** - 実行回数:
```python
print(f"Frame: {rk.frame}")  # 0, 1, 2, ...
```

**remaining** - 残り時間:
```python
print(f"Remaining: {rk.remaining * 1000:.2f} ms")
```

**lagging** - 遅延フラグ:
```python
if rk.lagging:
  print("Average DT is too high!")
```

**avg_dt** - 平均周期:
```python
avg_period = rk.avg_dt.get_average()
print(f"Average period: {avg_period * 1000:.2f} ms")
```

---

## 実際の使用例

### 例1：controlsd（100Hz制御ループ）

```python
from openpilot.common.realtime import Ratekeeper, config_realtime_process, Priority, DT_CTRL
import cereal.messaging as messaging

def controlsd_thread():
  # リアルタイム設定（Core 3、優先度53）
  config_realtime_process(3, Priority.CTRL_HIGH)
  
  # メッセージング
  sm = messaging.SubMaster(['carState', 'lateralPlan', 'longitudinalPlan'])
  pm = messaging.PubMaster(['controlsState', 'carControl'])
  
  # 100Hz維持（50ms以上遅延で警告）
  rk = Ratekeeper(1 / DT_CTRL, print_delay_threshold=0.05)
  
  while True:
    # データ受信
    sm.update()
    
    # 制御計算
    car_control = compute_control(sm['carState'], sm['lateralPlan'], sm['longitudinalPlan'])
    
    # 送信
    pm.send('carControl', car_control)
    
    # 100Hz維持（10ms周期）
    rk.keep_time()
```

### 例2：plannerd（20Hz プランニング）

```python
from openpilot.common.realtime import Ratekeeper, config_realtime_process, Priority, DT_MDL

def plannerd_thread():
  # リアルタイム設定（Core 2、優先度51）
  config_realtime_process(2, Priority.CTRL_LOW)
  
  # 20Hz維持
  rk = Ratekeeper(1 / DT_MDL, print_delay_threshold=0.1)
  
  while True:
    # プランニング計算
    lateral_plan = compute_lateral_plan()
    longitudinal_plan = compute_longitudinal_plan()
    
    # 送信
    send_plans(lateral_plan, longitudinal_plan)
    
    # 20Hz維持（50ms周期）
    rk.keep_time()
```

### 例3：modeld（20Hz モデル推論）

```python
from openpilot.common.realtime import Ratekeeper, DT_MDL

def modeld_thread():
  # モデルはGPU使用のためCPU優先度設定なし
  
  # 20Hz維持
  rk = Ratekeeper(1 / DT_MDL, print_delay_threshold=0.1)
  
  model = load_model()
  
  while True:
    # カメラ画像取得
    frame = get_camera_frame()
    
    # モデル推論（GPU）
    outputs = model.run(frame)
    
    # 結果送信
    send_model_outputs(outputs)
    
    # 20Hz維持
    rk.keep_time()
```

### 例4：処理時間の監視

```python
from openpilot.common.realtime import Ratekeeper
import time

def monitored_loop():
  rk = Ratekeeper(100, print_delay_threshold=0.05)
  
  while True:
    start = time.time()
    
    # 処理
    do_heavy_computation()
    
    elapsed = time.time() - start
    
    # 処理時間チェック
    if elapsed > 0.008:  # 8ms以上
      print(f"WARNING: Processing took {elapsed*1000:.2f} ms")
    
    # 周波数維持
    lagged = rk.keep_time()
    
    if lagged:
      print(f"Frame {rk.frame}: Lagged! Avg DT = {rk.avg_dt.get_average()*1000:.2f} ms")
```

---

## CPU Affinity（コア割り当て）

### set_core_affinity() - コア固定

**シグネチャ**:
```python
def set_core_affinity(cores: list[int]) -> None
```

**目的**: プロセスを特定CPUコアに固定

**使用例**:
```python
from openpilot.common.realtime import set_core_affinity

# Core 3に固定
set_core_affinity([3])

# Core 2, 3に固定（両方使用可能）
set_core_affinity([2, 3])
```

**効果**:
- **キャッシュヒット率向上**: 同じコアで実行し続ける
- **コンテキストスイッチ削減**: 他コアへの移動を防ぐ
- **予測可能な性能**: コア競合を回避

---

## リアルタイムスケジューリング（SCHED_FIFO）

### Linux スケジューラ

**通常プロセス（SCHED_OTHER）**:
- タイムシェアリング
- 優先度は動的に変化
- 公平性重視

**リアルタイムプロセス（SCHED_FIFO）**:
- 高優先度
- 先入先出（First In First Out）
- CPUを占有可能

### openpilotの優先度設計

```
優先度（高→低）
├─ 55: pandad（CAN通信、最優先）
├─ 55: modeld（モデル推論）
├─ 54: camerad（カメラ）
├─ 53: controlsd（制御ループ）
└─ 51: plannerd, radard（プランニング）
```

**理由**:
1. **pandad最優先**: CANメッセージを逃さない
2. **controlsd高優先**: 制御遅延を最小化
3. **plannerd低優先**: 20Hzで十分、制御を妨げない

---

## パフォーマンス特性

### 実測ジッター

| プロセス | 周波数 | 目標周期 | 平均ジッター | 最大ジッター |
|---------|-------|---------|------------|------------|
| controlsd | 100Hz | 10ms | ±0.5ms | ±2ms |
| plannerd | 20Hz | 50ms | ±2ms | ±10ms |
| modeld | 20Hz | 50ms | ±5ms | ±20ms（GPU待ち） |

### CPU使用率

| プロセス | 平均CPU | ピークCPU | 割り当てコア |
|---------|--------|---------|------------|
| controlsd | 15% | 30% | Core 3 |
| plannerd | 20% | 40% | Core 2 |
| modeld | 80% | 95%（GPU） | Core 2 |
| camerad | 25% | 50% | Core 1 |

---

## ベストプラクティス

### 1. GC無効化

**リアルタイムプロセスでは必須**:
```python
import gc

# プロセス起動時に1回
gc.disable()

# または config_realtime_process() が自動で無効化
config_realtime_process(3, Priority.CTRL_HIGH)
```

**理由**: ガベージコレクションで数十msブロックする可能性

### 2. 適切な優先度設定

**制御系（高優先度）**:
```python
# controlsd
config_realtime_process(3, Priority.CTRL_HIGH)
```

**プランニング系（低優先度）**:
```python
# plannerd
config_realtime_process(2, Priority.CTRL_LOW)
```

### 3. 遅延監視

**警告閾値の設定**:
```python
# 制御系: 5ms以上で警告
rk = Ratekeeper(100, print_delay_threshold=0.005)

# プランニング系: 10ms以上で警告
rk = Ratekeeper(20, print_delay_threshold=0.01)
```

### 4. 処理時間の最適化

```python
rk = Ratekeeper(100)

while True:
  start = time.time()
  
  # 処理
  result = do_work()
  
  elapsed = time.time() - start
  
  # 目標の80%以内に収める
  target = 0.01  # 10ms
  if elapsed > target * 0.8:
    optimize_or_reduce_work()
  
  rk.keep_time()
```

---

## トラブルシューティング

### 問題1：「lagging by XX ms」警告

**症状**: 
```
controlsd lagging by 25.42 ms
```

**原因**:
- 処理時間が目標周期を超えている
- CPU負荷が高い

**解決策**:
```python
import time

# 処理時間を計測
start = time.time()
do_work()
elapsed = time.time() - start

if elapsed > 0.008:  # 10msの80%
  print(f"Processing time: {elapsed*1000:.2f} ms (too slow!)")
  # 処理を最適化
```

### 問題2：平均DTが高い

**症状**:
```python
if rk.lagging:
  print("Average DT too high!")
```

**原因**: 長期的に周期が遅れている

**解決策**:
```python
# 平均周期を確認
avg_dt = rk.avg_dt.get_average()
target_dt = 0.01  # 10ms

if avg_dt > target_dt * 1.1:
  print(f"Avg DT: {avg_dt*1000:.2f} ms (target: {target_dt*1000:.2f} ms)")
  # CPU負荷を減らす
  reduce_workload()
```

### 問題3：CPUコアが固定されない

**症状**: プロセスが複数コアを移動

**原因**: PCモードで実行している

**確認**:
```python
from openpilot.system.hardware import PC

if PC:
  print("Running on PC, realtime features disabled")
```

**解決策**: comma device上で実行

---

## 高度な使用例

### カスタム周波数の動的変更

```python
class AdaptiveRatekeeper:
  def __init__(self, min_rate, max_rate):
    self.min_rate = min_rate
    self.max_rate = max_rate
    self.current_rate = min_rate
    self.rk = Ratekeeper(min_rate)
  
  def adjust_rate(self, load):
    # 負荷に応じて周波数調整
    if load > 0.8:
      self.current_rate = self.min_rate
    else:
      self.current_rate = self.max_rate
    
    self.rk = Ratekeeper(self.current_rate)
  
  def keep_time(self):
    return self.rk.keep_time()

# 使用
ark = AdaptiveRatekeeper(min_rate=20, max_rate=100)

while True:
  cpu_load = get_cpu_load()
  ark.adjust_rate(cpu_load)
  
  do_work()
  ark.keep_time()
```

---

## まとめ

realtimeモジュールのポイント:

1. **周波数定義**: DT_CTRL (100Hz), DT_MDL (20Hz)等
2. **リアルタイム設定**: `config_realtime_process()`でCPU割り当て・優先度設定
3. **Ratekeeper**: `keep_time()`で周波数維持
4. **GC無効化**: リアルタイムプロセスでは必須
5. **遅延監視**: `print_delay_threshold`で警告
6. **CPU Affinity**: 特定コアに固定してキャッシュヒット率向上

---

## 関連ドキュメント

- [README.md](README.md) - common全体の概要
- [utilities.md](utilities.md) - MovingAverage等の関連ユーティリティ

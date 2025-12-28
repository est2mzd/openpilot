# messaging API - Python/C++ API詳細

## 概要

openpilotの**メッセージングAPI**は、プロセス間通信（IPC）を簡単に行うための高レベルインターフェースです。

- **メインファイル**: `cereal/messaging/__init__.py`
- **バックエンド**: msgq（Cap'n Proto + 共有メモリ）
- **主要クラス**: `PubMaster`, `SubMaster`
- **言語**: Python, C++

---

## 主要クラス

### 1. PubMaster（送信側）

**目的**: メッセージを送信（Publish）

#### 基本的な使い方

```python
import cereal.messaging as messaging

# 送信するサービスを指定
pm = messaging.PubMaster(['controlsState', 'carControl'])

# メッセージ作成
msg = messaging.new_message('controlsState')
cs = msg.controlsState

# データ設定
cs.enabled = True
cs.vEgo = 25.0  # m/s
cs.aEgo = 0.5   # m/s²

# 送信
pm.send('controlsState', msg)
```

#### コンストラクタ

```python
class PubMaster:
  def __init__(self, services: List[str]):
    """
    Args:
      services: 送信するサービス名のリスト
    """
```

**例**:
```python
pm = messaging.PubMaster(['controlsState', 'carControl', 'lateralPlan'])
```

#### send()メソッド

```python
def send(self, s: str, dat: Union[bytes, capnp.lib.capnp._DynamicStructBuilder]) -> None:
  """
  Args:
    s: サービス名
    dat: 送信データ（メッセージまたはバイト列）
  """
```

**例**:
```python
msg = messaging.new_message('controlsState')
msg.controlsState.enabled = True
pm.send('controlsState', msg)
```

#### wait_for_readers_to_update()

```python
def wait_for_readers_to_update(self, s: str, timeout: int, dt: float = 0.05) -> bool:
  """
  全ての受信者が更新するまで待機
  
  Args:
    s: サービス名
    timeout: タイムアウト（秒）
    dt: ポーリング間隔（秒）
    
  Returns:
    True: 全受信者が更新
    False: タイムアウト
  """
```

**用途**: テストやデバッグ時に同期を取る

```python
pm.send('controlsState', msg)
if pm.wait_for_readers_to_update('controlsState', timeout=1):
  print("All readers updated")
else:
  print("Timeout")
```

---

### 2. SubMaster（受信側）

**目的**: メッセージを受信（Subscribe）

#### 基本的な使い方

```python
import cereal.messaging as messaging

# 受信するサービスを指定
sm = messaging.SubMaster(['controlsState', 'carState', 'modelV2'])

while True:
  # メッセージ更新（100msタイムアウト）
  sm.update(timeout=100)
  
  # データ取得
  if sm.updated['controlsState']:
    cs = sm['controlsState']
    print(f"vEgo: {cs.vEgo} m/s")
  
  if sm.updated['carState']:
    car = sm['carState']
    print(f"Gas: {car.gas}, Brake: {car.brake}")
```

#### コンストラクタ

```python
class SubMaster:
  def __init__(self, 
               services: List[str], 
               poll: Optional[str] = None,
               ignore_alive: Optional[List[str]] = None, 
               ignore_avg_freq: Optional[List[str]] = None,
               ignore_valid: Optional[List[str]] = None, 
               addr: str = "127.0.0.1", 
               frequency: Optional[float] = None):
    """
    Args:
      services: 受信するサービス名のリスト
      poll: ポーリングするサービス（他はnon-blocking）
      ignore_alive: 生存チェックを無視するサービス
      ignore_avg_freq: 周波数チェックを無視するサービス
      ignore_valid: validフラグチェックを無視するサービス
      addr: IPアドレス（通常 "127.0.0.1"）
      frequency: 更新周波数（pollサービスの周波数を使用）
    """
```

**例1：基本的な受信**:
```python
sm = messaging.SubMaster(['controlsState', 'carState'])
```

**例2：ポーリング指定**:
```python
# controlsStateをポーリング、他はnon-blocking
sm = messaging.SubMaster(['controlsState', 'carState', 'modelV2'], 
                         poll='controlsState')
```

**例3：チェック無視**:
```python
# carStateの生存チェックを無視（オプショナル）
sm = messaging.SubMaster(['controlsState', 'carState'],
                         ignore_alive=['carState'])
```

#### update()メソッド

```python
def update(self, timeout: int = 100) -> None:
  """
  メッセージを受信して内部状態を更新
  
  Args:
    timeout: タイムアウト（ミリ秒）
  """
```

**使用例**:
```python
while True:
  sm.update(timeout=100)  # 100ms待機
  
  # データ処理
  if sm.updated['controlsState']:
    print("controlsState updated!")
```

#### データアクセス

##### [ ]演算子（最新データ）

```python
sm = messaging.SubMaster(['carState'])
sm.update()

# 最新のcarStateを取得
car = sm['carState']
print(f"Speed: {car.vEgo} m/s")
print(f"Gas: {car.gas}")
print(f"Brake: {car.brake}")
```

##### updated辞書（更新フラグ）

```python
if sm.updated['carState']:
  print("carState was updated in this cycle")
else:
  print("carState was not updated")
```

##### recv_time辞書（受信時刻）

```python
print(f"Last received at: {sm.recv_time['carState']}")
```

##### logMonoTime辞書（送信時刻）

```python
# 送信側のタイムスタンプ
print(f"Sent at: {sm.logMonoTime['carState']}")

# レイテンシ計算
import time
latency = time.monotonic() - sm.logMonoTime['carState'] / 1e9
print(f"Latency: {latency*1000:.1f} ms")
```

#### 状態チェックメソッド

##### all_alive()

```python
def all_alive(self, service_list: Optional[List[str]] = None) -> bool:
  """
  全サービスが生存しているか
  
  生存条件: 最後の受信から (10 / 周波数) 秒以内
  例: 100Hzなら0.1秒以内、20Hzなら0.5秒以内
  """
```

**使用例**:
```python
if not sm.all_alive():
  print("ERROR: Some services are not alive!")
  for s in sm.services:
    if not sm.alive[s]:
      print(f"  {s} is dead")
```

##### all_freq_ok()

```python
def all_freq_ok(self, service_list: Optional[List[str]] = None) -> bool:
  """
  全サービスの受信周波数が正常か
  
  周波数が期待値の80%～120%の範囲内かチェック
  """
```

**使用例**:
```python
if not sm.all_freq_ok():
  print("WARNING: Frequency check failed!")
```

##### all_valid()

```python
def all_valid(self, service_list: Optional[List[str]] = None) -> bool:
  """
  全サービスのvalidフラグがTrueか
  """
```

**使用例**:
```python
if sm.all_valid():
  # データが有効な場合のみ処理
  process_data(sm['carState'])
```

##### all_checks()

```python
def all_checks(self, service_list: Optional[List[str]] = None) -> bool:
  """
  all_alive() AND all_freq_ok() AND all_valid()
  
  全てのチェックが通ったかを確認
  """
```

**使用例**:
```python
if sm.all_checks(['carState', 'controlsState']):
  # 全チェックOKの場合のみ処理
  control_car(sm['carState'], sm['controlsState'])
else:
  # エラーハンドリング
  disable_controls()
```

---

## メッセージ作成関数

### new_message()

```python
def new_message(service: Optional[str], size: Optional[int] = None, **kwargs) -> capnp.lib.capnp._DynamicStructBuilder:
  """
  新しいメッセージを作成
  
  Args:
    service: サービス名（log.capnpのunion名）
    size: リストサイズ（リスト型の場合）
    **kwargs: 追加フィールド
    
  Returns:
    Cap'n Protoメッセージビルダー
  """
```

**例1：基本的なメッセージ**:
```python
msg = messaging.new_message('controlsState')
msg.controlsState.enabled = True
msg.controlsState.vEgo = 25.0
```

**例2：リスト型メッセージ**:
```python
# 10個の要素を持つリスト
msg = messaging.new_message('radarState', size=10)
for i in range(10):
  msg.radarState[i].trackId = i
  msg.radarState[i].dRel = 50.0 + i * 5.0
```

**例3：カスタムタイムスタンプ**:
```python
import time

msg = messaging.new_message('controlsState', 
                            logMonoTime=int(time.monotonic() * 1e9))
```

---

## 実際の使用例

### 例1：制御ループ（controlsd）

```python
import cereal.messaging as messaging
import time

def controlsd_thread():
  # 購読（受信）
  sm = messaging.SubMaster(['carState', 'modelV2', 'lateralPlan', 'longitudinalPlan'])
  
  # 配信（送信）
  pm = messaging.PubMaster(['controlsState', 'carControl'])
  
  while True:
    # データ受信（100ms待機）
    sm.update(timeout=100)
    
    # 全データチェック
    if not sm.all_checks(['carState', 'lateralPlan', 'longitudinalPlan']):
      # エラー: データが不正
      continue
    
    # 制御計算
    car_state = sm['carState']
    lat_plan = sm['lateralPlan']
    long_plan = sm['longitudinalPlan']
    
    # 横制御
    steer_torque = calculate_steer(car_state, lat_plan)
    
    # 縦制御
    accel = calculate_accel(car_state, long_plan)
    
    # 制御コマンド送信
    msg = messaging.new_message('carControl')
    cc = msg.carControl
    cc.enabled = True
    cc.actuators.steer = steer_torque
    cc.actuators.accel = accel
    pm.send('carControl', msg)
    
    # 状態送信（UI用）
    msg = messaging.new_message('controlsState')
    cs = msg.controlsState
    cs.enabled = True
    cs.vEgo = car_state.vEgo
    cs.aEgo = car_state.aEgo
    pm.send('controlsState', msg)
    
    # 100Hz（10ms周期）
    time.sleep(0.01)

if __name__ == '__main__':
  controlsd_thread()
```

### 例2：モデル処理（modeld）

```python
import cereal.messaging as messaging
import numpy as np

def modeld_thread():
  # カメラ画像を受信
  sm = messaging.SubMaster(['roadCameraState'])
  
  # モデル出力を送信
  pm = messaging.PubMaster(['modelV2'])
  
  # モデルロード
  model = load_supercombo_model()
  
  while True:
    sm.update(timeout=100)
    
    if not sm.updated['roadCameraState']:
      continue
    
    # 画像取得
    camera_state = sm['roadCameraState']
    image = get_image_from_camera_state(camera_state)
    
    # モデル推論
    outputs = model.run(image)
    
    # modelV2メッセージ作成
    msg = messaging.new_message('modelV2')
    model_v2 = msg.modelV2
    
    # 経路予測
    model_v2.position.x = outputs['path_x']
    model_v2.position.y = outputs['path_y']
    model_v2.position.t = outputs['path_t']
    
    # 車線検出（4本の車線）
    model_v2.laneLines = outputs['lane_lines']
    model_v2.laneLineProbs = outputs['lane_probs']
    
    # 先行車検出
    model_v2.leads = outputs['leads']
    
    # 送信
    pm.send('modelV2', msg)
    
    # 20Hz（50ms周期）
    time.sleep(0.05)
```

### 例3：UI表示（ui）

```python
import cereal.messaging as messaging

def ui_thread():
  # 複数のサービスを購読
  sm = messaging.SubMaster([
    'controlsState',
    'carState',
    'modelV2',
    'lateralPlan',
    'deviceState',
  ])
  
  while True:
    sm.update(timeout=100)
    
    # 制御状態表示
    if sm.updated['controlsState']:
      cs = sm['controlsState']
      if cs.enabled:
        draw_green_indicator()
      draw_speed(cs.vEgo)
      draw_alerts(cs.alertText1, cs.alertText2)
    
    # 車両状態表示
    if sm.updated['carState']:
      car = sm['carState']
      draw_pedals(car.gas, car.brake)
      draw_steering(car.steeringAngleDeg)
    
    # モデル表示
    if sm.updated['modelV2']:
      model = sm['modelV2']
      draw_path(model.position.x, model.position.y)
      draw_lane_lines(model.laneLines)
      draw_leads(model.leads)
    
    # デバイス状態表示
    if sm.updated['deviceState']:
      device = sm['deviceState']
      draw_cpu_usage(device.cpuUsagePercent)
      draw_temperature(device.cpuTempC)
    
    # 60Hz（16.7ms周期）
    time.sleep(1.0 / 60)
```

### 例4：カスタムサービスの実装

```python
import cereal.messaging as messaging
import time

def custom_service_thread():
  """
  カスタムデータを配信するサービス
  """
  # 他のサービスを購読
  sm = messaging.SubMaster(['carState'])
  
  # カスタムサービスを配信
  pm = messaging.PubMaster(['customReserved0'])
  
  while True:
    sm.update(timeout=100)
    
    if not sm.updated['carState']:
      continue
    
    car = sm['carState']
    
    # カスタムロジック
    mads_enabled = check_mads_conditions(car)
    dec_confidence = calculate_dec_confidence(car)
    
    # カスタムメッセージ送信
    msg = messaging.new_message('customReserved0')
    custom = msg.customReserved0
    
    custom.madsEnabled = mads_enabled
    custom.decConfidence = dec_confidence
    custom.debugMessage = f"MADS: {mads_enabled}, DEC: {dec_confidence}"
    
    pm.send('customReserved0', msg)
    
    # 100Hz
    time.sleep(0.01)
```

---

## 低レベルAPI

### drain_sock()

```python
def drain_sock(sock: SubSocket, wait_for_one: bool = False) -> List[capnp.lib.capnp._DynamicStructReader]:
  """
  ソケットから全メッセージを取得
  
  Args:
    sock: SubSocket
    wait_for_one: 最低1つ受信するまで待機
    
  Returns:
    受信した全メッセージのリスト
  """
```

**使用例**:
```python
from cereal.messaging import sub_sock, drain_sock

sock = sub_sock('carState')
while True:
  msgs = drain_sock(sock, wait_for_one=True)
  for msg in msgs:
    print(f"vEgo: {msg.carState.vEgo}")
```

### recv_sock()

```python
def recv_sock(sock: SubSocket, wait: bool = False) -> Optional[capnp.lib.capnp._DynamicStructReader]:
  """
  最新のメッセージのみ取得（古いメッセージは破棄）
  
  Args:
    sock: SubSocket
    wait: メッセージ受信まで待機
    
  Returns:
    最新メッセージ（なければNone）
  """
```

**使用例**:
```python
from cereal.messaging import sub_sock, recv_sock

sock = sub_sock('carState')
while True:
  msg = recv_sock(sock, wait=True)
  if msg:
    print(f"vEgo: {msg.carState.vEgo}")
```

### recv_one()

```python
def recv_one(sock: SubSocket) -> Optional[capnp.lib.capnp._DynamicStructReader]:
  """
  1つのメッセージを受信（ブロッキング）
  
  Returns:
    メッセージ（タイムアウトならNone）
  """
```

### recv_one_or_none()

```python
def recv_one_or_none(sock: SubSocket) -> Optional[capnp.lib.capnp._DynamicStructReader]:
  """
  1つのメッセージを受信（ノンブロッキング）
  
  Returns:
    メッセージ（なければ即座にNone）
  """
```

---

## C++ API

### PubMaster（C++）

```cpp
#include "cereal/messaging/messaging.h"

// 送信側
PubMaster pm({"controlsState", "carControl"});

// メッセージ作成
MessageBuilder msg;
auto cs = msg.initEvent().initControlsState();
cs.setEnabled(true);
cs.setVEgo(25.0);

// 送信
pm.send("controlsState", msg);
```

### SubMaster（C++）

```cpp
#include "cereal/messaging/messaging.h"

// 受信側
SubMaster sm({"carState", "controlsState"});

while (true) {
  // 更新（100ms待機）
  sm.update(100);
  
  // データ取得
  if (sm.updated("carState")) {
    auto car = sm["carState"].getCarState();
    std::cout << "vEgo: " << car.getVEgo() << " m/s" << std::endl;
  }
}
```

---

## パフォーマンス最適化

### 1. conflate（最新メッセージのみ保持）

デフォルトでSubMasterは`conflate=True`が設定されています。

**動作**:
- 受信バッファに複数メッセージがある場合、最新のみ保持
- 古いメッセージは自動的に破棄

**利点**:
- 低速な受信側が高速な送信側に追いつける
- メモリ使用量削減

```python
# デフォルトでconflate有効
sm = messaging.SubMaster(['carState'])  # conflate=True

# 手動でソケット作成する場合
from cereal.messaging import sub_sock
sock = sub_sock('carState', conflate=True)
```

### 2. ポーリング vs ノンブロッキング

**ポーリング**（推奨）:
```python
# controlsStateをポーリング、他はnon-blocking
sm = messaging.SubMaster(['controlsState', 'carState', 'modelV2'],
                         poll='controlsState')

while True:
  # controlsStateが到着するまで待機（最大100ms）
  sm.update(timeout=100)
  
  # controlsStateは必ず更新されている
  cs = sm['controlsState']
  
  # carState, modelV2は更新されているかもしれない
  if sm.updated['carState']:
    car = sm['carState']
```

**全てノンブロッキング**:
```python
# 全サービスをnon-blocking受信
sm = messaging.SubMaster(['controlsState', 'carState'])

while True:
  # タイムアウト100ms（どれも来なければ100ms後に戻る）
  sm.update(timeout=100)
  
  if sm.updated['controlsState']:
    cs = sm['controlsState']
```

### 3. 周波数の最適化

**送信周波数**:
```python
# services.py
SERVICE_LIST = {
  "myService": (100., True),  # 100Hz
}

# 送信側（100Hz = 10ms周期）
pm = messaging.PubMaster(['myService'])
while True:
  msg = messaging.new_message('myService')
  # ... データ設定
  pm.send('myService', msg)
  time.sleep(0.01)  # 10ms
```

**受信周波数調整**:
```python
# 送信100Hz、受信20Hzの場合（間引き）
pm = messaging.PubMaster(['myService'])
sm = messaging.SubMaster(['myService'])

# 送信側: 100Hz
while True:
  pm.send('myService', msg)
  time.sleep(0.01)

# 受信側: 20Hz（5回に1回受信）
while True:
  sm.update()
  if sm.updated['myService']:
    # 処理
  time.sleep(0.05)  # 50ms
```

---

## トラブルシューティング

### 問題1：メッセージが受信できない

**チェック項目**:
1. サービスがservices.pyに登録されているか
2. 送信側が正しくメッセージを送信しているか
3. タイムアウトが短すぎないか

```python
# デバッグ用
sm = messaging.SubMaster(['myService'])
sm.update(timeout=1000)  # 1秒待機

if not sm.updated['myService']:
  print("ERROR: myService not received!")
else:
  print("OK: myService received")
```

### 問題2：周波数エラー

**症状**: `all_freq_ok()`が`False`

**原因**: 送信周波数がservices.pyの設定と一致しない

**解決策**:
```python
# services.py
SERVICE_LIST = {
  "myService": (50., True),  # 50Hz
}

# 送信側: 50Hz（20ms）で送信
while True:
  pm.send('myService', msg)
  time.sleep(0.02)  # 20ms
```

### 問題3：レイテンシが大きい

**計測方法**:
```python
import time

sm = messaging.SubMaster(['carState'])
sm.update()

if sm.updated['carState']:
  # 送信時刻（ナノ秒）
  sent_time = sm.logMonoTime['carState'] / 1e9
  
  # 受信時刻
  recv_time = sm.recv_time['carState']
  
  # レイテンシ
  latency = recv_time - sent_time
  print(f"Latency: {latency*1000:.1f} ms")
```

**一般的なレイテンシ**:
- 同一デバイス内: 1-5ms
- 異常: 50ms以上（CPUが重い、メモリ不足等）

---

## まとめ

messaging APIの主要ポイント:

1. **PubMaster**: メッセージ送信
   - `send(service, msg)`: メッセージ送信
   
2. **SubMaster**: メッセージ受信
   - `update(timeout)`: 全サービス更新
   - `sm[service]`: 最新データ取得
   - `sm.updated[service]`: 更新フラグ
   - `all_checks()`: 全チェック

3. **new_message()**: メッセージ作成

4. **設計パターン**:
   - 100Hz: 制御ループ（controlsd）
   - 20Hz: モデル処理（modeld, plannerd）
   - 60Hz: UI更新
   
5. **最適化**:
   - conflate: 最新のみ保持
   - polling: 特定サービスを待機
   - 適切な周波数設定

---

## 関連ドキュメント

- [README.md](README.md) - cereal全体の概要
- [services詳細](services.md) - サービス定義と周波数設定
- [log.capnp詳細](log_capnp.md) - メッセージ構造体定義

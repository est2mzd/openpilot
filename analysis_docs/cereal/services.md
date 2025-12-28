# services.py - サービス定義詳細

## 概要

`services.py`は、openpilotの**全メッセージングサービス**を定義するファイルです。

- **ファイルパス**: `/home/takuya/work/comma/openpilot/cereal/services.py`
- **行数**: 124行
- **役割**: サービス名、周波数、ログ設定の一元管理
- **形式**: Python辞書

---

## サービス定義の構造

### 基本フォーマット

```python
SERVICE_LIST = {
  "サービス名": (周波数, ログ記録, 間引き倍率),
}
```

**各フィールドの意味**:
- **サービス名** (str): msgqのトピック名（log.capnpのunion名と一致）
- **周波数** (float): 更新周波数（Hz）
- **ログ記録** (bool): ログに記録するか
- **間引き倍率** (int, optional): ログ間引き倍率（例: 10 = 10回に1回記録）

---

## 完全なサービスリスト

### 制御系サービス（高周波数）

```python
SERVICE_LIST = {
  # 制御状態（最重要）
  "controlsState": (100., True),       # 100Hz, ログ記録（間引きなし）
  
  # 車両状態
  "carState": (100., True),            # 100Hz, ログ記録
  "carControl": (100., False),         # 100Hz, ログなし（制御コマンド）
  "carOutput": (100., False),          # 100Hz, ログなし
  "carParams": (0.02, True),           # 0.02Hz（50秒に1回）, ログ記録
  
  # 自動運転状態
  "selfdriveState": (100., True),      # 100Hz, ログ記録
  
  # センダー確認
  "sendcan": (100., False),            # 100Hz, ログなし（送信CAN）
}
```

---

### モデル系サービス（中周波数）

```python
SERVICE_LIST = {
  # ニューラルネットワークモデル
  "modelV2": (20., True),              # 20Hz, ログ記録
  
  # プラン
  "longitudinalPlan": (20., True),     # 20Hz, ログ記録
  "lateralPlan": (20., True),          # 20Hz, ログ記録
  
  # ドライバーモニタリング
  "driverStateV2": (10., True),        # 10Hz, ログ記録
  "driverMonitoringState": (10., True),# 10Hz, ログ記録
  
  # レーダー
  "radarState": (20., True),           # 20Hz, ログ記録
}
```

---

### センサー系サービス（高周波数）

```python
SERVICE_LIST = {
  # CAN
  "can": (100., True),                 # 100Hz, ログ記録
  
  # IMU（慣性計測装置）
  "accelerometer": (104., True),       # 104Hz, ログ記録
  "gyroscope": (104., True),           # 104Hz, ログ記録
  "magnetometer": (100., True),        # 100Hz, ログ記録
  
  # GPS
  "gpsLocation": (10., True),          # 10Hz, ログ記録
  "gpsLocationExternal": (10., True),  # 10Hz, ログ記録
  
  # カメラ
  "roadCameraState": (20., True),      # 20Hz, ログ記録
  "driverCameraState": (20., True),    # 20Hz, ログ記録
  "wideRoadCameraState": (20., True),  # 20Hz, ログ記録
}
```

---

### デバイス状態系サービス（低周波数）

```python
SERVICE_LIST = {
  # デバイス
  "deviceState": (2., True),           # 2Hz, ログ記録
  "pandaStates": (2., True),           # 2Hz, ログ記録
  "peripheralState": (2., True),       # 2Hz, ログ記録
  
  # ログ管理
  "managerState": (2., True),          # 2Hz, ログ記録
  "uploaderState": (0., True),         # 0Hz（イベント駆動）
  
  # 温度監視
  "deviceThermalState": (1., True),    # 1Hz, ログ記録
}
```

---

### 完全なサービスリスト（周波数順）

| サービス名 | 周波数 | ログ | 間引き | 説明 |
|-----------|-------|------|-------|------|
| **高周波数（100Hz以上）** |
| `accelerometer` | 104 Hz | ○ | - | 加速度計 |
| `gyroscope` | 104 Hz | ○ | - | ジャイロスコープ |
| `controlsState` | 100 Hz | ○ | - | 制御状態 |
| `carState` | 100 Hz | ○ | - | 車両状態 |
| `carControl` | 100 Hz | × | - | 車両制御コマンド |
| `carOutput` | 100 Hz | × | - | 車両出力 |
| `sendcan` | 100 Hz | × | - | CAN送信 |
| `selfdriveState` | 100 Hz | ○ | - | 自動運転状態 |
| `can` | 100 Hz | ○ | - | CANメッセージ |
| `magnetometer` | 100 Hz | ○ | - | 磁気センサー |
| **中周波数（10-50Hz）** |
| `modelV2` | 20 Hz | ○ | - | NNモデル出力 |
| `longitudinalPlan` | 20 Hz | ○ | - | 縦プラン |
| `lateralPlan` | 20 Hz | ○ | - | 横プラン |
| `radarState` | 20 Hz | ○ | - | レーダー状態 |
| `roadCameraState` | 20 Hz | ○ | - | 道路カメラ |
| `driverCameraState` | 20 Hz | ○ | - | ドライバーカメラ |
| `wideRoadCameraState` | 20 Hz | ○ | - | 広角カメラ |
| `driverStateV2` | 10 Hz | ○ | - | ドライバー状態 |
| `driverMonitoringState` | 10 Hz | ○ | - | DM状態 |
| `gpsLocation` | 10 Hz | ○ | - | GPS位置 |
| `gpsLocationExternal` | 10 Hz | ○ | - | 外部GPS |
| **低周波数（1-5Hz）** |
| `deviceState` | 2 Hz | ○ | - | デバイス状態 |
| `pandaStates` | 2 Hz | ○ | - | panda状態 |
| `peripheralState` | 2 Hz | ○ | - | 周辺機器状態 |
| `managerState` | 2 Hz | ○ | - | マネージャー状態 |
| `deviceThermalState` | 1 Hz | ○ | - | 温度状態 |
| `liveCalibration` | 4 Hz | ○ | - | キャリブレーション |
| **極低周波数・イベント** |
| `carParams` | 0.02 Hz | ○ | - | 車両パラメータ（50秒に1回） |
| `uploaderState` | 0 Hz | ○ | - | アップロード（イベント） |
| `onroadEvents` | 0 Hz | ○ | - | 運転イベント |

---

## ログ間引き設定

### 間引きの仕組み

高周波数サービスは**間引き**してログ容量を削減します。

```python
SERVICE_LIST = {
  # 例：100Hz更新だが10Hzでログ記録
  "controlsState": (100., True),       # 間引きなし
  "carState": (100., True),            # 間引きなし
  "can": (100., True),                 # 間引きなし
}
```

実際の間引き処理は`loggerd`で実装:

```python
# system/loggerd/loggerd.py

DECIMATION = {
  "controlsState": 10,    # 10回に1回 → 10Hz記録
  "carState": 10,         # 10回に1回 → 10Hz記録
  "can": 2053,            # 2053回に1回 → 約0.05Hz記録
  # ... 他
}
```

**間引き例**:
- `controlsState`: 100Hz受信 → 10倍間引き → 10Hz記録
- `can`: 100Hz受信 → 2053倍間引き → ~0.05Hz記録（約20秒に3メッセージ）

---

## 実際の使用例

### 1. サービスの登録

新しいメッセージタイプを追加する際は`services.py`に登録が必須です。

**例：カスタムサービス追加**

```python
# cereal/services.py

SERVICE_LIST = {
  # ... 既存サービス
  
  # カスタム追加
  "customReserved0": (100., True),     # 100Hz, ログ記録
  "myDebugData": (10., False),         # 10Hz, ログなし
}
```

### 2. 送信側（Publisher）

```python
# selfdrive/controls/controlsd.py

import cereal.messaging as messaging

# PubMaster作成（services.pyから自動的に設定を読み込む）
pm = messaging.PubMaster(['controlsState', 'carControl'])

while True:
  # メッセージ作成
  msg = messaging.new_message('controlsState')
  cs = msg.controlsState
  
  # データ設定
  cs.enabled = True
  cs.vEgo = 25.0  # m/s
  
  # 送信（services.pyの100Hz設定に従う）
  pm.send('controlsState', msg)
  
  time.sleep(0.01)  # 100Hz = 10ms
```

### 3. 受信側（Subscriber）

```python
# selfdrive/ui/ui.py

import cereal.messaging as messaging

# SubMaster作成（services.pyから周波数とログ設定を自動読み込み）
sm = messaging.SubMaster(['controlsState', 'carState', 'modelV2'])

while True:
  # 更新（タイムアウト付き）
  sm.update(timeout=100)  # 100ms
  
  # データ取得
  if sm.updated['controlsState']:
    cs = sm['controlsState']
    print(f"vEgo: {cs.vEgo} m/s")
  
  if sm.updated['carState']:
    car = sm['carState']
    print(f"Gas: {car.gas}")
```

---

## サービス周波数の選択基準

### 制御ループ周波数（100Hz）

**対象**: リアルタイム制御が必要なデータ
- `controlsState`
- `carState`
- `carControl`
- `sendcan`

**理由**: 
- 10ms以下の応答時間が必要
- 安定した制御のため高頻度更新

### モデル処理周波数（20Hz）

**対象**: ニューラルネットワーク処理
- `modelV2`
- `longitudinalPlan`
- `lateralPlan`
- `radarState`

**理由**:
- GPU処理時間が30-40ms
- 50ms周期が適切

### センサー周波数（10-104Hz）

**対象**: 物理センサー
- `accelerometer`: 104Hz（ハードウェア制約）
- `gyroscope`: 104Hz
- `gpsLocation`: 10Hz（GPS更新レート）

**理由**: ハードウェアの仕様に依存

### 状態監視周波数（1-2Hz）

**対象**: 低頻度モニタリング
- `deviceState`: 2Hz
- `pandaStates`: 2Hz
- `deviceThermalState`: 1Hz

**理由**: 
- リアルタイム性不要
- ログ容量削減

### イベント駆動（0Hz）

**対象**: 発生時のみ送信
- `uploaderState`
- `onroadEvents`

**理由**: 
- 定期的な更新が不要
- イベント発生時のみ通知

---

## ログ容量の計算

### 1時間のドライブログ容量

**高頻度ログ**:
```
controlsState: 100 Hz × 3600s × 200 bytes ≈ 72 MB/h
carState:      100 Hz × 3600s × 300 bytes ≈ 108 MB/h
can:           0.05Hz × 3600s × 100 bytes ≈ 0.02 MB/h（間引き後）
```

**中頻度ログ**:
```
modelV2:       20 Hz × 3600s × 2000 bytes ≈ 144 MB/h
```

**低頻度ログ**:
```
deviceState:   2 Hz × 3600s × 500 bytes ≈ 3.6 MB/h
```

**合計**:
約 **1-2 GB/h** （圧縮前）

**圧縮後**:
約 **200-400 MB/h** （Cap'n Proto + gzip圧縮）

---

## サービス依存関係

### プロセス間データフロー

```
[carstate]
  ↓ carState (100Hz)
[controlsd]
  ↓ controlsState (100Hz)
  ↓ carControl (100Hz)
[carcontroller]
  ↓ sendcan (100Hz)
[boardd]
  ↓ (CANバスへ送信)
```

```
[camerad]
  ↓ roadCameraState (20Hz)
[modeld]
  ↓ modelV2 (20Hz)
[plannerd]
  ↓ lateralPlan (20Hz)
  ↓ longitudinalPlan (20Hz)
[controlsd]
  ↓ carControl (100Hz)
```

### 依存関係マップ

| 送信プロセス | サービス名 | 受信プロセス |
|------------|-----------|------------|
| `carstate` | `carState` | `controlsd`, `plannerd`, `ui` |
| `controlsd` | `controlsState` | `ui`, `loggerd` |
| `controlsd` | `carControl` | `carcontroller` |
| `modeld` | `modelV2` | `plannerd`, `ui` |
| `plannerd` | `lateralPlan` | `controlsd` |
| `plannerd` | `longitudinalPlan` | `controlsd` |
| `boardd` | `can` | `carstate`, `loggerd` |
| `boardd` | `pandaStates` | `ui`, `loggerd` |

---

## カスタムサービスの追加手順

### ステップ1：services.pyに追加

```python
# cereal/services.py

SERVICE_LIST = {
  # ... 既存サービス
  
  # 新規サービス
  "myCustomService": (50., True),  # 50Hz, ログ記録
}
```

### ステップ2：log.capnpに定義

```capnp
# cereal/log.capnp

struct Event {
  union {
    # ... 既存メッセージ
    
    myCustomService @200 :MyCustomService;
  }
}

struct MyCustomService {
  data @0 :Float32;
  timestamp @1 :UInt64;
}
```

または`custom.capnp`を使用:

```capnp
# cereal/custom.capnp

struct CustomReserved0 {
  data @0 :Float32;
  timestamp @1 :UInt64;
}
```

```python
# cereal/services.py
SERVICE_LIST = {
  "customReserved0": (50., True),
}
```

### ステップ3：再ビルド

```bash
cd /home/takuya/work/comma/openpilot/cereal
scons -j8
```

### ステップ4：送信プロセス作成

```python
# selfdrive/myprocess/myprocessd.py

import cereal.messaging as messaging
import time

def main():
  pm = messaging.PubMaster(['myCustomService'])
  
  while True:
    msg = messaging.new_message('myCustomService')
    msg.myCustomService.data = 123.45
    msg.myCustomService.timestamp = int(time.time() * 1e9)
    
    pm.send('myCustomService', msg)
    time.sleep(0.02)  # 50Hz

if __name__ == '__main__':
  main()
```

### ステップ5：manager.pyに登録

```python
# selfdrive/manager/manager.py

PROCS = {
  # ... 既存プロセス
  
  "myprocessd": ("selfdrive.myprocess.myprocessd", True),
}
```

---

## トラブルシューティング

### 問題1：サービスが見つからない

**エラー**:
```
RuntimeError: service myCustomService not found
```

**原因**: `services.py`に登録されていない

**解決策**:
```python
# services.pyに追加
SERVICE_LIST = {
  "myCustomService": (50., True),
}
```

### 問題2：周波数が合わない

**症状**: データが途切れる、遅延が大きい

**原因**: 送信周波数と`services.py`の設定が不一致

**解決策**:
```python
# services.pyで50Hz設定
SERVICE_LIST = {
  "myService": (50., True),
}

# 送信側も50Hz（20ms）で送信
time.sleep(0.02)
```

### 問題3：ログ容量が大きすぎる

**原因**: 高周波数サービスを間引きなしで記録

**解決策**:
```python
# loggerd.pyで間引き設定
DECIMATION = {
  "myHighFreqService": 10,  # 10倍間引き
}
```

---

## 高度な使用例

### イベント駆動サービス

```python
# services.py
SERVICE_LIST = {
  "myEventService": (0., True),  # 0Hz = イベント駆動
}

# 送信側
pm = messaging.PubMaster(['myEventService'])

def on_event():
  msg = messaging.new_message('myEventService')
  msg.myEventService.event = "collision_detected"
  pm.send('myEventService', msg)

# 条件発生時のみ送信
if collision_detected:
  on_event()
```

### 可変周波数サービス

```python
# services.py（基準周波数）
SERVICE_LIST = {
  "adaptiveService": (20., True),  # 基準20Hz
}

# 送信側（実際は状況に応じて変動）
while True:
  if high_priority:
    freq = 100  # 高優先度時は100Hz
  else:
    freq = 10   # 通常時は10Hz
  
  # ... 送信処理
  time.sleep(1.0 / freq)
```

---

## まとめ

`services.py`の主要ポイント:

1. **一元管理**: すべてのサービスの周波数とログ設定を定義
2. **自動設定**: PubMaster/SubMasterが自動的に読み込む
3. **ログ最適化**: 間引き設定でログ容量を削減
4. **カスタム追加**: 新規サービスは必ずservices.pyに登録
5. **周波数設計**: 用途に応じて適切な周波数を選択
   - 制御: 100Hz
   - モデル: 20Hz
   - 監視: 1-2Hz
   - イベント: 0Hz

---

## 関連ドキュメント

- [README.md](README.md) - cereal全体の概要
- [log.capnp詳細](log_capnp.md) - メインメッセージ定義
- [messaging API詳細](messaging_api.md) - PubMaster/SubMaster使用方法

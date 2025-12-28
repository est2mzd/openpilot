# log.capnp - メインメッセージ定義詳細

## 概要

`log.capnp`は、openpilotの**全メッセージタイプ**を定義する最重要ファイルです。

- **ファイルパス**: `/home/takuya/work/comma/openpilot/cereal/log.capnp`
- **行数**: 2,671行
- **中核構造体**: `Event`
- **役割**: すべてのプロセス間通信メッセージの統一インターフェース

---

## Event構造体

### 基本構造

```capnp
struct Event {
  # すべてのメッセージに共通
  logMonoTime @0 :UInt64;  # モノトニッククロック（ナノ秒）
  valid @1 :Bool;           # データの有効性フラグ
  
  # メッセージタイプ（union: 1つだけ選択）
  union {
    initData @2 :InitData;
    frameData @3 :FrameData;
    controlsState @4 :ControlsState;
    carState @5 :Car.CarState;
    # ... 100以上のメッセージタイプ
  }
}
```

### logMonoTime

**目的**: タイムスタンプ（単調増加時計）

- **型**: UInt64
- **単位**: ナノ秒
- **ソース**: `time.monotonic()` × 10^9
- **用途**: 
  - イベントの時系列順序付け
  - ログ再生時の同期
  - レイテンシ計測

### valid

**目的**: データ有効性フラグ

- **型**: Bool
- **意味**:
  - `true`: データは有効で使用可能
  - `false`: データは無効（初期化中、エラー等）
- **用途**: データ品質チェック

---

## 主要メッセージタイプ

### 1. 制御系メッセージ

#### ControlsState (@0x97ff69c53601abf1)

**openpilotの制御状態**

```capnp
struct ControlsState {
  enabled @0 :Bool;                    # openpilot有効
  active @1 :Bool;                     # アクティブ制御中
  
  vEgo @2 :Float32;                    # 車両速度 (m/s)
  vEgoRaw @3 :Float32;                 # 生の車速データ
  aEgo @4 :Float32;                    # 車両加速度 (m/s²)
  
  curvature @5 :Float32;               # 経路曲率 (1/m)
  
  state @6 :OpenpilotState;            # システム状態
  enum OpenpilotState {
    disabled @0;
    preEnabled @1;
    enabled @2;
    softDisabling @3;
  }
  
  longControlState @7 :LongControlState;  # 縦制御状態
  enum LongControlState {
    off @0;
    pid @1;
    stopping @2;
    starting @3;
  }
  
  lateralControlState @8 :LateralControlState;  # 横制御状態
  
  alertText1 @9 :Text;                 # アラートテキスト（上）
  alertText2 @10 :Text;                # アラートテキスト（下）
  alertStatus @11 :AlertStatus;        # アラート重要度
  alertSize @12 :AlertSize;            # アラート表示サイズ
  alertSound @13 :AudibleAlert;        # アラート音
  
  canMonoTimes @14 :List(UInt64);      # CANメッセージ受信時刻
  
  longitudinalPlanMonoTime @15 :UInt64;  # 縦プラン計算時刻
  lateralPlanMonoTime @16 :UInt64;       # 横プラン計算時刻
  
  # ... 他多数のフィールド
}
```

**主要フィールド解説**:

- **enabled**: openpilotが有効かどうか（緑色表示）
- **active**: 実際に制御中かどうか
- **vEgo**: 車両速度（制御ループで最も重要な値）
- **curvature**: 経路の曲率（ステアリング制御に使用）
- **state**: システム全体の状態機械
- **alertText1/2**: UI表示用のアラートメッセージ

**更新周波数**: 100Hz（10ms毎）

#### SelfdriveState

**自動運転の高レベル状態**

```capnp
struct SelfdriveState {
  enabled @0 :Bool;                    # 自動運転有効
  active @1 :Bool;                     # アクティブ制御中
  
  state @2 :OpenpilotState;            # システム状態
  engageable @3 :Bool;                 # エンゲージ可能
  
  alertText1 @4 :Text;                 # アラートテキスト
  alertText2 @5 :Text;
  alertStatus @6 :AlertStatus;
  alertSize @7 :AlertSize;
  alertSound @8 :AudibleAlert;
  
  personality @9 :Personality;         # 運転個性
  enum Personality {
    aggressive @0;
    standard @1;
    relaxed @2;
  }
}
```

**用途**: UIでの状態表示、高レベルロジック

### 2. 車両状態メッセージ

#### CarState (car.capnpから)

**車両の現在状態**

```capnp
struct CarState {
  # 速度と加速度
  vEgo @0 :Float32;                    # 車速 (m/s)
  vEgoRaw @1 :Float32;                 # 生車速
  aEgo @2 :Float32;                    # 加速度 (m/s²)
  vEgoCluster @3 :Float32;             # クラスター表示速度
  
  # ステアリング
  steeringAngleDeg @4 :Float32;        # ステアリング角度 (度)
  steeringAngleOffsetDeg @5 :Float32;  # オフセット
  steeringTorque @6 :Float32;          # トルク (Nm)
  steeringTorqueEps @7 :Float32;       # EPS測定トルク
  steeringPressed @8 :Bool;            # ユーザーがハンドル操作中
  steeringRateDeg @9 :Float32;         # ステアリング角速度
  
  # ペダル
  gas @10 :Float32;                    # アクセル (0-1)
  gasPressed @11 :Bool;                # アクセル踏み込み
  brake @12 :Float32;                  # ブレーキ (0-1)
  brakePressed @13 :Bool;              # ブレーキ踏み込み
  
  # ギア
  gearShifter @14 :GearShifter;
  enum GearShifter {
    unknown @0;
    park @1;
    drive @2;
    neutral @3;
    reverse @4;
    sport @5;
    low @6;
    brake @7;
    eco @8;
  }
  
  # クルーズコントロール
  cruiseState @15 :CruiseState;
  struct CruiseState {
    enabled @0 :Bool;
    speed @1 :Float32;
    speedCluster @2 :Float32;
    available @3 :Bool;
    speedOffset @4 :Float32;
    standstill @5 :Bool;
  }
  
  # ドア、シートベルト等
  doorOpen @16 :Bool;
  seatbeltUnlatched @17 :Bool;
  leftBlinker @18 :Bool;
  rightBlinker @19 :Bool;
  
  # ... 他多数
}
```

**更新周波数**: 100Hz

#### CarControl (car.capnpから)

**車両への制御コマンド**

```capnp
struct CarControl {
  enabled @0 :Bool;                    # 制御有効
  latActive @1 :Bool;                  # 横制御アクティブ
  longActive @2 :Bool;                 # 縦制御アクティブ
  
  actuators @3 :Actuators;
  struct Actuators {
    # 横制御
    steer @0 :Float32;                 # ステアリング指令 (-1 to 1)
    steeringAngleDeg @1 :Float32;      # 角度指令 (度)
    
    # 縦制御
    accel @2 :Float32;                 # 加速度指令 (m/s²)
    longControlState @3 :LongControlState;
    
    # その他
    speed @4 :Float32;                 # 速度指令
    curvature @5 :Float32;             # 曲率
  }
  
  hudControl @4 :HUDControl;           # HUD表示制御
  cruiseControl @5 :CruiseControl;     # クルーズ制御
}
```

**更新周波数**: 100Hz

### 3. モデル系メッセージ

#### ModelDataV2

**ニューラルネットワークモデルの出力**

```capnp
struct ModelDataV2 {
  frameId @0 :UInt32;                  # フレームID
  frameIdExtra @1 :UInt32;             # 追加フレームID
  frameAge @2 :UInt32;                 # フレーム経過時間
  frameDropPerc @3 :Float32;           # フレームドロップ率
  timestampEof @4 :UInt64;             # End of Frameタイムスタンプ
  modelExecutionTime @5 :Float32;      # モデル実行時間 (秒)
  gpuExecutionTime @6 :Float32;        # GPU実行時間
  
  # 経路予測
  position @7 :XYZTData;               # 位置 (x, y, z, t)
  orientation @8 :XYZTData;            # 姿勢
  velocity @9 :XYZTData;               # 速度
  orientationRate @10 :XYZTData;       # 姿勢変化率
  acceleration @11 :XYZTData;          # 加速度
  
  # 車線検出
  laneLines @12 :List(XYZTData);       # 車線線 (左から右)
  laneLineProbs @13 :List(Float32);    # 確信度
  laneLineStds @14 :List(XYZTData);    # 標準偏差
  
  # 道路エッジ
  roadEdges @15 :List(XYZTData);
  roadEdgeStds @16 :List(XYZTData);
  
  # 鉛車両（先行車）
  leads @17 :List(LeadDataV2);
  struct LeadDataV2 {
    prob @0 :Float32;                  # 検出確率
    t @1 :Float32;                     # 時間 (秒)
    xyzt @2 :List(Float32);            # 位置 (x, y, z, t)
    xyztStd @3 :List(Float32);         # 標準偏差
  }
  
  # メタ情報
  meta @18 :MetaData;
  struct MetaData {
    engagedProb @0 :Float32;           # エンゲージ推奨確率
    desirePrediction @1 :List(Float32); # 意図予測
    desireState @2 :List(Float32);     # 意図状態
  }
}
```

**主要データ**:
- **position**: 経路の位置予測（x, y座標）
- **laneLines**: 検出された車線線（通常4本: 左外、左、右、右外）
- **leads**: 先行車の位置と速度
- **meta.engagedProb**: openpilot推奨確率

**更新周波数**: 20Hz

**データ形式 - XYZTData**:
```capnp
struct XYZTData {
  x @0 :List(Float32);  # x座標の時系列 (通常33点)
  y @1 :List(Float32);  # y座標の時系列
  z @2 :List(Float32);  # z座標（高さ）
  t @3 :List(Float32);  # 時間（秒）
  xStd @4 :List(Float32);  # x標準偏差
  yStd @5 :List(Float32);  # y標準偏差
  zStd @6 :List(Float32);  # z標準偏差
}
```

### 4. プラン系メッセージ

#### LongitudinalPlan (@0xe00b5b3eba12876c)

**縦方向（加減速）プラン**

```capnp
struct LongitudinalPlan {
  # 加速度プラン
  accels @0 :List(Float32);            # 加速度計画 (m/s²)
  speeds @1 :List(Float32);            # 速度計画 (m/s)
  jerks @2 :List(Float32);             # ジャーク計画 (m/s³)
  
  # ターゲット
  aTarget @3 :Float32;                 # 目標加速度
  vTarget @4 :Float32;                 # 目標速度
  vTargetFuture @5 :Float32;           # 将来の目標速度
  
  # ソース
  longitudinalPlanSource @6 :LongitudinalPlanSource;
  enum LongitudinalPlanSource {
    cruise @0;                         # クルーズコントロール
    lead0 @1;                          # 先行車追従
    lead1 @2;                          # 2台目の先行車
    lead2 @3;
    e2e @4;                            # End-to-End (experimental)
  }
  
  # 制御フラグ
  hasLead @7 :Bool;                    # 先行車あり
  fcw @8 :Bool;                        # Forward Collision Warning
  longitudinalPlanMonoTime @9 :UInt64;
  
  # ソリューション詳細
  solverExecutionTime @10 :Float32;    # ソルバー実行時間
  
  # 制約
  aTargetMin @11 :Float32;             # 加速度下限
  aTargetMax @12 :Float32;             # 加速度上限
  
  # 距離
  xState @13 :List(Float32);           # 位置状態
  vState @14 :List(Float32);           # 速度状態
  aState @15 :List(Float32);           # 加速度状態
}
```

**更新周波数**: 20Hz

#### LateralPlan (@0xe1e9318e2ae8b51e)

**横方向（ステアリング）プラン**

```capnp
struct LateralPlan {
  # 経路
  psis @0 :List(Float32);              # ヨー角 (rad)
  curvatures @1 :List(Float32);        # 曲率 (1/m)
  curvatureRates @2 :List(Float32);    # 曲率変化率
  
  # ターゲット
  laneWidth @3 :Float32;               # 車線幅 (m)
  
  # 車線変更
  dPathPoints @4 :List(Float32);       # 経路偏差
  
  # メタ情報
  desire @5 :Desire;
  enum Desire {
    none @0;
    turnLeft @1;
    turnRight @2;
    laneChangeLeft @3;
    laneChangeRight @4;
    keepLeft @5;
    keepRight @6;
  }
  
  laneChangeState @6 :LaneChangeState;
  enum LaneChangeState {
    off @0;
    preLaneChange @1;
    laneChangeStarting @2;
    laneChangeFinishing @3;
  }
  
  laneChangeDirection @7 :LaneChangeDirection;
  enum LaneChangeDirection {
    none @0;
    left @1;
    right @2;
  }
  
  useLaneLines @8 :Bool;               # 車線線使用中
  
  lateralPlanMonoTime @9 :UInt64;
  solverExecutionTime @10 :Float32;
}
```

**更新周波数**: 20Hz

### 5. センサー系メッセージ

#### SensorEventData

**IMU（慣性計測装置）データ**

```capnp
struct SensorEventData {
  version @0 :Int32;
  sensor @1 :Int32;                    # センサータイプ
  type @2 :Int32;                      # データタイプ
  timestamp @3 :Int64;                 # タイムスタンプ
  
  # 加速度計
  acceleration @4 :SensorVec;
  struct SensorVec {
    v @0 :List(Float32);               # [x, y, z]
    status @1 :Int8;
  }
  
  # ジャイロスコープ
  gyro @5 :SensorVec;
  gyroUncalibrated @6 :SensorVec;
  
  # 磁気センサー
  magnetic @7 :SensorVec;
  magneticUncalibrated @8 :SensorVec;
  
  # 光センサー
  light @9 :Float32;
  
  # 温度
  temperature @10 :Float32;
}
```

**センサー周波数**:
- 加速度計: 104Hz
- ジャイロ: 104Hz
- 磁気: 25Hz

#### GpsLocationData

**GPS位置情報**

```capnp
struct GpsLocationData {
  # 位置
  latitude @0 :Float64;                # 緯度 (度)
  longitude @1 :Float64;               # 経度 (度)
  altitude @2 :Float64;                # 高度 (m)
  
  # 精度
  accuracy @3 :Float32;                # 位置精度 (m)
  bearingAccuracyDeg @4 :Float32;      # 方位精度 (度)
  speedAccuracy @5 :Float32;           # 速度精度 (m/s)
  verticalAccuracy @6 :Float32;        # 垂直精度 (m)
  
  # 速度と方位
  speed @7 :Float32;                   # 速度 (m/s)
  bearingDeg @8 :Float32;              # 方位 (度)
  
  # メタ
  timestamp @9 :Int64;
  source @10 :SensorSource;
  enum SensorSource {
    android @0;
    iOS @1;
    car @2;
    velodyne @3;
    fusion @4;
    external @5;
    ublox @6;
    trimble @7;
    qcomdiag @8;
  }
  
  # GPS固有
  vNED @11 :List(Float32);             # NED座標系速度 [北, 東, 下]
  verticalAccuracy @12 :Float32;
  bearingAccuracyDeg @13 :Float32;
  speedAccuracy @14 :Float32;
}
```

**更新周波数**: 1-10Hz（ソースによる）

#### CanData

**CANバスメッセージ**

```capnp
struct CanData {
  address @0 :UInt32;                  # CANメッセージID
  busTime @1 :UInt16;                  # バスタイムスタンプ
  dat @2 :Data;                        # データ（最大8バイト）
  src @3 :UInt8;                       # バス番号 (0, 1, 2)
}
```

**更新周波数**: 100Hz（CANバスから連続受信）

### 6. デバイス状態メッセージ

#### DeviceState (@0xa4d8b5af2aa492eb)

**comma deviceの状態**

```capnp
struct DeviceState {
  cpuUsagePercent @0 :List(Int8);      # CPU使用率（コアごと）
  gpuUsagePercent @1 :Int8;            # GPU使用率
  memoryUsagePercent @2 :Int8;         # メモリ使用率
  
  # 温度
  cpuTempC @3 :List(Int16);            # CPU温度 (°C)
  gpuTempC @4 :List(Int16);            # GPU温度
  ambientTempC @5 :Float32;            # 周囲温度
  
  # ストレージ
  freeSpacePercent @6 :Float32;        # 空き容量 (%)
  
  # バッテリー
  batteryPercent @7 :Int16;            # バッテリー残量
  batteryCurrent @8 :Int32;            # 電流 (mA)
  batteryVoltage @9 :Int32;            # 電圧 (mV)
  batteryTempC @10 :Int16;             # 温度 (°C)
  batteryStatus @11 :Text;             # ステータス
  
  # ネットワーク
  networkType @12 :NetworkType;
  enum NetworkType {
    none @0;
    wifi @1;
    cell2G @2;
    cell3G @3;
    cell4G @4;
    cell5G @5;
    ethernet @6;
  }
  
  networkStrength @13 :NetworkStrength;
  
  # ファン
  fanSpeedPercentDesired @14 :UInt16;  # 目標ファン速度
  
  # 起動時間
  startedMonoTime @15 :UInt64;
  
  # 画面
  screenBrightnessPercent @16 :Int8;
  
  # オフロード
  offroad @17 :Bool;                   # 車外モード
}
```

**更新周波数**: 2Hz

#### PandaState (@0xa7649e2575e4591e)

**pandaデバイスの状態**

```capnp
struct PandaState {
  pandaType @0 :PandaType;
  enum PandaType {
    unknown @0;
    whitePanda @1;
    greyPanda @2;
    blackPanda @3;
    redPanda @4;
    redPandaV2 @5;
    tres @6;
  }
  
  # 電圧
  voltage @1 :UInt32;                  # 電圧 (mV)
  current @2 :UInt32;                  # 電流 (mA)
  
  # CAN統計
  safetyModel @3 :SafetyModel;
  canSendErrs @4 :UInt32;              # CAN送信エラー数
  canFwdErrs @5 :UInt32;               # CAN転送エラー数
  canRxErrs @6 :UInt32;                # CAN受信エラー数
  
  gmlanSendErrs @7 :UInt32;
  
  # ハードウェア
  pandaType @8 :PandaType;
  controlsAllowed @9 :Bool;            # 制御許可
  
  gasInterceptorDetected @10 :Bool;    # ガスインターセプター検出
  
  safetyRxChecksInvalid @11 :Bool;     # 安全チェック失敗
  
  # ファームウェア
  harnessStatus @12 :HarnessStatus;
  enum HarnessStatus {
    notConnected @0;
    normal @1;
    flipped @2;
  }
  
  # 追加統計
  heartbeatLost @13 :Bool;
  blocked @14 :Bool;
  
  safetyParam @15 :UInt16;             # 安全パラメータ
  safetyParam2 @16 :UInt32;
  
  faultStatus @17 :FaultStatus;
  enum FaultStatus {
    none @0;
    faultTemp @1;
    faultPerm @2;
  }
  
  powerSaveEnabled @18 :Bool;
  uptime @19 :UInt32;                  # 起動時間 (秒)
  
  fanPower @20 :UInt8;                 # ファン電力 (%)
  fanStallCount @21 :UInt8;            # ファン停止回数
}
```

**更新周波数**: 10Hz

---

## イベントシステム

### OnroadEvent

**運転中イベント（アラート、警告等）**

```capnp
struct OnroadEvent {
  name @0 :EventName;                  # イベント名（100以上の種類）
  
  # イベントタイプフラグ
  enable @1 :Bool;                     # エンゲージ許可
  noEntry @2 :Bool;                    # エンゲージ不可
  warning @3 :Bool;                    # 警告
  userDisable @4 :Bool;                # ユーザー無効化
  softDisable @5 :Bool;                # ソフト無効化
  immediateDisable @6 :Bool;           # 即座に無効化
  preEnable @7 :Bool;                  # プリエンゲージ
  permanent @8 :Bool;                  # 常時表示
  overrideLateral @10 :Bool;           # 横制御オーバーライド
  overrideLongitudinal @9 :Bool;       # 縦制御オーバーライド
  
  enum EventName {
    # システムエラー
    canError @0;
    commIssue @43;
    controlsMismatch @22;
    
    # ドライバー状態
    driverDistracted @35;
    driverUnresponsive @38;
    seatbeltNotLatched @4;
    doorOpen @3;
    
    # 車両状態
    wrongGear @2;
    reverseGear @8;
    parkBrake @27;
    brakeHold @26;
    speedTooLow @16;
    speedTooHigh @56;
    
    # 安全システム
    stockAeb @52;                      # 純正AEB作動
    stockFcw @59;                      # 純正FCW作動
    fcw @65;                           # openpilot FCW
    
    # エンゲージメント
    buttonEnable @10;                  # ボタンでエンゲージ
    buttonCancel @9;                   # ボタンでキャンセル
    pedalPressed @11;                  # ペダル踏み込み
    steerOverride @14;                 # ハンドル操作
    
    # キャリブレーション
    calibrationIncomplete @19;
    calibrationInvalid @20;
    
    # ハードウェア
    overheat @18;
    lowBattery @40;
    fanMalfunction @91;
    cameraMalfunction @92;
    
    # 車線変更
    preLaneChangeLeft @48;
    preLaneChangeRight @49;
    laneChange @50;
    laneChangeBlocked @57;
    
    # ... 他90以上
  }
}
```

**イベント処理フロー**:
1. 各プロセスが`OnroadEvent`を生成
2. `selfdrived`が集約
3. `controlsd`が状態遷移を判定
4. UIにアラート表示

---

## ログ記録

### ログバージョン

```capnp
const logVersion :Int32 = 1;
```

**用途**: ログフォーマットのバージョン管理

### 主要ログ対象メッセージ

すべてのメッセージは`services.py`で定義された設定に従ってログ記録されます。

**高頻度ログ（重要データ）**:
- `controlsState`: 100Hz → 10倍間引き → 10Hz記録
- `carState`: 100Hz → 10倍間引き → 10Hz記録
- `modelV2`: 20Hz → 間引きなし → 20Hz記録

**低頻度ログ（メタデータ）**:
- `deviceState`: 2Hz → 1倍間引き → 2Hz記録
- `pandaStates`: 10Hz → 1倍間引き → 10Hz記録

**極度に間引かれたログ**:
- `can`: 100Hz → 2053倍間引き → ~0.05Hz記録（約3メッセージ/20秒）

---

## まとめ

`log.capnp`の主要ポイント:

1. **Event構造体**: すべてのメッセージの統一インターフェース
2. **100以上のメッセージタイプ**: 制御、車両、モデル、センサー、デバイス等
3. **高頻度更新**: 制御系は100Hz、モデル系は20Hz
4. **SI単位系**: すべての物理量はm/s、m/s²、rad等で統一
5. **後方互換性**: 構造体追加は安全、既存フィールド変更は危険
6. **ログ記録**: services.pyで定義された周波数と間引き設定

---

## 関連ドキュメント

- [README.md](README.md) - cereal全体の概要
- [car.capnp詳細](car_capnp.md) - 車両関連メッセージ詳細
- [services詳細](services.md) - サービス定義とログ設定

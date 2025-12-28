# car.capnp - 車両関連メッセージ詳細

## 概要

`car.capnp`は、openpilotの**車両インターフェース**を定義するファイルです。

- **ファイルパス**: `/home/takuya/work/comma/openpilot/cereal/car.capnp`
- **行数**: 740行
- **主要構造体**: `CarParams`, `CarState`, `CarControl`, `RadarData`
- **役割**: 車種ごとの特性定義と車両状態の抽象化

---

## 主要構造体

### 1. CarParams (@0x8e2af1e708af8b8d)

**車両パラメータ（起動時に1回設定）**

```capnp
struct CarParams {
  # 基本情報
  carName @0 :Text;                    # 車名（例: "TOYOTA PRIUS"）
  carFingerprint @1 :Text;             # 車種フィンガープリント
  carVin @66 :Text;                    # VIN（車両識別番号）
  
  # プラットフォーム
  platform @88 :Text;                  # プラットフォーム名
  
  # ブランド/モデル
  brand @2 :Brand;
  enum Brand {
    custom @0;
    tesla @1;
    toyota @2;
    honda @3;
    nissan @4;
    mazda @5;
    hyundai @6;
    chrysler @7;
    subaru @8;
    gm @9;
    volkswagen @10;
    ford @11;
    body @12;
    # ... 他
  }
  
  # 安全性
  safetyModel @3 :SafetyModel;
  enum SafetyModel {
    silent @0;                         # 制御なし（観察のみ）
    hondaNidec @1;
    toyota @2;
    elm327 @3;
    gm @4;
    hondaBosch @5;
    ford @6;
    cadillac @7;
    hyundai @8;
    chrysler @9;
    tesla @10;
    subaru @11;
    gmPassive @12;
    mazda @13;
    nissan @14;
    volkswagen @15;
    toyotaIpas @16;
    allOutput @17;
    gmAscm @18;
    noOutput @19;
    hondaBoschGiraffe @20;
    volkswagenPq @21;
    subaruLegacy @22;
    hyundaiLegacy @23;
    volkswagenMlb @24;
    toyotaSafetySense1 @25;
    toyotaSafetySense2 @26;
  }
  
  safetyParam @67 :Int16;              # 安全パラメータ
  safetyParam2 @94 :UInt32;            # 追加安全パラメータ
  
  # 質量と寸法
  mass @4 :Float32;                    # 車両質量 (kg)
  wheelbase @5 :Float32;               # ホイールベース (m)
  centerToFront @6 :Float32;           # 重心から前軸 (m)
  steerRatio @7 :Float32;              # ステアリングレシオ
  steerRatioRear @37 :Float32;         # 後輪ステアリングレシオ
  
  # 回転慣性
  rotationalInertia @8 :Float32;       # 回転慣性 (kg·m²)
  tireStiffnessFactor @39 :Float32;    # タイヤ剛性係数
  
  # 縦制御（加減速）
  longitudinalTuning @10 :LongitudinalPIDTuning;
  struct LongitudinalPIDTuning {
    kpBP @0 :List(Float32);            # 比例ゲイン breakpoints
    kpV @1 :List(Float32);             # 比例ゲイン values
    kiBP @2 :List(Float32);            # 積分ゲイン breakpoints
    kiV @3 :List(Float32);             # 積分ゲイン values
    kf @6 :Float32;                    # フィードフォワードゲイン
    deadzone @7 :Float32;              # デッドゾーン
  }
  
  # 横制御（ステアリング）
  lateralTuning @9 :LateralTuning;
  union LateralTuning {
    pid @0 :LateralPIDTuning;
    indi @1 :LateralINDITuning;
    lqr @2 :LateralLQRTuning;
    torque @3 :LateralTorqueTuning;
  }
  
  struct LateralPIDTuning {
    kpBP @0 :List(Float32);
    kpV @1 :List(Float32);
    kiBP @2 :List(Float32);
    kiV @3 :List(Float32);
    kf @3 :Float32;
  }
  
  struct LateralINDITuning {
    outerLoopGainBP @4 :List(Float32);
    outerLoopGainV @5 :List(Float32);
    innerLoopGain @6 :Float32;
    outerLoopGain @7 :Float32;
    timeConstant @8 :Float32;
    actuatorEffectiveness @9 :Float32;
  }
  
  struct LateralLQRTuning {
    scale @0 :Float32;
    ki @1 :Float32;
    dcGain @2 :Float32;
  }
  
  struct LateralTorqueTuning {
    useSteeringAngle @0 :Bool;
    kp @1 :Float32;
    ki @2 :Float32;
    friction @3 :Float32;
    kf @4 :Float32;
    steeringAngleDeadzoneDeg @5 :Float32;
  }
  
  # ステアリング限界
  steerMaxBP @11 :List(Float32);       # 最大ステアリング breakpoints (m/s)
  steerMaxV @12 :List(Float32);        # 最大ステアリング values (deg)
  
  steerActuatorDelay @13 :Float32;     # ステアリング遅延 (秒)
  steerRateCost @14 :Float32;          # ステアリング変化コスト
  steerLimitTimer @47 :Float32;        # ステアリング制限タイマー
  
  # 制御機能
  openpilotLongitudinalControl @17 :Bool;  # openpilot縦制御有効
  carVin @66 :Text;                    # VIN
  
  # クルーズコントロール
  minEnableSpeed @20 :Float32;         # 最小エンゲージ速度 (m/s)
  minSteerSpeed @21 :Float32;          # 最小ステアリング速度 (m/s)
  
  # ペダル
  gasMaxBP @22 :List(Float32);         # 最大アクセル breakpoints
  gasMaxV @23 :List(Float32);          # 最大アクセル values
  
  brakeMaxBP @24 :List(Float32);       # 最大ブレーキ breakpoints
  brakeMaxV @25 :List(Float32);        # 最大ブレーキ values
  
  # 純正ACC/LKAS
  pcmCruise @26 :Bool;                 # 純正クルーズあり
  communityFeature @46 :Bool;          # コミュニティ機能使用
  
  # レーダー
  radarUnavailable @35 :Bool;          # レーダーなし
  
  # 代替エクスペリエンス
  enableGasInterceptor @33 :Bool;      # ガスインターセプター有効
  enableCamera @19 :Bool;              # カメラ制御有効
  enableDsu @41 :Bool;                 # DSU有効
  enableBsm @56 :Bool;                 # BSM有効
  
  # 駐車支援
  enableApgs @30 :Bool;                # APGS有効
  
  # ネットワーク
  networkLocation @64 :NetworkLocation;
  enum NetworkLocation {
    fwdCamera @0;                      # 前方カメラCAN
    gateway @1;                        # ゲートウェイCAN
  }
  
  # タイミング
  radarTimeStep @59 :Float32;          # レーダー更新間隔 (秒)
  
  # ドキュメント
  transmissionType @65 :TransmissionType;
  enum TransmissionType {
    unknown @0;
    automatic @1;
    manual @2;
    direct @3;
  }
  
  carFw @68 :List(CarFw);              # ファームウェアバージョン
  struct CarFw {
    ecu @0 :Ecu;
    fwVersion @1 :Data;
    address @2 :UInt32;
    subAddress @3 :UInt32;
    
    enum Ecu {
      eps @0;                          # EPS（電動パワステ）
      abs @1;                          # ABS
      fwdRadar @2;                     # 前方レーダー
      fwdCamera @3;                    # 前方カメラ
      engine @4;                       # エンジンECU
      transmission @5;                 # トランスミッション
      hybrid @6;                       # ハイブリッドECU
      srs @7;                          # エアバッグ
      gateway @8;                      # ゲートウェイ
      hud @9;                          # HUD
      combinationMeter @10;            # メーター
      electricBrakeBooster @11;        # 電動ブレーキブースター
      shiftByWire @12;                 # シフトバイワイヤ
      adas @13;                        # ADAS ECU
      # ... 他
    }
  }
  
  # 停止制御
  stopAccel @60 :Float32;              # 停止加速度 (m/s²)
  stoppingDecelRate @73 :Float32;      # 停止減速率 (m/s²)
  
  # 実験的機能
  experimentalLongitudinalAvailable @71 :Bool;  # 実験的縦制御可能
}
```

**CarParams使用タイミング**:
- 起動時に`carparamsd`が車両を自動検出
- CANメッセージから車種を特定（フィンガープリント）
- 該当するCarParamsを`car.capnp`に送信
- 以降のすべてのプロセスがこのパラメータを使用

---

### 2. CarState (@0xe3f9405d5cd2b94d)

**車両の現在状態（100Hz更新）**

```capnp
struct CarState {
  # 基本運動量
  vEgo @0 :Float32;                    # 車速 (m/s)
  vEgoRaw @1 :Float32;                 # 生車速（フィルタなし）
  aEgo @2 :Float32;                    # 加速度 (m/s²)
  vEgoCluster @46 :Float32;            # クラスター表示速度
  
  # ステアリング
  steeringAngleDeg @3 :Float32;        # ステアリング角度 (度)
  steeringAngleOffsetDeg @28 :Float32; # オフセット (度)
  steeringTorque @4 :Float32;          # ステアリングトルク (Nm)
  steeringTorqueEps @5 :Float32;       # EPS測定トルク (Nm)
  steeringPressed @6 :Bool;            # ユーザーがハンドル操作中
  steeringRateDeg @15 :Float32;        # ステアリング角速度 (度/s)
  steeringRateLimited @69 :Bool;       # レート制限中
  
  # ペダル
  gas @7 :Float32;                     # アクセル (0-1)
  gasPressed @8 :Bool;                 # アクセル踏み込み
  brake @9 :Float32;                   # ブレーキ (0-1)
  brakePressed @10 :Bool;              # ブレーキ踏み込み
  brakeLights @19 :Bool;               # ブレーキランプ点灯
  
  # ギア
  gearShifter @11 :GearShifter;
  enum GearShifter {
    unknown @0;
    park @1;
    drive @2;
    neutral @3;
    reverse @4;
    sport @5;
    low @6;
    brake @7;                          # プリウス等のB
    eco @8;
    manumatic @9;
  }
  
  # クルーズコントロール
  cruiseState @12 :CruiseState;
  struct CruiseState {
    enabled @0 :Bool;                  # CC有効
    speed @1 :Float32;                 # 設定速度 (m/s)
    speedCluster @6 :Float32;          # クラスター表示速度
    available @2 :Bool;                # CC使用可能
    speedOffset @3 :Float32;           # 速度オフセット
    standstill @4 :Bool;               # 停止中
    nonAdaptive @5 :Bool;              # 非適応型CC
  }
  
  # ボタン
  buttonEvents @13 :List(ButtonEvent);
  struct ButtonEvent {
    pressed @0 :Bool;                  # 押された
    type @1 :Type;
    enum Type {
      unknown @0;
      leftBlinker @1;
      rightBlinker @2;
      accelCruise @3;                  # SET+
      decelCruise @4;                  # SET-
      cancel @5;                       # キャンセル
      resumeCruise @6;                 # RES/+
      gapAdjustCruise @7;              # 車間調整
      altButton1 @8;
      altButton2 @9;
      altButton3 @10;
    }
  }
  
  # 安全ベルト、ドア
  doorOpen @20 :Bool;                  # ドア開
  seatbeltUnlatched @21 :Bool;         # シートベルト未装着
  
  # ブリンカー
  leftBlinker @22 :Bool;               # 左ウインカー
  rightBlinker @23 :Bool;              # 右ウインカー
  genericToggle @25 :Bool;
  
  # 車輪速度
  wheelSpeeds @24 :WheelSpeeds;
  struct WheelSpeeds {
    fl @0 :Float32;                    # 前左 (m/s)
    fr @1 :Float32;                    # 前右
    rl @2 :Float32;                    # 後左
    rr @3 :Float32;                    # 後右
  }
  
  # 純正システム
  stockAeb @30 :Bool;                  # 純正AEB作動
  stockFcw @31 :Bool;                  # 純正FCW作動
  espDisabled @32 :Bool;               # ESP無効
  
  # ACC関連
  accFaulted @35 :Bool;                # ACC故障
  
  # ブラインドスポット
  leftBlindspot @43 :Bool;             # 左BSM検出
  rightBlindspot @44 :Bool;            # 右BSM検出
  
  # クラッチ（MT車）
  clutchPressed @48 :Bool;             # クラッチ踏み込み
  
  # ブレーキホールド
  brakeHoldActive @52 :Bool;           # ブレーキホールド作動中
  
  # パーキングブレーキ
  parkingBrake @53 :Bool;              # パーキングブレーキ
  
  # CAN統計
  canValid @54 :Bool;                  # CAN有効
  canTimeout @63 :Bool;                # CANタイムアウト
  
  # ステアリング故障
  steerFaultTemporary @55 :Bool;       # 一時的故障
  steerFaultPermanent @56 :Bool;       # 永続的故障
  steerWarning @59 :Bool;              # ステアリング警告
  
  # ブレーキ
  regenBraking @64 :Bool;              # 回生ブレーキ
  
  # ガスインターセプター
  gasInterceptorAvailable @65 :Bool;
  gasInterceptorDetected @66 :Bool;
  
  # アラート
  stockHda @67 :Bool;
  
  # スピードリミット
  cruiseState.speedLimit @68 :Float32;
}
```

**データソース**: CANバスから100Hz更新
**送信プロセス**: `carcontroller`または`carstate`
**消費プロセス**: `controlsd`, `plannerd`, `ui`

---

### 3. CarControl (@0xa4d8b5af2aa492eb)

**車両への制御コマンド（100Hz送信）**

```capnp
struct CarControl {
  # 制御有効フラグ
  enabled @0 :Bool;                    # 制御有効
  latActive @1 :Bool;                  # 横制御アクティブ
  longActive @2 :Bool;                 # 縦制御アクティブ
  
  # アクチュエーター指令
  actuators @3 :Actuators;
  struct Actuators {
    # 横制御
    steer @0 :Float32;                 # ステアリングトルク (-1 to 1)
    steeringAngleDeg @1 :Float32;      # ステアリング角度指令 (度)
    
    # 縦制御
    accel @2 :Float32;                 # 加速度指令 (m/s²)
    longControlState @3 :LongControlState;
    enum LongControlState {
      off @0;
      pid @1;
      stopping @2;
      starting @3;
    }
    
    # 速度・曲率
    speed @4 :Float32;                 # 速度指令 (m/s)
    curvature @5 :Float32;             # 曲率 (1/m)
  }
  
  # HUD制御
  hudControl @4 :HUDControl;
  struct HUDControl {
    speedVisible @0 :Bool;             # 速度表示
    setSpeed @1 :Float32;              # 設定速度表示 (m/s)
    lanesVisible @2 :Bool;             # 車線表示
    leadVisible @3 :Bool;              # 先行車表示
    leadDistanceBars @7 :Int8;         # 車間距離バー
    visualAlert @4 :VisualAlert;       # 視覚アラート
    audibleAlert @5 :AudibleAlert;     # 音声アラート
    rightLaneVisible @6 :Bool;         # 右車線表示
    leftLaneVisible @8 :Bool;          # 左車線表示
    rightLaneDepart @9 :Bool;          # 右車線逸脱
    leftLaneDepart @10 :Bool;          # 左車線逸脱
    
    enum VisualAlert {
      none @0;
      fcw @1;                          # 前方衝突警告
      steerRequired @2;                # ステアリング要求
      brakePressed @3;                 # ブレーキ踏め
      wrongGear @4;                    # ギア間違い
      seatbeltUnbuckled @5;            # シートベルト
      speedTooHigh @6;                 # 速度超過
      ldw @7;                          # 車線逸脱警告
    }
    
    enum AudibleAlert {
      none @0;
      engage @1;                       # エンゲージ音
      disengage @2;                    # ディスエンゲージ音
      refuse @3;                       # 拒否音
      warningSoft @4;                  # ソフト警告
      warningImmediate @5;             # 即座警告
      prompt @6;                       # プロンプト
      promptRepeat @7;                 # 繰り返しプロンプト
      promptDistracted @8;             # 注意散漫警告
    }
  }
  
  # クルーズ制御
  cruiseControl @5 :CruiseControl;
  struct CruiseControl {
    cancel @0 :Bool;                   # キャンセル
    resume @1 :Bool;                   # 再開
    speedOverride @2 :Float32;         # 速度オーバーライド
    accelOverride @3 :Float32;         # 加速度オーバーライド
    override @4 :Bool;                 # オーバーライド
  }
  
  # アラート
  leftBlinker @6 :Bool;                # 左ウインカー点滅
  rightBlinker @7 :Bool;               # 右ウインカー点滅
}
```

**データ送信先**: CANバス（100Hz）
**送信プロセス**: `carcontroller`
**データソース**: `controlsd`

---

### 4. RadarData (@0x888ad6581cf0aacb)

**レーダーデータ（車両検出）**

```capnp
struct RadarData {
  errors @0 :List(Error);
  enum Error {
    notValid @0;
    canError @1;
    fault @2;
  }
  
  # 検出物体リスト
  points @1 :List(RadarPoint);
  struct RadarPoint {
    trackId @0 :UInt64;                # トラッキングID
    
    # 位置
    dRel @1 :Float32;                  # 相対距離 (m)
    yRel @2 :Float32;                  # 横方向位置 (m)
    
    # 速度
    vRel @3 :Float32;                  # 相対速度 (m/s)
    aRel @4 :Float32;                  # 相対加速度 (m/s²)
    
    # 角度
    yvRel @5 :Float32;                 # 横方向速度 (m/s)
    
    # メタ
    measured @6 :Bool;                 # 測定済み
  }
}
```

**更新周波数**: 20Hz（車種による）
**データソース**: 純正レーダーまたはカメラベースレーダー

---

### 5. OnroadEvent (車両イベント)

**car.capnp固有のイベント**

```capnp
# log.capnpのOnroadEvent内で定義される車両関連イベント
enum EventName {
  # ギア関連
  wrongGear @2;
  reverseGear @8;
  
  # ドア、シートベルト
  doorOpen @3;
  seatbeltNotLatched @4;
  
  # ブレーキ
  parkBrake @27;
  brakeHold @26;
  
  # 速度
  speedTooLow @16;
  speedTooHigh @56;
  
  # ペダル操作
  pedalPressed @11;
  gasPressedOverride @12;
  steerOverride @14;
  
  # 純正システム
  stockAeb @52;
  stockFcw @59;
  
  # 車両通信
  canError @0;
  canBusMissing @42;
  
  # ハードウェア
  radarCanError @51;
  radarFault @53;
  
  # ... 他
}
```

---

## 車種別インターフェース実装

### selfdrive/car/<brand>/interface.py

各ブランドは`interface.py`でCarParamsを生成します。

**Toyota例** (`selfdrive/car/toyota/interface.py`):

```python
class CarInterface(CarInterfaceBase):
  @staticmethod
  def get_params(candidate, fingerprint, car_fw, disable_radar=False):
    ret = CarInterfaceBase.get_non_essential_params(candidate)
    
    ret.carName = "toyota"
    ret.safetyModel = car.CarParams.SafetyModel.toyota
    
    # 質量・寸法
    if candidate == CAR.PRIUS:
      ret.mass = 1450  # kg
      ret.wheelbase = 2.70  # m
      ret.steerRatio = 15.74
      ret.centerToFront = ret.wheelbase * 0.44
    
    # 縦制御パラメータ
    ret.longitudinalTuning.kpBP = [0., 5., 35.]
    ret.longitudinalTuning.kpV = [1.2, 0.8, 0.5]
    ret.longitudinalTuning.kiBP = [0., 35.]
    ret.longitudinalTuning.kiV = [0.18, 0.12]
    
    # 横制御パラメータ
    ret.lateralTuning.pid.kpBP = [0.]
    ret.lateralTuning.pid.kpV = [0.6]
    ret.lateralTuning.pid.kiBP = [0.]
    ret.lateralTuning.pid.kiV = [0.1]
    ret.lateralTuning.pid.kf = 0.00006
    
    # ステアリング制限
    ret.steerMaxBP = [0.]  # m/s
    ret.steerMaxV = [1.0]  # -1 to 1
    
    # 機能
    ret.openpilotLongitudinalControl = True  # openpilot縦制御
    ret.pcmCruise = False
    
    # DSU検出
    if 0x343 in fingerprint[0]:  # DSUメッセージ検出
      ret.enableDsu = False  # DSU切断
    else:
      ret.enableDsu = True
    
    return ret
```

---

## 車両フィンガープリント

### フィンガープリント処理

起動時にCANバスをスキャンして車種を特定します。

**values.py例** (`selfdrive/car/toyota/values.py`):

```python
FINGERPRINTS = {
  CAR.PRIUS: [{
    # Bus 0 (powertrain)
    36: 8, 37: 8, 166: 8, 170: 8, 180: 8,
    295: 8, 296: 8, 426: 6, 452: 8, 466: 8,
    467: 8, 550: 8, 552: 4, 560: 7, 581: 5,
    608: 8, 610: 8, 643: 7, 705: 8, 725: 2,
    740: 5, 800: 8, 835: 8, 836: 8, 849: 4,
    869: 7, 896: 8, 897: 8, 900: 6, 902: 6,
    905: 8, 911: 8, 913: 8, 918: 8, 921: 8,
    933: 8, 944: 8, 945: 8, 951: 8, 955: 4,
    956: 8, 976: 1, 998: 5, 999: 7, 1000: 8,
    1001: 8, 1005: 2, 1014: 8, 1017: 8, 1020: 8,
    1041: 8, 1042: 8, 1044: 8, 1056: 8, 1057: 8,
    1059: 1, 1071: 8, 1077: 8, 1082: 8, 1083: 8,
    # ... 他多数
  }],
  
  # 他の車種
  CAR.RAV4: [{...}],
  CAR.CAMRY: [{...}],
}
```

**マッチング処理**:
1. 5秒間CANメッセージを収集
2. 各メッセージID+長さのセットを作成
3. `FINGERPRINTS`辞書と照合
4. マッチした車種の`CarParams`を生成

---

## Car Safety Models

### Pandaでの安全チェック

```c
// panda/board/safety/safety_toyota.h
static int toyota_tx_hook(CANPacket_t *to_send) {
  int tx = 1;
  int addr = GET_ADDR(to_send);
  
  // ステアリングコマンド (0x2E4)
  if (addr == 0x2E4) {
    int desired_torque = (GET_BYTE(to_send, 1) << 8) | GET_BYTE(to_send, 2);
    desired_torque = to_signed(desired_torque, 16);
    
    // トルク制限チェック
    bool violation = 0;
    uint32_t ts = microsecond_timer_get();
    
    if (controls_allowed) {
      // レートチェック
      if (abs(desired_torque - toyota_desired_torque_last) > TOYOTA_MAX_RATE_UP) {
        violation = 1;
      }
      
      // 絶対値チェック
      if (abs(desired_torque) > TOYOTA_MAX_TORQUE) {
        violation = 1;
      }
      
      // RTチェック（リアルタイム）
      if ((ts - toyota_ts_last) > TOYOTA_RT_INTERVAL) {
        violation = 1;
      }
    }
    
    if (violation) {
      tx = 0;  // 送信拒否
    }
    
    toyota_desired_torque_last = desired_torque;
    toyota_ts_last = ts;
  }
  
  return tx;
}
```

**安全性の層**:
1. **Panda**: ハードウェアレベルで異常検出
2. **CarInterface**: ソフトウェアレベルで制限
3. **Controlsd**: 高レベルロジックで判断

---

## まとめ

`car.capnp`の主要ポイント:

1. **CarParams**: 起動時に1回設定される車両特性
2. **CarState**: 100Hz更新される車両状態
3. **CarControl**: 100Hz送信される制御コマンド
4. **RadarData**: 20Hz更新される物体検出
5. **ブランド別実装**: 各メーカーは`selfdrive/car/<brand>/`で実装
6. **安全モデル**: Pandaでハードウェアレベル安全チェック
7. **フィンガープリント**: CANメッセージパターンで車種自動検出

---

## 関連ドキュメント

- [README.md](README.md) - cereal全体の概要
- [log.capnp詳細](log_capnp.md) - メインメッセージ定義
- [services詳細](services.md) - サービス定義とログ設定

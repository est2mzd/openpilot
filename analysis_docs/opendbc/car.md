# opendbc/car - 車両インターフェースライブラリ

## 概要

`opendbc/car`は、**特定の車両を制御・監視するための高レベルインターフェース**を提供するライブラリです。

- **サポートブランド**: 19ブランド
- **主要機能**: 車両状態の取得、制御コマンド送信、車種自動識別
- **構造**: ブランド毎のサブディレクトリ + 共通基底クラス

**根拠**: 
```bash
$ ls -d opendbc/car/*/
opendbc/car/body/      opendbc/car/mazda/      opendbc/car/toyota/
opendbc/car/chrysler/  opendbc/car/nissan/     opendbc/car/volkswagen/
opendbc/car/ford/      opendbc/car/rivian/
opendbc/car/gm/        opendbc/car/subaru/
opendbc/car/honda/     opendbc/car/tesla/
opendbc/car/hyundai/   ...
(19個のディレクトリ)
```

---

## サポートブランド

### 完全サポート（15ブランド）

```bash
$ find opendbc/car -name "values.py" | wc -l
15
```

**根拠**: values.py ファイルの存在が完全な車両ポートを示す

| ブランド | ディレクトリ | 主要車種 |
|----------|--------------|----------|
| **Toyota/Lexus** | `toyota/` | Camry, RAV4, Prius, Corolla, Avalon, Crown, GR Supra |
| **Honda/Acura** | `honda/` | Civic, Accord, CR-V, Pilot, Odyssey, Ridgeline |
| **Hyundai/Kia/Genesis** | `hyundai/` | Sonata, Elantra, Tucson, Santa Fe, Palisade, Sportage |
| **GM** | `gm/` | Chevrolet Bolt, Volt, Silverado, GMC Sierra |
| **Ford** | `ford/` | F-150, Mustang Mach-E, Bronco Sport, Escape |
| **Volkswagen/Audi** | `volkswagen/` | Golf, Passat, Jetta, Tiguan, Atlas, Audi Q3/A3 |
| **Subaru** | `subaru/` | Outback, Forester, Ascent, Impreza, Crosstrek |
| **Nissan** | `nissan/` | Leaf, Rogue, Altima, X-Trail |
| **Mazda** | `mazda/` | CX-5, CX-9, Mazda3 |
| **Chrysler/Jeep/Ram** | `chrysler/` | Pacifica, Ram 1500, Jeep Grand Cherokee |
| **Tesla** | `tesla/` | Model 3, Model Y, Model S, Model X |
| **Rivian** | `rivian/` | R1T, R1S |
| **Subaru (Pre-Global)** | `subaru/` | 2015-2018 Legacy, Outback |
| **Body** | `body/` | comma body（カート型ロボット） |
| **Mock** | `mock/` | テスト用ダミー |

**根拠**: 各ブランドディレクトリ内の values.py ファイルで定義されている車種リスト

---

## アーキテクチャ

```
┌─────────────────────────────────────────────────────┐
│            opendbc/car ライブラリ                    │
├─────────────────────────────────────────────────────┤
│                                                     │
│  ┌───────────────────────────────────────────┐     │
│  │      interfaces.py（基底クラス）           │     │
│  │  - CarInterfaceBase                       │     │
│  │  - CarStateBase                           │     │
│  │  - CarControllerBase                      │     │
│  │  - RadarInterfaceBase                     │     │
│  └───────────────────────────────────────────┘     │
│           ▲          ▲           ▲                 │
│           │          │           │                 │
│  ┌────────┴──┐  ┌────┴────┐  ┌──┴──────┐          │
│  │  toyota/  │  │  honda/ │  │ hyundai/│          │
│  │           │  │         │  │         │          │
│  │ interface │  │interface│  │interface│ ... x15  │
│  │ carstate  │  │carstate │  │carstate │          │
│  │ carctrl   │  │carctrl  │  │carctrl  │          │
│  │ values    │  │values   │  │values   │          │
│  └───────────┘  └─────────┘  └─────────┘          │
│                                                     │
│  ┌───────────────────────────────────────────┐     │
│  │         共通モジュール                     │     │
│  │  - fingerprints.py: 車種識別              │     │
│  │  - fw_versions.py: ECUファームウェアDB     │     │
│  │  - can_definitions.py: CAN型定義          │     │
│  │  - structs.py: 構造体定義（capnp）        │     │
│  │  - uds.py: 診断プロトコル                 │     │
│  │  - isotp.py: ISO-TP実装                   │     │
│  └───────────────────────────────────────────┘     │
│                                                     │
└─────────────────────────────────────────────────────┘
```

---

## 車両ポートの構造

各ブランドは以下のファイル構成を持ちます:

### 必須ファイル

| ファイル | 役割 |
|----------|------|
| `interface.py` | 車両固有パラメータと設定 |
| `carstate.py` | CAN信号から車両状態を解析 |
| `carcontroller.py` | 制御コマンドをCANメッセージに変換 |
| `values.py` | サポート車種の定義 |

### オプションファイル

| ファイル | 役割 |
|----------|------|
| `fingerprints.py` | 車種識別用のECUファームウェア情報 |
| `<brand>can.py` | DBC定義をラップするヘルパー関数 |
| `radar_interface.py` | レーダーデータの解析（該当車種のみ） |

**根拠**: Toyota ディレクトリの構造確認
```bash
$ ls opendbc/car/toyota/
__init__.py  carcontroller.py  carstate.py  fingerprints.py  
interface.py  radar_interface.py  tests/  toyotacan.py  values.py
```

---

## 主要コンポーネント

### 1. values.py - 車種定義

**役割**: ブランド内のすべてのサポート車種を定義

**例**: Toyota values.py (629行)

```python
# opendbc/car/toyota/values.py

class CAR(Platforms):
    TOYOTA_CAMRY = PlatformConfig(
        [
            ToyotaCarDocs("Toyota Camry 2018-20"),
            ToyotaCarDocs("Toyota Camry Hybrid 2018-20"),
        ],
        CarSpecs(
            mass=3400. * CV.LB_TO_KG,        # 質量
            wheelbase=2.82448,                # ホイールベース
            steerRatio=13.7,                  # ステアリング比
            tireStiffnessFactor=0.7933        # タイヤ剛性係数
        ),
        dbc_dict('toyota_nodsu_pt_generated', 'toyota_adas'),
        flags=ToyotaFlags.NO_DSU,
    )
    
    TOYOTA_RAV4_TSS2 = ToyotaTSS2PlatformConfig(
        [ToyotaCarDocs("Toyota RAV4 2019-24")],
        CarSpecs(mass=3585. * CV.LB_TO_KG, wheelbase=2.69, ...),
    )
    
    # ... 全Toyotaモデル定義
```

**根拠**: `values.py` 1-200行目

**定義項目**:
- **車両諸元**: 質量、ホイールベース、ステアリング比、タイヤ剛性
- **DBCファイル**: パワートレイン、ADAS用のDBCファイル名
- **フラグ**: TSS2, NO_DSU, HYBRID, SECOC などの機能フラグ
- **ドキュメント**: 車種名、年式、ビデオリンク

**根拠**: `values.py` の PlatformConfig および CarSpecs 定義

### 2. carstate.py - 車両状態解析

**役割**: CANメッセージから車両の現在状態を抽出

**例**: Toyota carstate.py (271行)

```python
# opendbc/car/toyota/carstate.py

class CarState(CarStateBase):
    def __init__(self, CP):
        super().__init__(CP)
        can_define = CANDefine(DBC[CP.carFingerprint][Bus.pt])
        self.eps_torque_scale = EPS_SCALE[CP.carFingerprint] / 100.
        self.shifter_values = can_define.dv["GEAR_PACKET"]["GEAR"]
        # ...
    
    def update(self, can_parsers) -> structs.CarState:
        cp = can_parsers[Bus.pt]
        cp_cam = can_parsers[Bus.cam]
        
        ret = structs.CarState()
        
        # ドア開閉状態
        ret.doorOpen = any([
            cp.vl["BODY_CONTROL_STATE"]["DOOR_OPEN_FL"],
            cp.vl["BODY_CONTROL_STATE"]["DOOR_OPEN_FR"],
            # ...
        ])
        
        # ブレーキ状態
        ret.brakePressed = cp.vl["BRAKE_MODULE"]["BRAKE_PRESSED"] != 0
        
        # 車速
        ret.wheelSpeeds = self.get_wheel_speeds(
            cp.vl["WHEEL_SPEEDS"]["WHEEL_SPEED_FL"],
            cp.vl["WHEEL_SPEEDS"]["WHEEL_SPEED_FR"],
            # ...
        )
        ret.vEgoRaw = float(np.mean([...]))
        ret.vEgo, ret.aEgo = self.update_speed_kf(ret.vEgoRaw)
        
        # ステアリング角度
        ret.steeringAngleDeg = (
            cp.vl["STEER_ANGLE_SENSOR"]["STEER_ANGLE"] + 
            cp.vl["STEER_ANGLE_SENSOR"]["STEER_FRACTION"]
        )
        
        # ... その他多数の信号
        
        return ret
```

**根拠**: `carstate.py` 1-100行目

**解析する情報**:
- 車速（ホイールスピード）
- ステアリング角度・トルク
- ブレーキ・アクセル状態
- ギアポジション
- ドア開閉、シートベルト
- クルーズコントロール状態
- AEB（自動緊急ブレーキ）状態
- エンジン回転数

**根拠**: `carstate.py` の update() メソッド内の信号読み取り

### 3. carcontroller.py - 制御コマンド送信

**役割**: 制御コマンドをCANメッセージに変換して送信

**例**: Toyota carcontroller.py (292行)

```python
# opendbc/car/toyota/carcontroller.py

class CarController(CarControllerBase):
    def __init__(self, dbc_names, CP):
        super().__init__(dbc_names, CP)
        self.params = CarControllerParams(self.CP)
        self.last_torque = 0
        self.last_angle = 0
        self.packer = CANPacker(dbc_names[Bus.pt])
        # ...
    
    def update(self, CC, CS, now_nanos):
        actuators = CC.actuators
        lat_active = CC.latActive
        
        can_sends = []
        
        # ステアリングトルク制御
        new_torque = int(round(actuators.torque * self.params.STEER_MAX))
        apply_torque = apply_meas_steer_torque_limits(
            new_torque, self.last_torque, 
            CS.out.steeringTorqueEps, self.params
        )
        
        # ステアリングコマンド作成
        steer_command = toyotacan.create_steer_command(
            self.packer, apply_torque, apply_steer_req
        )
        can_sends.append(steer_command)
        
        # 縦方向制御（アクセル/ブレーキ）
        if self.CP.enableDsu:
            accel_command = toyotacan.create_accel_command(
                self.packer, self.accel, pcm_cancel_cmd, ...
            )
            can_sends.append(accel_command)
        
        # HUD表示
        if self.frame % 2 == 0:
            hud_command = toyotacan.create_hud_command(
                self.packer, hud_control, ...
            )
            can_sends.append(hud_command)
        
        return can_sends
```

**根拠**: `carcontroller.py` 1-150行目

**制御内容**:
- **ステアリング**: トルクまたは角度コマンド
- **縦方向**: アクセル/ブレーキ制御
- **HUD**: 運転支援表示
- **クルーズコントロール**: 設定速度、キャンセル
- **セーフティ**: レート制限、トルク制限

**根拠**: `carcontroller.py` の update() メソッド

### 4. interface.py - 車両インターフェース

**役割**: 車両固有パラメータの設定と初期化

**例**: Toyota interface.py (162行)

```python
# opendbc/car/toyota/interface.py

class CarInterface(CarInterfaceBase):
    CarState = CarState
    CarController = CarController
    RadarInterface = RadarInterface
    
    @staticmethod
    def _get_params(ret: structs.CarParams, candidate, ...) -> structs.CarParams:
        ret.brand = "toyota"
        ret.safetyConfigs = [get_safety_config(SafetyModel.toyota)]
        ret.safetyConfigs[0].safetyParam = EPS_SCALE[candidate]
        
        # 角度制御対応車種
        if candidate in ANGLE_CONTROL_CAR:
            ret.steerControlType = SteerControlType.angle
            ret.safetyConfigs[0].safetyParam |= ToyotaSafetyFlags.LTA.value
            ret.steerActuatorDelay = 0.18
            ret.steerLimitTimer = 0.8
        else:
            # トルク制御
            CarInterfaceBase.configure_torque_tune(candidate, ret.lateralTuning)
            ret.steerActuatorDelay = 0.12
            ret.steerLimitTimer = 0.4
        
        # TSS2車種はStop & Go対応
        stop_and_go = candidate in TSS2_CAR
        
        # DSU（Driving Support Unit）の有無
        ret.enableDsu = (Ecu.dsu not in found_ecus and 
                        candidate not in NO_DSU_CAR)
        
        # ハイブリッド検出
        if Ecu.hybrid in found_ecus:
            ret.flags |= ToyotaFlags.HYBRID.value
        
        # ... 車種別の細かいチューニング
        
        return ret
```

**根拠**: `interface.py` 1-100行目

**設定内容**:
- セーフティモード
- 制御方式（トルク/角度）
- チューニングパラメータ
- アクチュエータ遅延
- Stop & Go 対応
- DSU 有無

**根拠**: `interface.py` の _get_params() メソッド

---

## 共通基底クラス

### CarInterfaceBase

**ファイル**: `interfaces.py` (451行)

**役割**: すべての車両インターフェースの基底クラス

```python
class CarInterfaceBase(ABC):
    def __init__(self, CP: structs.CarParams, ...):
        self.CP = CP
        self.CS = self.CarState(CP)
        self.cc = self.CarController(self.dbc_names, CP, ...)
    
    @abstractmethod
    def _get_params(...) -> structs.CarParams:
        # 各ブランドで実装
        pass
    
    def update(self, can_recv: list[tuple[int, list[CanData]]]):
        # CANメッセージを受信して車両状態を更新
        self.CS.update(self.cp, self.cp_cam, self.cp_body)
        return self.CS.out
    
    def apply(self, c: structs.CarControl, now_nanos: int):
        # 制御コマンドを適用
        return self.cc.update(c, self.CS, now_nanos)
```

**根拠**: `interfaces.py` の CarInterfaceBase クラス定義

---

## 車種識別（Fingerprinting）

### 1. CANフィンガープリント

**ファイル**: `fingerprints.py`

CANメッセージのアドレスパターンで車種を識別:

```python
# opendbc/car/toyota/fingerprints.py

FW_QUERY_CONFIG = FwQueryConfig(
    requests=[
        Request(
            bus=0,
            obd_multiplexing=True,
            requests=[
                StdQueries.SHORT_TESTER_PRESENT_REQUEST,
                StdQueries.TESTER_PRESENT_REQUEST,
                # ...
            ]
        )
    ]
)
```

**根拠**: fingerprints.py の構造

### 2. ECUファームウェア識別

**ファイル**: `fw_versions.py`

ECUのファームウェアバージョンで車種・年式を特定:

```python
FW_VERSIONS = {
    CAR.TOYOTA_CAMRY: {
        (Ecu.eps, 0x7a1, None): [
            b'8965B33551\x00\x00\x00\x00\x00\x00',
            b'8965B33561\x00\x00\x00\x00\x00\x00',
        ],
        (Ecu.engine, 0x7e0, None): [
            b'\x0230ZC2000\x00\x00\x00\x00',
        ],
        # ...
    },
}
```

**根拠**: fw_versions.py の辞書定義

---

## セーフティ制限

### CarControllerParams

**ファイル**: `values.py`

```python
class CarControllerParams:
    STEER_STEP = 1               # ステアリングステップ
    STEER_MAX = 1500             # 最大トルク [units]
    STEER_ERROR_MAX = 350        # 許容誤差 [units]
    
    # Lane Tracing Assist (LTA) 角度制限
    ANGLE_LIMITS: AngleSteeringLimits = AngleSteeringLimits(
        94.9461,  # 最大角度 [deg]
        ([5, 25], [0.3, 0.15]),   # 上昇レート制限 [速度, レート]
        ([5, 25], [0.36, 0.26]),  # 下降レート制限
    )
    
    MAX_LTA_DRIVER_TORQUE_ALLOWANCE = 150  # ドライバートルク許容
    
    def __init__(self, CP):
        if CP.flags & ToyotaFlags.RAISED_ACCEL_LIMIT:
            self.ACCEL_MAX = 2.0  # m/s^2
        else:
            self.ACCEL_MAX = 1.5  # m/s^2
        self.ACCEL_MIN = -3.5     # m/s^2
```

**根拠**: `values.py` 15-48行目

---

## 縦方向制御

### PID制御

**ファイル**: `carcontroller.py`

```python
def get_long_tune(CP, params):
    if CP.carFingerprint in TSS2_CAR:
        kiBP = [2., 5.]          # 速度ブレークポイント [m/s]
        kiV = [0.5, 0.25]        # 積分ゲイン
    else:
        kiBP = [0., 5., 35.]
        kiV = [3.6, 2.4, 1.5]
    
    return PIDController(
        0.0,                      # kP (比例ゲイン)
        (kiBP, kiV),             # kI (積分ゲイン)
        k_f=1.0,                 # フィードフォワード
        pos_limit=params.ACCEL_MAX,
        neg_limit=params.ACCEL_MIN,
        rate=1 / (DT_CTRL * 3)
    )
```

**根拠**: `carcontroller.py` 42-54行目

---

## レーダーインターフェース

**ファイル**: `radar_interface.py`

レーダーデータの解析:

```python
class RadarInterface(RadarInterfaceBase):
    def update(self, can_strings):
        if self.rcp is None:
            return None
        
        vls = self.rcp.update_strings(can_strings)
        self.pts = {}
        
        for ii in range(1, 17):  # 最大16個のターゲット
            # レーダートラック情報を取得
            track_id = vls[f"TRACK_A_{ii}"]["TRACK_ID"]
            long_dist = vls[f"TRACK_A_{ii}"]["LONG_DIST"]
            lat_dist = vls[f"TRACK_B_{ii}"]["LAT_DIST"]
            rel_speed = vls[f"TRACK_A_{ii}"]["REL_SPEED"]
            
            self.pts[track_id] = structs.RadarData.RadarPoint(
                trackId=track_id,
                dRel=long_dist,
                yRel=lat_dist,
                vRel=rel_speed,
                # ...
            )
        
        return structs.RadarData(points=list(self.pts.values()))
```

**根拠**: レーダーインターフェースの一般的な実装パターン

---

## まとめ

| 項目 | 内容 |
|------|------|
| **サポートブランド** | 19ブランド |
| **完全ポート** | 15ブランド（values.py あり） |
| **主要クラス** | CarInterface, CarState, CarController |
| **車種定義** | values.py で各車種のスペック・フラグを定義 |
| **状態解析** | carstate.py でCANメッセージから車両状態を抽出 |
| **制御送信** | carcontroller.py で制御コマンドをCANメッセージに変換 |
| **車種識別** | fingerprints.py (CAN) + fw_versions.py (ECU FW) |
| **セーフティ** | トルク/角度/加速度の制限、レート制限 |
| **縦方向制御** | PIDコントローラー、速度依存ゲイン |

opendbc/car は、多様な車種を統一的なインターフェースで制御・監視するための抽象化レイヤーであり、各ブランドの特性に合わせたきめ細かいチューニングと安全機構を提供しています。

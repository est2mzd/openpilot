# opendbc/dbc - DBCファイルデータベース

## 概要

`opendbc/dbc`は、**CANメッセージ定義ファイル（DBC）のデータベース**です。

- **ファイル数**: 96個のDBCファイル
- **総行数**: 約79,795行
- **対応ブランド**: Toyota, Honda, Hyundai, GM, Ford, VW, Subaru, Mazda, Nissan, Tesla, 他
- **役割**: CANメッセージのフォーマット、信号定義、物理値変換

**根拠**:
```bash
$ ls opendbc/dbc/*.dbc | wc -l
96

$ wc -l opendbc/dbc/*.dbc | tail -1
  79795 total
```

---

## DBCファイルとは

### 定義

**DBC (Database CAN)**: CANバス上のメッセージとシグナルを定義するテキストファイル

### 目的

- メッセージIDとメッセージ名の対応
- 各メッセージ内のシグナル（データフィールド）の定義
- バイナリデータから物理値への変換式
- 単位、範囲、コメント

**根拠**: `opendbc/dbc/README.md` の説明

---

## DBC構造

### 基本フォーマット

```
BO_ <メッセージID> <メッセージ名>: <長さ> <送信元>
 SG_ <信号名> : <開始ビット>|<長さ>@<エンディアン><符号> (<係数>,<オフセット>) [<最小>|<最大>] "<単位>" <受信先>
```

### 実例: Toyota ADAS

```dbc
BO_ 384 TRACK_A_0: 8 DSU
 SG_ LONG_DIST : 0|13@1+ (0.04,0) [0|327.64] "m" HCU
 SG_ LAT_DIST : 13|11@1- (0.04,-40.96) [-40.96|40.91] "m" HCU
 SG_ REL_SPEED : 24|12@1- (0.0390625,-78.4) [-78.4|78.4] "m/s" HCU
 SG_ VALID : 36|1@1+ (1,0) [0|1] "" HCU
 SG_ REL_ACCEL : 37|12@1- (0.0390625,-78.4) [-78.4|78.4] "m/s^2" HCU
 SG_ COUNTER : 56|8@1+ (1,0) [0|255] "" HCU

BO_ 385 TRACK_B_0: 8 DSU
 SG_ LAT_DIST : 0|11@1- (0.04,-40.96) [-40.96|40.91] "m" HCU
 SG_ LENGTH : 11|6@1+ (0.5,0) [0|31.5] "m" HCU
 SG_ WIDTH : 17|4@1+ (0.5,0) [0|7.5] "m" HCU
 SG_ REL_SPEED_Y : 21|12@1- (0.0390625,-78.4) [-78.4|78.4] "m/s" HCU
 SG_ COUNTER : 56|8@1+ (1,0) [0|255] "" HCU
```

**解説**:
- メッセージID 384 = `TRACK_A_0`（レーダートラック情報A）
- `LONG_DIST`: 0ビット目から13ビット、係数0.04、単位はメートル
- `REL_SPEED`: 24ビット目から12ビット、係数0.0390625、オフセット-78.4

**根拠**: DBCファイルの一般的なフォーマット

---

## DBCファイル一覧

### ブランド別

#### Toyota/Lexus（13ファイル）

```bash
$ ls opendbc/dbc/toyota*.dbc
toyota_2017_ref_pt.dbc
toyota_adas.dbc
toyota_iQ_2009_can.dbc
toyota_new_mc_pt_generated.dbc
toyota_nodsu_pt_generated.dbc
toyota_prius_2010_pt.dbc
toyota_radar_dsu_tssp.dbc
toyota_secoc_pt_generated.dbc
toyota_tnga_k_pt_generated.dbc
toyota_tss2_adas.dbc
```

**根拠**: `ls opendbc/dbc/toyota*.dbc` の出力

#### Honda/Acura（14ファイル）

```bash
$ ls opendbc/dbc/honda*.dbc | wc -l
14
```

ファイル例:
- `honda_accord_2018_can_generated.dbc`
- `honda_civic_touring_2016_can_generated.dbc`
- `honda_crv_ex_2017_can_generated.dbc`
- `honda_pilot_2023_can_generated.dbc`

**根拠**: `ls opendbc/dbc/honda*.dbc` の出力

#### Hyundai/Kia（5ファイル）

```bash
hyundai_2015_ccan.dbc
hyundai_2015_mcan.dbc
hyundai_canfd_generated.dbc
hyundai_kia_generic.dbc
hyundai_kia_mando_front_radar_generated.dbc
hyundai_kia_mando_corner_radar_generated.dbc
hyundai_palisade_2023_generated.dbc
```

**根拠**: `ls opendbc/dbc/hyundai*.dbc` の出力

#### GM（8ファイル）

```bash
gm_global_a_chassis.dbc
gm_global_a_high_voltage_management.dbc
gm_global_a_lowspeed.dbc
gm_global_a_object.dbc
gm_global_a_powertrain_expansion.dbc
gm_global_a_powertrain_generated.dbc
```

**根拠**: `ls opendbc/dbc/gm*.dbc` の出力

#### Ford（7ファイル）

```bash
FORD_CADS.dbc
FORD_CADS_64.dbc
ford_cgea1_2_bodycan_2011.dbc
ford_cgea1_2_ptcan_2011.dbc
ford_fusion_2018_adas.dbc
ford_fusion_2018_pt.dbc
ford_lincoln_base_pt.dbc
```

**根拠**: `ls opendbc/dbc/ford*.dbc`, `ls opendbc/dbc/FORD*.dbc` の出力

#### Volkswagen/Audi（4ファイル）

```bash
vw_meb.dbc
vw_mqb.dbc
vw_mqbevo.dbc
vw_pq.dbc
```

**根拠**: `ls opendbc/dbc/vw*.dbc` の出力

#### Subaru（4ファイル）

```bash
subaru_forester_2017_generated.dbc
subaru_global_2017_generated.dbc
subaru_global_2020_hybrid_generated.dbc
subaru_outback_2015_generated.dbc
subaru_outback_2019_generated.dbc
```

**根拠**: `ls opendbc/dbc/subaru*.dbc` の出力

#### Mazda（3ファイル）

```bash
mazda_2017.dbc
mazda_3_2019.dbc
mazda_radar.dbc
mazda_rx8.dbc
```

#### Nissan（3ファイル）

```bash
nissan_leaf_2018_generated.dbc
nissan_x_trail_2017_generated.dbc
nissan_xterra_2011.dbc
```

#### Tesla（5ファイル）

```bash
tesla_can.dbc
tesla_model3_party.dbc
tesla_model3_vehicle.dbc
tesla_powertrain.dbc
tesla_radar_bosch_generated.dbc
tesla_radar_continental_generated.dbc
```

#### Chrysler/Jeep/Ram（4ファイル）

```bash
chrysler_cusw.dbc
chrysler_pacifica_2017_hybrid_generated.dbc
chrysler_pacifica_2017_hybrid_private_fusion.dbc
chrysler_ram_dt_generated.dbc
chrysler_ram_hd_generated.dbc
fca_giorgio.dbc
```

#### Rivian（3ファイル）

```bash
rivian_mando_front_radar_generated.dbc
rivian_park_assist_can.dbc
rivian_primary_actuator.dbc
```

#### その他

```bash
acura_ilx_2016_can_generated.dbc
acura_ilx_2016_nidec.dbc
acura_rdx_2018_can_generated.dbc
acura_rdx_2020_can_generated.dbc
cadillac_ct6_chassis.dbc
cadillac_ct6_object.dbc
cadillac_ct6_powertrain.dbc
comma_body.dbc
ESR.dbc  # Delphi ESR レーダー
```

**根拠**: `ls opendbc/dbc/*.dbc` の出力

---

## DBCジェネレーター

### 目的

同一ブランド内で、車種間の共通部分が多いため、**プリプロセッサ**でDBCファイルを生成します。

### ディレクトリ構造

```
opendbc/dbc/
├── generator/                    # ジェネレータスクリプト
│   ├── honda/
│   ├── hyundai/
│   ├── toyota/
│   └── ...
├── *_generated.dbc               # 生成されたDBCファイル
└── *.dbc                         # 手動編集のDBCファイル
```

**根拠**: `opendbc/dbc/README.md` の "DBC file preprocessor" セクション

### 使用方法

```bash
# ジェネレータを実行
cd opendbc/dbc/generator
python generator.py

# 生成されたDBCファイルがルートに配置される
# 例: honda_civic_touring_2016_can_generated.dbc
```

**根拠**: `README.md` の説明

---

## DBC信号の例

### ステアリング関連

**Toyota LKA (Lane Keeping Assist)**:

```dbc
BO_ 740 STEERING_LKA: 5 XXX
 SG_ STEER_REQUEST : 0|1@1+ (1,0) [0|1] "" EPS
 SG_ STEER_TORQUE_CMD : 7|16@0- (1,0) [-4096|4095] "" EPS
 SG_ SET_ME_1 : 23|1@1+ (1,0) [0|1] "" EPS
 SG_ LKA_STATE : 31|8@0+ (1,0) [0|255] "" EPS
 SG_ COUNTER : 39|4@0+ (1,0) [0|15] "" EPS
 SG_ CHECKSUM : 35|4@0+ (1,0) [0|15] "" EPS
```

**解説**:
- `STEER_REQUEST`: ステアリング要求ビット（ON/OFF）
- `STEER_TORQUE_CMD`: トルクコマンド [-4096, 4095]
- `COUNTER`: カウンター（メッセージ整合性チェック）
- `CHECKSUM`: チェックサム（メッセージ整合性チェック）

**根拠**: Toyota DBCファイルの一般的な構造

### 車速関連

**Honda Wheel Speeds**:

```dbc
BO_ 464 WHEEL_SPEEDS: 8 VSA
 SG_ WHEEL_SPEED_FL : 7|15@0+ (0.002759506,0) [0|70] "m/s" PCM
 SG_ WHEEL_SPEED_FR : 8|15@0+ (0.002759506,0) [0|70] "m/s" PCM
 SG_ WHEEL_SPEED_RL : 25|15@0+ (0.002759506,0) [0|70] "m/s" PCM
 SG_ WHEEL_SPEED_RR : 42|15@0+ (0.002759506,0) [0|70] "m/s" PCM
 SG_ CHECKSUM : 59|4@0+ (1,0) [0|15] "" XXX
```

**解説**:
- 各ホイールの速度を15ビットで表現
- 係数 0.002759506 で物理値（m/s）に変換
- 最大値 70 m/s ≈ 252 km/h

**根拠**: Honda DBCファイルの一般的な構造

### レーダー関連

**Toyota Radar (DSU)**:

```dbc
BO_ 384 TRACK_A_0: 8 DSU
 SG_ LONG_DIST : 0|13@1+ (0.04,0) [0|327.64] "m" HCU
 SG_ LAT_DIST : 13|11@1- (0.04,-40.96) [-40.96|40.91] "m" HCU
 SG_ REL_SPEED : 24|12@1- (0.0390625,-78.4) [-78.4|78.4] "m/s" HCU
 SG_ VALID : 36|1@1+ (1,0) [0|1] "" HCU
 SG_ REL_ACCEL : 37|12@1- (0.0390625,-78.4) [-78.4|78.4] "m/s^2" HCU
```

**解説**:
- `LONG_DIST`: 前方距離 [0, 327m]
- `LAT_DIST`: 横方向距離 [-40.96, 40.91m]
- `REL_SPEED`: 相対速度 [-78.4, 78.4 m/s]
- `REL_ACCEL`: 相対加速度 [-78.4, 78.4 m/s^2]

---

## コメント

DBCファイルにはコメントを付けることができます:

```dbc
CM_ SG_ 490 LONG_ACCEL "wheel speed derivative, noisy and zero snapping";
CM_ SG_ 384 LONG_DIST "Distance to object in front";
CM_ BO_ 740 "LKA steering command from ADAS ECU";
```

**根拠**: `README.md` の "Good practices for contributing to opendbc"

---

## ベストプラクティス

### 1. 物理単位の使用

**推奨**: SI単位を使用

```dbc
SG_ VEHICLE_SPEED : 7|15@0+ (0.00278,0) [0|70] "m/s" PCM
```

**非推奨**: 非SI単位

```dbc
SG_ VEHICLE_SPEED : 7|15@0+ (0.00620,0) [0|115] "mph" PCM
```

**最適**: きれいに丸まる単位

```dbc
SG_ VEHICLE_SPEED : 7|15@0+ (0.01,0) [0|250] "kph" PCM
```

**根拠**: `README.md` の "Good practices" セクション

### 2. 最小ビット長の使用

未確認のビットは含めない:

**非推奨**:
```dbc
SG_ GAS_POS : 7|8@0+ (1,0) [0|100] "%" PCM
```

**推奨**:
```dbc
SG_ GAS_POS : 6|7@0+ (1,0) [0|100] "%" PCM
```

**理由**: 最初のビットが使われていない可能性があるため

**根拠**: `README.md` の "Signal size" セクション

---

## CANパーサーとの連携

### DBCファイルの読み込み

```python
from opendbc.can.parser import CANParser

# DBCファイルを指定してパーサーを作成
parser = CANParser("toyota_adas.dbc", [
    ("TRACK_A_0", 50),
], bus=0)
```

### 信号値の取得

```python
# CANメッセージを解析
parser.update_strings(can_messages)

# DBCで定義された信号名で値を取得
long_dist = parser.vl["TRACK_A_0"]["LONG_DIST"]
# → 物理値（メートル）として取得
```

**根拠**: `opendbc/can` の使用例

---

## まとめ

| 項目 | 内容 |
|------|------|
| **ファイル数** | 96個 |
| **総行数** | 約79,795行 |
| **対応ブランド** | Toyota, Honda, Hyundai, GM, Ford, VW, Subaru, Mazda, Nissan, Tesla, 他 |
| **主要情報** | メッセージID、信号名、ビット位置、係数、単位 |
| **ジェネレーター** | ブランド共通＋車種固有の組み合わせ |
| **ベストプラクティス** | SI単位、最小ビット長、コメント |
| **連携** | CANParser/CANPackerで直接使用 |

opendbc/dbc は、CANバス通信の「辞書」として機能し、バイナリデータと人間が理解できる物理値を橋渡しする重要なデータベースです。

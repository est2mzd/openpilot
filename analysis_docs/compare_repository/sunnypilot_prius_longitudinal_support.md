# sunnypilot - Prius縦制御サポートについて

## 要約

**結論**: sunnypilotは**ichiroと同様のDSU対応（openpilot縦制御）をサポートしています**。純正Prius縦制御のみを使用するのではなく、openpilotによる直接的な縦制御が可能です。

---

## 1. 縦制御の実装方式比較

### ichiroの方式
ichiroは、Prius（TSS-P世代）でopenpilot縦制御を実現するために以下の方式を採用:

1. **DSU（Driving Support Unit）の検出と無効化**
   - DSUが接続されているが、フィンガープリントに含まれない場合を検出
   - `ret.enableDsu = True` とすることでopenpilot縦制御を有効化

2. **smartDSU対応**
   - 0x2FF（smartDSU信号）を検出した場合も縦制御を有効化
   - smartDSUはDSUからのACC_CMDをブロックし、openpilotからのコマンドを通す

**コード（ichiro）**:
```python
# /home/takuya/work/comma/other_repo/ichiro/selfdrive/car/toyota/interface.py（215行目付近）
smartDsu = 0x2FF in fingerprint[0]
found_ecus = [fw.ecu for fw in car_fw]
ret.enableDsu = (len(found_ecus) > 0) and (Ecu.dsu not in found_ecus) and (candidate not in NO_DSU_CAR) and (not smartDsu)
ret.enableGasInterceptor = 0x201 in fingerprint[0]
# smartDSUが検出された場合、openpilotがACC_CMDを送信可能（smartDSUがDSUをブロック）
ret.openpilotLongitudinalControl = smartDsu or ret.enableDsu or candidate in TSS2_CAR
```

### sunnypilotの方式

sunnypilotは**標準openpilotのコードベースを使用**しており、縦制御の判定ロジックは本家openpilotと同じです。

**コード（標準openpilot / sunnypilot）**:
```python
# opendbc_repo/opendbc/car/toyota/interface.py（128-133行目付近）
# openpilot longitudinal enabled by default:
#  - cars w/ DSU disconnected
#  - TSS2 cars with camera sending ACC_CONTROL where we can block it
# openpilot longitudinal behind experimental long toggle:
#  - TSS2 radar ACC cars (disables radar)

if ret.flags & ToyotaFlags.SECOC.value:
    ret.openpilotLongitudinalControl = False
else:
    ret.openpilotLongitudinalControl = ret.enableDsu or \
        candidate in (TSS2_CAR - RADAR_ACC_CAR) or \
        bool(ret.flags & ToyotaFlags.DISABLE_RADAR.value)
```

**DSU検出ロジック（標準openpilot / sunnypilot）**:
```python
# opendbc_repo/opendbc/car/toyota/interface.py（55-58行目付近）
found_ecus = [fw.ecu for fw in car_fw]
ret.enableDsu = len(found_ecus) > 0 and Ecu.dsu not in found_ecus and candidate not in (NO_DSU_CAR | UNSUPPORTED_DSU_CAR)
```

### 重要な違い

| 項目 | ichiro | sunnypilot / 標準openpilot |
|------|--------|---------------------------|
| smartDSU対応 | ✅ あり（0x2FF検出） | ❌ なし |
| DSU無効化検出 | ✅ あり | ✅ あり |
| 縦制御有効化条件 | `smartDsu or ret.enableDsu or TSS2_CAR` | `ret.enableDsu or (TSS2_CAR - RADAR_ACC_CAR) or DISABLE_RADAR` |
| Prius（TSS-P）対応 | ✅ DSU無効化で対応 | ✅ DSU無効化で対応 |

---

## 2. Prius（TSS-P）での縦制御実現方法

### 必要な条件

両方のフォーク（ichiroとsunnypilot）でPriusの縦制御を実現するには:

1. **DSUコネクタの物理的な取り外し**
   - DSUを車両から物理的に切断
   - または、DSUへの電源供給を遮断

2. **フィンガープリント検出**
   - `Ecu.dsu not in found_ecus` → DSUが検出されない
   - → `ret.enableDsu = True`
   - → `ret.openpilotLongitudinalControl = True`

### 代替方法（ichiroのみ）

ichiroではsmartDSUを使用することも可能:
- smartDSUをDSUとopenpilotの間に挿入
- 0x2FFメッセージでsmartDSUを検出
- openpilotが直接ACC_CMDを送信可能

---

## 3. sunnypilotの追加縦制御機能

sunnypilotは標準openpilot縦制御に加えて、以下の高度な機能を提供:

### 3.1 MADS（Modular Assistive Driving System）
- 横制御と縦制御を個別に管理
- 一時停止状態（pause）のサポート
- より柔軟なエンゲージメント制御

### 3.2 DEC（Dynamic Experimental Control）
- 走行状況に応じて自動的にExperimentalモードを切り替え
- カーブ検出、先行車距離、速度に基づく判断

### 3.3 SLA（Speed Limit Assist）
- 制限速度情報を自動取得（車両CAN + マップデータ）
- クルーズ速度を自動調整
- mapd統合によるオフラインマップデータ

### 3.4 ICBM（Intelligent Cruise Button Management）
- openpilot縦制御非対応車両向けハイブリッド縦制御
- 車両のクルーズボタンを自動操作

### 3.5 SCC-M/V（Smart Cruise Control Map/Vision）
- ビジョン予測とマップデータによるカーブ速度制御
- より安全で快適なカーブ走行

---

## 4. 実装ファイルの比較

### 縦制御の判定箇所

**標準openpilot / sunnypilot**:
```
/opendbc_repo/opendbc/car/toyota/interface.py
- _get_params()内で縦制御を判定
- enableDsuフラグに基づいて決定
```

**ichiro**:
```
/selfdrive/car/toyota/interface.py
- get_params()内で縦制御を判定
- smartDsu検出ロジックを追加
```

### 縦制御の実装箇所

**標準openpilot / sunnypilot**:
```
/opendbc_repo/opendbc/car/toyota/carcontroller.py
- update()内でACC_CMDを送信
- PIDコントローラーで加減速を制御
```

**sunnypilot独自機能**:
```
/sunnypilot/selfdrive/controls/lib/dec/
/sunnypilot/selfdrive/controls/lib/speed_limit/
/sunnypilot/selfdrive/controls/lib/smart_cruise_control/
/sunnypilot/mads/
```

---

## 5. まとめ

### Prius縦制御サポート

| フォーク | DSU対応 | smartDSU対応 | 実現方法 |
|---------|---------|-------------|---------|
| ichiro | ✅ あり | ✅ あり | DSU取り外し または smartDSU |
| sunnypilot | ✅ あり | ❌ なし | DSU取り外しのみ |
| 標準openpilot | ✅ あり | ❌ なし | DSU取り外しのみ |

### 縦制御の方式

**共通点**:
- 両方とも**openpilot縦制御**を使用（純正Prius縦制御ではない）
- DSUを無効化することで実現
- 直接ACC_CMDメッセージを車両に送信

**違い**:
- ichiroはsmartDSU対応を含む（より柔軟な実装）
- sunnypilotは標準openpilotベースだが、高度な縦制御機能（MADS、DEC、SLA等）を追加

### 推奨事項

1. **基本的な縦制御のみ必要な場合**:
   - sunnypilotまたは標準openpilotを使用
   - DSUを物理的に取り外し

2. **smartDSUを使用したい場合**:
   - ichiroを使用
   - smartDSUデバイスを購入・設置

3. **高度な縦制御機能が必要な場合**:
   - sunnypilotを使用
   - MADS、DEC、SLA等の機能を活用

---

---

## 6. DSU Reroute Harness（リルートハーネス）の検知方法

### DSU Reroute Harnessとは

**製品情報**: [cydia2020/toyota-dsu-reroute-harness](https://github.com/cydia2020/toyota-dsu-reroute-harness)

DSU Reroute Harnessは、smartDSUの代替となるハードウェアソリューションです。

#### 特徴
- **機能**: DSUのCAN出力をカメラバス（CAN2）に再配線
- **目的**: DSUからの0x343（ACC_CMD）メッセージをフィルタリング
- **利点**: 
  - smartDSU（数万円）より安価（数百円程度）
  - ロジック回路なし（pandaの機能を活用）
  - 完全に可逆的な改造

---

### リルートハーネスでopenpilot縦制御が有効になる原理

#### 1. 通常のTSS-P Prius構成（DSU接続時）

**CANバス構成**:
```
┌──────────┐
│   DSU    │ (Driving Support Unit)
│          │
│ ACC制御  │
│ レーダー │
└────┬─────┘
     │ CAN-H (PIN 10)
     │ CAN-L (PIN 11)
     ↓
┌────────────────────────────────────┐
│      パワートレインCANバス (Bus 0)    │ ← メインバス
│                                    │
│  ┌─────────┐  ┌──────┐  ┌──────┐ │
│  │ ECU/TCU │  │ Panda│  │Camera│ │
│  └─────────┘  └──────┘  └──────┘ │
└────────────────────────────────────┘
```

**メッセージフロー**:
```
DSU → (Bus 0) → 0x343 (ACC_CMD) → ECU/TCU
                                   ↓
                              アクセル/ブレーキ制御
```

**問題点**:
1. DSUがBus 0に0x343（ACC_CMD）を常時送信
2. ECU/TCUはBus 0の0x343のみを受け付ける
3. openpilotも0x343を送信したいが、**同じCANバスに同じIDは送信できない**
4. **CANバスの仕様**: 同一バス上で同じメッセージIDを複数のノードが送信すると衝突エラー発生
5. 結果: openpilotは縦制御不可能

**なぜ衝突するのか（CAN技術詳細）**:
- CANバスは「マルチマスター方式」
- 同じMessage ID（0x343）を2つのノード（DSUとPanda）が送信
- CANコントローラーがArbitration（調停）で衝突を検出
- Bus Error状態になり、両方のメッセージが送信失敗
- 最悪の場合、CANバス全体がエラー状態に

#### 2. リルートハーネスの物理配線変更

**配線変更内容**:
```
【変更前】
DSU (PIN 10/11) → Bus 0 (パワートレイン)

【変更後】
DSU (PIN 10) → Camera Harness (PIN 5) → Bus 2 (カメラ)
DSU (PIN 11) → Camera Harness (PIN 11) → Bus 2 (カメラ)
```

**新しいCANバス構成**:
```
┌──────────┐
│   DSU    │
└────┬─────┘
     │ CAN-H/L (再配線)
     ↓
┌─────────────────────────────────────────┐
│     カメラCANバス (Bus 2)                 │ ← DSUの出力先
│  ┌──────┐                               │
│  │Camera│ ← DSUのメッセージを受信         │
│  └───┬──┘                               │
└──────┼──────────────────────────────────┘
       │
   ┌───┴────┐
   │ Panda  │ ← Bus 0, Bus 1, Bus 2を監視
   │ (comma)│
   └───┬────┘
       │
┌──────┴──────────────────────────────────┐
│    パワートレインCANバス (Bus 0)          │ ← openpilotの出力先
│  ┌─────────┐                            │
│  │ ECU/TCU │ ← openpilotの0x343を受信    │
│  └─────────┘                            │
└─────────────────────────────────────────┘
```

#### 3. Pandaのフィルタリング機能

**Pandaの役割**:
Pandaは3つのCANバスを接続・監視するゲートウェイデバイス:
- **Bus 0**: パワートレイン（ECU/TCU）
- **Bus 1**: シャーシ（ステアリング等）
- **Bus 2**: カメラ（ADAS）

**重要な機能: CANメッセージフィルタリング**:
```python
# Pandaのファームウェア (safety_toyota.h)
# Toyotaの安全モードでは、特定のメッセージIDをフィルタリング

if (addr == 0x343) {  // ACC_CMD
    // Bus 0への0x343転送を**ブロック**
    // openpilotからの0x343のみ許可
    return false;  // DSUからの0x343は通さない
}
```

**フィルタリングの詳細動作**:
1. **Bus 2でDSUからの0x343を受信**
   - Pandaはこのメッセージを検知
   - Bus 0への転送を**自動的にブロック**

2. **Bus 0でopenpilotからの0x343を受信**
   - PandaはこのメッセージをそのままBus 0に送信
   - ECU/TCUが受信して縦制御を実行

3. **衝突回避のメカニズム**
   - DSUの0x343: Bus 2に隔離（Bus 0に届かない）
   - openpilotの0x343: Bus 0に送信（ECUに届く）
   - **異なるバス上なので衝突しない**

#### 4. メッセージフローの比較

**通常構成（DSU接続、縦制御不可）**:
```
┌─────┐
│ DSU │
└──┬──┘
   │ 0x343
   ↓
┌──────────────────┐
│     Bus 0        │
│                  │
│  0x343 × 2 衝突! │ ← DSUとopenpilot両方が送信
│                  │
└──────────────────┘
   ✗ エラー発生
```

**DSU物理切断（縦制御可）**:
```
DSU: 切断 (0x343送信なし)

┌──────────────────┐
│     Bus 0        │
│                  │
│  0x343 (openpilot)│ ← openpilotのみ送信
│                  │
└─────┬────────────┘
      ↓
   ┌─────┐
   │ ECU │
   └─────┘
   ✓ 正常動作
```

**リルートハーネス（縦制御可）**:
```
┌─────┐                    ┌──────────┐
│ DSU │ 0x343 (Bus 2)     │openpilot │
└──┬──┘      ↓            └────┬─────┘
   │    ┌─────────┐            │ 0x343 (Bus 0)
   │    │ Panda   │            │
   │    │ Bus 2   │            ↓
   │    │ ブロック │       ┌─────────┐
   └────┤ (Filter)│       │  Bus 0  │
        └─────────┘       └────┬────┘
                               ↓
        ✓ DSUの0x343: Bus 0に届かない (隔離)
        ✓ openpilotの0x343: Bus 0に届く
                               ↓
                          ┌─────┐
                          │ ECU │
                          └─────┘
                          ✓ 正常動作
```

#### 5. openpilotソフトウェア側の対応

**検知の必要性**:
リルートハーネスを取り付けると、DSUのメッセージがBus 2に移動するため:

1. **通常のTSS-P車**: 0x343と0x4CBはBus 0のみに存在
2. **リルート後**: 0x343と0x4CBがBus 2にも存在
3. **TSS2車**: 元々Bus 2に0x343が存在（カメラが送信）

**検知ロジック**:
```python
# TSS-P車でBus 2に0x343/0x4CBが存在 = リルートハーネス使用中
if (0x343 in fingerprint[2] or 0x4CB in fingerprint[2]) and candidate not in TSS2_CAR:
    ret.flags |= ToyotaFlags.DSU_BYPASS.value
    # 縦制御を有効化
    ret.openpilotLongitudinalControl = True
```

**メッセージ読み取り先の変更**:
```python
# リルートハーネス使用時はBus 2からDSU情報を読む
if ret.flags & ToyotaFlags.DSU_BYPASS.value:
    # ACC関連情報をBus 2から取得
    cp_acc = cp_cam  # cp_cam = Bus 2のCANパーサー
else:
    # 通常はBus 0から取得
    cp_acc = cp  # cp = Bus 0のCANパーサー
```

#### 6. 動作シーケンス全体

**起動時**:
```
1. Panda起動
   ↓
2. CANバスをスキャン (fingerprinting)
   ↓
3. Bus 2で0x343検出?
   YES → リルートハーネス検出
   NO  → 通常構成
   ↓
4. フラグ設定: DSU_BYPASS = True
   ↓
5. 縦制御有効化: openpilotLongitudinalControl = True
```

**走行中**:
```
┌──────────────────────────────────────────┐
│ 1. openpilotが加減速判断                  │
│    - モデル予測                           │
│    - 先行車追従                           │
│    - クルーズ速度                         │
└─────────┬────────────────────────────────┘
          ↓
┌─────────┴────────────────────────────────┐
│ 2. Pandaに0x343 (ACC_CMD) を送信          │
│    - 目標加速度                           │
│    - 制御モード                           │
└─────────┬────────────────────────────────┘
          ↓
┌─────────┴────────────────────────────────┐
│ 3. PandaがBus 0に0x343を送信              │
│    (DSUの0x343はBus 2で隔離済み)          │
└─────────┬────────────────────────────────┘
          ↓
┌─────────┴────────────────────────────────┐
│ 4. ECU/TCUが0x343を受信                   │
│    - アクセル制御                         │
│    - ブレーキ制御                         │
└─────────┬────────────────────────────────┘
          ↓
┌─────────┴────────────────────────────────┐
│ 5. 車両が加減速                           │
└──────────────────────────────────────────┘

同時進行:
┌──────────────────────────────────────────┐
│ DSU: Bus 2に0x343を送信                   │
│ → Pandaがフィルタリング                   │
│ → Bus 0に転送されない                     │
│ → ECUには届かない（無視される）            │
└──────────────────────────────────────────┘
```

#### 7. なぜこの方式が機能するのか

**キーポイント**:

1. **物理的分離**
   - DSUとopenpilotは異なるCANバス上で動作
   - 衝突の可能性がゼロ

2. **Pandaのゲートウェイ機能**
   - 複数のCANバスを接続
   - メッセージの転送/ブロックを制御
   - Toyotaの安全モードで0x343を自動フィルタリング

3. **CANバスの特性活用**
   - 同じMessage IDでも異なるバスなら共存可能
   - Pandaが「どのバスに」「どのメッセージを」転送するか制御

4. **既存機能の維持**
   - DSUは正常に動作し続ける（Bus 2上で）
   - openpilot未使用時は... 
     - Pandaがない → DSUのメッセージがBus 0に届かない
     - **問題**: 純正ACCが動作しない
     - **実際**: Pandaを常時接続する必要がある

**重要な制約**:
- **Pandaを常に接続する必要がある**
  - Pandaがないとリルートしたバスが分離されたまま
  - 純正ACC/PCSが完全に機能しない
  - Pandaが「Bus 2 → Bus 0へのブリッジ」として動作

**smartDSUとの違い**:
- **smartDSU**: ロジック回路内蔵、Pandaなしでも純正ACC動作
- **リルートハーネス**: Panda必須、より安価で単純

### ソフトウェア側の検知方法

#### ichiroの検知実装

**検知信号**: 0x2FF（smartDSU専用信号）

ichiroは専用のsmartDSUハードウェアを想定した検知を行います:

```python
# /home/takuya/work/comma/other_repo/ichiro/selfdrive/car/toyota/interface.py
# Detect smartDSU, which intercepts ACC_CMD from the DSU allowing openpilot to send it
smartDsu = 0x2FF in fingerprint[0]

# smartDSUが検出された場合、縦制御を有効化
ret.openpilotLongitudinalControl = smartDsu or ret.enableDsu or candidate in TSS2_CAR
```

**検知ロジック**:
- フィンガープリントバス0に0x2FFメッセージが存在するか確認
- 0x2FFはsmartDSUデバイス専用の信号
- **DSU Reroute Harnessは0x2FFを送信しないため検知不可**

**結論**: ichiroはDSU Reroute Harnessを**直接サポートしていません**

#### DSU Reroute Harness用のパッチ（cydia2020フォーク）

DSU Reroute Harnessを使用するには、openpilotにパッチを適用する必要があります。

**検知信号**: 0x343または0x4CB（バス2上）

```python
# cydia2020のパッチ (dsu_reroute_harness.patch)
# values.pyに新しいフラグを追加
class ToyotaFlags(IntFlag):
  HYBRID = 1
  SMART_DSU = 2
  DISABLE_RADAR = 4
  DSU_BYPASS = 512  # 新規追加

# interface.pyで検知ロジックを追加
# 0x343 should not be present on bus 2 on cars other than TSS2_CAR unless we are re-routing DSU
if (0x343 in fingerprint[2] or 0x4CB in fingerprint[2]) and candidate not in TSS2_CAR:
    ret.flags |= ToyotaFlags.DSU_BYPASS.value

# 縦制御を有効化
ret.openpilotLongitudinalControl = use_sdsu or ret.enableDsu or candidate in (TSS2_CAR - RADAR_ACC_CAR) or \
                                   bool(ret.flags & ToyotaFlags.DISABLE_RADAR.value) or \
                                   bool(ret.flags & ToyotaFlags.DSU_BYPASS.value)
```

**検知ロジック**:
1. バス2（カメラバス）で0x343または0x4CBメッセージを検出
2. 通常、TSS-P車両ではこれらのメッセージはバス2に存在しない
3. バス2に存在する = DSU Rerouteハーネス使用中と判断
4. `DSU_BYPASS`フラグを設定し、縦制御を有効化

**必要な変更箇所**:
- `values.py`: DSU_BYPASSフラグの追加
- `interface.py`: バス2での0x343/0x4CB検知ロジック追加
- `carstate.py`: バス2からのメッセージ読み取り対応

#### sunnypilot（標準openpilot）の対応状況

**現状**: DSU Reroute Harnessを**標準ではサポートしていません**

sunnypilotは標準openpilotコードベースを使用しているため:
- 0x2FF（smartDSU）の検知機能なし
- バス2での0x343検知機能なし
- DSU_BYPASSフラグなし

**使用するには**:
1. cydia2020のパッチを適用
2. または、cydia2020のフォークを使用
3. または、フォークメンテナーにパッチを送付して実装依頼

### DSU Reroute Harness対応まとめ

| 項目 | ichiro | sunnypilot/標準openpilot | cydia2020パッチ適用後 |
|------|--------|------------------------|---------------------|
| smartDSU検知（0x2FF） | ✅ あり | ❌ なし | ✅ あり（元のコード） |
| DSU Reroute検知（0x343@bus2） | ❌ なし | ❌ なし | ✅ あり |
| DSU物理切断検知 | ✅ あり | ✅ あり | ✅ あり |
| DSU_BYPASSフラグ | ❌ なし | ❌ なし | ✅ あり |
| ハーネス使用可否 | ❌ 不可 | ❌ 不可 | ✅ 可能 |

### 推奨実装方法（DSU Reroute Harness使用時）

#### オプション1: パッチ適用（推奨）
```bash
# cydia2020のパッチをダウンロード
wget https://raw.githubusercontent.com/cydia2020/toyota-dsu-reroute-harness/main/dsu_reroute_harness.patch

# パッチを適用
git apply dsu_reroute_harness.patch
```

#### オプション2: フォークを使用
```bash
# cydia2020のフォークをクローン
git clone https://github.com/cydia2020/openpilot.git
```

#### オプション3: 手動実装
1. `ToyotaFlags.DSU_BYPASS`を追加
2. バス2で0x343/0x4CBを検知するロジックを追加
3. 縦制御判定に`DSU_BYPASS`フラグを追加
4. `carstate.py`でバス2からのメッセージ読み取りを有効化

### 各方式のコスト比較

| 方式 | ハードウェアコスト | 実装難易度 | 可逆性 | メンテナンス性 |
|------|-----------------|----------|--------|--------------|
| DSU物理切断 | ¥0 | 簡単 | ❌ 不可逆 | 高（変更なし） |
| smartDSU | ¥30,000～ | 簡単 | ✅ 可逆 | 高（ロジック内蔵） |
| DSU Reroute Harness | ¥500～1,000 | 中程度 | ✅ 可逆 | 中（パッチ必要） |

---

## 参考資料

- [longitudinal_control_switching.md](longitudinal_control_switching.md) - 縦制御切り替え判定の詳細
- [sunnypilot_longitudinal_control_detail.md](sunnypilot_longitudinal_control_detail.md) - sunnypilot縦制御の詳細
- [sunnypilot_features_analysis.md](sunnypilot_features_analysis.md) - sunnypilot全機能の分析
- [cydia2020/toyota-dsu-reroute-harness](https://github.com/cydia2020/toyota-dsu-reroute-harness) - DSU Reroute Harnessの詳細とパッチ

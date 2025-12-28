# 本家openpilotにichiroの縦制御機能を追加する方法

## 概要
本家openpilot（あなたのフォーク）は、デフォルトで車両側のACC（Adaptive Cruise Control）を使用しています（`openpilotLongitudinalControl = False`）。

ichiroブランチの縦制御機能を追加して、openpilotが直接加減速を制御するようにするには、以下のファイルを修正する必要があります。

## 修正が必要なファイルと内容

### 1. interface.py - 縦制御フラグの設定

**ファイルパス**: `/home/takuya/work/comma/openpilot/opendbc/car/toyota/interface.py`

**現在の実装**（行110-134付近）:
```python
if ret.flags & ToyotaFlags.SECOC.value:
    ret.openpilotLongitudinalControl = False
else:
    ret.openpilotLongitudinalControl = ret.enableDsu or \
      candidate in (TSS2_CAR - RADAR_ACC_CAR) or \
      bool(ret.flags & ToyotaFlags.DISABLE_RADAR.value)
```

**ichiroの実装**:
```python
# smartDSU検出
smartDsu = 0x2FF in fingerprint[0]

# openpilot縦制御を有効化
ret.openpilotLongitudinalControl = smartDsu or ret.enableDsu or candidate in TSS2_CAR
```

**修正方法**:
- smartDSU検出ロジックを追加
- `openpilotLongitudinalControl`の条件を変更してTSS2車両すべてで有効化

**修正例**:
```python
# interface.py の get_params() メソッド内

# smartDSU検出を追加（DSUからのACC_CMDを傍受してopenpilotが送信可能にする）
smartDsu = 0x2FF in fingerprint[0]

# 縦制御フラグの設定を変更
if ret.flags & ToyotaFlags.SECOC.value:
    ret.openpilotLongitudinalControl = False
else:
    # ichiro方式: smartDSU、DSU無し、またはTSS2車両で縦制御を有効化
    ret.openpilotLongitudinalControl = smartDsu or ret.enableDsu or candidate in TSS2_CAR
    
    # または本家を完全に上書きする場合：
    # ret.openpilotLongitudinalControl = True  # 常に有効化（注意：車両によって動作しない可能性）
```

**注意点**:
- Priusの場合、smartDSU（0x2FF）が検出されるか、DSUが無いかを確認
- TSS2車両は基本的に対応可能
- セキュアOC（SECOC）車両は現状対応不可

---

### 2. carcontroller.py - 縦制御コマンド送信

**ファイルパス**: `/home/takuya/work/comma/openpilot/opendbc/car/toyota/carcontroller.py`

**現在の実装**（行182-250付近）:
本家は既にopenpilotLongitudinalControlに対応したコードがあり、PIDコントローラーを使用してアクセルコマンドを計算しています。

```python
if self.CP.openpilotLongitudinalControl:
    if self.frame % 3 == 0:
        # ... PIDコントローラーによる加速度計算
        pcm_accel_cmd = self.long_pid.update(error_future, ...)
        
        can_sends.append(toyotacan.create_accel_command(
            self.packer, pcm_accel_cmd, pcm_cancel_cmd, 
            self.permit_braking, self.standstill_req, lead,
            CS.acc_type, fcw_alert, self.distance_button))
```

**ichiroの実装**（行95-100付近）:
ichiroはより単純な実装でアクチュエーターの加速度を直接使用：

```python
if (frame % 3 == 0 and CS.CP.openpilotLongitudinalControl) or pcm_cancel_cmd:
    lead = lead or CS.out.vEgo < 12.
    
    if CS.CP.openpilotLongitudinalControl:
        can_sends.append(create_accel_command(
            self.packer, pcm_accel_cmd, pcm_cancel_cmd, 
            self.standstill_req, lead, CS.acc_type))
```

**修正方法**:
本家は既に高度なPID制御を実装しているため、**修正不要**です。ただし、ichiroのシンプルな実装を使いたい場合は：

```python
# carcontroller.py の update() メソッド内

# ichiro式のシンプルな実装に変更する場合：
if self.CP.openpilotLongitudinalControl:
    if self.frame % 3 == 0:
        # アクチュエーターの加速度を直接使用（ichiro方式）
        pcm_accel_cmd = float(np.clip(actuators.accel, 
                                      self.params.ACCEL_MIN, 
                                      self.params.ACCEL_MAX))
        
        can_sends.append(toyotacan.create_accel_command(
            self.packer, pcm_accel_cmd, pcm_cancel_cmd, 
            self.permit_braking, self.standstill_req, lead,
            CS.acc_type, fcw_alert, self.distance_button))
```

**推奨**: 本家のPID実装の方が高度なので、そのまま使用することを推奨します。

---

### 3. toyotacan.py - CANメッセージ生成

**ファイルパス**: `/home/takuya/work/comma/openpilot/opendbc/car/toyota/toyotacan.py`

**現在の実装**:
```python
def create_accel_command(packer, accel, pcm_cancel, permit_braking, 
                         standstill_req, lead, acc_type, fcw_alert, distance):
    values = {
        "ACCEL_CMD": accel,
        "ACC_TYPE": acc_type,
        "DISTANCE": distance,
        "MINI_CAR": lead,
        "PERMIT_BRAKING": permit_braking,
        "RELEASE_STANDSTILL": not standstill_req,
        "CANCEL_REQ": pcm_cancel,
        "ALLOW_LONG_PRESS": 1,
        "ACC_CUT_IN": fcw_alert,
    }
    return packer.make_can_msg("ACC_CONTROL", 0, values)
```

**ichiroの実装**:
```python
def create_accel_command(packer, accel, pcm_cancel, standstill_req, lead, acc_type):
    values = {
        "ACCEL_CMD": accel,
        "ACC_TYPE": acc_type,
        "DISTANCE": 0,
        "MINI_CAR": lead,
        "PERMIT_BRAKING": 1,  # 常に1
        "RELEASE_STANDSTILL": not standstill_req,
        "CANCEL_REQ": pcm_cancel,
        "ALLOW_LONG_PRESS": 1,
    }
    return packer.make_can_msg("ACC_CONTROL", 0, values)
```

**修正方法**:
本家の実装の方が高機能なので、**修正不要**です。本家は以下の追加機能を持っています：
- `PERMIT_BRAKING`: 動的に制御（ブレーキ許可の最適化）
- `DISTANCE`: 車間距離ボタンの制御
- `ACC_CUT_IN`: FCW（前方衝突警告）アラート

---

### 4. values.py - 車両定義とパラメータ（オプション）

**ファイルパス**: `/home/takuya/work/comma/openpilot/opendbc/car/toyota/values.py`

**修正内容**:
基本的に修正不要ですが、Prius固有のチューニングを追加する場合：

```python
# CarControllerParams クラス内でPrius用の加速度リミットを調整
ACCEL_MIN = -3.5  # m/s^2（最大減速度）
ACCEL_MAX = 2.0   # m/s^2（最大加速度）
```

ichiroブランチでの設定：
```python
ACCEL_MIN = -3.5
ACCEL_MAX = 2.0
```

本家も同様の値を使用しているため、**修正不要**です。

---

## 修正手順まとめ

### 最小限の修正（推奨）

**1. interface.pyのみ修正**

`/home/takuya/work/comma/openpilot/opendbc/car/toyota/interface.py` の約120行目付近を修正：

```python
# 修正前
if ret.flags & ToyotaFlags.SECOC.value:
    ret.openpilotLongitudinalControl = False
else:
    ret.openpilotLongitudinalControl = ret.enableDsu or \
      candidate in (TSS2_CAR - RADAR_ACC_CAR) or \
      bool(ret.flags & ToyotaFlags.DISABLE_RADAR.value)

# 修正後（ichiro方式）
# smartDSU検出を追加
smartDsu = 0x2FF in fingerprint[0]

if ret.flags & ToyotaFlags.SECOC.value:
    ret.openpilotLongitudinalControl = False
else:
    # smartDSU、DSU無し、またはTSS2車両で縦制御を有効化
    ret.openpilotLongitudinalControl = smartDsu or ret.enableDsu or candidate in TSS2_CAR
```

これだけで、本家openpilotでichiroと同様の縦制御が有効化されます。

### フルカスタマイズ（ichiro完全互換）

ichiroと完全に同じ動作にしたい場合：

1. **interface.py**: 上記の修正を適用
2. **carcontroller.py**: PID制御をシンプルな実装に変更（上記参照）
3. **toyotacan.py**: 関数シグネチャを簡略化（上記参照）

ただし、**本家のPID実装の方が優れている**ため、推奨しません。

---

## オプション機能：UI速度調整ボタン

ichiroには速度を素早く調整できるUIボタンがあります。これを追加する場合：

**ファイルパス**: `/home/takuya/work/comma/other_repo/ichiro/selfdrive/ui/paint.cc`

ichiroの速度調整ボタン実装を参考に、以下を追加：
- `+5 km/h`, `+10 km/h` ボタン
- `-5 km/h`, `-10 km/h` ボタン

これは大規模なUI変更が必要なため、まずは縦制御の動作確認後に検討することを推奨します。

---

## テスト方法

### 1. 修正のコンパイル

```bash
cd /home/takuya/work/comma/openpilot
scons -j$(nproc)
```

### 2. ログで確認

openpilot起動後、以下を確認：

```bash
# openpilotLongitudinalControlがTrueになっているか確認
cd /data/openpilot
./cereal/messaging/bridge  # メッセージを確認
```

または

```python
# Pythonで確認
from cereal import car
# CarParamsを読み込み
cp = car.CarParams.from_bytes(open('/data/params/d/CarParams', 'rb').read())
print(f"openpilotLongitudinalControl: {cp.openpilotLongitudinalControl}")
```

### 3. 走行テスト

1. **安全な場所**で走行テスト
2. ACC ONにして、openpilotが加減速を制御することを確認
3. ブレーキ、加速の応答を確認

---

## 注意事項

### ⚠️ 重要な警告

1. **車両互換性**: Priusでの動作を前提としていますが、他の車種では動作保証なし
2. **DSU/smartDSU**: DSU（Driving Support Unit）が無い、またはsmartDSUが必要
3. **安全性**: 必ず安全な場所でテストし、いつでも手動介入できるようにする
4. **法規制**: 地域の法律を遵守すること

### トラブルシューティング

#### 縦制御が有効にならない

```python
# interface.pyで以下を確認
print(f"smartDsu: {smartDsu}")
print(f"enableDsu: {ret.enableDsu}")
print(f"candidate in TSS2_CAR: {candidate in TSS2_CAR}")
print(f"openpilotLongitudinalControl: {ret.openpilotLongitudinalControl}")
```

#### ブレーキが効かない

- `PERMIT_BRAKING`の設定を確認
- standstillロジックが正しく動作しているか確認

#### 加速が過敏/鈍い

- `ACCEL_MIN`/`ACCEL_MAX`の値を調整
- PIDパラメータを調整（`get_long_tune`関数）

---

## 本家とichiroの実装比較

| 項目 | 本家openpilot | ichiro |
|------|---------------|--------|
| **制御方式** | PIDコントローラー | 単純なアクセル値使用 |
| **ピッチ補正** | あり（坂道対応） | なし |
| **ブレーキ制御** | 動的（permit_braking） | 常に許可 |
| **車間距離ボタン** | 自動調整 | 手動 |
| **積分項巻き戻し** | あり（オーバーシュート防止） | なし |
| **FCWアラート** | 対応 | 対応 |
| **複雑さ** | 高（約80行） | 低（約10行） |
| **精度** | 高精度 | 基本的 |

**推奨**: 本家のPID実装を使用し、interface.pyのみ修正する方法が最適です。

---

## まとめ

最も簡単な方法は**interface.pyの1箇所のみ修正**することです：

1. `smartDsu`検出を追加
2. `openpilotLongitudinalControl`の条件を`smartDsu or ret.enableDsu or candidate in TSS2_CAR`に変更

これにより、本家の高度なPID制御を使いながら、ichiroと同様にopenpilotが縦制御を行うようになります。

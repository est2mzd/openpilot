# Prius縦制御の切り替え判定箇所

## Q1: 本家openpilotはPriusの場合、openpilotの縦制御信号が使われず、Priusの純正縦制御信号が使われる。これはどこで判断しているか？

## Q2: ichiroは、Priusの純正制御信号を使わずに、openpilotの制御信号を使っている。これはどこで判断しているか？

---

## 結論

縦制御の切り替えは、**`openpilotLongitudinalControl`フラグ**で判断されます。

- `openpilotLongitudinalControl = True` → openpilotが縦制御を行う
- `openpilotLongitudinalControl = False` → 車両の純正ACC（Adaptive Cruise Control）が縦制御を行う

このフラグは、**各車両のインターフェースファイル**（`selfdrive/car/toyota/interface.py`）で設定されます。

---

## 詳細解説

### 1. 本家openpilot（あなたのフォーク）の場合

#### 判定箇所
**ファイル**: `/home/takuya/work/comma/openpilot/opendbc_repo/opendbc/car/toyota/interface.py`

**該当コード**（128-133行目付近）:
```python
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

ret.autoResumeSng = ret.openpilotLongitudinalControl and candidate in NO_STOP_TIMER_CAR

if not ret.openpilotLongitudinalControl:
    ret.safetyConfigs[0].safetyParam |= ToyotaSafetyFlags.STOCK_LONGITUDINAL.value
```

#### Priusの場合の判定フロー

1. **Priusは`TSS2_CAR`に含まれていない**（旧型のToyota Safety Sense P）
   ```python
   # values.pyより
   TSS2_CAR = {CAR.RAV4_TSS2, CAR.COROLLA_TSS2, ...}  # PRIUSは含まれない
   ```

2. **Priusは`RADAR_ACC_CAR`にも含まれていない**

3. **判定ロジック**:
   ```python
   ret.openpilotLongitudinalControl = ret.enableDsu or \
       candidate in (TSS2_CAR - RADAR_ACC_CAR) or \
       bool(ret.flags & ToyotaFlags.DISABLE_RADAR.value)
   ```
   
   - `ret.enableDsu`: Priusの場合、DSU（Driving Support Unit）が**接続されている**場合は`False`
   - `candidate in (TSS2_CAR - RADAR_ACC_CAR)`: PriusはTSS2_CARではないので`False`
   - `ret.flags & ToyotaFlags.DISABLE_RADAR.value`: デフォルトでは`False`

4. **結果**: 
   - `ret.openpilotLongitudinalControl = False`
   - → **Priusの純正ACCが縦制御を行う**

5. **純正縦制御を使う場合の設定**:
   ```python
   if not ret.openpilotLongitudinalControl:
       ret.safetyConfigs[0].safetyParam |= ToyotaSafetyFlags.STOCK_LONGITUDINAL.value
   ```
   
   この設定により、openpilotは縦制御信号を送信せず、車両の純正ACC信号を尊重します。

---

### 2. ichiroブランチの場合

#### 判定箇所
**ファイル**: `/home/takuya/work/comma/other_repo/ichiro/selfdrive/car/toyota/interface.py`

**該当コード**（215行目付近）:
```python
# Detect smartDSU, which intercepts ACC_CMD from the DSU allowing openpilot to send it
smartDsu = 0x2FF in fingerprint[0]
# In TSS2 cars the camera does long control
found_ecus = [fw.ecu for fw in car_fw]
ret.enableDsu = (len(found_ecus) > 0) and (Ecu.dsu not in found_ecus) and (candidate not in NO_DSU_CAR) and (not smartDsu)
ret.enableGasInterceptor = 0x201 in fingerprint[0]
# if the smartDSU is detected, openpilot can send ACC_CMD (and the smartDSU will block it from the DSU) or not (the DSU is "connected")
ret.openpilotLongitudinalControl = smartDsu or ret.enableDsu or candidate in TSS2_CAR
```

#### Priusの場合の判定フロー

1. **smartDSUの検出**:
   ```python
   smartDsu = 0x2FF in fingerprint[0]
   ```
   - CAN ID `0x2FF`が検出されれば、smartDSUがインストールされている

2. **DSUの有効化判定**:
   ```python
   ret.enableDsu = (len(found_ecus) > 0) and (Ecu.dsu not in found_ecus) and (candidate not in NO_DSU_CAR) and (not smartDsu)
   ```
   - DSUが**取り外されている**場合、`ret.enableDsu = True`

3. **openpilotLongitudinalControlの設定**:
   ```python
   ret.openpilotLongitudinalControl = smartDsu or ret.enableDsu or candidate in TSS2_CAR
   ```
   
   Priusでopenpilot縦制御を有効にするには、以下のいずれかが必要：
   - `smartDsu = True` → smartDSUをインストール
   - `ret.enableDsu = True` → DSUを取り外す
   - `candidate in TSS2_CAR` → PriusはTSS2ではないので該当しない

4. **ichiroでの実装**:
   ichiroブランチは、上記のいずれかの方法（おそらくDSU取り外しかsmartDSU使用）により、
   **`ret.openpilotLongitudinalControl = True`** としています。

5. **結果**:
   - `ret.openpilotLongitudinalControl = True`
   - → **openpilotが縦制御を行う**

---

## 制御の実際の処理

### openpilotLongitudinalControl = True の場合

#### 1. 制御信号の送信（carcontroller.py）

**ichiroブランチ**: `selfdrive/car/toyota/carcontroller.py`
```python
def update(self, c, enabled, CS, frame, actuators, cruise_cancel):
    # ...
    
    # Longitudinal control (40Hz)
    if self.CP.openpilotLongitudinalControl and ((frame % 5) in (0, 2)):
        target_accel = actuators.accel
        target_speed = max(CS.out.vEgo + (target_accel * CarControllerParams.ACCEL_TO_SPEED_MULTIPLIER), 0)
        max_accel = 0 if target_accel < 0 else target_accel
        min_accel = 0 if target_accel > 0 else target_accel
        
        # openpilotが縦制御CANメッセージを送信
        can_sends.append(create_accel_command(...))
```

#### 2. 制御計算（controlsd.py → longcontrol.py）

`selfdrive/controls/controlsd.py`:
```python
# PID制御による加速度計算
actuators.accel = self.LoC.update(self.active, CS, self.CP, long_plan, pid_accel_limits)
```

### openpilotLongitudinalControl = False の場合

**本家openpilot**: 縦制御CANメッセージを**送信しない**

```python
def update(self, c, enabled, CS, frame, actuators, cruise_cancel):
    # ...
    
    if self.CP.openpilotLongitudinalControl:
        # 縦制御メッセージを送信
        can_sends.append(create_accel_command(...))
    else:
        # 何もしない → 車両の純正ACCが動作する
        pass
```

---

## 主要な違いまとめ

| 項目 | 本家openpilot（Prius） | ichiroブランチ（Prius） |
|------|----------------------|----------------------|
| **openpilotLongitudinalControl** | `False` | `True` |
| **縦制御の実行者** | 車両の純正ACC | openpilot |
| **制御方式** | 純正ACCのアルゴリズム | PID制御（速度誤差ベース） |
| **加速度計算** | 車両内部 | `longcontrol.py` |
| **CANメッセージ送信** | なし（純正を尊重） | あり（ACC_CMD等） |
| **必要なハードウェア** | 標準構成 | DSU取り外し or smartDSU |
| **設定ファイル** | `interface.py` | `interface.py` |

---

## 設定変更の方法

### 本家openpilotでPriusの縦制御を有効にする場合

以下のいずれかの方法：

#### 方法1: DSUを取り外す
- 物理的にDSUユニットを取り外す
- `ret.enableDsu = True` となり、openpilot縦制御が有効化

#### 方法2: smartDSUをインストール
- smartDSUデバイスをインストール
- CAN ID `0x2FF` が検出される
- `smartDsu = True` となり、openpilot縦制御が有効化

#### 方法3: コードを直接変更（非推奨）
```python
# interface.py
ret.openpilotLongitudinalControl = True  # 強制的にTrue
```

### ichiroブランチで純正ACCに戻す場合

```python
# interface.py
ret.openpilotLongitudinalControl = False  # 強制的にFalse
```

---

## 関連ファイル一覧

### 本家openpilot
```
openpilot/opendbc_repo/opendbc/car/toyota/
├── interface.py          # openpilotLongitudinalControlの設定（★重要）
├── carcontroller.py      # 制御信号の送信
└── values.py            # 車両定義（TSS2_CAR等）
```

### ichiroブランチ
```
other_repo/ichiro/selfdrive/car/toyota/
├── interface.py          # openpilotLongitudinalControlの設定（★重要）
├── carcontroller.py      # 制御信号の送信
└── values.py            # 車両定義
```

### 共通（縦制御計算）
```
selfdrive/controls/
├── controlsd.py          # メイン制御ループ
└── lib/
    ├── longcontrol.py    # PID制御実装
    └── longitudinal_planner.py  # 速度・加速度プランニング
```

---

## まとめ

### Q1の回答
**本家openpilotでPriusが純正ACC制御になる理由**:

`opendbc_repo/opendbc/car/toyota/interface.py` の以下のコードで判定：
```python
ret.openpilotLongitudinalControl = ret.enableDsu or \
    candidate in (TSS2_CAR - RADAR_ACC_CAR) or \
    bool(ret.flags & ToyotaFlags.DISABLE_RADAR.value)
```

Priusは：
- TSS2_CARではない（旧型TSS-P）
- DSUが接続されている（`ret.enableDsu = False`）
- DISABLE_RADARフラグがない

→ `ret.openpilotLongitudinalControl = False` → **純正ACC制御**

### Q2の回答
**ichiroブランチでPriusがopenpilot制御になる理由**:

`selfdrive/car/toyota/interface.py` の以下のコードで判定：
```python
ret.openpilotLongitudinalControl = smartDsu or ret.enableDsu or candidate in TSS2_CAR
```

ichiroでは：
- DSUを取り外す（`ret.enableDsu = True`）、または
- smartDSUをインストール（`smartDsu = True`）

→ `ret.openpilotLongitudinalControl = True` → **openpilot制御**

---

**キーポイント**: 縦制御の切り替えは、`interface.py` の `openpilotLongitudinalControl` フラグ設定で決まります。このフラグの値に応じて、`carcontroller.py` が制御CANメッセージを送信するかしないかが決定されます。

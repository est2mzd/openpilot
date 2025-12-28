# cereal - openpilotメッセージングシステム分析

## 概要

**cereal**は、openpilotの中核となるメッセージングシステムです。プロセス間通信（IPC）とデータシリアライゼーションを担当し、openpilotの全コンポーネント間でリアルタイムにデータを交換します。

### 基本情報

- **場所**: `/home/takuya/work/comma/openpilot/cereal/`
- **役割**: プロセス間メッセージング、データシリアライゼーション
- **バックエンド**: msgq (pub/sub)
- **シリアライゼーション**: Cap'n Proto
- **言語**: Cap'n Proto スキーマ定義 + Python/C++ 実装

---

## アーキテクチャ

### 1. 主要コンポーネント

```
cereal/
├── log.capnp           # メインメッセージ定義 (2,671行) - 全イベントタイプ
├── car.capnp           # 車両関連メッセージ (740行)
├── custom.capnp        # カスタムフォーク用予約領域 (71行)
├── legacy.capnp        # 後方互換性用レガシー定義 (574行)
├── services.py         # サービス定義とログ設定
├── services.h          # C++サービス定義
├── messaging/          # メッセージング実装
│   ├── messaging.h     # メッセージングAPI
│   ├── socketmaster.cc # ソケット管理
│   └── bridge.cc       # メッセージブリッジ
└── gen/                # 自動生成されたコード

合計: 約4,056行のスキーマ定義
```

### 2. メッセージングフロー

```
┌─────────────┐
│  Process A  │ (例: modeld)
│             │
│  publish()  │
└──────┬──────┘
       │ Cap'n Proto シリアライズ
       ↓
┌──────────────────────────────┐
│     msgq (Pub/Sub Queue)     │
│                              │
│  メッセージキュー管理         │
│  - トピック: "modelV2"       │
│  - 周波数: 20Hz              │
│  - ログ記録: Yes             │
└──────┬───────────────────────┘
       │ Cap'n Proto デシリアライズ
       ↓
┌─────────────┐
│  Process B  │ (例: controlsd)
│             │
│ subscribe() │
└─────────────┘
```

---

## 主要ファイルの詳細

### log.capnp - メインメッセージ定義

**最重要ファイル**: すべてのメッセージタイプを定義

- **行数**: 2,671行
- **中核構造体**: `Event` - すべてのメッセージの基底型

**主要な構造体群**:
1. **制御系**: `ControlsState`, `SelfdriveState`, `CarControl`
2. **車両状態**: `CarState`, `CarParams`
3. **センサー系**: `SensorEventData`, `GpsLocationData`, `CanData`
4. **モデル系**: `ModelDataV2`, `DrivingModelData`
5. **デバイス系**: `DeviceState`, `PandaState`
6. **プラン系**: `LongitudinalPlan`, `LateralPlan`

詳細: [log_capnp.md](cereal_analysis/log_capnp.md)

### car.capnp - 車両関連メッセージ

**車両固有のデータ構造**

- **行数**: 740行
- **主要構造体**: `CarParams`, `CarState`, `CarControl`

**内容**:
- 車両パラメータ（質量、ホイールベース、チューニング等）
- 車両状態（速度、加速度、ステアリング等）
- 車両制御コマンド（アクチュエーション）
- イベント定義（`OnroadEventDEPRECATED`）

詳細: [car_capnp.md](cereal_analysis/car_capnp.md)

### custom.capnp - カスタムフォーク用

**フォーク開発者向けの予約領域**

- **行数**: 71行
- **目的**: メインラインとの互換性を保ちながらカスタム機能追加

**重要な原則**:
- 予約された構造体のみを変更
- 識別子（`@0x...`）は変更しない
- 構造体名は変更可能
- フィールド追加は自由

**予約構造体**:
- `CustomReserved0` 〜 `CustomReserved9` (10個)

詳細: [custom_capnp.md](cereal_analysis/custom_capnp.md)

### services.py - サービス定義

**メッセージングサービスの設定**

**定義内容**:
- サービス名（例: "controlsState", "carState"）
- ログ記録の可否
- 更新周波数（Hz）
- ログ間引き設定（decimation）

**主要サービス**:
```python
"controlsState": (True, 100Hz, 10倍間引き)
"carState": (True, 100Hz, 10倍間引き)
"modelV2": (True, 20Hz, なし)
"can": (True, 100Hz, 2053倍間引き)
```

詳細: [services.md](cereal_analysis/services.md)

---

## 技術仕様

### Cap'n Proto スキーマ

**特徴**:
- 高速シリアライゼーション（ゼロコピー）
- 後方互換性のサポート
- 型安全性
- 多言語対応（C++, Python, Go等）

**スキーマ構造**:
```capnp
struct Event {
  logMonoTime @0 :UInt64;  # ナノ秒タイムスタンプ
  valid @1 :Bool;           # データ有効性フラグ
  
  union {
    # 100以上のメッセージタイプ
    controlsState @4 :ControlsState;
    carState @5 :Car.CarState;
    modelV2 @123 :ModelDataV2;
    # ... etc
  }
}
```

### メッセージング設計原則

**ベストプラクティス**:

1. **SI単位系の使用**
   - すべての物理量はSI単位で表現
   - 例外: フィールド名で明示的に指定（例: `speedMph`）

2. **明確なフィールド名**
   - コンテキスト内で曖昧さがないこと
   - 例: `steeringAngleDeg` (degree単位を明示)

3. **プロット可能な値**
   - 最小限のパースで人間が読める
   - グラフ化しやすいデータ構造

4. **後方互換性**
   - 構造体の追加: 安全
   - フィールドの追加: 安全
   - 既存フィールドの変更: 危険
   - フィールドの削除: 危険

---

## メッセージングパターン

### 1. Publish-Subscribe

**基本パターン**:
```python
# Publisher側 (modeld)
from cereal import log
pm = messaging.PubMaster(['modelV2'])

msg = messaging.new_message('modelV2')
msg.modelV2.frameId = frame_id
# ... データ設定
pm.send('modelV2', msg)

# Subscriber側 (controlsd)
sm = messaging.SubMaster(['modelV2'])
sm.update()
model_data = sm['modelV2']
```

### 2. 周波数とタイミング

**主要サービスの周波数**:
- **100Hz**: `controlsState`, `carState`, `carControl`, `can`
- **20Hz**: `modelV2`, `radarState`, `longitudinalPlan`
- **10Hz**: `pandaStates`, `gpsLocation`
- **4Hz**: `liveCalibration`, `liveTorqueParameters`

### 3. ログ記録と間引き

**ログ戦略**:
```python
# services.pyでの定義例
"can": (True, 100Hz, 2053倍間引き)
# → 100Hzで動作、ログには約3メッセージ/セグメント のみ記録
```

---

## 使用例

### メッセージの作成と送信

```python
import cereal.messaging as messaging
from cereal import log

# PubMasterの作成
pm = messaging.PubMaster(['customEvent'])

# メッセージの作成
msg = messaging.new_message('customEvent')
msg.logMonoTime = int(time.monotonic() * 1e9)
msg.valid = True

# データ設定（例: カスタムイベント）
msg.customEvent.value = 42
msg.customEvent.name = "test"

# 送信
pm.send('customEvent', msg)
```

### メッセージの受信

```python
import cereal.messaging as messaging

# SubMasterの作成
sm = messaging.SubMaster(['carState', 'controlsState'])

while True:
  # メッセージ更新
  sm.update()
  
  # データ取得
  car_state = sm['carState']
  controls_state = sm['controlsState']
  
  print(f"Speed: {car_state.vEgo} m/s")
  print(f"Enabled: {controls_state.enabled}")
```

---

## カスタムフォーク開発者向けガイド

### custom.capnpの使用方法

**ステップ1: 予約構造体を選択**
```capnp
# 変更前
struct CustomReserved0 @0x81c2f05a394cf4af {
}

# 変更後
struct MyCustomData @0x81c2f05a394cf4af {
  temperature @0 :Float32;
  pressure @1 :Float32;
  humidity @2 :Float32;
}
```

**ステップ2: log.capnpのEventに追加**
```capnp
struct Event {
  # ... 既存フィールド
  
  union {
    # ... 既存のメッセージタイプ
    
    customReserved0 @150 :Custom.MyCustomData;
    # ↑ 予約番号（150-159）を使用
  }
}
```

**ステップ3: services.pyに登録**
```python
_services = {
  # ... 既存サービス
  
  "myCustomData": (True, 10.),  # 10Hz, ログ記録あり
}
```

### 注意事項

⚠️ **やってはいけないこと**:
- 既存の構造体を変更
- 識別子（`@0x...`）を変更
- 予約されていない番号を使用
- メインラインのフィールドを削除

✅ **やるべきこと**:
- `CustomReserved` のみを使用
- 構造体名のリネーム
- 新しいフィールドの追加
- 適切なドキュメント作成

---

## パフォーマンス特性

### シリアライゼーション速度

**Cap'n Protoの利点**:
- ゼロコピー: メモリコピーなしで直接読み書き
- エンコード不要: バイナリ表現がそのままワイヤーフォーマット
- 高速: Protocol BuffersやJSON比で10-100倍高速

### メモリ使用量

**主要メッセージのサイズ**（概算）:
- `ModelDataV2`: ~50KB
- `ControlsState`: ~500B
- `CarState`: ~300B
- `CanData`: 可変（1-8バイト/メッセージ）

### リアルタイム性

**レイテンシ**:
- メッセージング: < 1ms
- シリアライゼーション: < 100μs
- 合計（end-to-end）: 通常 < 5ms

---

## トラブルシューティング

### よくある問題

1. **スキーマ変更後のコンパイルエラー**
   ```bash
   # 解決策: 再ビルド
   scons -j$(nproc)
   ```

2. **メッセージが受信できない**
   - サービス名のタイプミス確認
   - `services.py`に登録されているか確認
   - `sm.update()`を呼んでいるか確認

3. **後方互換性の破損**
   - 既存フィールドを変更していないか確認
   - `custom.capnp`のみを使用しているか確認

---

## 関連ドキュメント

- [log.capnp詳細](cereal_analysis/log_capnp.md) - 全メッセージタイプの詳細
- [car.capnp詳細](cereal_analysis/car_capnp.md) - 車両関連メッセージ
- [custom.capnp詳細](cereal_analysis/custom_capnp.md) - カスタムフォーク開発ガイド
- [services詳細](cereal_analysis/services.md) - サービス定義とログ設定
- [メッセージングAPI](cereal_analysis/messaging_api.md) - C++/Python API詳細

## 参考資料

- [Cap'n Proto公式ドキュメント](https://capnproto.org/)
- [msgq リポジトリ](https://github.com/commaai/msgq)
- [openpilot公式Wiki](https://github.com/commaai/openpilot/wiki)

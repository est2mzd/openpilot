# custom.capnp - カスタムフォーク開発ガイド

## 概要

`custom.capnp`は、**openpilotフォーク開発者**のための予約領域です。

- **ファイルパス**: `/home/takuya/work/comma/openpilot/cereal/custom.capnp`
- **行数**: 71行
- **目的**: 本家openpilotとのマージコンフリクト防止
- **対象**: sunnypilot, dragonpilot等のカスタムフォーク

---

## なぜcustom.capnpが必要か？

### 問題：マージコンフリクト

openpilotフォークを開発する際、`log.capnp`に新しいメッセージを追加すると：

```capnp
# log.capnpに追加したい（フォーク開発者）
struct Event {
  union {
    # ... 既存のメッセージ
    
    # カスタム追加（コンフリクトリスク！）
    myCustomMessage @999 :MyCustomMessage;
  }
}
```

**問題点**:
1. 本家が同じ`@999`を使うとコンフリクト
2. 本家の変更をマージするたびに手動解決が必要
3. フィールド番号の重複でバイナリ互換性が壊れる

### 解決策：custom.capnp

comma.aiが提供する**専用領域**:

```capnp
# custom.capnpで定義（安全！）
struct CustomReserved1 {
  myCustomField @0 :UInt32;
  myCustomData @1 :Text;
}
```

本家の`log.capnp`から参照:

```capnp
struct Event {
  union {
    # ... 既存メッセージ
    
    # フォーク用予約領域（本家が提供）
    customReserved1 @101 :Custom.CustomReserved1;
    customReserved2 @102 :Custom.CustomReserved2;
    # ... 最大9個
  }
}
```

**利点**:
- `@101`～`@109`はフォーク用に**永久予約**
- 本家の変更とコンフリクトしない
- バイナリ互換性を維持

---

## custom.capnpの構造

### 完全なファイル内容

```capnp
@0xc438490a0e3c2f64;

using import "log.capnp".Map;

# WARNING: DO NOT USE THESE IN MAINLINE OPENPILOT
# These are reserved for custom forks to minimize merge conflicts

struct CustomReserved0 {
}

struct CustomReserved1 {
}

struct CustomReserved2 {
}

struct CustomReserved3 {
}

struct CustomReserved4 {
}

struct CustomReserved5 {
}

struct CustomReserved6 {
}

struct CustomReserved7 {
}

struct CustomReserved8 {
}

struct CustomReserved9 {
}
```

### 予約領域の使用状況

| 構造体 | log.capnpフィールド番号 | 使用例 |
|--------|----------------------|--------|
| `CustomReserved0` | `@101` | sunnypilot: 一般用途 |
| `CustomReserved1` | `@102` | dragonpilot: 設定データ |
| `CustomReserved2` | `@103` | 未使用 |
| `CustomReserved3` | `@104` | 未使用 |
| `CustomReserved4` | `@105` | 未使用 |
| `CustomReserved5` | `@106` | 未使用 |
| `CustomReserved6` | `@107` | 未使用 |
| `CustomReserved7` | `@108` | 未使用 |
| `CustomReserved8` | `@109` | 未使用 |
| `CustomReserved9` | `@110` | 未使用 |

---

## 実際の使用例

### 例1：sunnypilotのカスタム機能

**要件**: MADS（Modified Assistive Driving Safety）状態を送信

#### ステップ1：custom.capnpを編集

```capnp
# /home/takuya/work/comma/openpilot/cereal/custom.capnp

struct CustomReserved0 {
  # MADS (Modified Assistive Driving Safety)
  madsEnabled @0 :Bool;                # MADS有効
  madsLeadBraking @1 :Bool;            # 先行車ブレーキ中
  madsPauseLateral @2 :Bool;           # 横制御一時停止
  
  # DEC (Dynamic End-to-end Control)
  decEnabled @3 :Bool;
  decConfidence @4 :Float32;
  
  # SLA (Speed Limit Assist)
  slaEnabled @5 :Bool;
  slaSpeedLimit @6 :Float32;           # 速度制限 (m/s)
  slaSpeedLimitOffset @7 :Float32;     # オフセット
  
  # カスタムログ
  debugMessage @8 :Text;
}
```

#### ステップ2：データを送信

```python
# selfdrive/controls/controlsd.py

import cereal.messaging as messaging
from cereal import custom, log

# msgq送信準備
sm = messaging.SubMaster(['carState', 'modelV2'])
pm = messaging.PubMaster(['customReserved0'])

while True:
  # データ収集
  sm.update()
  
  # カスタムメッセージ作成
  msg = messaging.new_message('customReserved0')
  custom_data = msg.customReserved0
  
  # MADS状態設定
  custom_data.madsEnabled = self.mads.enabled
  custom_data.madsLeadBraking = self.mads.lead_braking
  custom_data.madsPauseLateral = self.mads.pause_lateral
  
  # DEC状態設定
  custom_data.decEnabled = self.dec.enabled
  custom_data.decConfidence = self.dec.confidence
  
  # SLA状態設定
  custom_data.slaEnabled = self.sla.enabled
  custom_data.slaSpeedLimit = self.sla.speed_limit
  custom_data.slaSpeedLimitOffset = self.sla.offset
  
  # デバッグ
  custom_data.debugMessage = f"MADS: {self.mads.state}"
  
  # 送信
  pm.send('customReserved0', msg)
  
  time.sleep(0.01)  # 100Hz
```

#### ステップ3：データを受信

```python
# selfdrive/ui/ui.py

import cereal.messaging as messaging

sm = messaging.SubMaster(['customReserved0'])

while True:
  sm.update()
  
  if sm.updated['customReserved0']:
    custom = sm['customReserved0']
    
    # UI表示
    if custom.madsEnabled:
      draw_mads_indicator(custom.madsLeadBraking)
    
    if custom.slaEnabled:
      draw_speed_limit(custom.slaSpeedLimit)
    
    # デバッグ
    print(custom.debugMessage)
```

#### ステップ4：services.pyに登録

```python
# cereal/services.py

SERVICE_LIST = {
  # ... 既存サービス
  
  # カスタムサービス
  "customReserved0": (100., True),  # 100Hz, ログ記録
}
```

---

### 例2：dragonpilotの設定データ

**要件**: UI設定をプロセス間で共有

```capnp
# custom.capnp

struct CustomReserved1 {
  # UI設定
  uiBrightness @0 :UInt8;              # 画面輝度 (0-100)
  uiVolume @1 :UInt8;                  # 音量 (0-100)
  
  # 運転設定
  drivingPersonality @2 :Personality;
  enum Personality {
    normal @0;
    sport @1;
    eco @2;
  }
  
  # 車間距離設定
  followDistance @3 :FollowDistance;
  enum FollowDistance {
    short @0;
    medium @1;
    long @2;
  }
  
  # 機能切り替え
  lanelessMode @4 :Bool;               # レーンレスモード
  dynamicFollowMode @5 :Bool;          # ダイナミックフォロー
  
  # 設定マップ（柔軟性）
  settings @6 :List(Setting);
  struct Setting {
    key @0 :Text;
    value @1 :Text;
  }
}
```

---

### 例3：マップデータの追加

**要件**: ローカルマップデータを配信

```capnp
# custom.capnp

struct CustomReserved2 {
  # マップタイル
  mapTiles @0 :List(MapTile);
  struct MapTile {
    lat @0 :Float64;                   # 緯度
    lon @1 :Float64;                   # 経度
    zoom @2 :UInt8;                    # ズームレベル
    data @3 :Data;                     # タイルデータ（PNG等）
  }
  
  # 速度制限情報
  speedLimits @1 :List(SpeedLimit);
  struct SpeedLimit {
    lat @0 :Float64;
    lon @1 :Float64;
    limit @2 :Float32;                 # 制限速度 (m/s)
    radius @3 :Float32;                # 有効範囲 (m)
  }
  
  # POI（Point of Interest）
  pois @2 :List(POI);
  struct POI {
    lat @0 :Float64;
    lon @1 :Float64;
    type @2 :POIType;
    name @3 :Text;
    
    enum POIType {
      restaurant @0;
      gasStation @1;
      chargingStation @2;
      parking @3;
    }
  }
}
```

---

## ベストプラクティス

### 1. 予約領域の選択

**ガイドライン**:
- `CustomReserved0`: 最初のフォーク機能に使用
- `CustomReserved1`: 2番目の主要機能
- 複数の小機能は1つの構造体にまとめる

**悪い例**（無駄）:
```capnp
struct CustomReserved0 {
  feature1 @0 :Bool;
}

struct CustomReserved1 {
  feature2 @0 :Bool;
}

struct CustomReserved2 {
  feature3 @0 :Bool;
}
```

**良い例**（効率的）:
```capnp
struct CustomReserved0 {
  feature1 @0 :Bool;
  feature2 @1 :Bool;
  feature3 @2 :Bool;
  # まだ6個以上追加可能
}
```

### 2. フィールド番号の管理

**ルール**:
- 一度使ったフィールド番号は**絶対に再利用しない**
- 削除する場合はコメントアウトのみ

**悪い例**（後方互換性破壊）:
```capnp
struct CustomReserved0 {
  oldField @0 :UInt32;  # 削除して番号再利用
  newField @0 :Text;    # NG！！！
}
```

**良い例**（後方互換性維持）:
```capnp
struct CustomReserved0 {
  # oldField @0 :UInt32;  # 廃止（将来の互換性のため番号保持）
  newField @1 :Text;      # 新しい番号を使用
}
```

### 3. デフォルト値の設定

**Cap'n Protoのデフォルト**:
- `Bool`: `false`
- 数値: `0`
- `Text`: `""`（空文字列）
- `List`: `[]`（空リスト）

**明示的なデフォルト値**（推奨）:
```capnp
struct CustomReserved0 {
  enabled @0 :Bool = false;            # 明示的
  timeout @1 :Float32 = 5.0;           # デフォルト5秒
  mode @2 :Mode = normal;              # デフォルトノーマル
  
  enum Mode {
    normal @0;
    sport @1;
  }
}
```

### 4. ドキュメント化

**必須コメント**:
```capnp
struct CustomReserved0 {
  # MADS (Modified Assistive Driving Safety)
  # 目的: 先行車追従中の横制御一時停止
  # 更新周波数: 100Hz
  # 作成者: sunnypilot team
  # バージョン: v1.2.0
  
  madsEnabled @0 :Bool;                # MADS機能有効化
  madsLeadBraking @1 :Bool;            # 先行車ブレーキ検出
  madsPauseLateral @2 :Bool;           # 横制御一時停止フラグ
}
```

### 5. 複数フォーク間の調整

**推奨される使い分け**:

| フォーク | 使用予約領域 | 備考 |
|----------|------------|------|
| sunnypilot | CustomReserved0, 1 | MADS, DEC, SLA等 |
| dragonpilot | CustomReserved2, 3 | UI設定、DP機能 |
| FrogPilot | CustomReserved4, 5 | カエル機能 |
| 他の小規模フォーク | CustomReserved6-9 | 共用可 |

**コミュニティ調整**:
- Discord等で使用予約領域を公開
- 重複を避けて開発

---

## 高度な使用例

### Map型の活用

`log.capnp`で定義されている`Map`型を使用:

```capnp
using import "log.capnp".Map;

struct CustomReserved3 {
  # 動的な設定（キー・バリュー）
  settings @0 :List(Map(Text, Text));
  
  # 例:
  # [
  #   {key: "mads_enabled", value: "true"},
  #   {key: "dec_mode", value: "aggressive"},
  #   {key: "sla_offset", value: "5.0"}
  # ]
}
```

**使用例**:
```python
msg = messaging.new_message('customReserved3')
custom = msg.customReserved3

# 設定追加
settings = []
settings.append({'key': 'mads_enabled', 'value': 'true'})
settings.append({'key': 'dec_mode', 'value': 'aggressive'})
custom.settings = settings
```

### ネストした構造体

```capnp
struct CustomReserved4 {
  # 複雑なデータ構造
  vehicles @0 :List(Vehicle);
  
  struct Vehicle {
    id @0 :UInt64;
    position @1 :Position;
    velocity @2 :Velocity;
    classification @3 :Classification;
    
    struct Position {
      x @0 :Float32;
      y @1 :Float32;
      z @2 :Float32;
    }
    
    struct Velocity {
      vx @0 :Float32;
      vy @1 :Float32;
      vz @2 :Float32;
    }
    
    enum Classification {
      unknown @0;
      car @1;
      truck @2;
      pedestrian @3;
      bicycle @4;
    }
  }
}
```

---

## トラブルシューティング

### 問題1：custom.capnpの変更が反映されない

**原因**: Cap'n Protoの再コンパイルが必要

**解決策**:
```bash
cd /home/takuya/work/comma/openpilot/cereal
scons -j8  # cerealを再ビルド
```

### 問題2：古いログが読めない

**原因**: フィールドを削除または変更した

**解決策**: 後方互換性を保つ
```capnp
# 削除せずコメントアウト
struct CustomReserved0 {
  # oldField @0 :UInt32;  # 廃止（互換性のため残す）
  newField @1 :Text;
}
```

### 問題3：他のフォークとコンフリクト

**原因**: 同じCustomReservedXを使用

**解決策**: 使用領域を調整
```capnp
# フォークA: CustomReserved0使用
# フォークB: CustomReserved1に変更
```

---

## 本家へのコントリビューション

### カスタム機能を本家に提案する場合

**ステップ**:
1. `custom.capnp`でプロトタイプ開発
2. 機能が安定したらPull Requestを作成
3. comma.aiのレビュー後、`log.capnp`に正式追加

**移行例**:
```capnp
# custom.capnp（開発中）
struct CustomReserved0 {
  experimentalFeature @0 :Bool;
}

# ↓ 本家採用後

# log.capnp（正式版）
struct Event {
  union {
    # ... 
    experimentalFeature @150 :ExperimentalFeature;  # 新しい番号
  }
}

struct ExperimentalFeature {
  enabled @0 :Bool;
}
```

---

## まとめ

`custom.capnp`のポイント:

1. **マージコンフリクト回避**: 本家との衝突を防ぐ
2. **10個の予約領域**: CustomReserved0～9を使い分け
3. **後方互換性**: フィールド番号を再利用しない
4. **フォーク間調整**: 重複しないよう使用領域を公開
5. **本家への道**: プロトタイプ→PR→正式採用

**推奨ワークフロー**:
1. 新機能は`custom.capnp`でプロトタイプ
2. `services.py`に登録して動作確認
3. ドキュメント化してコミュニティ共有
4. 安定したら本家へPR

---

## 関連ドキュメント

- [README.md](README.md) - cereal全体の概要
- [log.capnp詳細](log_capnp.md) - メインメッセージ定義
- [services詳細](services.md) - サービス定義とログ設定

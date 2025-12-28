# sunnypilot - UIの詳細比較

## 概要
sunnypilotと本家openpilot（あなたのフォーク）のユーザーインターフェース（UI）の違いを詳しく比較します。

sunnypilotは、本家openpilotのUIを完全に再設計し、より直感的で機能豊富なユーザー体験を提供しています。

---

## 1. オフロードUI（設定画面）の完全再設計

### 本家openpilot

**構造**: シンプルな設定リスト
```
Settings
├── Device
├── Network
├── Toggles（各種ON/OFF設定が縦に並ぶ）
└── Software
```

特徴:
- 全ての設定が1つのリストに表示
- カテゴリ分けが最小限
- スクロールが多い

### sunnypilot

**構造**: 専用パネルによる組織化
```
Settings
├── Device（デバイス設定）
├── Network（ネットワーク設定）
├── Steering（ステアリング設定）★新設
├── Longitudinal（縦制御設定）★新設
├── Vehicle（車両設定）★新設
├── Models（モデル管理）★新設
├── Visuals（表示設定）★新設
├── Display（ディスプレイ設定）★新設
├── Trips（トリップ記録）★新設
└── Software（ソフトウェア設定）
```

**ファイルパス**: `/home/takuya/work/comma/other_repo/sunny/selfdrive/ui/qt/offroad/`

特徴:
- 機能ごとに専用パネル
- 見つけやすく、管理しやすい
- Advanced Controlsトグルで初心者/上級者モード切り替え

---

## 2. 新設パネルの詳細

### 2.1 Steering パネル

**設定項目**:
- NNLC (Neural Network Lateral Control) 有効化
- Pause Lateral on Blinker（ウインカーで横制御一時停止）
- Enforce Torque Lateral Control（トルク制御強制）
- Custom Lateral Tuning（カスタムチューニング）

### 2.2 Longitudinal パネル

**設定項目**:
- MADS有効化
- DEC有効化
- Speed Limit Assist (SLA) 設定
  - SLA有効化
  - オフセット調整（+5 km/hなど）
  - データソース選択（Car / Map / Combined）
- ICBM有効化
- SCC-M / SCC-V有効化
- Custom ACC Setpoint Increments（ACC速度増分カスタマイズ）

### 2.3 Vehicle パネル

**設定項目**:
- Vehicle Selector（車両手動選択）
- Platform Selector（プラットフォーム選択）
- Current Fingerprint表示
- 車種固有の設定

### 2.4 Models パネル ★最も革新的

**ファイルパス**: `/home/takuya/work/comma/other_repo/sunny/selfdrive/ui/qt/offroad/models/`

**機能**:
1. **モデル一覧表示**
   - 86+のドライビングモデルを表示
   - モデル名、日付、バージョン情報

2. **ダウンロードマネージャー**
   ```
   [Model Name]
   Status: Not Downloaded / Downloading (45%) / Downloaded
   Size: 50 MB
   [Download] [Delete] [⭐ Favorite]
   ```

3. **お気に入り機能**
   - モデルに⭐をつけて管理
   - お気に入りモデルのみフィルター表示

4. **Fuzzy Search（あいまい検索）**
   - モデル名で検索
   - 例: "night" → "Night Strike"モデルが表示

5. **モデルフォルダー管理**
   - モデルの保存場所指定
   - 外部ストレージ対応

6. **キャッシュ更新**
   - 最新モデルリストを取得
   - リフレッシュボタン

### 2.5 Visuals パネル

**設定項目**:
- Rainbow Road Path（Tesla風の虹色パス表示）🌈
- On-Screen Turn Signals（画面上のウインカー表示）
- Blind Spot Indicators（ブラインドスポット表示）
- Green Light Indicator（青信号表示）
- Lead Vehicle Indicator（先行車表示強化）
- Lead Chevron Info（先行車シェブロン情報）
- Standstill Timer（停止時間タイマー）
- Road Name Display（道路名表示）

### 2.6 Display パネル

**設定項目**:
- Brightness Controls（明るさ調整）
  - Auto Brightness（自動調整）
  - Manual Brightness（手動調整）
- Screen Off While Driving（運転中画面オフ）
  - Wake on Alert（アラート時に自動点灯）
  - Interaction Timeout（タッチ後の消灯時間）
- Custom Screen Timeout（カスタム画面タイムアウト）
- Developer UI（開発者UI）
  - Alert Positioning（アラート位置調整）
  - Error Log Viewer（エラーログ閲覧）

### 2.7 Trips パネル

**機能**:
- 走行記録の表示
- 距離、時間、平均速度
- トリップ履歴

---

## 3. オンロードUI（運転中画面）の強化

### 3.1 情報表示の拡張

#### 本家openpilot

基本情報のみ:
- 速度表示
- 先行車との距離
- パス表示
- エンゲージ状態

#### sunnypilot

豊富な情報表示:
```
┌─────────────────────────────────────┐
│  [Road Name: Route 101]            │ ← 道路名表示
│                                     │
│  ┌───────────────────────────┐     │
│  │     🚗 Lead Vehicle        │     │ ← 先行車情報強化
│  │     Distance: 50m          │     │
│  │     Rel Speed: -5 km/h     │     │
│  └───────────────────────────┘     │
│                                     │
│  Speed: 80 km/h                    │
│  Limit: 100 km/h [SLA Active]      │ ← 制限速度表示
│                                     │
│  🌈 Rainbow Road Path 🌈           │ ← レインボーパス
│                                     │
│  ◀ Turn Signal                     │ ← ウインカー表示
│                                     │
│  ⚠ Blind Spot Warning              │ ← ブラインドスポット
│                                     │
│  🚦 Green Light Ahead              │ ← 青信号表示
│                                     │
│  ⏱ Stopped: 00:45                  │ ← 停止時間タイマー
└─────────────────────────────────────┘
```

### 3.2 Rainbow Road Path（虹色パス）🌈

**ファイルパス**: `/home/takuya/work/comma/other_repo/sunny/selfdrive/ui/qt/onroad/`

**機能**:
- Tesla風の虹色グラデーションパス
- 視認性向上
- カスタマイズ可能な色

```python
# onroad_ui.py
def draw_rainbow_path(painter, path_points):
    for i, point in enumerate(path_points):
        # グラデーションカラー計算
        hue = (i / len(path_points)) * 360
        color = QColor.fromHsv(hue, 255, 255)
        painter.setPen(color)
        painter.drawPoint(point)
```

### 3.3 On-Screen Turn Signals

**機能**:
- ウインカー操作時に画面上に矢印表示
- 左右で異なるアニメーション
- 視認性向上

```
Left Turn:  ◀◀◀ (点滅)
Right Turn: ▶▶▶ (点滅)
```

### 3.4 Blind Spot Indicators

**機能**:
- ブラインドスポット警告を画面に表示
- 車両CANから情報取得
- サイドミラー視覚強化

### 3.5 Green Light Indicator

**機能**:
- 前方の信号が青になったことを通知
- モデルの信号認識から取得
- 発進タイミングをサポート

### 3.6 Lead Chevron Info

**機能**:
- 先行車情報を詳細表示
- 距離、相対速度、加速度
- シェブロンのサイズで距離を視覚化

### 3.7 Road Name Display

**機能**:
- 現在走行中の道路名を表示
- OpenStreetMapデータから取得
- mapd統合により実現

---

## 4. Screen Off While Driving（運転中画面オフ）

### 機能概要

**目的**: 夜間運転時の眩しさ軽減、バッテリー節約

**設定オプション**:
1. **完全オフ**
   - 画面を完全に消灯
   - タッチで復帰

2. **アラート時自動点灯**
   - 重要なアラート時のみ点灯
   - 警告を見逃さない

3. **カスタムタイムアウト**
   - タッチ後の消灯時間を設定
   - 5秒、10秒、30秒など

**実装**:
```python
# display_manager.py
class DisplayManager:
    def __init__(self):
        self.screen_off_while_driving = Params().get_bool("ScreenOffWhileDriving")
        self.wake_on_alert = Params().get_bool("WakeOnAlert")
        self.timeout = int(Params().get("ScreenTimeout"))
    
    def update(self, onroad, alert_level):
        if onroad and self.screen_off_while_driving:
            if self.wake_on_alert and alert_level > AlertLevel.LOW:
                self.turn_on_screen()
            else:
                self.turn_off_screen_after_timeout()
```

---

## 5. Advanced Controls Toggle

### 機能概要

**目的**: 初心者と上級者で設定の複雑さを調整

**動作**:
- **OFF（初心者モード）**: 基本的な設定のみ表示
- **ON（上級者モード）**: 全ての設定を表示

**例**:
```
Steering Panel:
├── [基本] Lateral Control Enable
└── [Advanced] ★ NNLC Fine Tuning
    └── [Advanced] ★ Custom Lateral Gains
        └── [Advanced] ★ Steer Ratio Override
```

★マークは Advanced Controls ON 時のみ表示

---

## 6. Branch & Platform Selectors

### Branch Selector（ブランチ選択）

**機能**:
- release / staging / dev ブランチを選択
- 検索機能付き
- カスタムブランチ名入力可能

**UI**:
```
Target Branch: [staging ▼]
─────────────────────────
Search: [_____________]
─────────────────────────
☑ release       (安定版)
☐ staging       (ベータ版)
☐ dev           (開発版)
☐ custom-branch (カスタム)
```

### Platform Selector（プラットフォーム選択）

**機能**:
- 現在のフィンガープリント表示
- 手動で車種選択可能
- Vehicle Selectorと統合

---

## 7. Developer UI強化

### Error Log Viewer（エラーログ閲覧）

**機能**:
- リアルタイムエラーログ表示
- フィルター機能
- エクスポート機能

**UI**:
```
Error Log Viewer
────────────────────────────────
Filter: [ERROR ▼] [WARN] [INFO]
────────────────────────────────
[2025-12-21 10:30:45] ERROR: CAN timeout
[2025-12-21 10:30:50] WARN: Model download slow
[2025-12-21 10:31:00] INFO: Speed limit updated
────────────────────────────────
[Export] [Clear] [Refresh]
```

### Alert Positioning（アラート位置調整）

**機能**:
- アラートの表示位置をカスタマイズ
- 上部/中央/下部
- サイズ調整

---

## 8. Convenience Features（便利機能）

### 8.1 Exit Offroad Button

**機能**: 設定画面から簡単に運転モードへ復帰

```
[← Back to Driving]  ← ボタン追加
```

### 8.2 Always Offroad Mode

**機能**: デバッグ用に常にオフロードモードを維持

### 8.3 Quiet Mode（静音モード）

**機能**: 
- アラート音を消音
- 視覚アラートのみ
- 夜間/静かな環境で有用

### 8.4 Max Time Offroad Setting

**機能**:
- オフロード状態の最大時間を設定
- バッテリー節約
- 自動シャットダウン

---

## 9. OpenStreetMap Database Downloader

### 機能概要

**ファイルパス**: `/home/takuya/work/comma/other_repo/sunny/sunnypilot/mapd/`

**機能**:
- OpenStreetMapデータのダウンロード
- オフライン使用可能
- SLA、道路名表示に使用

**UI**:
```
OpenStreetMap Downloader
──────────────────────────────────
Search Location: [Tokyo, Japan____]
──────────────────────────────────
📍 Tokyo, Japan (25 MB)
📍 Osaka, Japan (18 MB)
📍 Kyoto, Japan (12 MB)
──────────────────────────────────
Selected: Tokyo, Japan
[Download] [Delete] [Update]
──────────────────────────────────
Progress: ████████░░ 80%
```

**検索機能**:
- 都市名、州名、国名で検索
- Fuzzy Search対応
- 自動補完

---

## 10. UIアーキテクチャ比較

### 本家openpilot

```
selfdrive/ui/
├── qt/
│   ├── offroad/
│   │   ├── settings.cc        # 設定画面（シンプル）
│   │   └── wifiManager.cc     # Wi-Fi設定
│   └── onroad/
│       └── onroad_home.cc     # 運転中画面（基本）
└── soundd/                     # 音声管理
```

### sunnypilot

```
selfdrive/ui/
├── qt/
│   ├── offroad/
│   │   ├── settings.cc                # メイン設定
│   │   ├── panels/                    # ★専用パネル
│   │   │   ├── steering_panel.cc
│   │   │   ├── longitudinal_panel.cc
│   │   │   ├── vehicle_panel.cc
│   │   │   ├── models_panel.cc        # ★モデル管理
│   │   │   ├── visuals_panel.cc       # ★ビジュアル設定
│   │   │   ├── display_panel.cc
│   │   │   └── trips_panel.cc
│   │   ├── wifiManager.cc
│   │   └── advanced_controls.cc       # ★上級者設定
│   └── onroad/
│       ├── onroad_home.cc             # 運転中画面（拡張）
│       ├── rainbow_path.cc            # ★レインボーパス
│       ├── turn_signals.cc            # ★ウインカー表示
│       ├── blind_spot.cc              # ★ブラインドスポット
│       ├── green_light.cc             # ★青信号表示
│       └── road_name.cc               # ★道路名表示
├── soundd/                             # 音声管理
└── display/                            # ★ディスプレイ管理
    ├── screen_manager.cc               # 画面オン/オフ
    └── brightness_control.cc           # 明るさ制御
```

---

## 11. 主要な違いまとめ

| 項目 | 本家openpilot | sunnypilot |
|------|--------------|-----------|
| **設定構造** | 1つのリスト | 専用パネルで整理 |
| **設定項目数** | 約20項目 | 約100+項目 |
| **モデル管理** | コマンドライン | GUI（ダウンロード/削除/検索） |
| **オンロードUI** | 基本情報のみ | 豊富な情報表示 |
| **ビジュアル** | シンプル | カスタマイズ可能（虹色パス等） |
| **画面制御** | 常時点灯 | 運転中オフ可能 |
| **初心者対応** | 全設定表示 | Advanced Controlsで段階的 |
| **検索機能** | なし | あり（モデル、場所等） |
| **エラーログ** | コマンドライン | GUI閲覧可能 |
| **開発者UI** | 最小限 | 充実（ログ、デバッグ情報） |

---

## 12. ユーザー体験の向上

### 本家openpilotの課題

1. 設定が探しにくい
2. モデル管理が面倒（SSH必要）
3. 情報表示が少ない
4. カスタマイズ性が低い

### sunnypilotの改善

1. **見つけやすさ**: 専用パネルで整理
2. **使いやすさ**: GUI操作で完結
3. **情報量**: 豊富な表示オプション
4. **カスタマイズ**: 細かい調整が可能
5. **段階的学習**: Advanced Controlsで初心者にも優しい

---

## まとめ

sunnypilotのUIは、本家openpilotを大幅に改善し：

1. **整理されたUI**: 専用パネルで機能を分類
2. **モデル管理の革新**: GUIでダウンロード・管理
3. **豊富な情報表示**: 運転中の情報を大幅拡充
4. **視覚的魅力**: レインボーパス、ウインカー表示等
5. **ユーザビリティ**: 初心者から上級者まで対応

本家が必要最小限のUIを提供する一方、sunnypilotは快適で機能豊富なユーザー体験を提供しています。

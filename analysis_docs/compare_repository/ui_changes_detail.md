# UI変更の詳細比較

## 概要
ichiroブランチと本家openpilotのUI（ユーザーインターフェース）の違いと変更内容を詳細に解説します。

## 1. UIアーキテクチャの違い

### 本家openpilot
- **実装言語**: C++ (Qt5) + Python (一部レンダリング)
- **UIフレームワーク**: Qt Widgets
- **画面構成**: 
  - Onroad UI: AnnotatedCameraWidget（メインビュー） + Alerts（警告）+ Hud（速度表示）
  - Offroad UI: ホーム画面、設定画面、統計画面
- **ディレクトリ構造**: 
  ```
  selfdrive/ui/
  ├── qt/
  │   ├── onroad/         # 走行中のUI（新しい構造）
  │   │   ├── alerts.cc/h
  │   │   ├── annotated_camera.cc/h
  │   │   ├── hud.cc/h
  │   │   └── onroad_home.cc/h
  │   ├── widgets/
  │   └── home.cc/h
  ├── onroad/             # Python実装（レイリブ用）
  └── layouts/            # レイアウト定義
  ```

### ichiroブランチ
- **実装言語**: C++ (Qt5)
- **UIフレームワーク**: Qt Widgets（本家と同じ）
- **画面構成**: 本家とほぼ同じだが、カスタマイズあり
- **ディレクトリ構造**:
  ```
  selfdrive/ui/
  ├── qt/
  │   ├── onroad.cc/h     # 走行中のUI（単一ファイル、旧構造）
  │   ├── widgets/
  │   └── home.cc/h
  ├── navd/               # ナビゲーション関連（本家にない）
  └── mui.cc              # 追加UI（本家にない）
  ```

## 2. ファイル構造の主要な違い

### 2.1 Onroad UI の構造

#### 本家（モジュール化）
```
selfdrive/ui/qt/onroad/
├── alerts.cc/.h          # アラート表示専用
├── annotated_camera.cc/.h # カメラビュー + レーン描画
├── hud.cc/.h             # 速度表示HUD専用
└── onroad_home.cc/.h     # Onroad画面の統合
```

各機能が独立したファイルに分離されており、保守性が高い。

#### ichiro（モノリシック）
```
selfdrive/ui/qt/
└── onroad.cc/.h          # すべてを1ファイルに統合
    ├── OnroadWindow      # メインウィンドウ
    ├── OnroadAlerts      # アラート
    ├── OnroadHud         # HUD
    └── NvgWindow         # カメラビュー
```

すべての機能が単一ファイルに含まれている（旧openpilotの構造を踏襲）。

### 2.2 Alert（警告）システムの違い

#### 本家 alerts.cc
```cpp
class OnroadAlerts : public QWidget {
public:
  struct Alert {
    QString text1;
    QString text2;
    QString type;
    cereal::SelfdriveState::AlertSize size;
    cereal::SelfdriveState::AlertStatus status;
    
    bool equal(const Alert &other) const {
      return text1 == other.text1 && text2 == other.text2 && type == other.type;
    }
  };
  
  void updateState(const UIState &s);  // 状態更新
  void clear();                        // クリア機能
  Alert getAlert(const SubMaster &sm, uint64_t started_frame);  // アラート取得
  
private:
  const QMap<cereal::SelfdriveState::AlertStatus, QColor> alert_colors = {
    {cereal::SelfdriveState::AlertStatus::NORMAL, QColor(0x15, 0x15, 0x15, 0xf1)},
    {cereal::SelfdriveState::AlertStatus::USER_PROMPT, QColor(0xDA, 0x6F, 0x25, 0xf1)},
    {cereal::SelfdriveState::AlertStatus::CRITICAL, QColor(0xC9, 0x22, 0x31, 0xf1)},
  };
};
```

**特徴**:
- `updateState()` で自動的にアラート取得
- 新しいアラートタイプ：`SelfdriveState::AlertStatus`
- 3段階の色分け（NORMAL/USER_PROMPT/CRITICAL）

#### ichiro onroad.cc
```cpp
class OnroadAlerts : public QWidget {
public:
  void updateAlert(const Alert &a, const QColor &color);  // 外部からアラート受け取り
  
private:
  QColor bg;
  Alert alert = {};
};

// 呼び出し側（OnroadWindow）
Alert alert = Alert::get(*(s.sm), s.scene.started_frame);
if (alert.type == "controlsUnresponsive") {
  bgColor = bg_colors[STATUS_ALERT];
} else if (alert.type == "controlsUnresponsivePermanent") {
  bgColor = bg_colors[STATUS_DISENGAGED];
}
alerts->updateAlert(alert, bgColor);
```

**特徴**:
- `updateAlert()` で外部から直接アラート受け取り
- アラートタイプは文字列ベース
- 背景色は呼び出し側で決定

### 2.3 HUD（速度表示）の違い

#### 本家 hud.cc
```cpp
class HudRenderer {
public:
  void updateState(const UIState &s);
  void draw(QPainter &p, const QRect &surface_rect);
  
private:
  void drawSetSpeed(QPainter &p, const QRect &surface_rect);
  void drawCurrentSpeed(QPainter &p, const QRect &surface_rect);
  void drawText(QPainter &p, int x, int y, const QString &text, int alpha);
  
  // 状態変数
  QString speed;
  QString set_speed;
  int status = STATUS_DISENGAGED;
  bool is_cruise_set = false;
  // ...
};
```

**特徴**:
- 描画ロジックが細分化（`drawSetSpeed`, `drawCurrentSpeed`）
- HudRendererクラスとして独立

#### ichiro onroad.cc
```cpp
class OnroadHud : public QWidget {
  Q_OBJECT
  Q_PROPERTY(QString speed MEMBER speed NOTIFY valueChanged);
  Q_PROPERTY(QString speedUnit MEMBER speedUnit NOTIFY valueChanged);
  Q_PROPERTY(QString maxSpeed MEMBER maxSpeed NOTIFY valueChanged);
  Q_PROPERTY(bool is_cruise_set MEMBER is_cruise_set NOTIFY valueChanged);
  Q_PROPERTY(bool engageable MEMBER engageable NOTIFY valueChanged);
  Q_PROPERTY(bool dmActive MEMBER dmActive NOTIFY valueChanged);
  
public:
  void updateState(const UIState &s);
  
private:
  void paintEvent(QPaintEvent *event) override;
  void drawIcon(QPainter &p, int x, int y, QPixmap &img, QBrush bg, float opacity);
  void drawText(QPainter &p, int x, int y, const QString &text, int alpha = 255);
  
  QPixmap engage_img;
  QPixmap dm_img;
  const int radius = 192;
  const int img_size = radius / 2;
  // ... 状態変数
};
```

**特徴**:
- Qt Propertyシステムを使用（シグナル/スロット）
- `valueChanged` シグナルで自動再描画
- アイコン描画機能（engage_img, dm_img）

## 3. オンロードUIの追加機能（ichiro独自）

### 3.1 速度調整ボタンの追加

ichiroブランチには、オンロード画面に速度調整ボタンが追加されています（コミット履歴より）：

```
* 6f517b10e _set_speed_MAX_button.幅調整。
* 7d313cc2c ボタン判定範囲被り修正。
* 1a59fee52 onroadに⇧、↓ボタンを追加。
* 5d66ad168 -piY , ボタンCG追加
```

**機能**:
- 走行中に画面上で直接クルーズ速度を調整
- ⇧（上）ボタン：速度アップ
- ↓（下）ボタン：速度ダウン

### 3.2 追加情報表示

```
* 72bc427da 追加情報表示
* e20700acf standstillの状態を確認。
```

ichiroブランチでは、デバッグや確認用の追加情報が表示されます：
- `standstill` 状態の表示（完全停止の確認）
- その他の内部状態の可視化

## 4. ナビゲーション機能

### ichiro独自: navdディレクトリ
```
other_repo/ichiro/selfdrive/ui/navd/
```

本家にはない独自のナビゲーション関連UIが実装されています。

## 5. レイアウトとスタイリングの違い

### 本家のスタイル（統一感重視）
```cpp
// alerts.cc
const QMap<cereal::SelfdriveState::AlertStatus, QColor> alert_colors = {
  {cereal::SelfdriveState::AlertStatus::NORMAL, QColor(0x15, 0x15, 0x15, 0xf1)},
  {cereal::SelfdriveState::AlertStatus::USER_PROMPT, QColor(0xDA, 0x6F, 0x25, 0xf1)},
  {cereal::SelfdriveState::AlertStatus::CRITICAL, QColor(0xC9, 0x22, 0x31, 0xf1)},
};
```

### ichiroのスタイル（カスタマイズ）
```cpp
// onroad.cc
static std::map<UIStatus, QColor> bg_colors = {
  {STATUS_DISENGAGED, QColor(0x17, 0x33, 0x49, 0xc8)},
  {STATUS_ENGAGED, QColor(0x17, 0x86, 0x44, 0xf1)},
  {STATUS_WARNING, QColor(0xDA, 0x6F, 0x25, 0xf1)},
  {STATUS_ALERT, QColor(0xC9, 0x22, 0x31, 0xf1)},
};
```

## 6. Offroad UI（ホーム画面）の違い

### 本家
```cpp
// home.cc
class OffroadHome : public QFrame {
  // 標準的なホーム画面
  // - UpdateAlert（アップデート通知）
  // - OffroadAlert（オフロードアラート）
  // - DriveStats（走行統計）
  // - SetupWidget（セットアップ）
};
```

### ichiro
```cpp
// home.cc
class OffroadHome : public QFrame {
  // カスタマイズされたホーム画面
  // - 独自のレイアウト調整
  // - 追加のウィジェット
  alert_notif->setStyleSheet("background-color: #E22C2C;");
  // WiFi選択のスクロール上下パディング調整（コミット履歴より）
};
```

コミット履歴からの変更：
```
* 66b7a2cbf Wifi選択のスクロール上下パディング調整。
```

## 7. Python UI（レイリブ向け）

### 本家: 完全なPython UI実装
```python
# selfdrive/ui/onroad/alert_renderer.py
class AlertRenderer:
    def draw(self, rect: rl.Rectangle, sm: messaging.SubMaster) -> None:
        alert = self.get_alert(sm)
        if not alert:
            return
        # レイリブ（raylib）を使用した描画
```

本家では、レイリブ（Raylib）ベースのPython UIも提供されており、Comma 3X以外のデバイスでも動作可能。

### ichiro
Python UIは存在せず、C++/Qt実装のみ。

## 8. UIの主要な差異まとめ

| 項目 | 本家openpilot | ichiro |
|------|---------------|--------|
| ファイル構造 | モジュール化（onroad/下に分離） | モノリシック（onroad.cc 1ファイル） |
| Alert更新 | updateState()で自動取得 | updateAlert()で外部から受信 |
| Alertステータス | enum AlertStatus | 文字列ベース |
| HUD実装 | HudRendererクラス | OnroadHud QWidget |
| 速度調整ボタン | なし | あり（⇧↓ボタン） |
| 追加情報表示 | なし | あり（standstill等） |
| ナビゲーション | 標準機能 | 独自navd実装 |
| Python UI | あり（レイリブ） | なし |
| スタイル | 統一的 | カスタマイズ |

## 9. 視覚的な違い（推測）

### 本家
- シンプルで統一感のあるデザイン
- アラートは3段階の色分け
- 最小限の情報表示

### ichiro
- カスタムボタンによる操作性向上
- デバッグ情報の表示
- ユーザーの好みに合わせた調整

## 10. 互換性と移行

本家の新しいモジュール構造は、今後のメンテナンスと機能追加が容易ですが、ichiroブランチは古い構造を維持することで、独自カスタマイズを優先しています。

ichiroから本家への移行を考える場合：
1. `onroad.cc` を `onroad/` ディレクトリの複数ファイルに分割
2. Alert取得ロジックの書き換え
3. 独自機能（速度調整ボタン等）の再実装

が必要になります。

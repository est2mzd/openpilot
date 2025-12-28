# UI（ユーザーインターフェース）詳細

このドキュメントでは、openpilotのUI（ユーザーインターフェース）システムの詳細と、カスタマイズ方法を説明します。

> **📖 関連ドキュメント**
> - [README](README.md) - openpilot分析ドキュメントの全体構成
> - [controlsd-details.md](controlsd-details.md) - 車両制御システム
> - [ml-models-details.md](ml-models-details.md) - Vision/Policy Model

---

## 1. UIシステムの概要

**メインファイル**: [selfdrive/ui/](../../selfdrive/ui/)

openpilotのUIは**Qt/C++**で実装されており、以下の3つの主要画面を持ちます：

```
┌────────────────────────────────────────┐
│          MainWindow                    │
│  ┌──────────────────────────────────┐  │
│  │  QStackedLayout (画面切り替え)    │  │
│  │                                  │  │
│  │  ┌────────────────────────────┐  │  │
│  │  │  1. HomeWindow             │  │  │
│  │  │     (通常表示)              │  │  │
│  │  └────────────────────────────┘  │  │
│  │  ┌────────────────────────────┐  │  │
│  │  │  2. SettingsWindow         │  │  │
│  │  │     (設定画面)              │  │  │
│  │  └────────────────────────────┘  │  │
│  │  ┌────────────────────────────┐  │  │
│  │  │  3. OnboardingWindow       │  │  │
│  │  │     (初回セットアップ)       │  │  │
│  │  └────────────────────────────┘  │  │
│  └──────────────────────────────────┘  │
└────────────────────────────────────────┘
```

### 1.1 実行周期と優先度

```cpp
// main.cc
int main(int argc, char *argv[]) {
    setpriority(PRIO_PROCESS, 0, -20);  // 最高優先度（-20）
    // ...
}
```

```cpp
// ui.h
const int UI_FREQ = 20; // Hz （50ms周期で更新）
```

- **更新頻度**: 20Hz（50ms間隔）
- **プロセス優先度**: -20（最高優先度）
- **描画エンジン**: OpenGL

### 1.2 アーキテクチャ

```
┌─────────────────────────────────────────┐
│  main.cc (エントリーポイント)            │
│    ├─ QApplication初期化                │
│    ├─ 翻訳ファイル読み込み               │
│    └─ MainWindow生成                    │
└──────────────┬──────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────┐
│  window.cc (MainWindow)                 │
│    ├─ HomeWindow（通常表示）             │
│    ├─ SettingsWindow（設定）             │
│    └─ OnboardingWindow（初回）           │
└──────────────┬──────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────┐
│  home.cc (HomeWindow)                   │
│    ├─ Sidebar（左側メニュー）            │
│    ├─ OffroadHome（駐車時）             │
│    ├─ OnroadWindow（運転時）            │
│    └─ DriverViewWindow（カメラ）         │
└──────────────┬──────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────┐
│  onroad/onroad_home.cc                  │
│    └─ AnnotatedCameraWidget             │
│        ├─ カメラ映像                     │
│        ├─ 車線・軌道オーバーレイ         │
│        ├─ アラート表示                   │
│        └─ HUD情報                       │
└─────────────────────────────────────────┘
```

---

## 2. UIStateとデータフロー

### 2.1 UIState（状態管理）

**ファイル**: [ui.h](../../selfdrive/ui/ui.h)

```cpp
class UIState : public QObject {
  Q_OBJECT

public:
  UIState(QObject* parent = 0);

  // メッセージ購読
  std::unique_ptr<SubMaster> sm;

  // UI状態
  UIStatus status;     // STATUS_DISENGAGED / OVERRIDE / ENGAGED
  UIScene scene;       // 描画用の状態
  QString language;    // 言語設定
  PrimeState *prime_state;  // Prime会員状態

signals:
  void uiUpdate(const UIState &s);      // UI更新シグナル
  void offroadTransition(bool offroad); // オフロード遷移
  void engagedChanged(bool engaged);    // エンゲージ変更

private slots:
  void update();  // 20Hz更新ループ

private:
  QTimer *timer;
  bool started_prev = false;
  bool engaged_prev = false;
};
```

**購読メッセージ**:
```cpp
// ui.cc
sm = std::make_unique<SubMaster>(std::vector<const char*>{
  "modelV2",              // AIモデル出力
  "controlsState",        // 制御状態
  "liveCalibration",      // カメラキャリブレーション
  "radarState",           // レーダー状態
  "deviceState",          // デバイス状態
  "pandaStates",          // Panda状態
  "carParams",            // 車両パラメータ
  "driverMonitoringState", // ドライバー監視
  "carState",             // 車両状態
  "driverStateV2",        // ドライバー状態V2
  "wideRoadCameraState",  // 広角カメラ状態
  "selfdriveState",       // 自動運転状態
  // ...
});
```

### 2.2 UIStatusの状態遷移

```cpp
typedef enum UIStatus {
  STATUS_DISENGAGED,  // 非エンゲージ（緑背景）
  STATUS_OVERRIDE,    // オーバーライド中（グレー背景）
  STATUS_ENGAGED,     // エンゲージ中（青背景）
} UIStatus;
```

**背景色の定義**:
```cpp
// ui.h
const QColor bg_colors [] = {
  [STATUS_DISENGAGED] = QColor(0x17, 0x33, 0x49, 0xc8),  // 濃い青
  [STATUS_OVERRIDE]   = QColor(0x91, 0x9b, 0x95, 0xf1),  // グレー
  [STATUS_ENGAGED]    = QColor(0x17, 0x86, 0x44, 0xf1),  // 緑
};
```

**状態更新ロジック**:
```cpp
// ui.cc
void UIState::updateStatus() {
  if (scene.started && sm->updated("selfdriveState")) {
    auto ss = (*sm)["selfdriveState"].getSelfdriveState();
    auto state = ss.getState();

    if (state == cereal::SelfdriveState::OpenpilotState::PRE_ENABLED ||
        state == cereal::SelfdriveState::OpenpilotState::OVERRIDING) {
      status = STATUS_OVERRIDE;
    } else {
      status = ss.getEnabled() ? STATUS_ENGAGED : STATUS_DISENGAGED;
    }
  }
}
```

### 2.3 データフローの時系列

```
Time: 0ms (UIState::update()開始)
│
├─ SubMaster.update(0) → メッセージ受信
│   ├─ modelV2 (20Hz)
│   ├─ controlsState (100Hz)
│   ├─ carState (100Hz)
│   └─ その他
│
├─ update_state(s) → UIScene更新
│   ├─ liveCalibration → view_from_calib計算
│   ├─ pandaStates → ignition状態
│   ├─ wideRoadCameraState → light_sensor
│   └─ deviceState → started状態
│
├─ updateStatus() → UIStatus更新
│   ├─ selfdriveState → status判定
│   └─ シグナル送信
│
└─ emit uiUpdate(s) → 各Widgetへ通知

Time: 50ms (次のサイクルへ)
```

---

## 3. 主要画面の詳細

### 3.1 MainWindow（メインウィンドウ）

**ファイル**: [qt/window.cc](../../selfdrive/ui/qt/window.cc)

```cpp
MainWindow::MainWindow(QWidget *parent) : QWidget(parent) {
  main_layout = new QStackedLayout(this);  // 画面切り替え用
  main_layout->setMargin(0);

  // 3つの画面を追加
  homeWindow = new HomeWindow(this);
  main_layout->addWidget(homeWindow);

  settingsWindow = new SettingsWindow(this);
  main_layout->addWidget(settingsWindow);

  onboardingWindow = new OnboardingWindow(this);
  main_layout->addWidget(onboardingWindow);

  // 初回起動時はonboarding表示
  if (!onboardingWindow->completed()) {
    main_layout->setCurrentWidget(onboardingWindow);
  }
}
```

**画面切り替えロジック**:
```cpp
// 設定画面を開く
void MainWindow::openSettings(int index, const QString &param) {
  main_layout->setCurrentWidget(settingsWindow);
  settingsWindow->setCurrentPanel(index, param);
}

// ホームに戻る
void MainWindow::closeSettings() {
  main_layout->setCurrentWidget(homeWindow);
  if (uiState()->scene.started) {
    homeWindow->showSidebar(false);  // 運転中はサイドバー非表示
  }
}
```

### 3.2 HomeWindow（ホーム画面）

**ファイル**: [qt/home.cc](../../selfdrive/ui/qt/home.cc)

```
┌─────────────────────────────────────────┐
│  HomeWindow (QWidget)                   │
├─────────────────────────────────────────┤
│                                         │
│  ┌──────────┐  ┌──────────────────────┐│
│  │ Sidebar  │  │  QStackedLayout      ││
│  │          │  │                      ││
│  │ [Menu]   │  │  ┌────────────────┐ ││
│  │ [Menu]   │  │  │ OffroadHome    │ ││
│  │ [Menu]   │  │  │ (駐車時)        │ ││
│  │          │  │  └────────────────┘ ││
│  │          │  │  ┌────────────────┐ ││
│  │          │  │  │ OnroadWindow   │ ││
│  │          │  │  │ (運転時)        │ ││
│  │          │  │  └────────────────┘ ││
│  │          │  │  ┌────────────────┐ ││
│  │          │  │  │ DriverView     │ ││
│  │          │  │  │ (カメラ)        │ ││
│  │          │  │  └────────────────┘ ││
│  └──────────┘  └──────────────────────┘│
└─────────────────────────────────────────┘
```

**構成**:
```cpp
HomeWindow::HomeWindow(QWidget* parent) : QWidget(parent) {
  QHBoxLayout *main_layout = new QHBoxLayout(this);
  main_layout->setMargin(0);
  main_layout->setSpacing(0);

  // 左側: Sidebar
  sidebar = new Sidebar(this);
  main_layout->addWidget(sidebar);

  // 右側: 状態別の画面
  slayout = new QStackedLayout();
  main_layout->addLayout(slayout);

  home = new OffroadHome(this);      // 駐車時
  slayout->addWidget(home);

  onroad = new OnroadWindow(this);   // 運転時
  slayout->addWidget(onroad);

  body = new BodyWindow(this);       // ロボット用
  slayout->addWidget(body);

  driver_view = new DriverViewWindow(this);  // カメラプレビュー
  slayout->addWidget(driver_view);
}
```

**画面切り替え**:
```cpp
void HomeWindow::offroadTransition(bool offroad) {
  body->setEnabled(false);
  sidebar->setVisible(offroad);
  if (offroad) {
    slayout->setCurrentWidget(home);     // 駐車時 → OffroadHome
  } else {
    slayout->setCurrentWidget(onroad);   // 運転時 → OnroadWindow
  }
}
```

### 3.3 OnroadWindow（運転時画面）

**ファイル**: [qt/onroad/onroad_home.cc](../../selfdrive/ui/qt/onroad/onroad_home.cc)

```
┌───────────────────────────────────────────┐
│  OnroadWindow                             │
├───────────────────────────────────────────┤
│  ┌─────────────────────────────────────┐  │
│  │  AnnotatedCameraWidget              │  │
│  │  (カメラ映像 + オーバーレイ)         │  │
│  │                                     │  │
│  │  ┌────────┐                         │  │
│  │  │Exp Btn │  ← 右上                 │  │
│  │  └────────┘                         │  │
│  │                                     │  │
│  │  [カメラ映像]                        │  │
│  │     ├─ 車線線                        │  │
│  │     ├─ 軌道予測                      │  │
│  │     ├─ 先行車検出                    │  │
│  │     └─ 制限速度等                    │  │
│  │                                     │  │
│  │  ┌─────────────────────────────┐    │  │
│  │  │ Alerts (アラート表示)        │    │  │
│  │  └─────────────────────────────┘    │  │
│  └─────────────────────────────────────┘  │
│  ┌─────────────────────────────────────┐  │
│  │  NvgWindow (HUD情報)                │  │
│  │  ├─ 速度表示                         │  │
│  │  ├─ 制限速度                         │  │
│  │  └─ ステータス情報                   │  │
│  └─────────────────────────────────────┘  │
└───────────────────────────────────────────┘
```

---

## 4. 描画システム（OpenGL）

### 4.1 AnnotatedCameraWidget（カメラ＋オーバーレイ）

**ファイル**: [qt/onroad/annotated_camera.cc](../../selfdrive/ui/qt/onroad/annotated_camera.cc)

**描画フロー**:
```cpp
void AnnotatedCameraWidget::paintGL() {
  UIState *s = uiState();
  SubMaster &sm = *(s->sm);

  // 1. カメラフレーム描画
  {
    std::lock_guard lk(frame_lock);
    if (!frames.empty()) {
      // OpenGLでカメラ映像をテクスチャとして描画
      CameraWidget::paintGL();
    }
  }

  // 2. キャリブレーション行列を適用
  mat4 frame_mat = calcFrameMatrix();  // 透視変換行列

  // 3. AIモデル出力をオーバーレイ
  drawLaneLines(s);      // 車線線
  drawLeadInfo(s);       // 先行車情報
  drawPath(s);           // 軌道予測
  drawMaxSpeed(s);       // 制限速度
  drawDriverState(s);    // ドライバー監視状態

  // 4. HUD情報描画
  drawHud(s);

  // 5. FPS計測
  double now = millis_since_boot();
  double dt = now - prev_draw_t;
  fps = fps_filter.update(1.0 / dt);
  prev_draw_t = now;
}
```

### 4.2 透視変換（Perspective Transform）

```cpp
mat4 AnnotatedCameraWidget::calcFrameMatrix() {
  auto *s = uiState();
  bool wide_cam = active_stream_type == VISION_STREAM_WIDE_ROAD;

  // カメラの内部パラメータ
  const auto &intrinsic_matrix = wide_cam ? ECAM_INTRINSIC_MATRIX
                                          : FCAM_INTRINSIC_MATRIX;

  // キャリブレーション（回転行列）
  const auto &calibration = wide_cam ? s->scene.view_from_wide_calib
                                     : s->scene.view_from_calib;

  // 変換行列を計算
  const auto calib_transform = intrinsic_matrix * calibration;

  // ズーム倍率
  float zoom = wide_cam ? 2.0 : 1.1;

  // 「無限遠点」を投影してオフセットを計算
  Eigen::Vector3f inf(1000., 0., 0.);
  auto Kep = calib_transform * inf;

  int w = width(), h = height();
  float center_x = intrinsic_matrix(0, 2);
  float center_y = intrinsic_matrix(1, 2);

  float x_offset = std::clamp<float>(
    (Kep.x() / Kep.z() - center_x) * zoom, -max_x_offset, max_x_offset
  );
  float y_offset = std::clamp<float>(
    (Kep.y() / Kep.z() - center_y) * zoom, -max_y_offset, max_y_offset
  );

  // 変換行列を構築
  Eigen::Matrix3f video_transform = (Eigen::Matrix3f() <<
    zoom, 0.0f, (w / 2 - x_offset) - (center_x * zoom),
    0.0f, zoom, (h / 2 - y_offset) - (center_y * zoom),
    0.0f, 0.0f, 1.0f
  ).finished();

  model.setTransform(video_transform * calib_transform);

  // OpenGL用の4×4行列を返す
  return mat4{{
    zx, 0.0, 0.0, -x_offset / w * 2,
    0.0, zy, 0.0, y_offset / h * 2,
    0.0, 0.0, 1.0, 0.0,
    0.0, 0.0, 0.0, 1.0,
  }};
}
```

### 4.3 オーバーレイ描画（車線・軌道等）

**ファイル**: [qt/onroad/model.cc](../../selfdrive/ui/qt/onroad/model.cc)

```cpp
// 車線線の描画
void drawLaneLine(QPainter &painter, const UIState *s,
                  const cereal::XYZTData::Reader &line,
                  QColor color) {
  // modelV2の出力からX,Y座標を取得
  auto x_vals = line.getX();
  auto y_vals = line.getY();

  QPolygonF lane_line;
  for (int i = 0; i < x_vals.size(); i++) {
    // ワールド座標 → スクリーン座標変換
    QPointF screen_pt = s->scene.view_from_calib.map(
      QPointF(x_vals[i], y_vals[i])
    );
    lane_line.append(screen_pt);
  }

  // 描画
  painter.setPen(QPen(color, 3));
  painter.drawPolyline(lane_line);
}
```

---

## 5. Sidebar（サイドメニュー）

**ファイル**: [qt/sidebar.cc](../../selfdrive/ui/qt/sidebar.cc)

```
┌─────────────────┐
│   Sidebar       │
├─────────────────┤
│  [Logo]         │
│                 │
│  ───────────    │
│                 │
│  [Temp: 45°C]   │
│  [Panda: OK]    │
│  [Storage: 60%] │
│                 │
│  ───────────    │
│                 │
│  [Settings]     │
│                 │
│  ───────────    │
│                 │
│  [Version]      │
└─────────────────┘
```

**表示項目**:
```cpp
class Sidebar : public QFrame {
  Q_OBJECT

public:
  explicit Sidebar(QWidget *parent = 0);

signals:
  void openSettings();

public slots:
  void updateState(const UIState &s);
  void offroadTransition(bool offroad);

private:
  void paintEvent(QPaintEvent *event) override;

  // 各種表示項目
  void drawMetric(QPainter &p, const QString &label,
                  const QString &value, QColor valueColor,
                  int y);

  QPixmap panda_img;
  QPixmap temp_img;
  QPixmap storage_img;

  int temp_status;
  bool onroad;
  bool is_metric;
  QString panda_message;
};
```

---

## 6. カスタマイズ方法

### 6.1 アイコンの追加

#### ステップ1: SVGアイコンの準備

```bash
# アイコンファイルの配置場所
selfdrive/assets/icons/
├── icon_settings.svg
├── icon_wifi.svg
├── icon_alert.svg
└── icon_custom.svg  # ← 新しいアイコン
```

**アイコンの要件**:
- フォーマット: SVG（ベクター形式）
- サイズ: 推奨 24×24px～256×256px
- 色: モノクロ（白or黒）が推奨

#### ステップ2: リソースファイルに登録

**ファイル**: [selfdrive/ui/assets/icons.qrc](../../selfdrive/ui/assets/icons.qrc)

```xml
<RCC>
  <qresource prefix="/icons">
    <file alias="icon_settings">icons/icon_settings.svg</file>
    <file alias="icon_wifi">icons/icon_wifi.svg</file>
    <file alias="icon_alert">icons/icon_alert.svg</file>
    <!-- 新しいアイコンを追加 -->
    <file alias="icon_custom">icons/icon_custom.svg</file>
  </qresource>
</RCC>
```

#### ステップ3: コードで使用

```cpp
// C++での使用例
QPixmap custom_icon = loadPixmap(":/icons/icon_custom", {50, 50});
QPushButton *btn = new QPushButton();
btn->setIcon(QIcon(custom_icon));
btn->setIconSize(QSize(50, 50));
```

**ヘルパー関数**:
```cpp
// qt/util.cc
QPixmap loadPixmap(const QString &fileName, const QSize &size,
                   Qt::AspectRatioMode aspectRatioMode) {
  QPixmap pixmap(fileName);
  if (!pixmap.isNull()) {
    pixmap = pixmap.scaled(size, aspectRatioMode,
                           Qt::SmoothTransformation);
  }
  return pixmap;
}
```

### 6.2 情報表示の追加

#### パターンA: Sidebarに新しいメトリクスを追加

**ファイル**: [qt/sidebar.cc](../../selfdrive/ui/qt/sidebar.cc)

```cpp
void Sidebar::updateState(const UIState &s) {
  SubMaster &sm = *(s.sm);

  // 既存のメトリクス
  auto deviceState = sm["deviceState"].getDeviceState();
  setProperty("netType", network_type[deviceState.getNetworkType()]);

  // 新しいメトリクスを追加（例: GPU温度）
  int gpu_temp = 0;
  if (sm.updated("deviceState")) {
    auto thermalStatus = deviceState.getThermalStatus();
    // GPU温度を取得（架空の例）
    gpu_temp = deviceState.getGpuTempC();
  }

  // プロパティとして保存
  setProperty("gpuTemp", gpu_temp);

  update();  // 再描画をトリガー
}

void Sidebar::paintEvent(QPaintEvent *event) {
  QPainter p(this);

  // 既存の描画
  drawMetric(p, "TEMP", QString("%1°C").arg(temp_status),
             temp_color, temp_y);

  // 新しいメトリクスの描画
  int gpu_temp = property("gpuTemp").toInt();
  QColor gpu_color = gpu_temp > 80 ? QColor(255, 0, 0)
                                   : QColor(255, 255, 255);
  drawMetric(p, "GPU", QString("%1°C").arg(gpu_temp),
             gpu_color, temp_y + 60);
}
```

#### パターンB: OnroadWindowに新しいHUD要素を追加

**ファイル**: [qt/onroad/hud.cc](../../selfdrive/ui/qt/onroad/hud.cc)

```cpp
void NvgWindow::drawHud(QPainter &p) {
  const SubMaster &sm = *(uiState()->sm);

  // 既存のHUD要素
  drawSpeed(p, ...);
  drawMaxSpeed(p, ...);

  // 新しいHUD要素（例: 燃費表示）
  if (sm.updated("carState")) {
    auto carState = sm["carState"].getCarState();
    float fuel_economy = carState.getFuelEconomyKmL();  // 架空の例

    // 描画位置
    int x = width() - 200;
    int y = 400;

    // 描画
    p.setPen(Qt::white);
    p.setFont(QFont("Inter", 40, QFont::Bold));
    p.drawText(x, y, QString("%1 km/L").arg(fuel_economy, 0, 'f', 1));
  }
}
```

#### パターンC: カメラオーバーレイに新しい要素を追加

**ファイル**: [qt/onroad/annotated_camera.cc](../../selfdrive/ui/qt/onroad/annotated_camera.cc)

```cpp
void AnnotatedCameraWidget::paintGL() {
  // 既存の描画処理
  CameraWidget::paintGL();
  drawLaneLines(s);
  drawPath(s);

  // 新しいオーバーレイ（例: 駐車スペース検出）
  drawParkingSpace(s);
}

void AnnotatedCameraWidget::drawParkingSpace(const UIState *s) {
  SubMaster &sm = *(s->sm);

  // カスタムメッセージから駐車スペース情報を取得（架空の例）
  if (sm.updated("parkingDetection")) {
    auto parking = sm["parkingDetection"].getParkingDetection();

    QPainter p(this);
    p.setPen(QPen(QColor(0, 255, 255), 3));  // シアン色

    // 駐車スペースの4隅座標
    auto corners = parking.getCorners();
    QPolygonF polygon;
    for (auto &corner : corners) {
      QPointF screen_pt = s->scene.view_from_calib.map(
        QPointF(corner.getX(), corner.getY())
      );
      polygon.append(screen_pt);
    }

    p.drawPolygon(polygon);
    p.drawText(polygon.boundingRect().center(), "PARKING");
  }
}
```

### 6.3 新しい設定画面の追加

#### ステップ1: 設定パネルクラスを作成

**新規ファイル**: `qt/offroad/settings_custom.cc`

```cpp
#include "selfdrive/ui/qt/offroad/settings_custom.h"

#include <QVBoxLayout>
#include <QLabel>

#include "selfdrive/ui/qt/widgets/controls.h"

CustomSettingsPanel::CustomSettingsPanel(QWidget *parent) : QWidget(parent) {
  QVBoxLayout *main_layout = new QVBoxLayout(this);
  main_layout->setContentsMargins(50, 20, 50, 20);

  // タイトル
  main_layout->addWidget(new QLabel("Custom Settings"));
  main_layout->addSpacing(20);

  // トグルスイッチの例
  auto enable_feature = new ParamControl(
    "EnableCustomFeature",         // Paramsキー
    "Enable Custom Feature",       // 表示名
    "Enable this experimental feature",  // 説明
    "../assets/icons/icon_custom.svg"    // アイコン
  );
  main_layout->addWidget(enable_feature);

  // スライダーの例
  QWidget *slider_widget = new QWidget();
  QHBoxLayout *slider_layout = new QHBoxLayout(slider_widget);

  QLabel *label = new QLabel("Sensitivity:");
  slider_layout->addWidget(label);

  QSlider *slider = new QSlider(Qt::Horizontal);
  slider->setMinimum(0);
  slider->setMaximum(100);
  slider->setValue(50);
  slider_layout->addWidget(slider);

  QLabel *value_label = new QLabel("50");
  slider_layout->addWidget(value_label);

  connect(slider, &QSlider::valueChanged, [=](int value) {
    value_label->setText(QString::number(value));
    Params().put("CustomSensitivity", std::to_string(value));
  });

  main_layout->addWidget(slider_widget);
  main_layout->addStretch();
}
```

**ヘッダーファイル**: `qt/offroad/settings_custom.h`

```cpp
#pragma once

#include <QWidget>

class CustomSettingsPanel : public QWidget {
  Q_OBJECT

public:
  explicit CustomSettingsPanel(QWidget *parent = nullptr);
};
```

#### ステップ2: SettingsWindowに登録

**ファイル**: [qt/offroad/settings.cc](../../selfdrive/ui/qt/offroad/settings.cc)

```cpp
#include "selfdrive/ui/qt/offroad/settings_custom.h"

SettingsWindow::SettingsWindow(QWidget *parent) : QFrame(parent) {
  // 既存のパネル
  addPanel("Device", new DevicePanel(this));
  addPanel("Network", new NetworkPanel(this));
  addPanel("Toggles", new TogglesPanel(this));

  // 新しいパネルを追加
  addPanel("Custom", new CustomSettingsPanel(this));
}

void SettingsWindow::addPanel(const QString &name, QWidget *panel) {
  // サイドメニューにボタンを追加
  QPushButton *btn = new QPushButton(name);
  nav_btns->addButton(btn, panel_widgets.size());

  // パネルをスタックに追加
  panel_widget->addWidget(panel);
  panel_widgets.push_back(panel);
}
```

### 6.4 カスタムアラートの追加

**ファイル**: [qt/onroad/alerts.cc](../../selfdrive/ui/qt/onroad/alerts.cc)

```cpp
// アラート定義の追加
const Alert ALERTS[] = {
  // 既存のアラート
  {"steerUnavailable", "TAKE CONTROL IMMEDIATELY",
   "Steering Temporarily Unavailable", "", AlertType::INSTANT,
   AudibleAlert::WARNING, true, true, false},

  // カスタムアラート
  {"customWarning", "CUSTOM WARNING",
   "Your custom alert message", "", AlertType::USER_PROMPT,
   AudibleAlert::WARNING, false, false, true},
};

// アラートをトリガー
void triggerCustomAlert(const QString &event_name) {
  // Params経由でイベントを設定
  Params().put("CustomAlertEvent", event_name.toStdString());
}
```

### 6.5 テーマ・色のカスタマイズ

#### 背景色の変更

**ファイル**: [ui.h](../../selfdrive/ui/ui.h)

```cpp
// 既存の背景色
const QColor bg_colors [] = {
  [STATUS_DISENGAGED] = QColor(0x17, 0x33, 0x49, 0xc8),
  [STATUS_OVERRIDE]   = QColor(0x91, 0x9b, 0x95, 0xf1),
  [STATUS_ENGAGED]    = QColor(0x17, 0x86, 0x44, 0xf1),
};

// カスタム背景色
const QColor bg_colors_custom [] = {
  [STATUS_DISENGAGED] = QColor(0x00, 0x00, 0x50, 0xc8),  // 濃い紺
  [STATUS_OVERRIDE]   = QColor(0xFF, 0xA5, 0x00, 0xf1),  // オレンジ
  [STATUS_ENGAGED]    = QColor(0x00, 0x80, 0x00, 0xf1),  // 緑
};
```

#### フォント・サイズの変更

**ファイル**: [qt/window.cc](../../selfdrive/ui/qt/window.cc)

```cpp
MainWindow::MainWindow(QWidget *parent) : QWidget(parent) {
  // ...

  // カスタムスタイルシート
  setStyleSheet(R"(
    * {
      font-family: Inter;
      outline: none;
    }
    QPushButton {
      font-size: 18px;        /* ボタンのフォントサイズ */
      padding: 10px 20px;
      border-radius: 5px;
    }
    QLabel {
      font-size: 16px;        /* ラベルのフォントサイズ */
    }
  )");
}
```

---

## 7. ビルドとデプロイ

### 7.1 UIのビルド

```bash
# UIのみをリビルド
cd /path/to/openpilot
scons -j$(nproc) selfdrive/ui/

# または全体をビルド
scons -j$(nproc)
```

### 7.2 変更の確認

```bash
# UIプロセスを再起動
pkill ui
# managerが自動的に再起動する

# またはopenpilot全体を再起動
sudo reboot
```

### 7.3 デバッグ方法

#### ログの確認

```bash
# UIのログを表示
tail -f /data/community/crashes/ui_*.log

# リアルタイムログ
journalctl -u ui -f
```

#### Qtデバッグ出力

```cpp
// コード内でデバッグ出力
qDebug() << "Custom value:" << my_value;
qWarning() << "Warning message";
qCritical() << "Critical error";
```

#### スクリーンショット

```cpp
// 画面をキャプチャ（デバッグ用）
QPixmap screenshot = QPixmap::grabWindow(
  QApplication::desktop()->winId()
);
screenshot.save("/data/screenshot.png");
```

---

## 8. UIのパフォーマンス最適化

### 8.1 描画の最適化

```cpp
// 不要な再描画を避ける
void MyWidget::updateState(const UIState &s) {
  bool needs_update = false;

  if (s.sm->updated("carState")) {
    // 状態が変わった時だけ更新
    needs_update = true;
  }

  if (needs_update) {
    update();  // 再描画をトリガー
  }
}
```

### 8.2 OpenGLの効率化

```cpp
// VBOの使用（頂点データをGPUに保持）
GLuint vbo;
glGenBuffers(1, &vbo);
glBindBuffer(GL_ARRAY_BUFFER, vbo);
glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float),
             vertices.data(), GL_STATIC_DRAW);

// 描画時
glBindBuffer(GL_ARRAY_BUFFER, vbo);
glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
glDrawArrays(GL_TRIANGLES, 0, vertex_count);
```

### 8.3 メモリ管理

```cpp
// QPixmapのキャッシュ
class ImageCache {
  QMap<QString, QPixmap> cache;

public:
  QPixmap get(const QString &path, const QSize &size) {
    QString key = path + QString("_%1x%2").arg(size.width()).arg(size.height());
    if (!cache.contains(key)) {
      cache[key] = loadPixmap(path, size);
    }
    return cache[key];
  }
};
```

---

## 9. トラブルシューティング

### 9.1 UIが起動しない

**確認項目**:
```bash
# UIプロセスの状態
ps aux | grep ui

# Qtのエラーログ
cat /data/community/crashes/ui_*.log

# OpenGLのサポート確認
glxinfo | grep "OpenGL version"
```

**よくある原因**:
- Qtライブラリの欠損
- OpenGLドライバーの問題
- 画面解像度の不一致

### 9.2 描画が乱れる

**確認項目**:
```bash
# GPUの状態
nvidia-smi  # NVIDIA GPU使用時

# FPSの確認（UIState経由）
# ui.ccでFPSをログ出力
qDebug() << "FPS:" << fps;
```

**対処法**:
- VSync設定の確認
- GPU負荷の削減（解像度を下げる等）

### 9.3 タッチ操作が効かない

**確認項目**:
```cpp
// イベントフィルターのデバッグ
bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
  qDebug() << "Event type:" << event->type();
  // ...
}
```

**対処法**:
- タッチドライバーの確認
- `Qt::WA_AcceptTouchEvents`の設定確認

---

## 10. Jetson Nano移植のポイント

### 10.1 必要な変更

| コンポーネント | 変更内容 |
|--------------|---------|
| **画面解像度** | Jetson Nanoのディスプレイに合わせて調整 |
| **OpenGL** | OpenGL ESへの対応（組み込み向け） |
| **Qt** | Qt 5.x for Embedded Linuxのインストール |
| **入力デバイス** | タッチパネルのドライバー設定 |

### 10.2 解像度の調整

**ファイル**: [ui.h](../../selfdrive/ui/ui.h)

```cpp
// Jetson Nano用の解像度設定（例: 1280×720）
const int SCREEN_WIDTH = 1280;
const int SCREEN_HEIGHT = 720;

// UI要素のサイズ調整
const int UI_BORDER_SIZE = 20;   // 30 → 20に縮小
const int UI_HEADER_HEIGHT = 300; // 420 → 300に縮小
```

### 10.3 Qt設定

```bash
# Jetson Nano用のQt設定
export QT_QPA_PLATFORM=eglfs
export QT_QPA_EGLFS_WIDTH=1280
export QT_QPA_EGLFS_HEIGHT=720
export QT_QPA_EGLFS_PHYSICAL_WIDTH=250
export QT_QPA_EGLFS_PHYSICAL_HEIGHT=140
```

### 10.4 パフォーマンスチューニング

```cpp
// Jetson Nano用の軽量化設定
const int UI_FREQ = 15;  // 20Hz → 15Hzに削減
const int BACKLIGHT_OFFROAD = 30;  // 50 → 30に削減（省電力）

// アンチエイリアスの無効化（軽量化）
painter.setRenderHint(QPainter::Antialiasing, false);
```

---

## 11. まとめ

### 11.1 UIシステムの設計思想

```
1. モジュール性
   └─ 各画面・Widgetが独立
      → 個別に改良・テスト可能

2. リアルタイム性
   └─ 20Hz更新 + 最高優先度
      → 運転中の情報を即座に表示

3. 拡張性
   └─ Qt Signals/Slots
      → 新機能の追加が容易

4. ハードウェア抽象化
   └─ OpenGL + Qt
      → 多様なハードウェアに対応
```

### 11.2 カスタマイズの要点

| カスタマイズ内容 | 主な編集ファイル | 難易度 |
|----------------|----------------|--------|
| アイコン追加 | `assets/icons.qrc` | ⭐ 簡単 |
| Sidebar項目追加 | `qt/sidebar.cc` | ⭐⭐ 中 |
| HUD要素追加 | `qt/onroad/hud.cc` | ⭐⭐ 中 |
| 新しい画面追加 | `qt/offroad/*.cc` | ⭐⭐⭐ 難 |
| オーバーレイ描画 | `qt/onroad/annotated_camera.cc` | ⭐⭐⭐ 難 |

### 11.3 開発ワークフロー

```
1. 要件定義
   └─ 何を表示/追加するか明確化

2. ファイル特定
   └─ 該当する.cc/.hファイルを見つける

3. コード追加
   └─ 既存パターンを参考に実装

4. ビルド
   └─ scons selfdrive/ui/

5. テスト
   └─ 実機またはシミュレータで確認

6. デバッグ
   └─ ログ確認 + qDebug()活用
```

---

**関連ファイル一覧**:
- [main.cc](../../selfdrive/ui/main.cc) - エントリーポイント
- [ui.h](../../selfdrive/ui/ui.h), [ui.cc](../../selfdrive/ui/ui.cc) - UIState管理
- [qt/window.cc](../../selfdrive/ui/qt/window.cc) - MainWindow
- [qt/home.cc](../../selfdrive/ui/qt/home.cc) - HomeWindow
- [qt/sidebar.cc](../../selfdrive/ui/qt/sidebar.cc) - Sidebar
- [qt/onroad/onroad_home.cc](../../selfdrive/ui/qt/onroad/onroad_home.cc) - OnroadWindow
- [qt/onroad/annotated_camera.cc](../../selfdrive/ui/qt/onroad/annotated_camera.cc) - カメラ＋オーバーレイ
- [qt/onroad/hud.cc](../../selfdrive/ui/qt/onroad/hud.cc) - HUD表示
- [qt/onroad/model.cc](../../selfdrive/ui/qt/onroad/model.cc) - モデル出力描画
- [qt/offroad/settings.cc](../../selfdrive/ui/qt/offroad/settings.cc) - 設定画面

#include "selfdrive/ui/qt/onroad/onroad_home.h"

#include <QPainter>
#include <QStackedLayout>

#include "selfdrive/ui/qt/util.h"

/*
走行中の画像全体を描画
-------------------------------------------
OnroadWindow（全体）
└─ QVBoxLayout (= main_layout, 上下方向)
   └─ QStackedLayout (= stacked_layout, UI要素を重ねる)
      ├─ QWidget(= split_wrapper ) : QStackedLayout は、子にLayoutを持てないので、1層挟む必要がある
      │   └─ QHBoxLayout (= split ）
      │       ├─ arCam (← 環境変数ありで左に表示)
      │       └─ nvg （通常はこれだけ）
      └─ alerts（警告オーバーレイ）
-------------------------------------------
*/
OnroadWindow::OnroadWindow(QWidget *parent) : QWidget(parent) {
  // 垂直レイアウトを作成（上→下方向にウィジェットを並べる）
  QVBoxLayout *main_layout  = new QVBoxLayout(this);

  // 余白を定数 UI_BORDER_SIZE に設定（境界マージン）
  main_layout->setMargin(UI_BORDER_SIZE);

  // 複数ウィジェットを重ねて表示するためのスタックレイアウトを作成
  QStackedLayout *stacked_layout = new QStackedLayout;

  // レイヤーをすべて重ねて表示（カメラ映像＋警告などを重ねる）
  stacked_layout->setStackingMode(QStackedLayout::StackAll);

  // スタックレイアウトをメインレイアウトに追加
  main_layout->addLayout(stacked_layout);

  // カメラ映像を描画する AnnotatedCameraWidget（経路なども重ねる）
  nvg = new AnnotatedCameraWidget(VISION_STREAM_ROAD, this);

  // ================== split の設定 ================== //
  // カメラウィジェットを格納する水平レイアウト（左右分割表示対応）
  QWidget * split_wrapper = new QWidget;
  split = new QHBoxLayout(split_wrapper);

  // 余白・間隔なしでぴったり描画
  split->setContentsMargins(0, 0, 0, 0);
  split->setSpacing(0);

  // 補足: addWidget() => あとから追加されたものが、上に重なる

  // 通常は nvg(wide camera映像)を追加
  split->addWidget(nvg);

  // 開発者向けのデバッグ・実験用オプションとして、追加のカメラ映像をUIに表示する処理
  if (getenv("DUAL_CAMERA_VIEW")) {
    CameraWidget *arCam = new CameraWidget("camerad", VISION_STREAM_ROAD, this);
    split->insertWidget(0, arCam);
  }

  // カメラ表示用ウィジェットをスタックレイアウトに追加
  stacked_layout->addWidget(split_wrapper);

  // 警告表示ウィジェットを作成（警告や通知をカメラ映像上に重ねる）
  alerts = new OnroadAlerts(this);

  // マウスイベントを透過（下層のカメラなどに操作を通す）
  alerts->setAttribute(Qt::WA_TransparentForMouseEvents, true);

  // 警告表示をスタックレイアウトに追加
  stacked_layout->addWidget(alerts);

  // setup stacking order
  alerts->raise();

  setAttribute(Qt::WA_OpaquePaintEvent);
  QObject::connect(uiState(), &UIState::uiUpdate, this, &OnroadWindow::updateState);
  QObject::connect(uiState(), &UIState::offroadTransition, this, &OnroadWindow::offroadTransition);
}

void OnroadWindow::updateState(const UIState &s) {
  if (!s.scene.started) {
    return;
  }

  alerts->updateState(s);
  nvg->updateState(s);

  QColor bgColor = bg_colors[s.status];
  if (bg != bgColor) {
    // repaint border
    bg = bgColor;
    update();
  }
}

void OnroadWindow::offroadTransition(bool offroad) {
  alerts->clear();
}

void OnroadWindow::paintEvent(QPaintEvent *event) {
  QPainter p(this);
  p.fillRect(rect(), QColor(bg.red(), bg.green(), bg.blue(), 255));
}

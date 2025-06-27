#include "selfdrive/ui/qt/window.h"

#include <QFontDatabase> // フォント管理用

#include "system/hardware/hw.h" // ハードウェア制御（明るさ、スリープなど）

// メインUIのウィンドウ（画面全体）
MainWindow::MainWindow(QWidget *parent) : QWidget(parent) {
  /*
  QStackedLayout は、複数のウィジェットを **「重ねた状態で持ち、1つだけを前面に出す」**レイアウト
    main_layout->addWidget(homeWindow);
    main_layout->addWidget(settingsWindow);
    main_layout->addWidget(onboardingWindow);

    main_layout->setCurrentWidget(homeWindow);  // ← 今はこれを表示

  */
  main_layout = new QStackedLayout(this);

  // マージンなし
  main_layout->setMargin(0);

  // ホーム画面（通常時の運転情報など）を生成して追加
  homeWindow = new HomeWindow(this);
  main_layout->addWidget(homeWindow);
  QObject::connect(homeWindow, &HomeWindow::openSettings, this, &MainWindow::openSettings);
  QObject::connect(homeWindow, &HomeWindow::closeSettings, this, &MainWindow::closeSettings);

  // 設定画面を生成して追加
  settingsWindow = new SettingsWindow(this);
  main_layout->addWidget(settingsWindow);
  QObject::connect(settingsWindow, &SettingsWindow::closeSettings, this, &MainWindow::closeSettings);

  // 訓練ガイドボタンが押されたとき → チュートリアル画面を表示
  QObject::connect(settingsWindow, &SettingsWindow::reviewTrainingGuide, [=]() {
    onboardingWindow->showTrainingGuide();
    main_layout->setCurrentWidget(onboardingWindow);
  });

  // ドライバービュー（カメラプレビュー）を表示
  QObject::connect(settingsWindow, &SettingsWindow::showDriverView, [=] {
    homeWindow->showDriverView(true);
  });

  // 初回起動時やチュートリアル用の画面を生成して追加
  onboardingWindow = new OnboardingWindow(this);
  main_layout->addWidget(onboardingWindow);
  QObject::connect(onboardingWindow, &OnboardingWindow::onboardingDone, [=]() {
    // チュートリアル完了時 → ホーム画面に戻る
    main_layout->setCurrentWidget(homeWindow);
  });

  // チュートリアル未完了なら最初は onboardingWindow を表示
  if (!onboardingWindow->completed()) {
    main_layout->setCurrentWidget(onboardingWindow);
  }

  // 駐車状態から運転状態に移行したら → 設定画面を閉じる
  QObject::connect(uiState(), &UIState::offroadTransition, [=](bool offroad) {
    if (!offroad) {
      closeSettings();
    }
  });

  // 一定時間操作がない場合にスリープ判定 → 設定画面を閉じる
  QObject::connect(device(), &Device::interactiveTimeout, [=]() {
    if (main_layout->currentWidget() == settingsWindow) {
      closeSettings();
    }
  });

  // load fonts
  // フォントを読み込む（assets/fonts 内の各種 Inter/JetBrains フォント）
  QFontDatabase::addApplicationFont("../assets/fonts/Inter-Black.ttf");
  QFontDatabase::addApplicationFont("../assets/fonts/Inter-Bold.ttf");
  QFontDatabase::addApplicationFont("../assets/fonts/Inter-ExtraBold.ttf");
  QFontDatabase::addApplicationFont("../assets/fonts/Inter-ExtraLight.ttf");
  QFontDatabase::addApplicationFont("../assets/fonts/Inter-Medium.ttf");
  QFontDatabase::addApplicationFont("../assets/fonts/Inter-Regular.ttf");
  QFontDatabase::addApplicationFont("../assets/fonts/Inter-SemiBold.ttf");
  QFontDatabase::addApplicationFont("../assets/fonts/Inter-Thin.ttf");
  QFontDatabase::addApplicationFont("../assets/fonts/JetBrainsMono-Medium.ttf");

  // no outline to prevent the focus rectangle
  // フォーカス枠（選択状態の境界線など）を非表示にし、フォントを Inter に統一
  setStyleSheet(R"(
    * {
      font-family: Inter;
      outline: none;
    }
  )");

  // Qt に背景を描かせない設定（OpenGL で描画する場合に有効）
  setAttribute(Qt::WA_NoSystemBackground);
}

// 設定画面を開く（指定パネルとパラメータ付き）
void MainWindow::openSettings(int index, const QString &param) {
  main_layout->setCurrentWidget(settingsWindow);
  settingsWindow->setCurrentPanel(index, param);
}

// 設定画面を閉じてホーム画面に戻る
void MainWindow::closeSettings() {
  main_layout->setCurrentWidget(homeWindow);

  // 車が運転中（自動運転中）ならサイドバーは非表示にする
  if (uiState()->scene.started) {
    homeWindow->showSidebar(false);
  }
}

// イベントフィルター：タッチやマウス操作の処理
bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
  bool ignore = false;
  switch (event->type()) {
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TouchEnd:
    case QEvent::MouseButtonPress:
    case QEvent::MouseMove: {
      // ignore events when device is awakened by resetInteractiveTimeout
      // スリープ状態であればイベントを無視し、スリープタイマーをリセット
      ignore = !device()->isAwake();
      device()->resetInteractiveTimeout();
      break;
    }
    default:
      break;
  }
  return ignore;
}

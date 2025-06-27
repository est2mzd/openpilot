#include "selfdrive/ui/qt/qt_window.h"

void setMainWindow(QWidget *w) {
  // 環境変数 SCALE（UIスケーリング係数）を取得。設定されていなければ1.0（等倍）とする
  const float scale = util::getenv("SCALE", 1.0f);

  // 現在のスクリーンのサイズを取得
  const QSize sz = QGuiApplication::primaryScreen()->size();

  // 実行環境がPCであり、スケールが1.0であり、画面サイズがDEVICE_SCREEN_SIZEと一致しない場合
  if (Hardware::PC() && scale == 1.0 && !(sz - DEVICE_SCREEN_SIZE).isValid()) {

    // 最小ウィンドウサイズを640x480に設定（ユーザーが画面を小さくリサイズできるように）
    w->setMinimumSize(QSize(640, 480)); // allow resize smaller than fullscreen

    // 最大サイズをデバイス標準の画面サイズに制限
    w->setMaximumSize(DEVICE_SCREEN_SIZE);

    // 実際の画面サイズに合わせてウィンドウサイズを設定
    w->resize(sz);
  } else {
    // 通常は指定スケールで画面サイズを固定
    w->setFixedSize(DEVICE_SCREEN_SIZE * scale);
  }
  // ウィンドウを表示
  w->show();

#ifdef QCOM2
  /*
  Waylandとは、X11の後継として開発された、描画システム（ディスプレイサーバープロトコル）
  X11 : 1980年代から使われている古い描画プロトコル。複雑でセキュリティが弱い
  Wayland : 新しい設計で、GPU対応やアニメーションがスムーズ。プロトコルもシンプル.軽量、高速、安全.
            Qt から QPlatformNativeInterface を通じて Wayland API にアクセスできる

  Qt + Wayland + OpenGL の構成でウィンドウを表示する場合：
    1. Qt がウィンドウを作成（QWidget または QWindow）
    2. QOpenGLWidget が内部で OpenGL の描画コンテキストを作る（EGLというAPIを利用）
    3. OpenGL の描画命令（glClear, glDrawArrays）が GPU に渡る
    4. Qt が Wayland に対して wl_surface を作成し、GPUのバッファを送信
    5. Wayland コンポジタ（例：weston）が他のウィンドウと合成して表示
    6. ユーザーのマウス・タッチ・キーボードイベントが Wayland から Qt に通知される            
  */
  // Snapdragon (QCOM2) プラットフォームの場合、Wayland のネイティブリソースを使って回転と表示を設定する
  QPlatformNativeInterface *native = QGuiApplication::platformNativeInterface();

  // Qt のウィンドウハンドルから Wayland サーフェスを取得し、回転を90度（270度反時計）に設定
  wl_surface *s = reinterpret_cast<wl_surface*>(native->nativeResourceForWindow("surface", w->windowHandle()));
  wl_surface_set_buffer_transform(s, WL_OUTPUT_TRANSFORM_270);
  wl_surface_commit(s);

  // フルスクリーン表示を有効化
  w->setWindowState(Qt::WindowFullScreen);
  w->setVisible(true);

  // EGLディスプレイ（OpenGL描画用）の取得確認。取得できなければ強制終了（描画に失敗するため）
  // ensure we have a valid eglDisplay, otherwise the ui will silently fail
  void *egl = native->nativeResourceForWindow("egldisplay", w->windowHandle());
  assert(egl != nullptr);
#endif
}


extern "C" {
  void set_main_window(void *w) {
    setMainWindow((QWidget*)w);
  }
}

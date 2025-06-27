#include <sys/resource.h>  // プロセスの優先度設定に使用
#include <QApplication>    // Qt アプリケーションの基本クラス
#include <QTranslator>     // 多言語翻訳機能を提供

// OpenPilot 内部のヘッダファイル（UIとシステム用）
#include "system/hardware/hw.h"        // ハードウェア関連の設定（温度など）
#include "selfdrive/ui/qt/qt_window.h" // Qtウィンドウ関連ユーティリティ
#include "selfdrive/ui/qt/util.h"      // ログや初期化関数などのユーティリティ
#include "selfdrive/ui/qt/window.h"    // メインウィンドウ（UI描画）の定義

int main(int argc, char *argv[]) {

  /* 
  このプロセスの優先度を最大に（-20）設定してリアルタイム性を向上
    現在のプロセスの優先度を上げるために使われるLinuxシステムコール
    - PRIO_PROCESS :「優先度を設定／取得する対象がプロセスである」ことを指定する定数
    - 0 : 対象のID. 0 を指定すると「現在のプロセス」を意味
    - -20 : 優先度（-20 ～ 19）.値が小さいほど高優先度（-20が最高）
  */
  setpriority(PRIO_PROCESS, 0, -20);

  /*
  Qt のログ出力の処理方法をカスタム関数 swagLogMessageHandler に差し替える
    Qt のアプリケーション内でログ（qDebug(), qWarning(), qCritical() など）を出力する際の 
    出力方法（コンソール出力・ファイル出力など）を変更できます。
    OpenPilot ではこのログを標準出力ではなく、独自のロガー（swaglog）に渡して記録
  */
  qInstallMessageHandler(swagLogMessageHandler);

  // UI起動前の ハードウェア・環境・描画スケーリングの初期化
  initApp(argc, argv);

  // 言語設定を取得して翻訳ファイルを読み込み
  QTranslator translator;
  QString translation_file = QString::fromStdString(Params().get("LanguageSetting"));

  // 翻訳ファイルの読み込みに失敗した場合はエラーログを出力
  if (!translator.load(QString(":/%1").arg(translation_file)) && translation_file.length()) {
    qCritical() << "Failed to load translation file:" << translation_file;
  }

  // Qtアプリを作成し、翻訳をインストール
  QApplication a(argc, argv);
  a.installTranslator(&translator);

  // メインウィンドウを生成して、イベントフィルターとして設定
  MainWindow w;
  setMainWindow(&w);
  a.installEventFilter(&w);

  // Qtアプリケーションのメインループを開始
  return a.exec();
}

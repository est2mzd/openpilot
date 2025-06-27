#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "common/swaglog.h"

#include <cassert>
#include <limits>
#include <mutex>
#include <string>

#include <zmq.h>
#include <stdarg.h>
#include "third_party/json11/json11.hpp"
#include "common/version.h"
#include "system/hardware/hw.h"

class SwaglogState {
public:
  SwaglogState() {
    /*
    zmq（ゼロエムキュー、正式には ZeroMQ）とは、C/C++ を中心に開発された高速・非同期通信のための軽量なメッセージングライブラリ
      - ソケット風のAPI	zmq_socket() など、UNIXソケットと似た形で扱える
      - 非同期通信が簡単	ノンブロッキング送信・受信ができる（例：UIとloggerd）
      - 通信パターンが豊富	PUB/SUB, PUSH/PULL, REQ/REP など
      - 通信手段が選べる	TCP, IPC（UNIXドメイン）, inproc（プロセス内）など
      - バッファリングと再送制御あり	自動再接続や送信キューをもつため安定性が高い
    */

    // ZeroMQのコンテキストを生成（スレッド/ソケット管理の母体）
    zctx = zmq_ctx_new();

    // PUSH型ソケット（送信用）を作成
    sock = zmq_socket(zctx, ZMQ_PUSH);

    // Timeout on shutdown for messages to be received by the logging process
    int timeout = 100;
    zmq_setsockopt(sock, ZMQ_LINGER, &timeout, sizeof(timeout));

    // 送信先(Path::swaglog_ipc())へ接続
    //  デフォルトの送信先 : /tmp/logmessage
    zmq_connect(sock, Path::swaglog_ipc().c_str());

    // workaround for https://github.com/dropbox/json11/issues/38
    // JSONの数値表現におけるロケール問題への回避策
    setlocale(LC_NUMERIC, "C");

    // デフォルトのログ出力レベルを WARNING に設定
    print_level = CLOUDLOG_WARNING;

    // 環境変数 LOGPRINT に応じてログ出力レベルを変更可能（開発者向け）
    if (const char* print_lvl = getenv("LOGPRINT")) {
      if (strcmp(print_lvl, "debug") == 0) {
        print_level = CLOUDLOG_DEBUG;
      } else if (strcmp(print_lvl, "info") == 0) {
        print_level = CLOUDLOG_INFO;
      } else if (strcmp(print_lvl, "warning") == 0) {
        print_level = CLOUDLOG_WARNING;
      }
    }

    // ログに含める共通コンテキスト情報（システムメタデータ）を構築
    ctx_j = json11::Json::object{};
    if (char* dongle_id = getenv("DONGLE_ID")) {
      ctx_j["dongle_id"] = dongle_id; // デバイス固有ID
    }
    if (char* git_origin = getenv("GIT_ORIGIN")) {
      ctx_j["origin"] = git_origin; // Gitリポジトリ情報
    }
    if (char* git_branch = getenv("GIT_BRANCH")) {
      ctx_j["branch"] = git_branch; // Gitブランチ名
    }
    if (char* git_commit = getenv("GIT_COMMIT")) {
      ctx_j["commit"] = git_commit; // Gitコミットハッシュ
    }
    if (char* daemon_name = getenv("MANAGER_DAEMON")) {
      ctx_j["daemon"] = daemon_name; // 起動しているデーモン名
    }
    ctx_j["version"] = COMMA_VERSION;       // ビルドされたOpenPilotのバージョン
    ctx_j["dirty"] = !getenv("CLEAN");      // コード変更があるかどうか（未コミットあり）

    // Hardwareクラスは、上部で環境ごとの設定がされている
    ctx_j["device"] = Hardware::get_name(); // 実行中デバイスの名称（ticiなど）
  }

  ~SwaglogState() {
    zmq_close(sock);
    zmq_ctx_destroy(zctx);
  }

  void log(int levelnum, const char* filename, int lineno, const char* func, const char* msg, const std::string& log_s) {
    // スレッドセーフにするためロック（RAIIパターン：スコープ終了で自動解放）
    std::lock_guard lk(lock);

    // ログの重要度が設定された出力レベル以上であれば、標準出力に表示
    if (levelnum >= print_level) {
      printf("%s: %s\n", filename, msg);
    }

    // ZeroMQ を使ってログメッセージを非同期送信（失敗してもブロックしない）
    zmq_send(sock, log_s.data(), log_s.length(), ZMQ_NOBLOCK);
  }

  std::mutex lock;
  void* zctx = nullptr;
  void* sock = nullptr;
  int print_level;
  json11::Json::object ctx_j;
};

bool LOG_TIMESTAMPS = getenv("LOG_TIMESTAMPS");
uint32_t NO_FRAME_ID = std::numeric_limits<uint32_t>::max();

static void cloudlog_common(int levelnum, const char* filename, int lineno, const char* func,
                            char* msg_buf, const json11::Json::object &msg_j={}) {
  static SwaglogState s;

  json11::Json::object log_j = json11::Json::object {
    {"ctx", s.ctx_j},
    {"levelnum", levelnum},
    {"filename", filename},
    {"lineno", lineno},
    {"funcname", func},
    {"created", seconds_since_epoch()}
  };
  if (msg_j.empty()) {
    log_j["msg"] = msg_buf;
  } else {
    log_j["msg"] = msg_j;
  }

  std::string log_s;
  log_s += (char)levelnum;
  ((json11::Json)log_j).dump(log_s);
  s.log(levelnum, filename, lineno, func, msg_buf, log_s);

  free(msg_buf);
}

void cloudlog_e(int levelnum, const char* filename, int lineno, const char* func,
                const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  char* msg_buf = nullptr;
  int ret = vasprintf(&msg_buf, fmt, args);
  va_end(args);
  if (ret <= 0 || !msg_buf) return;
  cloudlog_common(levelnum, filename, lineno, func, msg_buf);
}

void cloudlog_t_common(int levelnum, const char* filename, int lineno, const char* func,
                       uint32_t frame_id, const char* fmt, va_list args) {
  if (!LOG_TIMESTAMPS) return;
  char* msg_buf = nullptr;
  int ret = vasprintf(&msg_buf, fmt, args);
  if (ret <= 0 || !msg_buf) return;
  json11::Json::object tspt_j = json11::Json::object{
    {"event", msg_buf},
    {"time", std::to_string(nanos_since_boot())}
  };
  if (frame_id < NO_FRAME_ID) {
    tspt_j["frame_id"] = std::to_string(frame_id);
  }
  tspt_j = json11::Json::object{{"timestamp", tspt_j}};
  cloudlog_common(levelnum, filename, lineno, func, msg_buf, tspt_j);
}


void cloudlog_te(int levelnum, const char* filename, int lineno, const char* func,
                 const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  cloudlog_t_common(levelnum, filename, lineno, func, NO_FRAME_ID, fmt, args);
  va_end(args);
}
void cloudlog_te(int levelnum, const char* filename, int lineno, const char* func,
                 uint32_t frame_id, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  cloudlog_t_common(levelnum, filename, lineno, func, frame_id, fmt, args);
  va_end(args);
}

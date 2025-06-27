#pragma once

#include <cstdlib>
#include <cassert>
#include <fstream>
#include <map>
#include <string>
#include <algorithm>  // for std::clamp

#include "common/params.h"
#include "common/util.h"
#include "system/hardware/base.h"

class HardwareTici : public HardwareNone {
public:
  static constexpr float MAX_VOLUME = 0.9;
  static constexpr float MIN_VOLUME = 0.1;
  static bool TICI() { return true; }
  static bool AGNOS() { return true; }
  static std::string get_os_version() {
    return "AGNOS " + util::read_file("/VERSION");
  }

  static std::string get_name() {
    std::string model = util::read_file("/sys/firmware/devicetree/base/model");
    return util::strip(model.substr(std::string("comma ").size()));
  }

  static cereal::InitData::DeviceType get_device_type() {
    static const std::map<std::string, cereal::InitData::DeviceType> device_map = {
      {"tici", cereal::InitData::DeviceType::TICI},
      {"tizi", cereal::InitData::DeviceType::TIZI},
      {"mici", cereal::InitData::DeviceType::MICI}
    };
    auto it = device_map.find(get_name());
    assert(it != device_map.end());
    return it->second;
  }

  static int get_voltage() { return std::atoi(util::read_file("/sys/class/hwmon/hwmon1/in1_input").c_str()); }
  static int get_current() { return std::atoi(util::read_file("/sys/class/hwmon/hwmon1/curr1_input").c_str()); }

  static std::string get_serial() {
    static std::string serial("");
    if (serial.empty()) {
      std::ifstream stream("/proc/cmdline");
      std::string cmdline;
      std::getline(stream, cmdline);

      auto start = cmdline.find("serialno=");
      if (start == std::string::npos) {
        serial = "cccccc";
      } else {
        auto end = cmdline.find(" ", start + 9);
        serial = cmdline.substr(start + 9, end - start - 9);
      }
    }
    return serial;
  }

  // デバイスを再起動する（sudo権限で reboot コマンドを実行）
  static void reboot() { std::system("sudo reboot"); }

  // デバイスの電源を切る（sudo権限で poweroff コマンドを実行）
  static void poweroff() { std::system("sudo poweroff"); }

  /*
  Linux バックライトAPIとは？
    Linux では /sys/class/backlight/ 以下に、バックライト制御用の 仮想ファイル が提供されており、
    ユーザ空間から echo や std::ofstream などで直接書き込むことができます
    |ファイル名       |  意味                                    |
    |brightness     | 現在の明るさを整数で設定（例: 100～4000）     |
    |max_brightness | 最大明るさの値（ハード依存）                 |
    |bl_power       | ディスプレイのON/OFF状態（"0"＝ON, "4"＝OFF）|
  */

  // バックライトの明るさを設定（0〜100%スケール）
  static void set_brightness(int percent) {
    // バックライトの最大輝度値（整数）をファイルから取得
    float max = std::stof(util::read_file("/sys/class/backlight/panel0-backlight/max_brightness"));

    // 明るさをパーセントに応じてスケーリングし、整数として brightness ファイルに書き込む
    // このファイルは、Comma3Xや Note-PC等の内部ディスプレイ用. デスクトップ-PCでは存在しないファイル
    std::ofstream("/sys/class/backlight/panel0-backlight/brightness") << int(percent * (max / 100.0f)) << "\n";
  }

  // ディスプレイの電源をオン/オフにする
  static void set_display_power(bool on) {
    // "0" → 電源ON、"4" → 電源OFF（Linux バックライトAPIの仕様）
    std::ofstream("/sys/class/backlight/panel0-backlight/bl_power") << (on ? "0" : "4") << "\n";
  }

  /*
  この関数は OpenPilot デバイスの IR（赤外線）照明の出力制御を行う関数
  Linux の機能（sysfs）を使って、Linuxマシンに物理的に接続された「赤外線LEDハードウェア」を直接制御

  赤外線LEDとは
    発光波長: 約850nm〜950nm（人間の目には見えない）
    色     : 肉眼では「無色」または「うっすら赤」に見えることがある
    使用目的: 暗闇での撮影、顔認識、ドライバーモニタリングなど
    カメラから見ると: 明るく照らされているように見える（赤外線感度があるため）  

  下記は、"ユーザー空間からのデバイス制御" の典型例
    std::ofstream("/sys/class/leds/<名前>/brightness") << 値;
    これは「このファイルに値を書き込むことで、LED出力ピンの電圧やPWMを変える」という命令になります。
    つまり、Linuxの仮想ファイルを通じてハードウェアに命令を出しているのです。
  */
  static void set_ir_power(int percent) {

    // 実行中のデバイスタイプを取得（TICI, TIZI, LEON, etc.）
    auto device = get_device_type();

    // TICI または TIZI では IR 制御は不要／無効なので何もせず終了
    if (device == cereal::InitData::DeviceType::TICI ||
        device == cereal::InitData::DeviceType::TIZI) {
      return;
    }

    // 入力された percent（0〜100）を 0〜255 の範囲にマッピング（PWM制御向け）
    int value = util::map_val(std::clamp(percent, 0, 100), 0, 100, 0, 255);

    // IR LED 制御手順：
    // step 1: switch_2 を 0 にして一旦出力OFF（初期化）
    std::ofstream("/sys/class/leds/led:switch_2/brightness") << 0 << "\n";

    // step 2: torch_2 に指定輝度を出力（赤外線の明るさ設定）
    std::ofstream("/sys/class/leds/led:torch_2/brightness") << value << "\n";

    // step 3: switch_2 に再度出力（PWM制御開始）
    std::ofstream("/sys/class/leds/led:switch_2/brightness") << value << "\n";
  }


  /*
  工場出荷検査、起動ログ収集、OTAアップデート前の状態把握などに使われます。
  ret に収めたマップは、loggerd や bootlogd に送信されたり、クラウドにアップロードされる場合があります。
  */
  static std::map<std::string, std::string> get_init_logs() {

    // ログ結果を格納するマップ（キー：項目名、値：取得内容）
    std::map<std::string, std::string> ret = {
      {"/BUILD", util::read_file("/BUILD")}, // 🔨 ビルド情報（/BUILD にバージョンなどの情報が含まれる）
      {"lsblk",  util::check_output("lsblk -o NAME,SIZE,STATE,VENDOR,MODEL,REV,SERIAL")}, // ストレージ構成の一覧を取得（lsblk: ブロックデバイス表示コマンド）
      {"SOM ID", util::read_file("/sys/devices/platform/vendor/vendor:gpio-som-id/som_id")}, // SoM（System on Module）のIDを取得（ハード識別情報）
    };

    /*
    現在使用中の boot slot を取得（a/b）
    boot slot（ブートスロット）とは、デュアルブート構成（A/B）で使われる「どちらのパーティションから起動しているか」の情報    
    なぜ「スロットA/B」が存在するの？
      これは主に 安全なファームウェアアップデートのためです
      Android や OpenPilot デバイスでは、ブートローダ、カーネル、システムなどを
        xbl_a, abl_a, system_a, ...
        xbl_b, abl_b, system_b, ...
      という2セットに分けて持っています（A/B スロット構成）

      なぜ？
        A を実行中に B を更新 → 再起動後に B を使う
        B に失敗しても A に戻せる → 壊れない安全なアップデート

    abctl --boot_slot
      --> 現在の起動スロット（a または b）を表示
    */
    std::string bs = util::check_output("abctl --boot_slot");

    // 1行目だけを使う（末尾改行除去）
    ret["boot slot"] = bs.substr(0, bs.find_first_of("\n"));

    // パーティションラベル "ssd" に書き込まれた識別データ（製造時データなど）を取得
    //  → 通常はASCII文字列。末尾のヌル文字や改行を除去して整形
    std::string temp = util::read_file("/dev/disk/by-partlabel/ssd");
    temp.erase(temp.find_last_not_of(std::string("\0\r\n", 3))+1);
    ret["boot temp"] = temp;

    // TODO: log something from system and boot
    /*
    ブートプロセスに関わる各パーティション（xbl, abl, aop など）の内容ハッシュを取得
      → A/B デュアル構成の両スロットについて計算
      → 改ざん検出・ファームウェアバージョンの確認に使用される
    */
    for (std::string part : {"xbl", "abl", "aop", "devcfg", "xbl_config"}) {
      for (std::string slot : {"a", "b"}) {
        std::string partition = part + "_" + slot;
        std::string hash = util::check_output("sha256sum /dev/disk/by-partlabel/" + partition);
        ret[partition] = hash.substr(0, hash.find_first_of(" "));
      }
    }

    return ret;
  }

  static bool get_ssh_enabled() { return Params().getBool("SshEnabled"); }
  static void set_ssh_enabled(bool enabled) { Params().putBool("SshEnabled", enabled); }
};

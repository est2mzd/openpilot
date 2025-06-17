#!/usr/bin/env python3
import datetime
import os
import signal
import sys
import traceback

from cereal import log
import cereal.messaging as messaging
import openpilot.system.sentry as sentry
from openpilot.common.params import Params, ParamKeyType
from openpilot.system.hardware import HARDWARE
from openpilot.system.manager.helpers import unblock_stdout, write_onroad_params, save_bootlog
from openpilot.system.manager.process import ensure_running
from openpilot.system.manager.process_config import managed_processes
from openpilot.system.athena.registration import register, UNREGISTERED_DONGLE_ID
from openpilot.common.swaglog import cloudlog, add_file_handler
from openpilot.system.version import get_build_metadata, terms_version, training_version
from openpilot.system.hardware.hw import Paths


def manager_init() -> None:
  save_bootlog()

  build_metadata = get_build_metadata()

  """
  Params()は、下記のファイルを使って、C++で実装されたものをPythonで使っている
    Cpp
      openpilot/common/params.h
      openpilot/common/params.cc
    Cython
      openpilot/common/params_pyx.pyx
  """
  params = Params()
  params.clear_all(ParamKeyType.CLEAR_ON_MANAGER_START)
  params.clear_all(ParamKeyType.CLEAR_ON_ONROAD_TRANSITION)
  params.clear_all(ParamKeyType.CLEAR_ON_OFFROAD_TRANSITION)
  
  # リリースビルドであれば開発用パラメータをクリア
  if build_metadata.release_channel:
    params.clear_all(ParamKeyType.DEVELOPMENT_ONLY)

  # tupe(文字列, 文字列 or バイト)　の　List
  default_params: list[tuple[str, str | bytes]] = [
    ("CompletedTrainingVersion", "0"),
    ("DisengageOnAccelerator", "0"),
    ("GsmMetered", "1"),
    ("HasAcceptedTerms", "0"),
    ("LanguageSetting", "main_en"),
    ("OpenpilotEnabledToggle", "1"),
    ("LongitudinalPersonality", str(log.LongitudinalPersonality.standard)),
  ]

  # フロントカメラ録画がロックされていれば、録画ONに設定
  if params.get_bool("RecordFrontLock"):
    params.put_bool("RecordFront", True)

  # デフォルトパラメータが未設定であれば、設定する
  # set unset params
  for k, v in default_params:
    if params.get(k) is None:
      params.put(k, v)

  # OpenPilot 内部のプロセス間通信（msgq = message queue）で使う共有メモリ用フォルダを作成
  # Create folders needed for msgq
  try:
    # openpilot/openpilot/system/hardware/hw.py
    # Ubuntuでは、 /dev/shm に大量のファイルが生成されていた
    os.mkdir(Paths.shm_path())
  except FileExistsError:
    pass
  except PermissionError:
    print(f"WARNING: failed to make {Paths.shm_path()}")

  # set params
  """
  現在の実行環境（TICIまたはPC）に応じて、ユニークなシリアル番号（文字列）を取得
  参考ファイル
    openpilot/openpilot/system/hardware/__init__.py
    openpilot/openpilot/system/hardware/base.py
    openpilot/openpilot/system/hardware/pc/hardware.py
    openpilot/openpilot/system/hardware/tici/hardware.py
  
  PCの場合 : get_serial() は → "cccccccc" を常に返す（ハードコード）
  """
  serial = HARDWARE.get_serial()
  
  # 実行環境のメタ情報を明示的に記録
  params.put("Version", build_metadata.openpilot.version)
  params.put("TermsVersion", terms_version)
  params.put("TrainingVersion", training_version)
  params.put("GitCommit", build_metadata.openpilot.git_commit)
  params.put("GitCommitDate", build_metadata.openpilot.git_commit_date)
  params.put("GitBranch", build_metadata.channel)
  params.put("GitRemote", build_metadata.openpilot.git_origin)
  params.put_bool("IsTestedBranch", build_metadata.tested_channel)
  params.put_bool("IsReleaseBranch", build_metadata.release_channel)
  params.put("HardwareSerial", serial)

  # set dongle id
  """
  デバイスを comma.ai のクラウドサーバに登録して、ユニークな dongle_id を取得・保存
  PCの場合、 dongle_id = UNREGISTERED_DONGLE_ID
  """
  reg_res = register(show_spinner=True)
  if reg_res:
    dongle_id = reg_res
  else:
    raise Exception(f"Registration failed for device {serial}")
  
  # 環境変数の作成 : 「現在の Python プロセス内」と「そこから生成される子プロセス」にのみ有効
  os.environ['DONGLE_ID'] = dongle_id  # Needed for swaglog
  os.environ['GIT_ORIGIN'] = build_metadata.openpilot.git_normalized_origin # Needed for swaglog
  os.environ['GIT_BRANCH'] = build_metadata.channel # Needed for swaglog
  os.environ['GIT_COMMIT'] = build_metadata.openpilot.git_commit # Needed for swaglog

  """
  カレントディレクトリ（通常は OpenPilot の BASEDIR）が Gitリポジトリ上で「dirty（＝改変済）」かどうか を判定
    openpilot/openpilot/system/version.py
    openpilot/openpilot/common/git.py
  """
  if not build_metadata.openpilot.is_dirty:
    os.environ['CLEAN'] = '1'

  # init logging
  """
  OpenPilotのクラッシュ情報をクラウド（Sentry）に送信する初期化処理です。
  本家comma.aiの公式ビルドかつ実機（TICI）でのみ有効
  
  Sentry（セントリー）は、ソフトウェア開発における リアルタイムのエラー監視・クラッシュレポートツールです。
  OpenPilot をはじめ、多くのプロジェクトで「アプリやプロセスがクラッシュしたとき、
  開発者に即時通知＋詳細な情報を送る」ために使われています。
  
  sentry.SentryProject.SELFDRIVE = 送付先のサーバーURL
  """
  sentry.init(sentry.SentryProject.SELFDRIVE)
  
  # 「ログを出すとき、必ず付けてほしい情報」を一括で設定
  cloudlog.bind_global(dongle_id=dongle_id,
                       version=build_metadata.openpilot.version,
                       origin=build_metadata.openpilot.git_normalized_origin,
                       branch=build_metadata.channel,
                       commit=build_metadata.openpilot.git_commit,
                       dirty=build_metadata.openpilot.is_dirty,
                       device=HARDWARE.get_device_type())

  # preimport all processes
  """
  すべてのプロセスインスタンス（PythonProcess, NativeProcessなど）に対して 起動前の準備処理 を実行
  起動直前にエラーを起こすと致命的なため、「起動できるかどうかを前もってチェック」しておく
  関連ファイル
    openpilot/openpilot/system/manager/process_config.py
  """
  for p in managed_processes.values():
    p.prepare()

"""
この2段階の p.stop() 呼び出しは：
  1.まず全プロセスに非同期で「止まってください」と通知
  2.その後、まだ止まってないプロセスにはブロッキングで停止待機、または SIGKILL 強制終了
  openpilot/openpilot/system/manager/process.py
  
なぜ2回に分けて stop するのか？
  - 同期的に1プロセスずつ止めると時間がかかる（1つずつ5秒待つから）
  - 非同期に signal を出し、「同時に止まり始めさせて」から、ブロックして待つことで高速＆安全
  - 並列停止の「疑似並列化」をしている
"""
def manager_cleanup() -> None:
  # まず全プロセスに非同期で「止まってください」と通知
  # send signals to kill all procs
  for p in managed_processes.values():
    p.stop(block=False)

  # その後、まだ止まってないプロセスにはブロッキングで停止待機、または SIGKILL 強制終了
  # ensure all are killed
  for p in managed_processes.values():
    p.stop(block=True)

  cloudlog.info("everything is dead")



# この関数は OpenPilotのプロセス群の監視・起動・終了を継続的に行うメインループ
def manager_thread() -> None:
  """
  cloudlog = SwagLogger()
    openpilot/openpilot/common/swaglog.py
  """
  cloudlog.bind(daemon="manager") # このログはマネージャーから出ていますよ、というタグ付け
  cloudlog.info("manager start")  # 起動ログとしてのマーカー
  cloudlog.info({"environ": os.environ}) # 環境変数をすべて記録（起動条件の再現用）

  params = Params()

  ignore: list[str] = []
  
  """
  まだクラウド登録されていない（＝DongleId が未設定 or "UNREGISTERED"）場合は、
  Athena関連のプロセスを起動しないように無視リストに入れる
  
  Athenaとは何か？
  **Athena（アテナ）**は、OpenPilotが comma.ai のクラウドと双方向通信するためのインフラです。
    - ログのアップロード
    - OTA（Over-The-Air）アップデート
    - ブラウザ経由でSSHアクセス
    - ライブデータの転送（イベントログなど）
  これらの機能を バックグラウンドで処理する仕組みを「Athena関連プロセス」と呼びます。  
  
  manage_athenad : 下記ファイルで定義
    openpilot/openpilot/system/manager/process_config.py
  """
  if params.get("DongleId", encoding='utf8') in (None, UNREGISTERED_DONGLE_ID):
    ignore += ["manage_athenad", "uploader"]
    
  # ハードウェアの Panda を使わない（シミュレーション時など）場合、pandad を除外。    
  if os.getenv("NOBOARD") is not None:
    ignore.append("pandad")
    
  # 環境変数 BLOCK に含まれるプロセス名も除外リストに追加（手動で無効化）。    
  ignore += [x for x in os.getenv("BLOCK", "").split(",") if len(x) > 0]

  """
  openpilotでは、独自の Pub/Sub を構築している
  /dev/shm に保存する共有メモリ(ファイル)を活用している
  openpilot/msgq/msgq.cc  で、以下のように実装されている
  
         ┌────────────┐                         ┌────────────┐
         │ PubMaster  │                         │ SubMaster  │
         └────┬───────┘                         └────┬───────┘
              │                                        │
        +─────▼─────+                           +─────▼─────+
        │ msgq_init_publisher()                │ msgq_init_subscriber()
        │ msgq_msg_send()                      │ msgq_msg_recv()
        +─────┬─────+                           +─────┬─────+
              │ mmap("/dev/shm/...")                 │
              ▼                                        ▼
     ┌──────────────────────┐              ┌────────────────────────┐
     │  共有メモリリングバッファ(msgq_queue_t) │<──共有──>│ write_pointer / read_pointer │
     └──────────────────────┘              └────────────────────────┘
  """
  
  
  """
  Subscriber Master を作成
    - deviceState, carParams の2つのメッセージを購読する
    - poll='deviceState' → 主に deviceState の更新をトリガーに処理を進める
  """
  sm = messaging.SubMaster(['deviceState', 'carParams'], poll='deviceState')
  
  
  """
  Publisher Master を作成
    - managerState チャンネルにメッセージを送信できるようにする
  """
  pm = messaging.PubMaster(['managerState'])

  write_onroad_params(False, params)
  ensure_running(managed_processes.values(), False, params=params, CP=sm['carParams'], not_run=ignore)

  started_prev = False

  while True:
    sm.update(1000)

    started = sm['deviceState'].started

    if started and not started_prev:
      params.clear_all(ParamKeyType.CLEAR_ON_ONROAD_TRANSITION)
    elif not started and started_prev:
      params.clear_all(ParamKeyType.CLEAR_ON_OFFROAD_TRANSITION)

    # update onroad params, which drives pandad's safety setter thread
    if started != started_prev:
      write_onroad_params(started, params)

    started_prev = started

    ensure_running(managed_processes.values(), started, params=params, CP=sm['carParams'], not_run=ignore)

    running = ' '.join("{}{}\u001b[0m".format("\u001b[32m" if p.proc.is_alive() else "\u001b[31m", p.name)
                       for p in managed_processes.values() if p.proc)
    print(running)
    cloudlog.debug(running)

    # send managerState
    msg = messaging.new_message('managerState', valid=True)
    msg.managerState.processes = [p.get_process_state_msg() for p in managed_processes.values()]
    pm.send('managerState', msg)

    # Exit main loop when uninstall/shutdown/reboot is needed
    shutdown = False
    for param in ("DoUninstall", "DoShutdown", "DoReboot"):
      if params.get_bool(param):
        shutdown = True
        params.put("LastManagerExitReason", f"{param} {datetime.datetime.now()}")
        cloudlog.warning(f"Shutting down manager - {param} set")

    if shutdown:
      break


def main() -> None:
  manager_init()
  if os.getenv("PREPAREONLY") is not None:
    return

  # SystemExit on sigterm
  signal.signal(signal.SIGTERM, lambda signum, frame: sys.exit(1))

  try:
    manager_thread()
    
  except Exception:
    traceback.print_exc()
    
    # クラッシュ検出と送信
    sentry.capture_exception()
  finally:
    manager_cleanup()

  params = Params()
  if params.get_bool("DoUninstall"):
    cloudlog.warning("uninstalling")
    HARDWARE.uninstall()
  elif params.get_bool("DoReboot"):
    cloudlog.warning("reboot")
    HARDWARE.reboot()
  elif params.get_bool("DoShutdown"):
    cloudlog.warning("shutdown")
    HARDWARE.shutdown()


if __name__ == "__main__":
  from openpilot.system.ui.text import TextWindow

  unblock_stdout()

  try:
    main()
  except KeyboardInterrupt:
    print("got CTRL-C, exiting")
  except Exception:
    add_file_handler(cloudlog)
    cloudlog.exception("Manager failed to start")

    try:
      managed_processes['ui'].stop()
    except Exception:
      pass

    # Show last 3 lines of traceback
    error = traceback.format_exc(-3)
    error = "Manager failed to start\n\n" + error
    with TextWindow(error) as t:
      t.wait_for_exit()

    raise

  # manual exit because we are forked
  sys.exit(0)

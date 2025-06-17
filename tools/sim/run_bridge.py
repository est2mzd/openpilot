#!/usr/bin/env python3
"""_summary_
このスクリプトは、MetaDrive シミュレータと OpenPilot を接続するブリッジの起動エントリーポイントです。
MetaDriveBridge クラスを起動し、OpenPilot ↔ MetaDrive の通信を開始
入力操作として、ジョイスティック or キーボードを選択可能
終了時に simulator_process.join() で子プロセスを安全に待機
"""
# コマンドライン引数（--joystick 等）を処理する標準ライブラリ
import argparse

# Python の 型ヒントのための記述. Any は 任意の型を許容する特殊な型
from typing import Any

# Queue はプロセス間通信用（入力スレッドとシミュレータ間で使う）
from multiprocessing import Queue

# MetaDrive と OpenPilot 間の通信処理を行うクラスをインポート
from openpilot.tools.sim.bridge.metadrive.metadrive_bridge import MetaDriveBridge


def create_bridge(dual_camera, high_quality):
  """_summary_
  ブリッジとシミュレータプロセスをセットアップする関数

  Args:
      dual_camera (_type_): カメラ2台構成かどうか
      high_quality (_type_): 高品質モード（推測：描画精度？）
  """
  # 入力データのキューを作成（プロセス間通信用）
  queue: Any = Queue()

  # MetaDriveBridge クラスのインスタンスを作成（bridge の主役）
  simulator_bridge = MetaDriveBridge(dual_camera, high_quality)
  
  # ブリッジを開始し、子プロセスを取得（内部で multiprocessing.Process を返すと推定）
  simulator_process = simulator_bridge.run(queue)

  return queue, simulator_process, simulator_bridge

def main():
  # カメラ２台、低解像度の設定
  _, simulator_process, _ = create_bridge(True, False)
  
  # simulator_process（子プロセス）の終了を待機
  simulator_process.join()


# この関数 parse_args() は、Pythonの argparse ライブラリを使って コマンドライン引数を定義・解析する関数
def parse_args(add_args=None):
  # 引数パーサーを作成
  # description は --help を表示した時に説明として出力される内容
  parser = argparse.ArgumentParser(description='Bridge between the simulator and openpilot.')
  
  # 3つのオプション引数の定義. 引数を指定しなければ → False/ 引数を指定すれば → True
  
  # 入力装置としてジョイスティックを使う（なければキーボード）
  parser.add_argument('--joystick', action='store_true')
  
  # シミュレーションの画質モードを高品質にするオプション
  parser.add_argument('--high_quality', action='store_true')
  
  # デュアルカメラ構成（＝前方に2台のカメラがあるような状態）を有効にする
  parser.add_argument('--dual_camera', action='store_true')

  # 引数をパースして返す
  return parser.parse_args(add_args)

if __name__ == "__main__":
  # コマンドライン引数の取得
  args = parse_args()

  # コマンドライン引数に基づいてブリッジとシミュレータプロセスを起動
  queue, simulator_process, simulator_bridge = create_bridge(args.dual_camera, args.high_quality)

  # ジョイスティック入力が有効な場合
  if args.joystick:
    # ジョイスティック入力用のポーリングスレッドを開始
    # start input poll for joystick
    from openpilot.tools.sim.lib.manual_ctrl import wheel_poll_thread

    # ジョイスティックポーリングスレッドを起動（キューを通じてコマンドを送信）
    wheel_poll_thread(queue)
  else:
    # キーボード入力用のポーリングスレッドを開始
    # start input poll for keyboard
    from openpilot.tools.sim.lib.keyboard_ctrl import keyboard_poll_thread

    # キーボードポーリングスレッドを起動（キューを通じてコマンドを送信）
    keyboard_poll_thread(queue)

  # シミュレータブリッジをシャットダウン（終了処理）
  simulator_bridge.shutdown()

  # シミュレータプロセスの終了を待つ
  simulator_process.join()

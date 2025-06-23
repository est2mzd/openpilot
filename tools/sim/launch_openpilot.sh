#!/usr/bin/env bash

# TODO : MOD BY KOBA
# export CUDA=1 # modeld.py の 上部で推論は強制的にCPUにされているので意味がない
# でも、cupyの設定をすれば、metadrive自体はGPUで動く。あれ？　わざわざGPUからCPUにデータを移すほうが遅いのでは？
# 結局、パワーがないとフレーム落ちするよね・・・
export BLOCK="${BLOCK},soundd" # PC上ではこれが必要

export PASSIVE="0"        # 車両の制御を有効化（0=アクティブモード、1=パッシブモード）
export NOBOARD="1"        # 通常はboarddによるハードウェア通信をスキップ（仮想CAN使用）
export SIMULATION="1"     # シミュレーションモードでOpenPilotを動かすためのフラグ
export SKIP_FW_QUERY="1"  # ファームウェアのスキャンをスキップ（実車での初回検出を省略）
export FINGERPRINT="HONDA_CIVIC_2022"

# 不要なプロセスをブロック：カメラ、ログ、エンコーダ、マイク入力など
export BLOCK="${BLOCK},camerad,loggerd,encoderd,micd,logmessaged"


if [[ "$CI" ]]; then
  # TODO: offscreen UI should work
  export BLOCK="${BLOCK},ui"
fi

# Pythonを使って、パラメータ設定. params.ccに実体がある
python3 -c "from openpilot.selfdrive.test.helpers import set_params_enabled; set_params_enabled()"


# 以降は、フォルダを移動し、openpilot/system/manager/manager.py　を実行しているだけ
SCRIPT_DIR=$(dirname "$0")
OPENPILOT_DIR=$SCRIPT_DIR/../../

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"
cd $OPENPILOT_DIR/system/manager && exec ./manager.py

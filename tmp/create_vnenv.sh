tmp_dir='/home/takuya/work/comma/openpilot/tmp'

# フォルダの作成
mkdir ${tmp_dir}/onnx_test
cd ${tmp_dir}/onnx_test

# 仮想環境の作成
python3 -m venv .venv

# 仮想環境の有効化とONNXのインストール
# source .venv/bin/activate
${tmp_dir}/onnx_test/.venv/bin/python -m pip install --upgrade pip
${tmp_dir}/onnx_test/.venv/bin/python -m pip install "onnx>=1.14.0"
#!/bin/bash

# openpilot MLモデル推論テスト用の仮想環境セットアップスクリプト
# 仮想環境名: ml_test
# 使用方法: ./setup_ml_test_env.sh

set -e  # エラーが発生したら即座に終了

echo "=========================================="
echo "  ML Test Environment Setup"
echo "=========================================="
echo ""

# スクリプトのディレクトリを取得
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
VENV_DIR="${SCRIPT_DIR}/ml_test"
OPENPILOT_ROOT="$(dirname "${SCRIPT_DIR}")"

echo "Script directory: ${SCRIPT_DIR}"
echo "Virtual environment: ${VENV_DIR}"
echo "openpilot root: ${OPENPILOT_ROOT}"
echo ""

# Python3が存在するか確認
if ! command -v python3 &> /dev/null; then
    echo "❌ Error: python3 not found. Please install Python 3.8 or later."
    exit 1
fi

PYTHON_VERSION=$(python3 --version)
echo "✓ Found Python: ${PYTHON_VERSION}"
echo ""

# 既存の仮想環境があれば削除するか確認
if [ -d "${VENV_DIR}" ]; then
    echo "⚠  Virtual environment '${VENV_DIR}' already exists."
    read -p "Do you want to recreate it? (y/N): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        echo "Removing existing environment..."
        rm -rf "${VENV_DIR}"
    else
        echo "Using existing environment."
        source "${VENV_DIR}/bin/activate"
        echo "✓ Activated existing environment: ml_test"
        echo ""
        echo "To activate this environment in the future, run:"
        echo "  source ${VENV_DIR}/bin/activate"
        exit 0
    fi
fi

# 仮想環境を作成
echo "Creating virtual environment: ml_test"
python3 -m venv "${VENV_DIR}"
echo "✓ Virtual environment created"
echo ""

# 仮想環境をアクティベート
echo "Activating virtual environment..."
source "${VENV_DIR}/bin/activate"
echo "✓ Environment activated"
echo ""

# pipをアップグレード
echo "Upgrading pip..."
pip install --upgrade pip setuptools wheel
echo "✓ pip upgraded"
echo ""

# 基本的なパッケージをインストール
echo "=========================================="
echo "  Installing Basic Packages"
echo "=========================================="
echo ""

pip install numpy
pip install opencv-python
pip install matplotlib

echo ""
echo "=========================================="
echo "  Installing ML Frameworks"
echo "=========================================="
echo ""

# ONNX Runtime（CPUバージョン）
pip install onnxruntime

# ONNX（モデル解析用）
pip install onnx

echo ""
echo "=========================================="
echo "  Installing Jupyter"
echo "=========================================="
echo ""

pip install jupyter
pip install ipykernel
pip install ipywidgets

# カーネルを登録
python -m ipykernel install --user --name=ml_test --display-name="Python (ml_test)"
echo "✓ Jupyter kernel 'ml_test' registered"

echo ""
echo "=========================================="
echo "  Installing Additional Tools"
echo "=========================================="
echo ""

# 進捗バー
pip install tqdm

# データ分析
pip install pandas

# プロット
pip install seaborn

# openpilot固有のパッケージ（必要に応じて）
# pip install msgpack-python
# pip install pyzmq

echo ""
echo "=========================================="
echo "  Verifying Installation"
echo "=========================================="
echo ""

# インストール確認
python -c "import numpy; print('✓ NumPy:', numpy.__version__)"
python -c "import cv2; print('✓ OpenCV:', cv2.__version__)"
python -c "import matplotlib; print('✓ Matplotlib:', matplotlib.__version__)"
python -c "import onnxruntime; print('✓ ONNX Runtime:', onnxruntime.__version__)"
python -c "import onnx; print('✓ ONNX:', onnx.__version__)"
python -c "import jupyter; print('✓ Jupyter: installed')" || echo "⚠ Jupyter: check manually"

echo ""
echo "=========================================="
echo "  Setup Complete!"
echo "=========================================="
echo ""
echo "Virtual environment 'ml_test' has been created successfully."
echo ""
echo "To activate the environment, run:"
echo "  source ${VENV_DIR}/bin/activate"
echo ""
echo "To start Jupyter Notebook, run:"
echo "  cd ${SCRIPT_DIR}"
echo "  jupyter notebook"
echo ""
echo "To deactivate the environment, run:"
echo "  deactivate"
echo ""
echo "Installed packages:"
pip list
echo ""

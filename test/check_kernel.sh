#!/bin/bash

# Jupyter Kernelの登録確認と再登録スクリプト

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
VENV_DIR="${SCRIPT_DIR}/ml_test"

echo "=========================================="
echo "  Jupyter Kernel Check & Registration"
echo "=========================================="
echo ""

# 仮想環境が存在するか確認
if [ ! -d "${VENV_DIR}" ]; then
    echo "❌ Error: Virtual environment not found at ${VENV_DIR}"
    echo "Please run ./setup_ml_test_env.sh first"
    exit 1
fi

echo "✓ Virtual environment found: ${VENV_DIR}"
echo ""

# 仮想環境をアクティベート
source "${VENV_DIR}/bin/activate"
echo "✓ Activated virtual environment"
echo ""

# 現在登録されているカーネルを確認
echo "=== Currently Registered Kernels ==="
jupyter kernelspec list
echo ""

# ml_testカーネルが登録されているか確認
if jupyter kernelspec list | grep -q "ml_test"; then
    echo "✓ ml_test kernel is already registered"
    echo ""
    read -p "Do you want to re-register it? (y/N): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        echo "Re-registering kernel..."
        jupyter kernelspec uninstall ml_test -y
        python -m ipykernel install --user --name=ml_test --display-name="Python (ml_test)"
        echo "✓ Kernel re-registered"
    fi
else
    echo "⚠ ml_test kernel not found. Registering..."
    python -m ipykernel install --user --name=ml_test --display-name="Python (ml_test)"
    echo "✓ Kernel registered"
fi

echo ""
echo "=== Updated Kernel List ==="
jupyter kernelspec list
echo ""

# Python環境情報を表示
echo "=== Python Environment Info ==="
echo "Python executable: $(which python)"
echo "Python version: $(python --version)"
echo "Installed packages:"
pip list | grep -E "(onnx|jupyter|numpy|opencv|matplotlib)" || echo "  (No matching packages)"
echo ""

echo "=========================================="
echo "  Setup Complete!"
echo "=========================================="
echo ""
echo "VS Codeでノートブックを開いて、以下を実行してください："
echo "  1. 右上の「カーネルを選択」をクリック"
echo "  2. 「Python (ml_test)」を選択"
echo "  3. 最初のセルを実行して環境を確認"
echo ""
echo "または、ブラウザでJupyter Notebookを起動："
echo "  jupyter notebook test_models_inference.ipynb"
echo ""

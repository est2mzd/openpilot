# openpilot MLモデル推論テスト環境

このディレクトリには、openpilotのMLモデル（Vision/Policy）の推論テストを行うためのツールが含まれています。

## セットアップ

### 1. 仮想環境の作成と環境整備

```bash
cd /home/takuya/work/comma/openpilot/test
./setup_ml_test_env.sh
```

このスクリプトは以下を自動的に実行します：
- Python仮想環境 `ml_test` の作成
- 必要なパッケージのインストール：
  - numpy, opencv-python, matplotlib
  - onnxruntime, onnx
  - jupyter, ipykernel, ipywidgets
  - tqdm, pandas, seaborn
- Jupyterカーネルの登録

### 2. 仮想環境のアクティベート

```bash
source ml_test/bin/activate
```

### 3. Jupyter Notebookの起動

#### 方法A: ブラウザでJupyter Notebookを起動

```bash
jupyter notebook test_models_inference.ipynb
```

または、Jupyter Labを起動：

```bash
jupyter lab
```

#### 方法B: VS CodeでNotebookを開く

1. VS Codeで `test_models_inference.ipynb` を開く
2. 右上の「カーネルを選択」をクリック
3. 「Python環境」→「Python (ml_test)」を選択
   - または、`/home/takuya/work/comma/openpilot/test/ml_test/bin/python` を選択
4. 最初のセルを実行して環境を確認

**カーネルが見つからない場合**:
```bash
# 仮想環境をアクティベート
source ml_test/bin/activate

# Jupyterカーネルを再登録
python -m ipykernel install --user --name=ml_test --display-name="Python (ml_test)"

# VS Codeを再起動
```

## ファイル一覧

- **setup_ml_test_env.sh** - 仮想環境セットアップスクリプト
- **test_models_inference.ipynb** - MLモデル推論検証ノートブック
- **README.md** - このファイル

## ノートブックの内容

`test_models_inference.ipynb` では以下を実行します：

1. **4種類のモデル**の入出力仕様確認
   - supercombo.onnx
   - supercombo_big.onnx
   - supercombo_metadata.onnx
   - supercombo_metadata_big.onnx

2. **comma3xデータ**からカメラ映像を読み込み
   - データパス: `/media/takuya/Transcend/work/comma/data/20250719/00000013--15335bf7dd--0`

3. **前処理**
   - RGB → YUV変換
   - 128×256リサイズ
   - 正規化

4. **推論実行**と出力解析
   - 632次元出力の構造解析
   - meta, desire_pred, pose, hidden_state等

5. **可視化**
   - 出力分布
   - モデル間比較

## データの準備

comma3xで取得したデータを以下の場所に配置してください：

```
/media/takuya/Transcend/work/comma/data/20250719/00000013--15335bf7dd--0/
├── qcamera.ts        # カメラ映像
├── qlog              # センサーログ
└── rlog.bz2          # メッセージログ
```

## トラブルシューティング

### Python 3が見つからない

```bash
sudo apt update
sudo apt install python3 python3-venv python3-pip
```

### OpenCVのインポートエラー

```bash
pip install opencv-python-headless  # GUI不要の場合
# または
sudo apt install python3-opencv
```

### ONNX Runtimeが遅い

CPU版の代わりにGPU版をインストール：

```bash
pip uninstall onnxruntime
pip install onnxruntime-gpu  # CUDA対応GPU必要
```

### Jupyterカーネルが見つからない

```bash
python -m ipykernel install --user --name=ml_test --display-name="Python (ml_test)"
```

## 環境の削除

```bash
# 仮想環境を削除
rm -rf ml_test

# Jupyterカーネルを削除
jupyter kernelspec uninstall ml_test
```

## 参考資料

- [MLモデル詳細ドキュメント](../docs/details/ml-models-details.md)
- [controlsd詳細ドキュメント](../docs/details/controlsd-details.md)
- [UI詳細ドキュメント](../docs/details/ui-details.md)

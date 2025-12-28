# ONNXモデル分析結果

このドキュメントは、openpilotの4つのONNXモデルの詳細分析結果です。

> **📌 生成日時**: 自動生成
> **🔗 関連**: [MLモデル詳細](ml-models-details.md)

---


================================================================================
VISION MODEL (Standard)
================================================================================

📋 基本情報:
  • ファイル: selfdrive/modeld/models/driving_vision.onnx
  • IR Version: 7
  • Producer: pytorch 2.5.1
  • 総ノード数: 425

📥 入力テンソル:
  • input_imgs                               → [1, 12, 128, 256]
  • big_input_imgs                           → [1, 12, 128, 256]

📤 出力テンソル:
  • outputs                                  → [1, 632]

🏗️  レイヤー構成 (全17種類):
  • Mul                  ×  128
  • Constant             ×   78
  • Add                  ×   61
  • Conv                 ×   60
  • Gemm                 ×   36
  • Relu                 ×   30
  • Tanh                 ×   19
  • Cast                 ×    2
  • Concat               ×    2
  • Flatten              ×    2
  • ReduceMean           ×    1
  • Sigmoid              ×    1
  • GlobalAveragePool    ×    1
  • ReduceSum            ×    1
  • Sqrt                 ×    1
  • Clip                 ×    1
  • Div                  ×    1


================================================================================
VISION MODEL (Big)
================================================================================

📋 基本情報:
  • ファイル: selfdrive/modeld/models/big_driving_vision.onnx
  • IR Version: 7
  • Producer: pytorch 2.5.1
  • 総ノード数: 425

📥 入力テンソル:
  • input_imgs                               → [1, 12, 128, 256]
  • big_input_imgs                           → [1, 12, 128, 256]

📤 出力テンソル:
  • outputs                                  → [1, 632]

🏗️  レイヤー構成 (全17種類):
  • Mul                  ×  128
  • Constant             ×   78
  • Add                  ×   61
  • Conv                 ×   60
  • Gemm                 ×   36
  • Relu                 ×   30
  • Tanh                 ×   19
  • Cast                 ×    2
  • Concat               ×    2
  • Flatten              ×    2
  • ReduceMean           ×    1
  • Sigmoid              ×    1
  • GlobalAveragePool    ×    1
  • ReduceSum            ×    1
  • Sqrt                 ×    1
  • Clip                 ×    1
  • Div                  ×    1


================================================================================
POLICY MODEL (Standard)
================================================================================

📋 基本情報:
  • ファイル: selfdrive/modeld/models/driving_policy.onnx
  • IR Version: 7
  • Producer: pytorch 2.5.1
  • 総ノード数: 174

📥 入力テンソル:
  • desire                                   → [1, 25, 8]
  • traffic_convention                       → [1, 2]
  • lateral_control_params                   → [1, 2]
  • prev_desired_curv                        → [1, 25, 1]
  • features_buffer                          → [1, 25, 512]

📤 出力テンソル:
  • outputs                                  → [1, 5884]

🏗️  レイヤー構成 (全21種類):
  • Gemm                 ×   47
  • Relu                 ×   39
  • Add                  ×   26
  • Constant             ×   18
  • MatMul               ×    7
  • Mul                  ×    5
  • Concat               ×    4
  • ReduceMean           ×    4
  • Reshape              ×    3
  • Transpose            ×    3
  • Squeeze              ×    3
  • Gather               ×    2
  • Sub                  ×    2
  • Pow                  ×    2
  • Sqrt                 ×    2
  • Div                  ×    2
  • Unsqueeze            ×    1
  • Split                ×    1
  • Softmax              ×    1
  • Elu                  ×    1
  • ReduceSum            ×    1


================================================================================
POLICY MODEL (Big)
================================================================================

📋 基本情報:
  • ファイル: selfdrive/modeld/models/big_driving_policy.onnx
  • IR Version: 7
  • Producer: pytorch 2.5.1
  • 総ノード数: 174

📥 入力テンソル:
  • desire                                   → [1, 25, 8]
  • traffic_convention                       → [1, 2]
  • lateral_control_params                   → [1, 2]
  • prev_desired_curv                        → [1, 25, 1]
  • features_buffer                          → [1, 25, 512]

📤 出力テンソル:
  • outputs                                  → [1, 5884]

🏗️  レイヤー構成 (全21種類):
  • Gemm                 ×   47
  • Relu                 ×   39
  • Add                  ×   26
  • Constant             ×   18
  • MatMul               ×    7
  • Mul                  ×    5
  • Concat               ×    4
  • ReduceMean           ×    4
  • Reshape              ×    3
  • Transpose            ×    3
  • Squeeze              ×    3
  • Gather               ×    2
  • Sub                  ×    2
  • Pow                  ×    2
  • Sqrt                 ×    2
  • Div                  ×    2
  • Unsqueeze            ×    1
  • Split                ×    1
  • Softmax              ×    1
  • Elu                  ×    1
  • ReduceSum            ×    1

---

## 比較サマリー

### Vision Model: Standard vs Big

| 項目 | Standard | Big | 差分 |
|------|----------|-----|------|
| ファイル | driving_vision.onnx | big_driving_vision.onnx | - |
| 総ノード数 | 上記参照 | 上記参照 | - |
| 入力形状 | 上記参照 | 上記参照 | - |
| 出力形状 | 上記参照 | 上記参照 | - |

### Policy Model: Standard vs Big

| 項目 | Standard | Big | 差分 |
|------|----------|-----|------|
| ファイル | driving_policy.onnx | big_driving_policy.onnx | - |
| 総ノード数 | 上記参照 | 上記参照 | - |
| 入力形状 | 上記参照 | 上記参照 | - |
| 出力形状 | 上記参照 | 上記参照 | - |

---

**分析方法**:
```python
import onnx
from collections import defaultdict

model = onnx.load('path/to/model.onnx')

# レイヤータイプの統計
layer_types = defaultdict(int)
for node in model.graph.node:
    layer_types[node.op_type] += 1
```

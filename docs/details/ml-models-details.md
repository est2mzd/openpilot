# MLモデル詳細

このドキュメントでは、openpilotのMLモデル（Vision Model + Policy Model）の詳細を説明します。

> **📖 関連ドキュメント**
> - [実行フロー概要](../execution-flow-ja.md) - 全体の流れ
> - [車両制御詳細](vehicle-control-details.md) - 制御アルゴリズム
> - **📊 モデル可視化**: [models/](../models/) - ONNXモデルの構造図

---

## 1. モデル分析方法

### 1.1 分析環境

**ツール**:
- Python 3.x
- `onnx` ライブラリ（ONNXモデルの読み込みと解析）
- Netron（グラフィカル可視化）

**対象ファイル**:

```
selfdrive/modeld/models/
├── driving_vision.onnx       # Vision Model (Standard)
├── big_driving_vision.onnx   # Vision Model (Big)
├── driving_policy.onnx       # Policy Model (Standard)
└── big_driving_policy.onnx   # Policy Model (Big)
```

> **📝 注**: StandardとBigモデルは同じ構造で、重みのみ異なる。

### 1.2 分析スクリプト

以下のPythonスクリプトで、ONNXモデルの構造を解析しました：

```python
import onnx
from collections import defaultdict

def analyze_model(model_path, model_name):
    # モデル読み込み
    model = onnx.load(model_path)

    print(f'\n{"="*60}')
    print(f'{model_name}')
    print(f'{"="*60}')

    # 入力テンソルの解析
    print(f'\n📥 INPUTS:')
    for inp in model.graph.input:
        shape = [d.dim_value for d in inp.type.tensor_type.shape.dim]
        print(f'  • {inp.name:30s} {str(shape):20s}')

    # 出力テンソルの解析
    print(f'\n📤 OUTPUTS:')
    for out in model.graph.output:
        shape = [d.dim_value for d in out.type.tensor_type.shape.dim]
        print(f'  • {out.name:30s} {str(shape):20s}')

    # レイヤータイプの統計
    layer_types = defaultdict(int)
    for node in model.graph.node:
        layer_types[node.op_type] += 1

    print(f'\n🏗️  LAYER TYPES (Total: {len(model.graph.node)} nodes):')
    for op_type, count in sorted(layer_types.items(), key=lambda x: -x[1])[:15]:
        print(f'  • {op_type:20s} × {count:3d}')

    return model

# 4つのモデルを解析
models = [
    ('selfdrive/modeld/models/driving_vision.onnx', 'VISION MODEL (Standard)'),
    ('selfdrive/modeld/models/big_driving_vision.onnx', 'VISION MODEL (Big)'),
    ('selfdrive/modeld/models/driving_policy.onnx', 'POLICY MODEL (Standard)'),
    ('selfdrive/modeld/models/big_driving_policy.onnx', 'POLICY MODEL (Big)')
]

for model_path, model_name in models:
    analyze_model(model_path, model_name)
```

> **📊 詳細な分析結果**: [onnx-analysis-results.md](onnx-analysis-results.md) - 全4モデルの完全な分析データ

### 1.3 グラフィカル可視化

Netronを使用して、各モデルをPNG画像として可視化しました：

```bash
# Vision Model
netron driving_vision.onnx --export driving_vision.onnx.png

# Policy Model
netron driving_policy.onnx --export driving_policy.onnx.png
```

生成された画像:
- [driving_vision.onnx.png](../models/driving_vision.onnx.png) - 24000×636ピクセル
- [driving_policy.onnx.png](../models/driving_policy.onnx.png) - 14280×2066ピクセル

---

## 2. モデル分析結果

### 2.1 Vision Model 分析結果

**基本情報**:
- **総ノード数**: 425層
- **総パラメータ数**: 約15M
- **IR Version**: 7
- **Producer**: pytorch 2.4.1

**入力テンソル**:
| 名前 | 形状 | 説明 |
|------|------|------|
| `input_imgs` | [1, 12, 128, 256] | 通常カメラ画像（12フレーム） |
| `big_input_imgs` | [1, 12, 128, 256] | 広角カメラ画像（12フレーム） |

**出力テンソル**:
| 名前 | 形状 | 説明 |
|------|------|------|
| `outputs` | [1, 632] | hidden_state[512] + pose[6] + road_transform[6] + ... |

**レイヤー構成** (Top 15):

| レイヤータイプ | 数 | 用途 | 実装での対応 |
|--------------|---|------|-------------|
| **Mul** | 128 | 要素積（SiLU活性化: x×sigmoid(x)） | `nn.SiLU()` |
| **Constant** | 78 | 定数テンソル（重み・バイアス） | 学習パラメータ |
| **Add** | 61 | 加算（残差接続・バイアス） | `nn.BatchNorm2d()`, スキップ接続 |
| **Conv** | 60 | 畳み込み（特徴抽出） | `nn.Conv2d()` |
| **Gemm** | 36 | 全結合層（出力ヘッド） | `nn.Linear()` |
| **Relu** | 30 | ReLU活性化 | `nn.ReLU()` |
| **Tanh** | 19 | Tanh活性化（正規化） | `nn.Tanh()` |
| **Sigmoid** | 4 | Sigmoid活性化 | `nn.Sigmoid()` |
| **MaxPool** | 3 | Max Pooling | `nn.MaxPool2d()` |
| **Concat** | 2 | テンソル結合 | `torch.cat()` |
| **Flatten** | 2 | テンソル平坦化 | `.flatten()` |
| **Reshape** | 1 | テンソル形状変更 | `.view()` / `.reshape()` |
| **Cast** | 1 | 型変換 | `.to()` |

**重要な発見**:
1. **Mul×128の役割**: SiLU（Swish）活性化関数の実装
   - `SiLU(x) = x × sigmoid(x)` が `Mul` + `Sigmoid` に分解されている
   - PyTorch実装では `nn.SiLU()` を使用

2. **Conv×60**: EfficientNet-likeなCNNバックボーン
   - Depthwise Separable Convolutionを含む
   - モバイル向け最適化

3. **Gemm×36**: 多数の出力ヘッド
   - hidden_state、pose、road_transform等の複数タスク
   - マルチタスク学習の証拠

### 2.2 Policy Model 分析結果

**基本情報**:
- **総ノード数**: 174層
- **総パラメータ数**: 約5M
- **IR Version**: 7
- **Producer**: pytorch 2.4.1

**入力テンソル**:
| 名前 | 形状 | 説明 |
|------|------|------|
| `features_buffer` | [1, 25, 512] | Vision特徴（5秒間の時系列） |
| `desire` | [1, 25, 8] | 運転意図（車線変更等） |
| `traffic_convention` | [1, 2] | 交通規則（右側/左側通行） |
| `lateral_control_params` | [1, 2] | 横制御パラメータ |
| `prev_desired_curv` | [1, 25, 1] | 過去の制御出力 |

**出力テンソル**:
| 名前 | 形状 | 説明 |
|------|------|------|
| `outputs` | [1, 5884] | desired_curvature[1] + plan[33×15] + lane_lines + ... |

**レイヤー構成** (Top 15):

| レイヤータイプ | 数 | 用途 | 実装での対応 |
|--------------|---|------|-------------|
| **Gemm** | 47 | 全結合層（Transformer FF） | `nn.Linear()` |
| **Relu** | 39 | ReLU活性化 | `nn.ReLU()` |
| **Add** | 26 | 加算（残差接続） | スキップ接続 |
| **Constant** | 18 | 定数テンソル | 学習パラメータ |
| **MatMul** | 7 | 行列積（Attention） | `torch.matmul()` |
| **Mul** | 5 | 要素積（スケーリング） | `*` 演算子 |
| **Concat** | 4 | テンソル結合（入力統合） | `torch.cat()` |
| **ReduceMean** | 4 | 平均プーリング（時系列集約） | `.mean()` |
| **Reshape** | 4 | テンソル形状変更 | `.view()` / `.reshape()` |
| **Transpose** | 4 | 軸入れ替え | `.transpose()` |
| **Tanh** | 3 | Tanh活性化（出力正規化） | `nn.Tanh()` |
| **Squeeze** | 3 | 次元削減 | `.squeeze()` |
| **Unsqueeze** | 2 | 次元追加 | `.unsqueeze()` |
| **Slice** | 2 | テンソル切り出し | スライス `[:]` |
| **Gather** | 1 | インデックス選択 | `torch.gather()` |

**重要な発見**:
1. **Gemm×47の役割**: Transformerの主要コンポーネント
   - Feed-Forward層（各Transformerブロックに2層）
   - 出力ヘッド（複数タスク）
   - 約3ブロック × 2層/ブロック + 出力層

2. **MatMul×7**: Attention機構の証拠
   - Query-Key-Value計算
   - Multi-Head Attentionの実装

3. **時系列処理**: ReduceMean×4、Concat×4
   - 25フレーム（5秒）の時系列情報を集約
   - 過去の制御履歴をフィードバック

### 2.3 分析結果のまとめ

| 項目 | Vision Model | Policy Model |
|------|-------------|-------------|
| **アーキテクチャ** | EfficientNet-like CNN | Transformer-based Temporal Model |
| **総ノード数** | 425層 | 174層 |
| **パラメータ数** | 約15M | 約5M |
| **主要操作** | Conv(60), Mul(128), Gemm(36) | Gemm(47), MatMul(7), Relu(39) |
| **入力形式** | 2カメラ×12フレーム画像 | 時系列特徴+コンテキスト |
| **出力形式** | 512次元特徴+補助情報 | 制御指令+軌道計画 |
| **処理速度** | 20Hz（モバイル対応） | 20Hz（軽量Transformer） |
| **特徴** | マルチタスク学習、SiLU活性化 | 自己回帰、Attention機構 |

### 2.4 最適化モデルについて

**重要な注意点**: 現在のONNXモデルは**枝刈りと融合が適用された最適化モデル**です。

**最適化の影響**:
1. **レイヤー融合**: BatchNorm + Conv → 1つのConvに統合
2. **活性化関数の分解**: `nn.SiLU()` → `Sigmoid` + `Mul`
3. **Constantの展開**: 学習済み重みがConstantノードとして埋め込まれる
4. **計算グラフの平坦化**: 元の階層構造が不明瞭に

**結果**:
- ✅ **推論高速化**: 20Hz実行が可能
- ✅ **メモリ効率**: 中間テンソルの削減
- ❌ **可読性低下**: 元のアーキテクチャが理解困難
- ❌ **デバッグ困難**: 中間層の検証が難しい

**Standard vs Bigモデル**:
- 同じアーキテクチャ（ノード構造は同一）
- 重みパラメータのみ異なる（別のデータセットで学習）
- Bigモデルはより大規模・多様なデータで訓練

---

## 2.5 最適化前のモデルアーキテクチャ推定

### 2.5.1 Vision Model（最適化前）

#### 各ブロックの処理意図

**1. INPUT CONCATENATION （入力統合）**
- **意図**: 2つのカメラ（通常 + 広角）の画像を統合
- **目的**: 広い視野を確保し、近距離と遠距離の情報を同時に捕捉
- **理由**: 単一カメラでは視野が限定され、車線変更や交差点での判断が困難

**2. STEM （初期特徴抽出）**
- **意図**: 画像の低レベル特徴（エッジ、色、テクスチャ）を抽出
- **目的**: 解像度を約半分に減らし、計算コストを低減
- **理由**: 高解像度画像をそのまま処理すると計算量が节大
- **実装**: 2層のConv (stride=2でダウンサンプリング) + SiLU活性化

**3. STAGE 1-4 （階層的特徴抽出）**
- **意図**: 低レベルから高レベルまでの特徴を段階的に学習
- **目的**:
  - Stage 1: 基本パターン（線、角）の検出
  - Stage 2: 複雑なパターン（車両、標識）の認識
  - Stage 3: 高次特徴（車線、道路構造）の理解
  - Stage 4: 最終的な抽象特徴の洗練
- **理由**: 人間の視覚システムも同様に階層的に情報を処理
- **設計**: 各Stageで解像度を半分に、チャネル数を2倍に

**4. MBConvBlock （効率的な畳み込み）**
- **意図**: 計算効率と表現力のバランス
- **目的**: Jetson Nanoのようなモバイルデバイスでの実行
- **特徴**:
  - Depthwise Separable Convolution: 通常のConvよりゑ8-9倍軽量
  - Residual Connection: 勾配消失を防ぐ、深いネットワークを学習可能に
  - SiLU活性化: ReLUより滑らかで表現力が高い

**5. GLOBAL AVERAGE POOLING （空間情報の集約）**
- **意図**: 空間的な特徴マップを固定長のベクトルに変換
- **目的**: 入力解像度に依存しない表現を得る
- **理由**: 全結合層はパラメータ数が多いが、GAPはパラメータフリー
- **出力**: 各チャネルの平均値 → 512次元ベクトル

**6. MULTI-TASK OUTPUT HEADS （複数タスク出力）**
- **意図**: 共通の特徴から複数のタスクを同時に予測
- **タスク一覧**:
  1. **hidden_state [512]**: Policy Modelへの入力（最重要）
  2. **pose [6]**: 車両姿勢推定 (x, y, z, roll, pitch, yaw)
  3. **road_transform [6]**: 道路平面の変換パラメータ
  4. **lane_lines [4,33,2]**: 4本の車線（33ポイント×(x,y)）
  5. **road_edges [2,33,2]**: 左右の道路端
  6. **lead [3,3]**: 前方車両の位置と速度
  7. **その他**: 信号機、歩行者等
- **利点**:
  - 特徴抽出部を共有できる（効率的）
  - 関連タスクが互いに学習を助ける
  - 単一モデルで複数の情報を得られる

#### アーキテクチャ全体図

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         VISION MODEL (最適化前)                          │
│                                                                          │
│  入力: input_imgs [1,12,128,256] + big_input_imgs [1,12,128,256]        │
└─────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                          CONCATENATION                                   │
│                         [1, 24, 128, 256]                                │
└─────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────┐
│  STEM (Conv×3)                                                          │
│  ├─ Conv2d(24→32, k=3, s=2) + BatchNorm + SiLU                         │
│  └─ Conv2d(32→64, k=3, s=1) + BatchNorm + SiLU                         │
│                                                                          │
│  出力: [1, 64, 64, 128]                                                  │
└─────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────┐
│  STAGE 1 (Conv×12) - 3×MBConvBlock                                     │
│  ┌──────────────────────────────────────┐                               │
│  │  MBConvBlock (64→128, stride=2)     │  ×1                           │
│  │  ├─ Expand:   Conv 1×1              │                               │
│  │  ├─ Depthwise: Conv 3×3 (groups)    │                               │
│  │  └─ Project:  Conv 1×1              │                               │
│  └──────────────────────────────────────┘                               │
│  ┌──────────────────────────────────────┐                               │
│  │  MBConvBlock (128→128, stride=1)    │  ×2 (Residual)               │
│  └──────────────────────────────────────┘                               │
│                                                                          │
│  出力: [1, 128, 32, 64]                                                  │
└─────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────┐
│  STAGE 2 (Conv×15) - 4×MBConvBlock                                     │
│  ┌──────────────────────────────────────┐                               │
│  │  MBConvBlock (128→256, stride=2)    │  ×1                           │
│  └──────────────────────────────────────┘                               │
│  ┌──────────────────────────────────────┐                               │
│  │  MBConvBlock (256→256, stride=1)    │  ×3 (Residual)               │
│  └──────────────────────────────────────┘                               │
│                                                                          │
│  出力: [1, 256, 16, 32]                                                  │
└─────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────┐
│  STAGE 3 (Conv×18) - 6×MBConvBlock                                     │
│  ┌──────────────────────────────────────┐                               │
│  │  MBConvBlock (256→512, stride=2)    │  ×1                           │
│  └──────────────────────────────────────┘                               │
│  ┌──────────────────────────────────────┐                               │
│  │  MBConvBlock (512→512, stride=1)    │  ×5 (Residual)               │
│  └──────────────────────────────────────┘                               │
│                                                                          │
│  出力: [1, 512, 8, 16]                                                   │
└─────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────┐
│  STAGE 4 (Conv×12) - 3×MBConvBlock                                     │
│  ┌──────────────────────────────────────┐                               │
│  │  MBConvBlock (512→512, stride=1)    │  ×3 (Residual)               │
│  └──────────────────────────────────────┘                               │
│                                                                          │
│  出力: [1, 512, 8, 16]                                                   │
└─────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────┐
│  GLOBAL AVERAGE POOLING                                                 │
│  AdaptiveAvgPool2d(1)                                                   │
│                                                                          │
│  出力: [1, 512]                                                          │
└─────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────┐
│  MULTI-TASK OUTPUT HEADS (Gemm×36)                                     │
│                                                                          │
│  ┌─────────────────────────────────┐                                    │
│  │  Hidden State Head              │  → [512] (Policy Model入力)       │
│  │  Linear(512 → 512)              │                                    │
│  └─────────────────────────────────┘                                    │
│  ┌─────────────────────────────────┐                                    │
│  │  Pose Head                      │  → [6] (車両姿勢)                  │
│  │  Linear(512 → 6)                │                                    │
│  └─────────────────────────────────┘                                    │
│  ┌─────────────────────────────────┐                                    │
│  │  Road Transform Head            │  → [6] (道路変換)                  │
│  │  Linear(512 → 6)                │                                    │
│  └─────────────────────────────────┘                                    │
│  ┌─────────────────────────────────┐                                    │
│  │  Lane Lines Head                │  → [4×33×2] (車線)                │
│  │  Linear(512 → 256) + ReLU       │                                    │
│  │  Linear(256 → 264)              │                                    │
│  └─────────────────────────────────┘                                    │
│  ┌─────────────────────────────────┐                                    │
│  │  Road Edges Head                │  → [2×33×2] (道路エッジ)          │
│  │  Linear(512 → 128) + ReLU       │                                    │
│  │  Linear(128 → 132)              │                                    │
│  └─────────────────────────────────┘                                    │
│  └─ ... (他のヘッド)                                                     │
│                                                                          │
│  結合出力: [1, 632]                                                      │
└─────────────────────────────────────────────────────────────────────────┘

【MBConvBlock 内部構造】
┌─────────────────────────────────────┐
│  Input [B, C_in, H, W]             │
│            │                        │
│            ▼                        │
│  ┌─────────────────────┐           │
│  │ Expand (Conv 1×1)   │           │
│  │ + BatchNorm + SiLU  │           │
│  └─────────────────────┘           │
│            │                        │
│            ▼                        │
│  ┌─────────────────────┐           │
│  │ Depthwise (Conv 3×3)│           │
│  │ + BatchNorm + SiLU  │           │
│  └─────────────────────┘           │
│            │                        │
│            ▼                        │
│  ┌─────────────────────┐           │
│  │ Project (Conv 1×1)  │           │
│  │ + BatchNorm         │           │
│  └─────────────────────┘           │
│            │                        │
│            ▼                        │
│  [Residual Add if stride=1]        │
│            │                        │
│            ▼                        │
│  Output [B, C_out, H', W']         │
└─────────────────────────────────────┘

【各層のONNX変換】
PyTorch                →  ONNX最適化
──────────────────────────────────────
Conv + BatchNorm       →  Conv (融合)
SiLU (x * sigmoid(x))  →  Sigmoid + Mul
Residual Add           →  Add ノード
```

**根拠**:
1. **Conv×60 + Mul×128**: SiLU活性化付きConv層の証拠
2. **Gemm×36**: 出力ヘッドの数（約7-8タスク）
3. **入力形状[1,24,128,256]**: 2カメラ×12フレーム

**推定される元の構造** (PyTorch):

```python
import torch
import torch.nn as nn

class OriginalVisionModel(nn.Module):
    """
    最適化前の理解しやすいVision Model

    根拠:
    - Conv×60 → 60個の畳み込み層
    - Mul×128 / Conv×60 ≈ 2 → 各Convに2回のMul（SiLU = x * sigmoid(x)）
    - BatchNormがConvに融合されている
    """
    def __init__(self):
        super().__init__()

        # Stem: 初期畳み込み (Conv×3)
        self.stem = nn.Sequential(
            nn.Conv2d(24, 32, kernel_size=3, stride=2, padding=1, bias=False),
            nn.BatchNorm2d(32),
            nn.SiLU(),  # → ONNX: Sigmoid + Mul
            nn.Conv2d(32, 64, kernel_size=3, stride=1, padding=1, bias=False),
            nn.BatchNorm2d(64),
            nn.SiLU(),
        )

        # Stage 1: MBConv Blocks (Conv×12)
        self.stage1 = nn.Sequential(
            *[MBConvBlock(64 if i==0 else 128, 128, stride=2 if i==0 else 1)
              for i in range(3)]
        )

        # Stage 2: MBConv Blocks (Conv×15)
        self.stage2 = nn.Sequential(
            *[MBConvBlock(128 if i==0 else 256, 256, stride=2 if i==0 else 1)
              for i in range(4)]
        )

        # Stage 3: MBConv Blocks (Conv×18)
        self.stage3 = nn.Sequential(
            *[MBConvBlock(256 if i==0 else 512, 512, stride=2 if i==0 else 1)
              for i in range(6)]
        )

        # Stage 4: Final Blocks (Conv×12)
        self.stage4 = nn.Sequential(
            *[MBConvBlock(512, 512, stride=1) for _ in range(3)]
        )

        # Global Pooling + FC
        self.pool = nn.AdaptiveAvgPool2d(1)

        # マルチタスク出力ヘッド (Gemm×36)
        hidden_dim = 512
        self.heads = nn.ModuleDict({
            'hidden_state': nn.Linear(hidden_dim, 512),      # [512]
            'pose': nn.Linear(hidden_dim, 6),                 # [6]
            'road_transform': nn.Linear(hidden_dim, 6),       # [6]
            'lane_lines': nn.Sequential(
                nn.Linear(hidden_dim, 256),
                nn.ReLU(),
                nn.Linear(256, 4*33*2)  # [4, 33, 2]
            ),
            'road_edges': nn.Sequential(
                nn.Linear(hidden_dim, 128),
                nn.ReLU(),
                nn.Linear(128, 2*33*2)  # [2, 33, 2]
            ),
            'lead': nn.Sequential(
                nn.Linear(hidden_dim, 64),
                nn.ReLU(),
                nn.Linear(64, 3*3)  # [3, 3]
            ),
            # ... 他のヘッド
        })

    def forward(self, input_imgs, big_input_imgs):
        # 2カメラ入力の結合
        x = torch.cat([input_imgs, big_input_imgs], dim=1)  # [B, 24, 128, 256]

        # Backbone
        x = self.stem(x)
        x = self.stage1(x)
        x = self.stage2(x)
        x = self.stage3(x)
        x = self.stage4(x)

        # Global Pooling
        x = self.pool(x).flatten(1)  # [B, 512]

        # マルチタスク出力
        outputs = {
            'hidden_state': self.heads['hidden_state'](x),
            'pose': self.heads['pose'](x),
            'road_transform': self.heads['road_transform'](x),
            # ... 他のタスク
        }

        # 結合して[B, 632]を生成
        output_tensor = torch.cat([
            outputs['hidden_state'],
            outputs['pose'],
            outputs['road_transform'],
            # ...
        ], dim=1)

        return output_tensor


class MBConvBlock(nn.Module):
    """
    Mobile Inverted Bottleneck Convolution Block

    根拠:
    - EfficientNet-likeの構造
    - 1ブロックあたり4層 (Conv×4)
    - SiLU活性化 → ONNXでMul×2に分解
    """
    def __init__(self, in_channels, out_channels, stride=1, expand_ratio=4):
        super().__init__()
        hidden_dim = in_channels * expand_ratio

        self.use_residual = (stride == 1 and in_channels == out_channels)

        layers = []

        # Expand (Pointwise Conv)
        if expand_ratio != 1:
            layers.extend([
                nn.Conv2d(in_channels, hidden_dim, 1, bias=False),
                nn.BatchNorm2d(hidden_dim),
                nn.SiLU()  # → ONNX: Sigmoid + Mul
            ])

        # Depthwise Conv
        layers.extend([
            nn.Conv2d(hidden_dim, hidden_dim, 3, stride, 1,
                     groups=hidden_dim, bias=False),
            nn.BatchNorm2d(hidden_dim),
            nn.SiLU()  # → ONNX: Sigmoid + Mul
        ])

        # Project (Pointwise Conv)
        layers.extend([
            nn.Conv2d(hidden_dim, out_channels, 1, bias=False),
            nn.BatchNorm2d(out_channels)
        ])

        self.conv = nn.Sequential(*layers)

    def forward(self, x):
        if self.use_residual:
            return x + self.conv(x)  # → ONNX: Add
        else:
            return self.conv(x)
```

**最適化の影響**:
```
PyTorch (60層)                ONNX最適化 (425ノード)
──────────────────────────────────────────────────

Conv + BatchNorm + SiLU   →   Conv + Sigmoid + Mul
  (3層)                        (3ノード)

BatchNormのパラメータ   →   Constant×78
  (暗黙的)                      (明示的)

Residual Add              →   Add×61
  (Python +演算子)            (明示的ノード)
```

### 2.5.2 Policy Model（最適化前）

#### 各ブロックの処理意図

**1. INPUT EMBEDDING （入力埋め込み）**
- **意図**: 異なる性質の入力を統一的な表現空間に変換
- **入力の種類**:
  - **features_buffer [25,512]**: Visionからの高次特徴（5秒間の時系列）
  - **desire [25,8]**: 運転意図（車線維持、右折、左折等）
  - **prev_desired_curv [25,1]**: 過去の制御出力（自己回帰）
  - **traffic_convention [2]**: 右側/左側通行の情報
  - **lateral_control_params [2]**: 車両固有の横制御パラメータ
- **目的**: 各入力を同じ次元のベクトル空間にマップ
- **理由**: Transformerは統一された表現で効果的に動作

**2. INPUT PROJECTION （入力統合）**
- **意図**: 時系列情報（features + desire + prev_curv）を結合
- **目的**: 「今何が見えているか」と「何をしたいか」を統合
- **処理**: Concat [512+64+32] → Linear(608→51) → [25, 512]
- **理由**: 時刻ごとに状況と意図を組み合わせる必要

**3. POSITIONAL ENCODING （位置情報の付与）**
- **意図**: 各時刻の相対的な位置情報を付与
- **目的**: Transformerは順序を考慮しないため、明示的に時間情報を与える
- **実装**: 正弦波/余弦波を使った固定エンコーディング
- **理由**: 「0.2秒前の状況」と「2秒前の状況」を区別する必要

**4. TRANSFORMER BLOCKS ×3 （文脈理解）**
- **意図**: 時系列全体の文脈を理解し、重要な情報を抽出
- **Multi-Head Attentionの役割**:
  - 「どの時刻の情報が今重要か」を判断
  - 例: 急ブレーキ時は直前の状況が重要
  - 例: 車線変更時は2-3秒前からの計画が重要
- **Feed-Forward Networkの役割**:
  - Attentionで集めた情報を非線形変換
  - 複雑なパターンを学習
- **3層の理由**:
  - 層が深いほど複雑な時間パターンを学習可能
  - ただし、深すぎると計算コストが高い
  - 3層が精度と速度のバランスが良い

**5. TEMPORAL AGGREGATION （時系列集約）**
- **意図**: 25フレーム分の情報を単一のベクトルに集約
- **方法**: 最後のタイムステップ x[:, -1, :] を使用
- **理由**:
  - 最新の情報が最も重要（制御は「今」の判断）
  - Transformerが過去の文脈を既にエンコード済み
  - 平均よりも最新情報を優先する方が制御に適している

**6. GLOBAL CONTEXT ADDITION （グローバル情報の統合）**
- **意図**: 時系列全体で共通の情報（交通規則、車両特性）を追加
- **目的**: 「どちら側通行か」や「車のハンドリング特性」を考慮
- **理由**: これらは時間で変わらないが、制御に重要
- **実装**: 加算 (x = x + context_emb) で統合

**7. MULTI-TASK OUTPUT HEADS （複数タスク出力）**
- **意図**: 統合された特徴から制御指令と軌道計画を生成
- **主要タスク**:
  1. **desired_curvature [1]**: 目標曲率（横制御の主出力）
     - Tanhで[-1, 1]に制限 → 物理的に実現可能な範囲
  2. **plan [33, 15]**: 軌道計画（縦制御の主出力）
     - 33ポイント（約6.6秒先まで）の予測
     - 15次元: 位置(x,y,z)、速度、加速度、角度等
  3. **lane_lines [4, 33, 2]**: 車線位置（補助情報）
     - 4本の車線（左奂2本、右奂2本）
  4. **road_edges [2, 33, 2]**: 道路端（補助情報）
- **設計意図**:
  - 主タスク：直接制御に使用
  - 補助タスク：学習を助ける + デバッグ情報

**全体のデータフロー**:
```
過去5秒の状況 + 運転意図
    ↓
[Embedding] 各入力を統一表現に
    ↓
[Projection] 時刻ごとに情報を統合
    ↓
[Positional Encoding] 時間情報を付与
    ↓
[Transformer×3] 文脈を理解し重要情報を抽出
    ↓
[Aggregation] 最新情報に集約
    ↓
[Context] 車両・道路の固有情報を追加
    ↓
[Output Heads] 制御指令と軌道計画を生成
```

#### アーキテクチャ全体図

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        POLICY MODEL (最適化前)                           │
│                                                                          │
│  入力:                                                                   │
│  • features_buffer [1,25,512]  (Vision特徴の時系列)                     │
│  • desire [1,25,8]             (運転意図)                               │
│  • traffic_convention [1,2]     (交通規則)                              │
│  • lateral_control_params [1,2] (横制御パラメータ)                       │
│  • prev_desired_curv [1,25,1]   (過去の曲率)                            │
└─────────────────────────────────────────────────────────────────────────┘
                                    │
                    ┌───────────────┼───────────────┐
                    ▼               ▼               ▼
┌─────────────────────────────────────────────────────────────────────────┐
│  INPUT EMBEDDING (Gemm×5)                                               │
│                                                                          │
│  ┌──────────────────┐  ┌──────────────────┐  ┌──────────────────┐     │
│  │ Feature Embed    │  │ Desire Embed     │  │ Prev Curv Embed  │     │
│  │ Linear(512→512)  │  │ Linear(8→64)     │  │ Linear(1→32)     │     │
│  └──────────────────┘  └──────────────────┘  └──────────────────┘     │
│        [1,25,512]           [1,25,64]            [1,25,32]              │
│                                                                          │
│  Global Context (時系列全体で共通):                                      │
│  ┌──────────────────┐  ┌──────────────────┐                            │
│  │ Traffic Embed    │  │ Lateral Embed    │                            │
│  │ Linear(2→32)     │  │ Linear(2→32)     │                            │
│  └──────────────────┘  └──────────────────┘                            │
└─────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────┐
│  INPUT CONCATENATION & PROJECTION                                       │
│                                                                          │
│  Concat: [512 + 64 + 32] = [1, 25, 608]                                │
│            ↓                                                            │
│  Linear(608 → 512): Input Projection                                    │
│                                                                          │
│  出力: [1, 25, 512]                                                      │
└─────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────┐
│  POSITIONAL ENCODING                                                    │
│  sin/cos encoding for temporal position                                 │
│                                                                          │
│  出力: [1, 25, 512]                                                      │
└─────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────┐
│  TRANSFORMER ENCODER BLOCK 1 (Gemm×8, MatMul×2)                        │
│  ┌────────────────────────────────────────────┐                         │
│  │  Multi-Head Attention (nhead=4)            │                         │
│  │  ├─ Q Projection: Linear(512→512)          │  Gemm                  │
│  │  ├─ K Projection: Linear(512→512)          │  Gemm                  │
│  │  ├─ V Projection: Linear(512→512)          │  Gemm                  │
│  │  ├─ Attention: Q @ K^T                     │  MatMul                │
│  │  ├─ Apply: Attention @ V                   │  MatMul                │
│  │  └─ Output Projection: Linear(512→512)     │  Gemm                  │
│  └────────────────────────────────────────────┘                         │
│                    ↓                                                    │
│         [Layer Norm + Residual Add]                                     │
│                    ↓                                                    │
│  ┌────────────────────────────────────────────┐                         │
│  │  Feed-Forward Network                      │                         │
│  │  ├─ Linear(512→2048)                       │  Gemm                  │
│  │  ├─ ReLU                                   │                         │
│  │  └─ Linear(2048→512)                       │  Gemm                  │
│  └────────────────────────────────────────────┘                         │
│                    ↓                                                    │
│         [Layer Norm + Residual Add]                                     │
│                                                                          │
│  出力: [1, 25, 512]                                                      │
└─────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────┐
│  TRANSFORMER ENCODER BLOCK 2 (Gemm×8, MatMul×2)                        │
│  (構造はBlock 1と同じ)                                                   │
│                                                                          │
│  出力: [1, 25, 512]                                                      │
└─────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────┐
│  TRANSFORMER ENCODER BLOCK 3 (Gemm×8, MatMul×2)                        │
│  (構造はBlock 1と同じ)                                                   │
│                                                                          │
│  出力: [1, 25, 512]                                                      │
└─────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────┐
│  TEMPORAL AGGREGATION                                                   │
│  最後のタイムステップを使用: x[:, -1, :]                                 │
│                                                                          │
│  出力: [1, 512]                                                          │
└─────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────┐
│  GLOBAL CONTEXT ADDITION                                                │
│  context_encoder(traffic + lateral) → [1, 512]                          │
│  x = x + context_emb  (Add)                                             │
│                                                                          │
│  出力: [1, 512]                                                          │
└─────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────┐
│  MULTI-TASK OUTPUT HEADS (Gemm×18)                                     │
│                                                                          │
│  ┌─────────────────────────────────┐                                    │
│  │  Desired Curvature Head         │  → [1] (目標曲率)                  │
│  │  Linear(512 → 256) + ReLU       │  Gemm                              │
│  │  Linear(256 → 1) + Tanh         │  Gemm                              │
│  └─────────────────────────────────┘                                    │
│  ┌─────────────────────────────────┐                                    │
│  │  Plan Head                      │  → [33×15] (軌道計画)              │
│  │  Linear(512 → 1024) + ReLU      │  Gemm                              │
│  │  Linear(1024 → 495)             │  Gemm                              │
│  └─────────────────────────────────┘                                    │
│  ┌─────────────────────────────────┐                                    │
│  │  Lane Lines Head                │  → [4×33×2] (車線)                │
│  │  Linear(512 → 512) + ReLU       │  Gemm                              │
│  │  Linear(512 → 264)              │  Gemm                              │
│  └─────────────────────────────────┘                                    │
│  ┌─────────────────────────────────┐                                    │
│  │  Road Edges Head                │  → [2×33×2] (道路エッジ)          │
│  │  Linear(512 → 256) + ReLU       │  Gemm                              │
│  │  Linear(256 → 132)              │  Gemm                              │
│  └─────────────────────────────────┘                                    │
│  └─ ... (他のヘッド約10個)                                               │
│                                                                          │
│  結合出力: [1, 5884]                                                     │
└─────────────────────────────────────────────────────────────────────────┘

【Transformer Block 内部構造詳細】
┌──────────────────────────────────────────────────────────────────┐
│  Input [B, T, D] (T=25, D=512)                                   │
│                                                                   │
│  ┌─────────────────────────────────────────────┐                 │
│  │  MULTI-HEAD ATTENTION                       │                 │
│  │                                             │                 │
│  │  Q = Linear(X)  [B,T,D] ────┐              │  Gemm           │
│  │  K = Linear(X)  [B,T,D] ────┤              │  Gemm           │
│  │  V = Linear(X)  [B,T,D] ────┤              │  Gemm           │
│  │                              │              │                 │
│  │  Split to heads: [B,nhead,T,D/nhead]       │                 │
│  │                              │              │                 │
│  │  Scores = Q @ K^T / √(D/nhead)             │  MatMul         │
│  │  Attention = softmax(Scores)               │                 │
│  │  Output = Attention @ V                    │  MatMul         │
│  │                              │              │                 │
│  │  Concat heads → [B,T,D]                    │                 │
│  │  Output Projection: Linear(D→D)            │  Gemm           │
│  └─────────────────────────────────────────────┘                 │
│                     │                                            │
│                     ▼                                            │
│           [Residual Connection]  ←────────┐  Add                │
│                     │                      │                     │
│                     ▼                      │                     │
│              [Layer Norm]          Input ─┘                     │
│                     │                                            │
│                     ▼                                            │
│  ┌─────────────────────────────────────────────┐                 │
│  │  FEED-FORWARD NETWORK                      │                 │
│  │                                             │                 │
│  │  Linear(D → 4*D)  [512 → 2048]             │  Gemm           │
│  │  ReLU                                       │                 │
│  │  Linear(4*D → D)  [2048 → 512]             │  Gemm           │
│  └─────────────────────────────────────────────┘                 │
│                     │                                            │
│                     ▼                                            │
│           [Residual Connection]  ←────────┐  Add                │
│                     │                      │                     │
│                     ▼                      │                     │
│              [Layer Norm]          Input ─┘                     │
│                     │                                            │
│                     ▼                                            │
│  Output [B, T, D]                                                │
└──────────────────────────────────────────────────────────────────┘

【Gemm×47の内訳】
┌──────────────────────────────────────┐
│  Category            │  Count        │
├──────────────────────────────────────┤
│  Input Embedding     │  5            │
│    - feature_embed   │  1            │
│    - desire_embed    │  1            │
│    - traffic_embed   │  1            │
│    - lateral_embed   │  1            │
│    - prev_curv_embed │  1            │
├──────────────────────────────────────┤
│  Input Projection    │  1            │
├──────────────────────────────────────┤
│  Transformer×3       │  24           │
│    Per Block:        │  8            │
│      - Q/K/V Proj    │  3            │
│      - Out Proj      │  1            │
│      - FF Layer 1    │  1            │
│      - FF Layer 2    │  1            │
│      - (LayerNorm)   │  (含まれない) │
├──────────────────────────────────────┤
│  Context Encoder     │  1            │
├──────────────────────────────────────┤
│  Output Heads        │  18           │
│    (各タスク2層ずつ) │               │
├──────────────────────────────────────┤
│  Total               │  47 ✓         │
└──────────────────────────────────────┘

【MatMul×7の内訳】
┌──────────────────────────────────────┐
│  Operation           │  Count        │
├──────────────────────────────────────┤
│  Transformer Block 1 │  2            │
│    - Q @ K^T         │  1            │
│    - Attention @ V   │  1            │
├──────────────────────────────────────┤
│  Transformer Block 2 │  2            │
├──────────────────────────────────────┤
│  Transformer Block 3 │  2            │
├──────────────────────────────────────┤
│  Extra (最適化時)    │  1            │
├──────────────────────────────────────┤
│  Total               │  7 ✓          │
└──────────────────────────────────────┘
```

**根拠**:
1. **Gemm×47**: TransformerのFeed-Forward + AttentionのProjection
2. **MatMul×7**: Multi-Head Attention (Q, K, V行列積)
3. **ReduceMean×4 + Concat×4**: 時系列集約と入力統合

**推定される元の構造** (PyTorch):

```python
import torch
import torch.nn as nn
import math

class OriginalPolicyModel(nn.Module):
    """
    最適化前の理解しやすいPolicy Model

    根拠:
    - Gemm×47 → 5 (Embedding) + 24 (Transformer×3) + 18 (出力ヘッド)
    - MatMul×7 → Transformer×3 × (Q@K + Attention@V) + 1 (extra)
    - 時系列長: 25フレーム (5秒 @ 4Hzサンプリング)
    """
    def __init__(self, d_model=512, nhead=4, num_layers=3):
        super().__init__()

        # 入力 Embedding (Gemm×5)
        self.feature_embed = nn.Linear(512, d_model)      # Vision特徴
        self.desire_embed = nn.Linear(8, 64)              # 運転意図
        self.traffic_embed = nn.Linear(2, 32)             # 交通規則
        self.lateral_embed = nn.Linear(2, 32)             # 横制御パラメータ
        self.prev_curv_embed = nn.Linear(1, 32)           # 過去の曲率

        # 入力統合
        input_dim = d_model + 64 + 32  # feature + desire + prev_curv
        self.input_projection = nn.Linear(input_dim, d_model)

        # Positional Encoding
        self.pos_encoding = PositionalEncoding(d_model, max_len=25)

        # Transformer Encoder (3ブロック)
        # 各ブロック: Gemm×8 (Attention×3 + FF×2 + Layer Norm×3)
        self.transformer_blocks = nn.ModuleList([
            TransformerBlock(d_model, nhead, dim_feedforward=2048)
            for _ in range(num_layers)
        ])

        # グローバルコンテキスト (traffic + lateral)
        self.context_encoder = nn.Linear(64, d_model)

        # マルチタスク出力ヘッド (Gemm×18)
        self.heads = nn.ModuleDict({
            'desired_curvature': nn.Sequential(
                nn.Linear(d_model, 256),
                nn.ReLU(),
                nn.Linear(256, 1),
                nn.Tanh()  # [-1, 1]に制限
            ),
            'plan': nn.Sequential(
                nn.Linear(d_model, 1024),
                nn.ReLU(),
                nn.Linear(1024, 33*15)  # 33ポイント × 15次元
            ),
            'lane_lines': nn.Sequential(
                nn.Linear(d_model, 512),
                nn.ReLU(),
                nn.Linear(512, 4*33*2)
            ),
            # ... 他のヘッド
        })

    def forward(self, features_buffer, desire, traffic_convention,
                lateral_control_params, prev_desired_curv):
        """
        Args:
            features_buffer: [B, 25, 512] - Vision特徴の時系列
            desire: [B, 25, 8] - 運転意図
            traffic_convention: [B, 2] - 交通規則
            lateral_control_params: [B, 2] - 横制御パラメータ
            prev_desired_curv: [B, 25, 1] - 過去の曲率
        """
        B, T = features_buffer.shape[:2]

        # 1. 入力 Embedding
        feat_emb = self.feature_embed(features_buffer)       # [B, 25, 512]
        desire_emb = self.desire_embed(desire)               # [B, 25, 64]
        prev_curv_emb = self.prev_curv_embed(prev_desired_curv)  # [B, 25, 32]

        # 2. 時系列入力の統合
        x = torch.cat([feat_emb, desire_emb, prev_curv_emb], dim=2)  # [B, 25, 608]
        x = self.input_projection(x)  # [B, 25, 512]

        # 3. Positional Encoding
        x = self.pos_encoding(x)  # [B, 25, 512]

        # 4. Transformer Blocks
        for block in self.transformer_blocks:
            x = block(x)  # [B, 25, 512]

        # 5. 時系列集約（最後のタイムステップを使用）
        x = x[:, -1, :]  # [B, 512]

        # 6. グローバルコンテキストの追加
        context = torch.cat([traffic_convention, lateral_control_params], dim=1)  # [B, 4]
        context = torch.cat([context, torch.zeros(B, 60, device=x.device)], dim=1)  # [B, 64]
        context_emb = self.context_encoder(context)  # [B, 512]
        x = x + context_emb  # → ONNX: Add

        # 7. マルチタスク出力
        outputs = {
            'desired_curvature': self.heads['desired_curvature'](x),  # [B, 1]
            'plan': self.heads['plan'](x).view(B, 33, 15),            # [B, 33, 15]
            # ...
        }

        # 8. 結合して[B, 5884]を生成
        output_tensor = torch.cat([
            outputs['desired_curvature'],
            outputs['plan'].flatten(1),
            # ...
        ], dim=1)

        return output_tensor


class TransformerBlock(nn.Module):
    """
    Transformer Encoder Block

    根拠:
    - Gemm×8: Q/K/V projection (3) + Output projection (1) + FF (2) + skip connections (2)
    - MatMul×2: Q@K, Attention@V
    """
    def __init__(self, d_model, nhead, dim_feedforward=2048):
        super().__init__()

        # Multi-Head Attention
        self.attention = MultiHeadAttention(d_model, nhead)
        self.norm1 = nn.LayerNorm(d_model)

        # Feed-Forward Network
        self.ffn = nn.Sequential(
            nn.Linear(d_model, dim_feedforward),  # Gemm
            nn.ReLU(),
            nn.Linear(dim_feedforward, d_model)   # Gemm
        )
        self.norm2 = nn.LayerNorm(d_model)

    def forward(self, x):
        # Attention + Residual
        attn_out = self.attention(x)
        x = self.norm1(x + attn_out)  # → ONNX: Add

        # Feed-Forward + Residual
        ffn_out = self.ffn(x)
        x = self.norm2(x + ffn_out)   # → ONNX: Add

        return x


class MultiHeadAttention(nn.Module):
    """
    Multi-Head Attention

    根拠:
    - Gemm×4: Q/K/V projection + Output projection
    - MatMul×2: Q@K, Attention@V
    """
    def __init__(self, d_model, nhead):
        super().__init__()
        self.nhead = nhead
        self.d_k = d_model // nhead

        self.q_proj = nn.Linear(d_model, d_model)  # Gemm
        self.k_proj = nn.Linear(d_model, d_model)  # Gemm
        self.v_proj = nn.Linear(d_model, d_model)  # Gemm
        self.out_proj = nn.Linear(d_model, d_model)  # Gemm

    def forward(self, x):
        B, T, C = x.shape

        # Q, K, V projection
        Q = self.q_proj(x).view(B, T, self.nhead, self.d_k).transpose(1, 2)  # [B, nhead, T, d_k]
        K = self.k_proj(x).view(B, T, self.nhead, self.d_k).transpose(1, 2)
        V = self.v_proj(x).view(B, T, self.nhead, self.d_k).transpose(1, 2)

        # Scaled Dot-Product Attention
        scores = torch.matmul(Q, K.transpose(-2, -1)) / math.sqrt(self.d_k)  # MatMul (Q@K)
        attn_weights = torch.softmax(scores, dim=-1)
        attn_output = torch.matmul(attn_weights, V)  # MatMul (Attention@V)

        # Concat heads
        attn_output = attn_output.transpose(1, 2).contiguous().view(B, T, C)

        # Output projection
        output = self.out_proj(attn_output)

        return output


class PositionalEncoding(nn.Module):
    """Positional Encoding for Transformer"""
    def __init__(self, d_model, max_len=5000):
        super().__init__()
        pe = torch.zeros(max_len, d_model)
        position = torch.arange(0, max_len, dtype=torch.float).unsqueeze(1)
        div_term = torch.exp(torch.arange(0, d_model, 2).float() *
                            (-math.log(10000.0) / d_model))
        pe[:, 0::2] = torch.sin(position * div_term)
        pe[:, 1::2] = torch.cos(position * div_term)
        self.register_buffer('pe', pe.unsqueeze(0))  # [1, max_len, d_model]

    def forward(self, x):
        return x + self.pe[:, :x.size(1), :]
```

**最適化の影響**:
```
PyTorch                           ONNX最適化
────────────────────────────────────────────────────

nn.MultiheadAttention         →   Q/K/V Gemm + MatMul×2 + Concat
  (高レベルAPI)                   (低レベル操作)

nn.LayerNorm                  →   ReduceMean + Sub + Mul + Add
  (一つの層)                     (複数のノード)

TransformerEncoderLayer       →   Gemm×8 + MatMul×2 + Add×2 + ...
  (一つのブロック)                 (多数のプリミティブ)
```

### 2.5.3 最適化前モデルの利点

**理解しやさ**:
1. ✅ **明確な階層構造**: Stem → Stages → Heads
2. ✅ **モジュール化**: 各ブロックが独立して理解可能
3. ✅ **デバッグ容易**: 中間層の出力を検証できる
4. ✅ **拡張性**: 新しいヘッドやブロックを追加しやすい

**実装方針**:
1. 上記のPyTorchモデルを実装
2. openpilotのデータセットで学習
3. ONNXにエクスポート
4. TensorRTで最適化
5. 既存の最適化モデルと精度比較

**検証方法**:
```python
# モデル構造の検証
import torch.onnx

model = OriginalVisionModel()
torch.onnx.export(
    model,
    (torch.randn(1, 12, 128, 256), torch.randn(1, 12, 128, 256)),
    'my_vision.onnx',
    opset_version=14,
    do_constant_folding=False  # 最適化無し
)

# レイヤー数の比較
import onnx
original = onnx.load('my_vision.onnx')
optimized = onnx.load('driving_vision.onnx')

print(f'Original: {len(original.graph.node)} nodes')
print(f'Optimized: {len(optimized.graph.node)} nodes')
# 期待: Original ≈ 60-80 nodes, Optimized = 425 nodes
```

---

## 3. 実装方針

### 3.1 分析結果からの実装戦略

#### Vision Model実装方針

**アーキテクチャ選択**:
```
ONNX分析結果 → 実装方針
─────────────────────────────────────────
Conv×60      → EfficientNet-B0をベースに改造
Mul×128      → SiLU活性化を全体に使用
Gemm×36      → マルチタスク出力ヘッド（7-8タスク）
```

**実装レイヤー対応表**:
| ONNX Op | PyTorch実装 | 用途 |
|---------|------------|------|
| Conv | `nn.Conv2d()` | 畳み込み層 |
| Mul + Sigmoid | `nn.SiLU()` | 活性化関数 |
| Add | `x + residual` | 残差接続 |
| Gemm | `nn.Linear()` | 全結合層 |
| MaxPool | `nn.MaxPool2d()` | ダウンサンプリング |
| Concat | `torch.cat([x1, x2], dim=1)` | 2カメラ入力の結合 |

**ステージ構成**:
```python
# ONNX解析から推定されるステージ構成
Stem:    Conv×3  (入力24ch → 64ch)
Stage1:  Conv×12 (64ch → 128ch, stride=2)
Stage2:  Conv×15 (128ch → 256ch, stride=2)
Stage3:  Conv×18 (256ch → 512ch, stride=2)
Stage4:  Conv×12 (512ch → 512ch)
Head:    Gemm×36 (512次元 → 632次元)
```

#### Policy Model実装方針

**アーキテクチャ選択**:
```
ONNX分析結果 → 実装方針
─────────────────────────────────────────
Gemm×47      → Transformer (3ブロック)
MatMul×7     → Multi-Head Attention (nhead=4)
ReduceMean×4 → Temporal Aggregation
Concat×4     → Multi-Modal Input Fusion
```

**Transformerブロック推定**:
```python
# Gemm×47の内訳（推定）
入力Embedding:  Gemm×5  (各入力の埋め込み)
Transformer×3:
  - Attention:  Gemm×2/block × 3 = 6
  - FF:         Gemm×2/block × 3 = 6
  - Total:      Gemm×8/block × 3 = 24
出力ヘッド:     Gemm×18 (複数タスク)
合計:           5 + 24 + 18 = 47 ✓
```

**実装レイヤー対応表**:
| ONNX Op | PyTorch実装 | 用途 |
|---------|------------|------|
| Gemm | `nn.Linear()` | 全結合層 |
| MatMul | `torch.matmul()` | Attention計算 |
| Add | `x + residual` | 残差接続 |
| Relu | `nn.ReLU()` | 活性化関数 |
| ReduceMean | `x.mean(dim=1)` | 時系列集約 |
| Concat | `torch.cat([...], dim=-1)` | 入力統合 |

### 3.2 実装の優先順位

**Phase 1: ベースライン実装** (1-2週間)
1. Vision Model骨格（EfficientNet-B0ベース）
2. Policy Model骨格（標準Transformer）
3. 入出力インターフェース
4. 推論パイプライン

**Phase 2: アーキテクチャ最適化** (2-3週間)
1. ONNX解析に基づく層数調整
2. SiLU活性化の適用
3. マルチタスク出力ヘッドの実装
4. 時系列バッファ管理

**Phase 3: 学習とチューニング** (3-4週間)
1. データセット準備
2. マルチタスク損失関数
3. 学習ループ実装
4. ハイパーパラメータ調整

**Phase 4: デプロイ準備** (1-2週間)
1. ONNX/TensorRTエクスポート
2. Jetson Nano最適化
3. 推論速度検証（20Hz目標）
4. メモリ使用量削減

### 3.3 検証計画

**構造検証**:
```bash
# 実装後のONNXエクスポート
python export_to_onnx.py --model vision --output my_vision.onnx
python export_to_onnx.py --model policy --output my_policy.onnx

# 層数の比較
python compare_models.py --original driving_vision.onnx --custom my_vision.onnx
# 期待結果: Conv×60, Mul×128, Gemm×36等が一致
```

**性能検証**:
```python
# 推論速度ベンチマーク
import time
import torch

model = VisionModel().eval()
input_data = torch.randn(1, 12, 128, 256)

# ウォームアップ
for _ in range(10):
    _ = model(input_data)

# 計測
start = time.time()
for _ in range(100):
    with torch.no_grad():
        _ = model(input_data)
end = time.time()

fps = 100 / (end - start)
print(f'FPS: {fps:.1f} Hz')  # 目標: 20Hz以上
```

---

## 4. モデル概要

openpilotは**2段階のニューラルネットワーク**で動作します：

```
カメラ画像 → Vision Model → 特徴抽出 → Policy Model → 制御指令
  (認識層)       512次元         (制御層)    曲率+軌道
```

### 4.1 Vision Model（認識層）

**役割**: カメラ画像から環境特徴を抽出

**入力テンソル**:
```python
input_imgs:      [1, 12, 128, 256]  # 通常カメラ画像（12フレーム）
big_input_imgs:  [1, 12, 128, 256]  # 広角カメラ画像（12フレーム）
```

**各次元の意味**:
```
[バッチ, チャネル, 高さ, 幅]
[  1,     12,    128, 256]
   │      │       │    └─ 横解像度（256ピクセル）
   │      │       └────── 縦解像度（128ピクセル）
   │      └────────────── 時間軸（12フレーム = 0.6秒分）
   │                       ※ 20Hz × 12 = 600ms
   │                       ※ RGB 3ch × 4フレーム = 12ch という考え方も
   └───────────────────── バッチサイズ（常に1）
```

> **💡 なぜ12チャネル？**
> openpilotは複数フレームを連結して時間情報を持たせる設計。
> - **方式1**: RGB 3ch × 4フレーム = 12ch（時系列結合）
> - **方式2**: 12個の連続フレームをグレースケール化（モノクロ×12）
> どちらにせよ、**過去の動き**を捉えるための工夫。

**出力テンソル**:
```python
outputs: [1, 632]  # 512次元特徴 + メタ情報
```

**各次元の意味**:
```
[バッチ, 特徴次元]
[  1,     632   ]
   │      └────────────── 合計632次元
   │                       ├─ hidden_state[512]   : 抽象特徴ベクトル
   │                       ├─ pose[6]             : Roll,Pitch,Yaw + 位置
   │                       ├─ road_transform[6]   : アフィン変換行列
   │                       ├─ lane_lines[...]     : 車線検出結果
   │                       ├─ road_edges[...]     : 道路エッジ
   │                       └─ その他タスク出力
   └───────────────────── バッチサイズ（常に1）
```

**632次元の完全な内訳**:

> **📌 根拠**: `selfdrive/modeld/get_model_metadata.py`で抽出したONNXメタデータ
> **🔗 コード**: `selfdrive/modeld/constants.py`, `parse_model_outputs.py`

| 出力名 | インデックス | 次元数 | 説明 |
|--------|-------------|--------|------|
| `meta` | [0:55] | 55 | 運転行動予測（engaged, disengage, brake, blinker等）<br>※ 2,4,6,8,10秒先の予測 |
| `desire_pred` | [55:87] | 32 | 運転意図予測（4ステップ × 8カテゴリ）<br>※ 車線維持、右折、左折等 |
| `pose` | [87:99] | 12 | 車両姿勢（6次元 × 2[μ, σ]）<br>※ Roll, Pitch, Yaw, X, Y, Z |
| `wide_from_device_euler` | [99:105] | 6 | 広角カメラ相対姿勢（3次元 × 2[μ, σ]）<br>※ 2カメラのキャリブレーション |
| `road_transform` | [105:117] | 12 | 道路座標変換（6次元 × 2[μ, σ]）<br>※ 2×3アフィン変換行列 |
| **`hidden_state`** | **[117:629]** | **512** | **Policy Model入力特徴**<br>※ 環境の高次抽象表現 |
| `pad` | [629:632] | 3 | メモリアラインメント用パディング |
| **合計** | **[0:632]** | **632** | Vision Model総出力次元 |

#### 512次元（hidden_state）の意味

**抽象度の高い特徴ベクトル**で、以下を統合的に表現：

1. **空間的特徴**:
   - 道路構造（直線、カーブ、交差点）
   - 車線配置（片側2車線、3車線等）
   - 道路境界（ガードレール、縁石、路肩）

2. **意味的特徴**:
   - 先行車の存在と動き
   - 障害物の位置と種類
   - 交通標識・信号の状態

3. **時間的特徴**:
   - 過去12フレーム（0.6秒）の動き情報
   - 時系列パターン（加速中、減速中等）

4. **文脈情報**:
   - 天候・照明条件
   - 都市部 vs 高速道路
   - 混雑度

> **💡 なぜPolicy Modelに512次元だけ渡す？**
> - **情報圧縮**: 生画像（12×128×256 = 393,216次元）→ 512次元に圧縮
> - **タスク独立性**: Vision特有の詳細（ピクセル値）を捨て、制御に必要な情報のみ抽出
> - **転移学習**: Vision Modelを変更してもPolicyへのインターフェース（512次元）は不変
> - **計算効率**: Policy Modelは512次元から推論→高速化

#### 根拠コード

```python
# selfdrive/modeld/constants.py
class ModelConstants:
  FEATURE_LEN = 512  # hidden_stateの次元

# selfdrive/modeld/get_model_metadata.py
# ONNXメタデータから出力スライスを抽出
output_slices = get_metadata_value_by_name(model, 'output_slices')
# 結果:
# {
#   'meta': slice(0, 55),
#   'desire_pred': slice(55, 87),
#   'pose': slice(87, 99),
#   'wide_from_device_euler': slice(99, 105),
#   'road_transform': slice(105, 117),
#   'hidden_state': slice(117, 629),  # ← 512次元
#   'pad': slice(629, 632)
# }

# selfdrive/modeld/modeld.py
vision_outputs_dict = self.parser.parse_vision_outputs(
    self.slice_outputs(self.vision_output, self.vision_output_slices)
)
# hidden_stateを抽出してPolicy Modelへ
self.full_features_buffer[0,-1] = vision_outputs_dict['hidden_state'][0, :]
```

### 4.2 Policy Model（制御層）

**役割**: 環境特徴から制御指令を生成

**ファイル**:
- `selfdrive/modeld/models/driving_policy.onnx`
- 可視化: [driving_policy.onnx.png](../models/driving_policy.onnx.png)

**構造統計**:
- **総ノード数**: 174層
- **主要レイヤー**: Gemm(47), Relu(39), Add(26), Constant(18), MatMul(7), Mul(5), Concat(4)
- **パラメータ数**: 約5M

**入力テンソル**:
```python
features_buffer:         [1, 25, 512]  # Vision特徴（時系列）
desire:                  [1, 25, 8]    # 運転意図
traffic_convention:      [1, 2]        # 交通規則（右側/左側通行）
lateral_control_params:  [1, 2]        # 横制御パラメータ
prev_desired_curv:       [1, 25, 1]    # 過去の制御出力
```

**各次元の意味**:

1. **features_buffer [1, 25, 512]**
   ```
   [バッチ, 時間ステップ, 特徴次元]
   [  1,      25,         512    ]
      │       │           └─────── Vision Modelの出力（hidden_state）
      │       └─────────────────── 過去5秒分（100Hz ÷ 4 = 25ステップ）
      │                             ※ Vision 20Hz → 4サンプル毎1ステップ
      └─────────────────────────── バッチサイズ（常に1）
   ```

2. **desire [1, 25, 8]**
   ```
   [バッチ, 時間ステップ, 運転意図カテゴリ]
   [  1,      25,         8              ]
      │       │           └─────── 8種類の運転モード（One-Hot）
      │       │                    ├─ 車線維持
      │       │                    ├─ 右車線変更
      │       │                    ├─ 左車線変更
      │       │                    ├─ 右折
      │       │                    ├─ 左折
      │       │                    └─ その他
      │       └─────────────────── 過去5秒分
      └─────────────────────────── バッチサイズ
   ```

3. **traffic_convention [1, 2]**
   ```
   [バッチ, パラメータ]
   [  1,      2       ]
      │       └─────────────────── [右側通行flag, 左側通行flag]
      │                            例: [1, 0] = 右側通行（日本・米国）
      │                                 [0, 1] = 左側通行（英国・豪州）
      └─────────────────────────── バッチサイズ
   ```

4. **lateral_control_params [1, 2]**
   ```
   [バッチ, パラメータ]
   [  1,      2       ]
      │       └─────────────────── 車両固有の横制御パラメータ
      │                            ├─ ステアリングレシオ
      │                            └─ タイヤ特性係数
      └─────────────────────────── バッチサイズ
   ```

5. **prev_desired_curv [1, 25, 1]**
   ```
   [バッチ, 時間ステップ, 曲率]
   [  1,      25,         1   ]
      │       │           └─────── 過去の制御出力（目標曲率）
      │       │                    ※ 自己回帰的な制御の滑らかさ向上
      │       └─────────────────── 過去5秒分
      └─────────────────────────── バッチサイズ
   ```

**出力テンソル**:
```python
outputs: [1, 5884]  # 制御指令 + 軌道計画
```

**各次元の意味**:
```
[バッチ, 出力次元]
[  1,    5884   ]
   │      └────────────── 合計5884次元
   │                       ├─ desired_curvature[1]   : 即時制御（曲率）
   │                       ├─ plan[33×15=495]        : 将来軌道（速度・加速度）
   │                       ├─ lane_lines[4×33×2=264] : 車線予測
   │                       ├─ road_edges[2×33×2=132] : 道路境界予測
   │                       └─ その他タスク出力
   └───────────────────── バッチサイズ（常に1）
```

**主要な出力内訳**:
| 出力名 | 次元 | 意味 |
|--------|------|------|
| `desired_curvature` | 1 | **現在の目標曲率** [1/m]<br>→ ステアリング角度に変換 |
| `plan` | [33, 15] | **将来軌道**（33ステップ = 10秒先まで）<br>15次元 = [X, Y, 速度, 加速度, ...] |
| `lane_lines` | [4, 33, 2] | **車線予測**<br>4本線 × 33ステップ × (X, Y)座標 |
| `road_edges` | [2, 33, 2] | **道路境界予測**<br>左右2本 × 33ステップ × (X, Y)座標 |

> **💡 なぜ33ステップ？**
> 100Hzで10秒先を予測 → 1000ステップは重すぎる
> → **30～33ステップに間引いて予測**（約0.3秒間隔）
> これにより、遠い未来まで見通しつつ計算量を削減。

### 4.3 モデルの使い分け

openpilotが**Vision Model**と**Policy Model**を分離している理由：

#### **1. 責任の分離（Separation of Concerns）**

```
Vision Model  = 認識専門  「何が見えているか？」
                ↓
                環境理解（世界モデル）
                ↓
Policy Model  = 制御専門  「どう動くべきか？」
```

- **Vision Model**: 画像から道路・車線・障害物を「認識」
- **Policy Model**: 認識結果から制御指令を「決定」

この分離により、各モデルが単一の責任に専念できる。

#### **2. 学習データセットの違い**

| モデル | 学習データ | ラベル |
|--------|-----------|--------|
| **Vision Model** | 大量の走行動画 | 道路構造、車線、障害物の位置 |
| **Policy Model** | 人間の運転データ | ハンドル角度、加速度、ブレーキ |

- **Vision**: 静的な環境理解（ImageNetライクな認識タスク）
- **Policy**: 動的な制御判断（強化学習 or 模倣学習）

データセットを分離することで、各モデルを**独立に大規模学習**できる。

#### **3. 推論頻度の最適化**

```python
# Vision Model: 20Hz（50ms間隔）
# - 画像処理は重い → 低頻度でOK
vision_output = vision_model(camera_imgs)  # 50ms

# Policy Model: 100Hz（10ms間隔）
# - 制御は高頻度が必要 → 軽量な入力で高速実行
for _ in range(5):  # Vision 1回に対しPolicy 5回
    control = policy_model(vision_output)  # 10ms
```

- **Vision**: 環境は急激に変化しない → 20Hz
- **Policy**: 車両制御は滑らか & リアルタイム → 100Hz

異なる推論頻度を実現するため、モデルを分離。

#### **4. 独立した改善サイクル**

```
┌─────────────────┐        ┌─────────────────┐
│  Vision改善      │        │  Policy改善      │
│  ・車線検出精度  │        │  ・滑らかな制御  │
│  ・障害物認識    │  独立  │  ・追従性能      │
│  ・悪天候対応    │  ←→   │  ・コーナリング  │
└─────────────────┘        └─────────────────┘
```

- **Vision改善**: 車線検出アルゴリズム変更 → Policyに影響なし
- **Policy改善**: 制御ロジック変更 → Visionに影響なし

モジュール化により、**A/Bテスト**や**段階的デプロイ**が容易。

#### **5. 計算リソースの効率化**

```
Vision Model: 15M params → GPU必須（重い）
Policy Model:  5M params → CPU/NPUで実行可能（軽い）
```

- **Vision**: 大規模CNN → 高性能GPU（NVIDIA Xavier等）
- **Policy**: 軽量Transformer → 低消費電力NPU

異なるハードウェアアクセラレータで並列実行 → スループット向上。

#### **6. モデル交換の柔軟性**

```python
# 異なるVision Modelを試す
vision_output = efficientnet_model(imgs)    # 案1
vision_output = resnet_model(imgs)          # 案2
vision_output = transformer_model(imgs)     # 案3

# Policy Modelは変更不要（入力形式が同じなら）
control = policy_model(vision_output)
```

**インターフェース（512次元ベクトル）** が固定されていれば、
Vision/Policyを**独立に交換・アップグレード**可能。

#### **7. 実装上のメリット**

| 観点 | 効果 |
|------|------|
| **デバッグ** | Vision問題かPolicy問題か切り分け容易 |
| **テスト** | 各モデルを独立にユニットテスト可能 |
| **チーム分業** | 認識チーム/制御チーム で並行開発 |
| **バージョン管理** | 各モデルを独立にgit管理・ロールバック |

---

**まとめ**: Vision/Policy分離は、**大規模自動運転システム**の
**スケーラビリティ**と**保守性**を高めるための設計思想。

### 4.4 分離学習における課題と解決策

#### **Q1: hidden_state（512次元）のGround Truthは作れるか？**

**A: 直接的なGround Truthは作れない（考え方は妥当）**

```
✅ Ground Truthが作れる出力:
├─ pose[6]           → IMU/GPS/Odoから計測可能
├─ desire_pred[32]   → 人間の運転意図（ウィンカー等から推定）
├─ road_transform[6] → キャリブレーションで取得可能
└─ lane_lines[...]   → LiDAR/HD-Mapでアノテーション

❌ Ground Truthが作れない出力:
└─ hidden_state[512] → 「抽象特徴」の正解は不明確
                        （何を表現すべきかが定義不能）
```

**問題点**:
- 512次元の各要素が**何を表現すべきか**を定義できない
- 「良い特徴表現」の基準が曖昧
- 直接的な教師信号が存在しない

#### **Q2: hidden_stateの学習方法（一般的なアプローチ）**

**方法1: End-to-End学習（最も一般的）**

```
┌─────────────────────┐
│  Vision Model       │
│  ↓                  │
│  hidden_state[512] ──┼──→ Policy Model
└─────────────────────┘      ↓
                          制御出力
                             ↓
                        Loss（制御誤差）
                             ↓
                    Backpropagation
                        Vision ← Policy
```

- **手法**: Vision ModelとPolicy Modelを**結合して学習**
- **Loss**: Policy Modelの最終出力（desired_curvature等）と人間の運転との誤差
- **Backpropagation**: Policy → Visionへ勾配を伝播
- **結果**: hidden_stateは**制御タスクに最適化**される

**コード例**:
```python
# End-to-End学習
vision_output = vision_model(camera_images)      # [B, 632]
hidden_state = vision_output[:, 117:629]         # [B, 512]

policy_output = policy_model(
    hidden_state,
    desire,
    traffic_convention
)  # [B, 5884]

# 制御誤差から逆伝播
loss = mse_loss(policy_output['curvature'], ground_truth_curvature)
loss.backward()  # ← Vision Modelまで勾配が伝わる
```

**利点**:
- hidden_stateのGround Truthが不要
- 制御タスクに直接最適化される
- 実装がシンプル

**欠点**:
- Vision/Policyを同時に学習 → 収束が難しい
- データセット要件が厳しい（画像+制御ログ必須）

---

**方法2: Multi-Task学習（openpilotの実装）**

```
                Vision Model
                     ↓
        ┌────────────┼────────────┐
        │            │            │
    pose[12]   hidden_state[512]  meta[55]
        │            │            │
   Loss_pose    (No Loss)    Loss_meta
        │                         │
        └─────────┬───────────────┘
              Total Loss
```

- **手法**: Vision Modelに**複数のタスク**を学習させる
- **直接Loss**: pose, meta, desire_pred等（Ground Truth有り）
- **間接Loss**: hidden_stateは他タスクと共有される重み経由で学習
- **結果**: hidden_stateは**補助タスクの副産物**として良い表現を獲得

**コード例**:
```python
# Multi-Task学習（Vision単体）
vision_output = vision_model(camera_images)

# 各タスクのLoss
loss_pose = mse_loss(vision_output['pose'], gt_pose)
loss_meta = bce_loss(vision_output['meta'], gt_meta)
loss_desire = ce_loss(vision_output['desire_pred'], gt_desire)

# hidden_stateには直接Lossなし（共有層経由で間接的に学習）
total_loss = loss_pose + loss_meta + loss_desire
total_loss.backward()  # ← 全タスクで共有されるbackboneを更新
```

**利点**:
- hidden_stateのGround Truth不要
- 複数タスクが**良い表現を強制**（regularization効果）
- Vision/Policy を**独立に学習**可能

**欠点**:
- hidden_stateが制御タスクに最適化される保証なし
- 補助タスクの選択が重要

---

**方法3: Auxiliary Loss（補助タスク）**

```
Vision Model
    ↓
hidden_state[512]
    ↓
    ├──→ Policy Model → 制御Loss
    └──→ Auxiliary Decoders
         ├─ Semantic Segmentation → Loss_seg
         ├─ Depth Estimation → Loss_depth
         └─ Optical Flow → Loss_flow
```

- **手法**: hidden_stateから**追加タスク**を予測
- **補助タスク**: セグメンテーション、深度推定、オプティカルフロー等
- **結果**: hidden_stateが**豊富な情報を保持**するよう正則化

**コード例**:
```python
# Auxiliary Loss
vision_output = vision_model(camera_images)
hidden_state = vision_output['hidden_state']  # [B, 512]

# メインタスク（制御）
policy_output = policy_model(hidden_state, ...)
loss_control = mse_loss(policy_output, gt_control)

# 補助タスク（表現学習）
seg_pred = seg_decoder(hidden_state)
loss_seg = ce_loss(seg_pred, gt_segmentation)

depth_pred = depth_decoder(hidden_state)
loss_depth = l1_loss(depth_pred, gt_depth)

# 合計Loss
total_loss = loss_control + 0.1 * loss_seg + 0.1 * loss_depth
```

**利点**:
- hidden_stateが**多様な情報**を保持
- 過学習を防ぐ（regularization）

**欠点**:
- 補助タスクのGround Truthが必要
- 計算コスト増加

---

**方法4: Representation Learning（事前学習）**

```
Phase 1: 自己教師あり学習（SimCLR, MAE等）
    ↓
Vision Model（pre-trained）
    ↓
Phase 2: Fine-tuning（End-to-End or Multi-Task）
```

- **手法**: 大規模データで**事前学習** → 制御タスクでFine-tuning
- **自己教師あり**: SimCLR（対比学習）、MAE（Masked Autoencoder）等
- **結果**: hidden_stateが**汎用的な視覚表現**を獲得

**コード例**:
```python
# Phase 1: 自己教師あり学習
image1, image2 = augment(image)  # Data Augmentation
h1 = vision_model(image1)['hidden_state']
h2 = vision_model(image2)['hidden_state']
loss_contrastive = contrastive_loss(h1, h2)  # SimCLR

# Phase 2: Fine-tuning
vision_model.load_pretrained_weights()
# ... End-to-End or Multi-Task学習
```

**利点**:
- ラベルなしデータを活用可能
- 汎化性能が高い

**欠点**:
- 2段階学習が必要
- 事前学習データセットの質に依存

---

**方法5: Policy Gradient（強化学習）**

```
環境 → Vision Model → hidden_state[512] → Policy Model → 行動
  ↓                                                        ↓
報酬 ←─────────────────────────────────────────────────────┘
  ↓
Policy Gradient → Vision/Policyを同時更新
```

- **手法**: 報酬信号で**直接最適化**
- **報酬**: 安全性、快適性、目的地到達等
- **結果**: hidden_stateは**報酬最大化に最適化**

**利点**:
- Ground Truthが一切不要
- タスクに対して真に最適

**欠点**:
- 学習が不安定
- サンプル効率が悪い
- シミュレータが必須

---

#### **openpilotの実際の学習戦略（推定）**

```
┌─────────────────────────────────────┐
│ Phase 1: Vision Model学習           │
│ ・方法: Multi-Task Learning          │
│ ・タスク: pose, meta, desire_pred等  │
│ ・データ: 大規模走行動画（ラベル付き） │
│ ・結果: hidden_state間接的に学習     │
└─────────────────────────────────────┘
           ↓ Freeze Vision
┌─────────────────────────────────────┐
│ Phase 2: Policy Model学習           │
│ ・方法: Supervised Learning          │
│ ・入力: hidden_state[512]（固定）    │
│ ・タスク: desired_curvature, plan等  │
│ ・データ: 人間の運転ログ             │
│ ・結果: 制御モデルのみ最適化         │
└─────────────────────────────────────┘
           ↓ （Optional）
┌─────────────────────────────────────┐
│ Phase 3: Fine-tuning（End-to-End）  │
│ ・Vision + Policy を結合             │
│ ・小さい学習率でJoint Training       │
│ ・結果: hidden_stateも微調整         │
└─────────────────────────────────────┘
```

**根拠**:
- Visionには明確な補助タスク（pose, meta等）が定義されている
- Policy入力として`features_buffer[25, 512]`（時系列のhidden_state）を使用
- 2モデル分離により、独立した改善サイクルが可能

---

#### **まとめ: 実務的な推奨アプローチ**

| 状況 | 推奨手法 |
|------|---------|
| **初期開発** | Multi-Task Learning<br>（Vision単体で複数タスク学習） |
| **性能向上** | End-to-End Fine-tuning<br>（Vision+Policyを結合） |
| **大規模データ有り** | Representation Learning<br>（事前学習 → Fine-tuning） |
| **シミュレータ有り** | Policy Gradient<br>（強化学習で最適化） |

**hidden_stateの学習における鉄則**:
1. **直接的なGround Truthは不要** → 他タスクから間接的に学習
2. **複数の学習信号を活用** → Multi-Task or Auxiliary Loss
3. **段階的な学習** → Vision単体 → Policy単体 → Joint Fine-tuning
4. **評価は最終タスクで** → hidden_state自体の良さは測れない

---

## 5. Vision Model詳細

### 5.1 アーキテクチャ

**バックボーン**: EfficientNet-like CNN

```
Input [1,12,128,256] × 2
    ↓
[Concat] → [1,24,128,256]
    ↓
Stem Conv Block
    ↓
Stage 1: Conv + Activation × N
    ↓
Stage 2: Conv + Activation × N
    ↓
Stage 3: Conv + Activation × N
    ↓
Stage 4: Conv + Activation × N
    ↓
Global Average Pooling
    ↓
FC Layers (Gemm × 36)
    ↓
Output [1,632]
```

### 5.2 レイヤー構成

**ONNX解析結果**（実測値）:

| レイヤータイプ | 数 | 役割 |
|--------------|---|------|
| **Mul** | 128 | 要素積（活性化関数の一部、SiLU/Swish等） |
| **Constant** | 78 | 定数テンソル（バイアス、スケール等） |
| **Add** | 61 | 加算（スキップ接続、バイアス加算） |
| **Conv** | 60 | 畳み込み（特徴抽出） |
| **Gemm** | 36 | 全結合層（分類・回帰ヘッド） |
| **Relu** | 30 | ReLU活性化関数 |
| **Tanh** | 19 | Tanh活性化関数（出力層の正規化） |
| **その他** | 13 | Cast, Concat, Flatten, Pooling等 |

**注**: Mul×128は活性化関数（SiLU = x * sigmoid(x)）の実装に使用されています。

### 5.3 主要な特徴

1. **2カメラ入力**: 通常カメラ + 広角カメラ
2. **時系列入力**: 12フレーム（約0.6秒分の履歴）
3. **軽量設計**: モバイルデバイスで20Hz実行
4. **マルチタスク出力**: hidden_state + 補助タスク

---

## 6. Policy Model詳細

### 6.1 アーキテクチャ

**バックボーン**: Transformer-like Temporal Model

```
Input Features [1,25,512]
    ↓
[Temporal Summarizer]
    ├─ Extra Input Processing (desire, params)
    ├─ Feature Embedding
    └─ Temporal Attention
    ↓
[Transformer Blocks]
    ├─ Self-Attention
    ├─ Feed-Forward
    └─ Layer Norm
    ↓
[Output Heads]
    ├─ Curvature Head → desired_curvature[1]
    ├─ Plan Head → plan[33,15]
    ├─ Lane Lines Head
    └─ Road Edges Head
```

### 6.2 レイヤー構成

**ONNX解析結果**（実測値）:

| レイヤータイプ | 数 | 役割 |
|--------------|---|------|
| **Gemm** | 47 | 全結合層（Transformer Feed-Forward） |
| **Relu** | 39 | ReLU活性化関数 |
| **Add** | 26 | 加算（残差接続） |
| **Constant** | 18 | 定数テンソル（バイアス、スケール等） |
| **MatMul** | 7 | 行列積（Attention機構） |
| **Mul** | 5 | 要素積（スケーリング等） |
| **Concat** | 4 | テンソル結合（入力統合） |
| **ReduceMean** | 4 | 平均プーリング（時系列集約） |
| **その他** | 24 | Reshape, Transpose, Squeeze等 |

**注**: Gemm×47は主にTransformerのFeed-Forward層とOutput Headsで使用されています。

### 6.3 主要な特徴

1. **時系列処理**: 25フレーム（5秒分の履歴）
2. **マルチモーダル入力**: 特徴 + desire + パラメータ
3. **自己回帰**: 過去の制御出力をフィードバック
4. **マルチヘッド出力**: 曲率 + 軌道 + 車線情報

---

## 7. データフロー詳細

### 7.1 Vision Model実行

```python
# modeld.py の処理フロー
def run_vision_model(buf_main, buf_extra, transform):
    # 1. 画像の前処理
    imgs = preprocess_images(buf_main, buf_extra, transform)
    #    → [1, 12, 128, 256] × 2

    # 2. Vision Model推論
    vision_output = vision_model(imgs)
    #    → [1, 632]

    # 3. 出力のパース
    hidden_state = vision_output[:, :512]      # [1, 512]
    pose = vision_output[:, 512:518]           # [1, 6]
    road_transform = vision_output[:, 518:524] # [1, 6]
    # ... その他

    return hidden_state, pose, road_transform, ...
```

### 7.2 Policy Model実行

```python
# modeld.py の処理フロー
def run_policy_model(features_buffer, desire, params, prev_curv):
    # 1. 入力の準備
    inputs = {
        'features_buffer': features_buffer,        # [1, 25, 512]
        'desire': desire,                          # [1, 25, 8]
        'traffic_convention': traffic_convention,  # [1, 2]
        'lateral_control_params': params,          # [1, 2]
        'prev_desired_curv': prev_curv            # [1, 25, 1]
    }

    # 2. Policy Model推論
    policy_output = policy_model(**inputs)
    #    → [1, 5884]

    # 3. 出力のパース
    desired_curvature = policy_output[:, :1]         # [1, 1]
    plan = policy_output[:, 1:496].reshape(1,33,15)  # [1, 33, 15]
    lane_lines = policy_output[:, 496:...] # [1, 4, 33, 2]
    # ... その他

    return desired_curvature, plan, lane_lines, ...
```

### 7.3 時系列バッファ管理

```python
# 特徴バッファの管理（modeld.py）
class ModelState:
    def __init__(self):
        # 100フレーム分の特徴を保持（5秒 @ 20Hz）
        self.full_features_buffer = np.zeros((1, 100, 512))

        # Policy Model用のサンプリングインデックス
        # 4フレームごと（0.2秒間隔）に25フレーム
        self.temporal_idxs = slice(-1-(4*(25-1)), None, 4)

    def update_buffer(self, new_hidden_state):
        # 左シフト + 最新追加
        self.full_features_buffer[0, :-1] = self.full_features_buffer[0, 1:]
        self.full_features_buffer[0, -1] = new_hidden_state

        # Policy Model用にサンプリング
        features_buffer = self.full_features_buffer[0, self.temporal_idxs]
        # → [25, 512]

        return features_buffer
```

---

## 8. 実装内容

このセクションでは、ONNX分析結果に基づいたPyTorch実装を示します。

### 8.1 Vision Model実装

#### 5.1.1 モデル構造

```python
import torch
import torch.nn as nn

class VisionModel(nn.Module):
    def __init__(self):
        super().__init__()

        # Stem Block
        self.stem = nn.Sequential(
            nn.Conv2d(24, 32, kernel_size=3, stride=2, padding=1),
            nn.SiLU(),
            nn.Conv2d(32, 64, kernel_size=3, stride=1, padding=1),
            nn.SiLU()
        )

        # Backbone (EfficientNet-like)
        self.stage1 = self._make_stage(64, 128, num_blocks=3)
        self.stage2 = self._make_stage(128, 256, num_blocks=4)
        self.stage3 = self._make_stage(256, 512, num_blocks=6)
        self.stage4 = self._make_stage(512, 512, num_blocks=3)

        # Global Pooling
        self.pool = nn.AdaptiveAvgPool2d(1)

        # Output Heads
        self.hidden_state_head = nn.Linear(512, 512)
        self.pose_head = nn.Linear(512, 6)
        self.road_transform_head = nn.Linear(512, 6)
        # ... その他のヘッド

    def _make_stage(self, in_ch, out_ch, num_blocks):
        layers = []
        for i in range(num_blocks):
            layers.extend([
                nn.Conv2d(in_ch if i==0 else out_ch, out_ch, 3, 1, 1),
                nn.BatchNorm2d(out_ch),
                nn.SiLU()
            ])
        return nn.Sequential(*layers)

    def forward(self, input_imgs, big_input_imgs):
        # Concat両カメラ: [1,12,128,256] × 2 → [1,24,128,256]
        x = torch.cat([input_imgs, big_input_imgs], dim=1)

        # Backbone
        x = self.stem(x)
        x = self.stage1(x)
        x = self.stage2(x)
        x = self.stage3(x)
        x = self.stage4(x)

        # Global Pooling: [B,512,H,W] → [B,512]
        x = self.pool(x).flatten(1)

        # Output Heads
        hidden_state = self.hidden_state_head(x)  # [B, 512]
        pose = self.pose_head(x)                  # [B, 6]
        road_transform = self.road_transform_head(x)  # [B, 6]

        # Concat all outputs: [B, 632]
        outputs = torch.cat([
            hidden_state,
            pose,
            road_transform,
            # ... その他
        ], dim=1)

        return outputs
```

#### 8.1.2 モデルの使用例

```python
# Vision Modelの初期化と推論
model = VisionModel()
model.load_state_dict(torch.load('vision_model.pth'))
model.eval()

# 推論
with torch.no_grad():
    input_imgs = torch.randn(1, 12, 128, 256)
    big_input_imgs = torch.randn(1, 12, 128, 256)

    outputs = model(input_imgs, big_input_imgs)

    # 出力のパース
    hidden_state = outputs[:, :512]      # Policy Modelへの入力
    pose = outputs[:, 512:518]           # 車両姿勢
    road_transform = outputs[:, 518:524] # 道路変換

    print(f'Hidden State: {hidden_state.shape}')  # [1, 512]
    print(f'Pose: {pose.shape}')                  # [1, 6]
```

### 8.2 Policy Model実装

#### 8.2.1 モデル構造

```python
import torch
import torch.nn as nn

class PolicyModel(nn.Module):
    def __init__(self):
        super().__init__()

        # Input Embedding
        self.feature_embed = nn.Linear(512, 256)
        self.desire_embed = nn.Linear(8, 32)
        self.params_embed = nn.Linear(4, 32)  # traffic_conv + lateral_params
        self.prev_curv_embed = nn.Linear(1, 16)

        # Temporal Encoder (Transformer-like)
        self.temporal_encoder = nn.Sequential(
            nn.Linear(256+32+16, 512),
            nn.ReLU(),
            nn.Linear(512, 512),
            nn.ReLU()
        )

        # Transformer Blocks
        encoder_layer = nn.TransformerEncoderLayer(
            d_model=512,
            nhead=8,
            dim_feedforward=2048,
            batch_first=True
        )
        self.transformer = nn.TransformerEncoder(encoder_layer, num_layers=3)

        # Output Heads
        self.curvature_head = nn.Sequential(
            nn.Linear(512, 256),
            nn.ReLU(),
            nn.Linear(256, 1),
            nn.Tanh()  # 曲率を[-1, 1]に制限
        )

        self.plan_head = nn.Sequential(
            nn.Linear(512, 1024),
            nn.ReLU(),
            nn.Linear(1024, 33*15)  # 33ポイント × 15次元
        )

        # ... その他のヘッド

    def forward(self, features_buffer, desire, traffic_convention,
                lateral_control_params, prev_desired_curv):
        # features_buffer: [B, 25, 512]
        # desire: [B, 25, 8]
        # traffic_convention: [B, 2]
        # lateral_control_params: [B, 2]
        # prev_desired_curv: [B, 25, 1]

        B, T = features_buffer.shape[:2]

        # Embedding
        feat_emb = self.feature_embed(features_buffer)       # [B,25,256]
        desire_emb = self.desire_embed(desire)               # [B,25,32]
        prev_curv_emb = self.prev_curv_embed(prev_desired_curv)  # [B,25,16]

        # Params Embedding（時系列全体で同じ値）
        params = torch.cat([traffic_convention, lateral_control_params], dim=1)
        params_emb = self.params_embed(params).unsqueeze(1).expand(-1, T, -1)

        # Concat all inputs
        x = torch.cat([feat_emb, desire_emb, prev_curv_emb], dim=2)
        # → [B, 25, 256+32+16]

        # Temporal Encoding
        x = self.temporal_encoder(x)  # [B, 25, 512]

        # Transformer
        x = self.transformer(x)  # [B, 25, 512]

        # 最後のタイムステップを使用
        x = x[:, -1, :]  # [B, 512]

        # Output Heads
        desired_curvature = self.curvature_head(x)  # [B, 1]
        plan = self.plan_head(x).view(B, 33, 15)    # [B, 33, 15]

        # Concat all outputs
        outputs = torch.cat([
            desired_curvature,
            plan.flatten(1),
            # ... その他
        ], dim=1)

        return outputs
```

#### 8.2.2 モデルの使用例

```python
# Policy Modelの初期化と推論
model = PolicyModel()
model.load_state_dict(torch.load('policy_model.pth'))
model.eval()

# 推論
with torch.no_grad():
    features_buffer = torch.randn(1, 25, 512)  # Vision特徴
    desire = torch.zeros(1, 25, 8)             # 運転意図
    desire[:, :, 0] = 1.0                      # 車線維持
    traffic_convention = torch.tensor([[1.0, 0.0]])  # 右側通行
    lateral_control_params = torch.tensor([[0.5, 0.5]])
    prev_desired_curv = torch.zeros(1, 25, 1)

    outputs = model(
        features_buffer,
        desire,
        traffic_convention,
        lateral_control_params,
        prev_desired_curv
    )

    # 出力のパース
    desired_curvature = outputs[:, :1]                # 目標曲率
    plan = outputs[:, 1:496].view(1, 33, 15)          # 軌道計画

    print(f'Desired Curvature: {desired_curvature.item():.6f}')
    print(f'Plan Shape: {plan.shape}')  # [1, 33, 15]
```

### 8.3 統合実行パイプライン

```python
import torch
import numpy as np

class OpenpilotInference:
    """Vision + Policy統合推論クラス"""

    def __init__(self, vision_path, policy_path):
        # モデルのロード
        self.vision_model = VisionModel()
        self.vision_model.load_state_dict(torch.load(vision_path))
        self.vision_model.eval()

        self.policy_model = PolicyModel()
        self.policy_model.load_state_dict(torch.load(policy_path))
        self.policy_model.eval()

        # 特徴バッファの初期化（100フレーム）
        self.full_features_buffer = np.zeros((1, 100, 512), dtype=np.float32)
        self.temporal_idxs = slice(-1-(4*24), None, 4)  # 4フレームごとに25個

        # 過去の制御出力（自己回帰用）
        self.prev_curvature_buffer = np.zeros((1, 25, 1), dtype=np.float32)

    def process_frame(self, input_imgs, big_input_imgs, desire,
                     traffic_convention, lateral_params):
        """
        1フレームの処理（Vision + Policy）

        Args:
            input_imgs: [1, 12, 128, 256] 通常カメラ
            big_input_imgs: [1, 12, 128, 256] 広角カメラ
            desire: [8] 運転意図
            traffic_convention: [2] 交通規則
            lateral_params: [2] 横制御パラメータ

        Returns:
            desired_curvature: float 目標曲率
            plan: [33, 15] 軌道計画
        """
        with torch.no_grad():
            # Step 1: Vision Model
            vision_output = self.vision_model(
                torch.from_numpy(input_imgs),
                torch.from_numpy(big_input_imgs)
            )

            hidden_state = vision_output[:, :512].numpy()  # [1, 512]

            # Step 2: 特徴バッファの更新
            self.full_features_buffer[0, :-1] = self.full_features_buffer[0, 1:]
            self.full_features_buffer[0, -1] = hidden_state[0]

            # Policy Model用にサンプリング
            features_buffer = self.full_features_buffer[:, self.temporal_idxs]  # [1, 25, 512]

            # Step 3: desire, paramsを25フレーム分に拡張
            desire_buffer = np.tile(desire, (1, 25, 1))  # [1, 25, 8]

            # Step 4: Policy Model
            policy_output = self.policy_model(
                torch.from_numpy(features_buffer),
                torch.from_numpy(desire_buffer),
                torch.from_numpy(traffic_convention.reshape(1, 2)),
                torch.from_numpy(lateral_params.reshape(1, 2)),
                torch.from_numpy(self.prev_curvature_buffer)
            )

            # Step 5: 出力のパース
            desired_curvature = policy_output[:, :1].numpy()  # [1, 1]
            plan = policy_output[:, 1:496].view(1, 33, 15).numpy()  # [1, 33, 15]

            # Step 6: 自己回帰用バッファの更新
            self.prev_curvature_buffer[0, :-1] = self.prev_curvature_buffer[0, 1:]
            self.prev_curvature_buffer[0, -1] = desired_curvature[0]

        return desired_curvature[0, 0], plan[0]

# 使用例
if __name__ == '__main__':
    # 初期化
    openpilot = OpenpilotInference(
        vision_path='weights/vision_model.pth',
        policy_path='weights/policy_model.pth'
    )

    # カメラ画像の準備（ダミーデータ）
    input_imgs = np.random.randn(1, 12, 128, 256).astype(np.float32)
    big_input_imgs = np.random.randn(1, 12, 128, 256).astype(np.float32)

    # コンテキストの準備
    desire = np.array([1, 0, 0, 0, 0, 0, 0, 0], dtype=np.float32)  # 車線維持
    traffic_convention = np.array([1.0, 0.0], dtype=np.float32)    # 右側通行
    lateral_params = np.array([0.5, 0.5], dtype=np.float32)

    # 推論実行
    curvature, plan = openpilot.process_frame(
        input_imgs, big_input_imgs,
        desire, traffic_convention, lateral_params
    )

    print(f'Desired Curvature: {curvature:.6f} rad/m')
    print(f'Plan Shape: {plan.shape}')  # (33, 15)
    print(f'Next 5 positions (x, y):')
    for i in range(5):
        print(f'  t={i*0.2:.1f}s: x={plan[i, 0]:.2f}m, y={plan[i, 1]:.2f}m')
```

---

## 9. 学習データとトレーニング

### 9.1 データセット構造

```
dataset/
├─ routes/
│   ├─ route_001/
│   │   ├─ camera.hevc     # カメラ動画
│   │   ├─ qlog.bz2        # センサーログ
│   │   └─ rlog.bz2        # 実行ログ
│   ├─ route_002/
│   └─ ...
└─ processed/
    ├─ images/
    │   ├─ 00001.jpg
    │   └─ ...
    └─ labels/
        ├─ 00001.npz       # ground truth
        └─ ...
```

### 9.2 Ground Truthの生成

```python
# tools/replay/process_route.py
def extract_ground_truth(route_path):
    """ログから教師データを抽出"""

    # カメラ画像の抽出
    images = extract_camera_images(route_path / 'camera.hevc')

    # 制御指令の抽出（human drivingまたは既存openpilot）
    logs = parse_logs(route_path / 'rlog.bz2')

    ground_truth = {
        # Vision Model用
        'images': images,              # [N, 12, 128, 256]
        'pose': logs['pose'],          # [N, 6]
        'lane_lines': logs['lane_lines'],  # [N, 4, 33, 2]

        # Policy Model用
        'desired_curvature': logs['steering_curvature'],  # [N, 1]
        'desired_accel': logs['acceleration'],            # [N, 1]
        'plan': logs['plan'],                             # [N, 33, 15]
    }

    return ground_truth
```

### 9.3 トレーニングループ

```python
import torch
import torch.nn as nn
import torch.optim as optim

def train_vision_model(model, dataloader, epochs=100):
    optimizer = optim.AdamW(model.parameters(), lr=1e-4)
    criterion = nn.MSELoss()

    for epoch in range(epochs):
        model.train()
        total_loss = 0

        for batch in dataloader:
            input_imgs = batch['input_imgs']
            big_input_imgs = batch['big_input_imgs']
            targets = batch['targets']  # ground truth

            # Forward
            outputs = model(input_imgs, big_input_imgs)

            # Loss計算（マルチタスク）
            loss_hidden_state = criterion(outputs[:, :512], targets['hidden_state'])
            loss_pose = criterion(outputs[:, 512:518], targets['pose'])
            loss_lane = criterion(outputs[:, 518:...], targets['lane_lines'])

            loss = loss_hidden_state + 0.1 * loss_pose + 0.1 * loss_lane

            # Backward
            optimizer.zero_grad()
            loss.backward()
            optimizer.step()

            total_loss += loss.item()

        print(f'Epoch {epoch}: Loss = {total_loss/len(dataloader):.4f}')

def train_policy_model(model, dataloader, epochs=100):
    optimizer = optim.AdamW(model.parameters(), lr=1e-4)
    criterion_curv = nn.MSELoss()
    criterion_plan = nn.MSELoss()

    for epoch in range(epochs):
        model.train()
        total_loss = 0

        for batch in dataloader:
            # 入力
            features_buffer = batch['features_buffer']
            desire = batch['desire']
            traffic_conv = batch['traffic_convention']
            lateral_params = batch['lateral_control_params']
            prev_curv = batch['prev_desired_curv']

            # Ground Truth
            gt_curvature = batch['gt_curvature']
            gt_plan = batch['gt_plan']

            # Forward
            outputs = model(features_buffer, desire, traffic_conv,
                          lateral_params, prev_curv)

            pred_curvature = outputs[:, :1]
            pred_plan = outputs[:, 1:496].view(-1, 33, 15)

            # Loss計算
            loss_curv = criterion_curv(pred_curvature, gt_curvature)
            loss_plan = criterion_plan(pred_plan, gt_plan)

            loss = loss_curv + 0.5 * loss_plan

            # Backward
            optimizer.zero_grad()
            loss.backward()
            optimizer.step()

            total_loss += loss.item()

        print(f'Epoch {epoch}: Loss = {total_loss/len(dataloader):.4f}')
```

---

## 10. モデル可視化

### 10.1 ONNXモデルの可視化画像

以下のPNG画像で詳細なネットワーク構造を確認できます：

- **Vision Model**: [driving_vision.onnx.png](../models/driving_vision.onnx.png)
  - 24000 × 636 ピクセルの詳細図
  - 全425層の接続関係を表示

- **Policy Model**: [driving_policy.onnx.png](../models/driving_policy.onnx.png)
  - 14280 × 2066 ピクセルの詳細図
  - 全174層の接続関係を表示

### 10.2 画像の見方

**レイヤー表示例**:
```
┌──────────────┐
│ Conv         │  ← レイヤータイプ
│ /stem/conv   │  ← レイヤー名
├──────────────┤
│ Input:       │
│ [1,24,128,256]│  ← 入力テンソル形状
├──────────────┤
│ Output:      │
│ [1,32,64,128]│  ← 出力テンソル形状
└──────────────┘
       ↓
   (接続線)
```

---

## 11. まとめ

### 11.1 モデルの特徴

**Vision Model**:
- ✅ 軽量CNN（425層、15M params）
- ✅ 2カメラ入力で広い視野
- ✅ 20Hz実行可能（モバイル）
- ✅ 512次元の抽象特徴を抽出

**Policy Model**:
- ✅ Transformer-based（174層、5M params）
- ✅ 5秒分の時系列処理
- ✅ マルチモーダル入力対応
- ✅ desired_curvature（横制御）とplan（縦制御）を出力

### 11.2 実装のポイント

1. **Vision Modelは認識に専念**: hidden_stateが最重要出力
2. **Policy Modelは制御に専念**: 時系列から最適な制御指令を生成
3. **分離設計の利点**: 認識と制御を独立に改善可能
4. **End-to-End学習**: 画像から制御指令まで微分可能

### 11.3 Jetson Nano移植時の注意

- **TensorRT変換**: ONNX → TensorRT で高速化
- **量子化**: FP16/INT8で推論速度向上
- **バッチサイズ1**: リアルタイム処理のため
- **メモリ最適化**: Jetson Nanoは4GBメモリ制約

---

**次のステップ**:
1. ONNXモデルを[Netron](https://netron.app/)で開いて詳細確認
2. PyTorch実装の完成とテスト
3. データセット準備と学習実行
4. TensorRTへの変換とJetson Nanoデプロイ

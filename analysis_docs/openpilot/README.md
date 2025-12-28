# openpilot システム分析ドキュメント

このディレクトリには、openpilot の主要プロセスとコンポーネントの詳細分析があります。

## ドキュメント構成

### 🚗 制御システム
- **[controlsd-details.md](controlsd-details.md)** - 車両制御の中核プロセス
  - 横制御（Lateral Control）: ステアリング制御、PID/LQR/INDI/TORQUE
  - 縦制御（Longitudinal Control）: 速度制御、ACC、車間距離
  - AIモデル出力の解釈と実車制御への変換

### 🔄 プロセス管理
- **[manager-details.md](manager-details.md)** - プロセスのライフサイクル管理
  - プロセス起動・監視・停止
  - onroad/offroad 状態管理
  - Params（設定管理システム）
  - プロセス間の依存関係

### 🧠 AI・機械学習
- **[ml-models-details.md](ml-models-details.md)** - Vision/Policy Model の詳細
  - Vision Model: カメラ画像から道路状況認識（632次元特徴量）
  - Policy Model: 特徴量から制御指令を生成
  - 入力/出力テンソルの詳細、推論処理
  
- **[onnx-analysis-results.md](onnx-analysis-results.md)** - ONNX モデル構造の自動分析結果
  - 4つのモデル（Standard/Big × Vision/Policy）の詳細データ
  - レイヤー構成、ノード数、入出力テンソル形状

### 🖥️ ユーザーインターフェース
- **[ui-details.md](ui-details.md)** - Qt/C++ UI システム
  - HomeWindow（運転画面）、SettingsWindow（設定）
  - リアルタイム描画（60 FPS）
  - カスタマイズ方法

## 関連ドキュメント

### コンポーネント別リポジトリ分析
これらのドキュメントは、Git submodule として管理されているコンポーネントのコード構造を分析しています：

- **[../msgq_analysis/](../msgq_analysis/)** - メッセージング・プロセス間通信
  - [runtime_details.md](../msgq_analysis/runtime_details.md) - 共有メモリとリングバッファの実行時動作
  - IPC API、ZMQ/msgq/fake 実装、VisionIPC
  
- **[../cereal_analysis/](../cereal_analysis/)** - メッセージスキーマ定義
  - Cap'n Proto による型安全なメッセージング
  - 全サービス一覧と構造
  
- **[../opendbc_analysis/](../opendbc_analysis/)** - CAN データベースと車両対応
  - DBC ファイル（96車種）、CANParser/Packer
  - セーフティモデル
  
- **[../panda/](../panda/)** - CAN インターフェースデバイス
  - [safety_detail.md](../panda/safety_detail.md) - セーフティチェックの詳細ロジック
  - ファームウェア、Python API、openpilot 統合

### 全体構造
- **[../openpilot_package_structure.md](../openpilot_package_structure.md)** - Python パッケージ構造とシンボリックリンク設計
- **[../msgq_repo_relationship.md](../msgq_repo_relationship.md)** - msgq リポジトリとシンボリックリンクの関係
- **[../opendbc_repo_relationship.md](../opendbc_repo_relationship.md)** - opendbc リポジトリとシンボリックリンクの関係

## ドキュメント間の関係

```
┌─────────────────────────────────────────────┐
│          openpilot システム全体             │
└─────────────────────────────────────────────┘
              │
              ├─ manager-details.md (プロセス管理)
              │   └─ 以下のプロセスを起動・監視
              │
              ├─ controlsd-details.md (制御)
              │   ├─ ml-models-details.md (AI出力を利用)
              │   └─ ../panda/ (CANコマンド送信)
              │
              ├─ ui-details.md (UI)
              │   └─ controlsd の状態を表示
              │
              └─ ../msgq_analysis/ (通信基盤)
                  └─ 全プロセス間の通信を支える
```

## 分析の視点

各ドキュメントは異なる視点から openpilot を分析しています：

| 視点 | ドキュメント | 内容 |
|------|------------|------|
| **プロセス実行** | manager-details, controlsd-details | 実行時の動作、制御ロジック |
| **コード構造** | msgq_analysis, cereal_analysis | リポジトリ構成、API設計 |
| **データフロー** | ml-models-details | AI推論の入出力 |
| **ハードウェア連携** | panda/ | 車両ECUとの物理インターフェース |
| **ユーザー体験** | ui-details | 画面表示とカスタマイズ |

重複を避けるため、各ドキュメントは特定の側面に焦点を当てています。

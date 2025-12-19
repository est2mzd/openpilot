# openpilot プログラム実行フロー

このドキュメントでは、openpilotの起動から車両制御までの全体フローを説明します。

> **📖 詳細ドキュメント**
> - [Manager詳細](details/manager-details.md) - プロセス管理の実装
> - [msgq詳細](details/msgq-details.md) - プロセス間通信の仕組み
> - [車両制御詳細](details/vehicle-control-details.md) - 縦制御・横制御のアルゴリズム
> - [MLモデル詳細](details/ml-models-details.md) - Vision/Policy Modelの入出力

## 1. 起動の流れ

```
launch_openpilot.sh
    ↓
launch_chffrplus.sh
    ↓
system/manager/manager.py
    ↓
各種プロセスの起動
```

### 起動スクリプト

1. **launch_openpilot.sh** - エントリーポイント
   - `launch_chffrplus.sh`を呼び出すだけのラッパー

2. **launch_chffrplus.sh** - メイン起動スクリプト
   - オーバーレイアップデートのチェック
   - 環境変数の設定（`PYTHONPATH`など）
   - ハードウェア固有の初期化（AGNOS）
   - tmuxセッションの作成
   - `system/manager/manager.py`の起動

## 2. Manager（マネージャー）

**ファイル**: [system/manager/manager.py](../system/manager/manager.py)

Managerはopenpilotの**中央管理プロセス**で、すべてのサブプロセスの起動・監視・停止を担当します。

### 2.1 主要な役割

1. **初期化処理**
   - Paramsの初期化（設定管理システム）
   - デバイス登録（dongle_id取得）
   - ログシステムの初期化（Sentry、cloudlog）

2. **メインループ（100Hz）**
   - 走行状態の監視（`deviceState.started`）
   - 必要なプロセスの起動・維持
   - プロセス状態の配信（`managerState`）

3. **クリーンアップ**
   - 全プロセスの安全な停止（2段階停止）

> **詳細**: [Manager詳細ドキュメント](details/manager-details.md)を参照

## 3. プロセス管理

**ファイル**: [system/manager/process_config.py](../system/manager/process_config.py)

### 3.1 プロセスタイプ

| クラス名 | 対象 | 実行方法 |
|---------|------|---------|
| **PythonProcess** | Pythonスクリプト | `python -m モジュール名` |
| **NativeProcess** | ネイティブバイナリ（C/C++） | `./バイナリ名` |
| **DaemonProcess** | バックグラウンド常駐 | 特別な起動・PID監視処理つき |

### 3.2 主要プロセス一覧

#### 制御系
- **controlsd**: 車両制御（ハンドル・アクセル・ブレーキ）
- **plannerd**: 経路計画（MPC）
- **radard**: レーダー処理
- **card**: 車両インターフェース

#### 認識系
- **modeld**: AIモデル（Vision + Policy Model）
- **dmonitoringmodeld**: ドライバー監視
- **camerad**: カメラ入力

#### 位置推定系
- **locationd**: 位置推定
- **calibrationd**: カメラキャリブレーション
- **paramsd**: 車両パラメータ推定

#### システム系
- **pandad**: Panda（CANデバイス）通信
- **ui**: ユーザーインターフェース
- **manage_athenad**: Athena通信管理

## 4. プロセス間通信（msgq）

openpilotは共有メモリベースのメッセージキューシステムを使用：

```
┌──────────┐   publish    ┌─────────┐   subscribe   ┌──────────┐
│ modeld   │──────────────>│  msgq   │<──────────────│controlsd │
└──────────┘              │(/dev/shm)│              └──────────┘
                          └─────────┘
```

### 4.1 特徴

- **共有メモリ**（`/dev/shm`）で高速通信
- **リングバッファ**構造
- **Cap'n Proto**フォーマット（高速シリアライゼーション）

### 4.2 使用例

```python
# Publisher側
pm = messaging.PubMaster(['controlsState'])
msg = messaging.new_message('controlsState')
pm.send('controlsState', msg)

# Subscriber側
sm = messaging.SubMaster(['controlsState'])
sm.update()
```

> **詳細**: [msgq詳細ドキュメント](details/msgq-details.md)を参照

## 5. 実行フロー図

### 5.1 起動フロー

```
[launch_openpilot.sh]
         │
         ▼
[launch_chffrplus.sh]
         │
         ├─ オーバーレイアップデート確認
         ├─ 環境変数設定
         ├─ ハードウェア初期化
         │
         ▼
[system/manager/manager.py]
         │
         ├─ manager_init()
         │    ├─ Params初期化
         │    ├─ デバイス登録
         │    └─ プロセス準備
         │
         └─ manager_thread()
              └─ メインループ（100Hz）
                   ├─ 走行状態監視
                   ├─ プロセス管理
                   └─ 状態配信
```

### 5.2 走行時のプロセスフロー

```
[センサー入力]
      │
      ├─ camerad ────────┐
      ├─ sensord ────────┤
      ├─ pandad (CAN) ───┤
      └─ locationd (GPS)─┤
                         │
                         ▼
                   [認識・推定]
                         │
      ┌──────────────────┼──────────────────┐
      │                  │                  │
      ▼                  ▼                  ▼
   modeld          calibrationd       paramsd
 (物体認識)      (キャリブレーション)  (車両パラメータ)
      │                  │                  │
      └──────────────────┼──────────────────┘
                         │
                         ▼
                    [経路計画]
                         │
                    plannerd
                         │
                         ▼
                    [車両制御]
                         │
                    controlsd
                         │
                         ▼
                   [CAN出力]
                         │
                    pandad → 車両
```

## 6. 車両制御の全体像

openpilotの車両制御は**縦制御（Longitudinal）**と**横制御（Lateral）**に分かれます。

### 6.1 縦制御（加速・減速）

```
┌─────────────────────────────────┐
│ 上位層: MPC (plannerd)          │
│ - 最適な加速度軌道を計画         │
│ - 出力: aTarget                 │
└──────────────┬──────────────────┘
               │
               ▼
┌─────────────────────────────────┐
│ 下位層: PID (controlsd)         │
│ - aTargetに追従する制御          │
│ - 出力: accel                   │
└─────────────────────────────────┘
```

**特徴**:
- **2段階構造**（計画 + 追従）
- **MPC**: 将来予測と最適化
- **PID**: 実時間での誤差補正

### 6.2 横制御（ステアリング）

```
カメラ画像 → Vision Model → Policy Model → desired_curvature
                                                  ↓
                                              曲率 → 横加速度 → トルク
                                                  ↓
                                              PID制御 → 車両EPS
```

**特徴**:
- **AI主導**（目標曲率はML modelが決定）
- **物理変換**（曲率→横加速度→トルク）
- **3種類の制御方式**（Torque/Angle/PID）

> **詳細**: [車両制御詳細ドキュメント](details/vehicle-control-details.md)を参照

## 7. MLモデルの役割

openpilotは**Vision Model**と**Policy Model**の2段階で動作：

### 7.1 Vision Model（認識層）

```
入力: カメラ画像（8フレーム履歴）
   ↓
処理: CNN特徴抽出
   ↓
出力: hidden_state[512次元] + 車線・道路エッジ等
```

### 7.2 Policy Model（制御層）

```
入力: hidden_state[25フレーム] + desire + 制御パラメータ
   ↓
処理: LSTM/Transformer時系列処理
   ↓
出力: desired_curvature（横制御） + plan（縦制御）
```

**重要ポイント**:
- **横制御の目標曲率は完全にAIが決定**
- 従来の制御理論（PID）はAI出力に追従するだけ
- Vision/Policy分離により認識と制御を独立に改善可能

> **詳細**: [MLモデル詳細ドキュメント](details/ml-models-details.md)を参照

## 8. データフロー（制御ループ）

```
1. pandad: CAN読み取り → carState配信
2. modeld: カメラ画像処理 → modelV2配信
3. plannerd: modelV2 + carState → lateralPlan, longitudinalPlan配信
4. controlsd: *Plan + carState → controlsState, carControl配信
5. pandad: carControl受信 → CANコマンド送信
```

**実行周期**:
- **カメラ/モデル**: 20Hz（50ms）
- **制御ループ**: 100Hz（10ms）

## 9. 重要なファイル・ディレクトリ

### 9.1 コア実行環境

| パス | 説明 |
|------|------|
| [launch_openpilot.sh](../launch_openpilot.sh) | エントリーポイント |
| [system/manager/manager.py](../system/manager/manager.py) | マネージャーメイン |
| [system/manager/process_config.py](../system/manager/process_config.py) | プロセス定義 |

### 9.2 プロセス間通信

| パス | 説明 |
|------|------|
| [msgq/](../msgq/) | メッセージキュー実装 |
| [cereal/](../cereal/) | メッセージ定義（Cap'n Proto） |

### 9.3 主要制御ロジック

| パス | 説明 |
|------|------|
| [selfdrive/controls/controlsd.py](../selfdrive/controls/controlsd.py) | 車両制御メイン |
| [selfdrive/controls/plannerd.py](../selfdrive/controls/plannerd.py) | 経路計画 |
| [selfdrive/modeld/modeld.py](../selfdrive/modeld/modeld.py) | AIモデル実行 |
| [selfdrive/car/](../selfdrive/car/) | 車両インターフェース |

### 9.4 設定・ユーティリティ

| パス | 説明 |
|------|------|
| [common/params.py](../common/params.py) | 設定管理システム |
| [system/hardware/](../system/hardware/) | ハードウェア抽象化 |

## 10. 開発時のヒント

### 10.1 プロセスのデバッグ

特定のプロセスを無効化：
```bash
export BLOCK="modeld,plannerd"
./launch_openpilot.sh
```

Pandaなしで起動（シミュレーション）：
```bash
export NOBOARD=1
./launch_openpilot.sh
```

### 10.2 ログの確認

```bash
# マネージャーのログ
tail -f /tmp/launch_log

# 個別プロセスのログ
# /data/community/crashes/ や /tmp/ に出力
```

### 10.3 プロセスの追加

1. `system/manager/process_config.py`に追加
2. モジュールを実装（PubMaster/SubMasterで通信）

## 11. Jetson Nano移植のポイント

### 11.1 ハードウェア依存部分

| コンポーネント | Comma3X | Jetson Nano代替案 |
|--------------|---------|------------------|
| **CAN通信** | Panda | SocketCAN + USB-CANアダプタ |
| **カメラ** | 専用モジュール | USB/CSIカメラ + OpenCV |
| **センサー** | 内蔵IMU/GPS | 外部IMU/GPSモジュール |

### 11.2 ソフトウェア依存部分

**必須プロセス**:
- pandad → SocketCANドライバーに置き換え
- card（車種固有CAN解析）
- camerad → USB/CSIカメラ対応に書き換え
- modeld（AI推論、TensorRT最適化）
- plannerd
- controlsd

**省略可能**:
- loggerd、ui、manage_athenad、uploader等

### 11.3 移植ステップ

1. **SocketCANドライバー実装**
2. **カメラ入力実装**
3. **モデル推論最適化**（TensorRT）
4. **シミュレーション検証**
5. **実車テスト**

> **詳細**: [車両制御詳細ドキュメント](details/vehicle-control-details.md)の「Jetson Nano移植」セクションを参照

## 12. まとめ

openpilotの実行フローは以下のように整理できます：

1. **起動**: シェルスクリプト → Manager起動
2. **初期化**: デバイス登録、ログ設定、プロセス準備
3. **メインループ**: 走行状態監視 → プロセス管理
4. **プロセス間通信**: msgq（共有メモリベースPub/Sub）
5. **制御ループ**: センサー入力 → 認識（AI） → 計画（MPC） → 制御（PID） → 出力（CAN）

この構造により、openpilotは高い拡張性と保守性を保ちながら、リアルタイム性が求められる自動運転システムを実現しています。

**Jetson Nano移植の鍵**: 制御ロジックは汎用的であり、ハードウェアインターフェース部分のみを置き換えれば動作する設計になっています。

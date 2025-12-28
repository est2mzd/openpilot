# MetaDrive - 自動運転シミュレーター

## 概要

**MetaDrive**は、AI・自動運転研究のためのオープンソース運転シミュレーターです。

- **コード量**: 約17,000行（Python）
- **リポジトリ**: https://github.com/est2mzd/metadrive.git
- **特徴**: 軽量、リアル、無限シーン生成
- **用途**: openpilotのシミュレーション環境

---

## アーキテクチャ

```
┌──────────────────────────────────────────────────────┐
│              MetaDrive Simulator                     │
├──────────────────────────────────────────────────────┤
│                                                      │
│  ┌─────────────────────────────────────────────┐    │
│  │  Environments (envs/)                        │    │
│  │  - MetaDriveEnv: 基本環境                     │    │
│  │  - SafeMetaDriveEnv: 安全運転環境             │    │
│  │  - MultiAgent: マルチエージェント              │    │
│  │  - ScenarioEnv: 実世界データ                  │    │
│  └───────────────┬─────────────────────────────┘    │
│                  │                                   │
│  ┌───────────────▼─────────────────────────────┐    │
│  │  Engine (engine/)                           │    │
│  │  - BaseEngine: Panda3Dベース                 │    │
│  │  - Physics: 物理シミュレーション               │    │
│  │  - Rendering: 3D/トップダウン描画             │    │
│  │  - Sensors: カメラ、LiDAR、検出器             │    │
│  └───────────────┬─────────────────────────────┘    │
│                  │                                   │
│  ┌───────────────▼─────────────────────────────┐    │
│  │  Components (component/)                    │    │
│  │  - Vehicle: 車両モデル                        │    │
│  │  - Map: 道路マップ生成                        │    │
│  │  - Traffic: 交通車両管理                      │    │
│  │  - Sensors: センサー実装                      │    │
│  └───────────────┬─────────────────────────────┘    │
│                  │                                   │
│  ┌───────────────▼─────────────────────────────┐    │
│  │  Scenario (scenario/)                       │    │
│  │  - nuScenes: nuScenesデータセット             │    │
│  │  - Waymo: Waymoデータセット                   │    │
│  │  - 実世界ログのリプレイ                        │    │
│  └─────────────────────────────────────────────┘    │
│                                                      │
└──────────────────────────────────────────────────────┘
```

---

## 主要コンポーネント

### 1. Environments - 環境定義

**ディレクトリ**: `metadrive/envs/`

**主要環境**:

| 環境 | 説明 |
|------|------|
| `MetaDriveEnv` | 基本的な運転環境（ランダムマップ生成） |
| `SafeMetaDriveEnv` | 安全運転課題（障害物、コスト関数） |
| `ScenarioEnv` | 実世界データ（nuScenes/Waymo） |
| `MultiAgentMetaDrive` | マルチエージェント環境 |
| `TopDownEnv` | トップダウン視点環境 |

**詳細**: [environments.md](environments.md)

---

### 2. Engine - シミュレーションエンジン

**ディレクトリ**: `metadrive/engine/`

**機能**:

| コンポーネント | 説明 |
|---------------|------|
| `BaseEngine` | Panda3Dベースのエンジン |
| Physics | 物理シミュレーション（BulletPhysics） |
| Rendering | 3D描画、トップダウン描画 |
| AssetLoader | 3Dモデル、テクスチャ読み込み |

**詳細**: [engine.md](engine.md)

---

### 3. Scenario - シナリオシステム

**ディレクトリ**: `metadrive/scenario/`

**対応データセット**:
- **nuScenes**: Boston、Singaporeの実世界データ
- **Waymo**: Waymo Open Datasetのログ

**機能**:
- 実世界ログのリプレイ
- トラフィック再現
- センサーデータ再現

**詳細**: [scenarios.md](scenarios.md)

---

## 主要機能

### 1. 無限シーン生成

**Compositional（組み合わせ可能）**:

```python
from metadrive import MetaDriveEnv

# 異なるマップ生成
env = MetaDriveEnv(config={
    "map": 3,  # ブロック数
    "traffic_density": 0.1,
    "num_scenarios": 1000  # 1000シナリオ生成
})
```

**マップタイプ**:
- Straight（直線）
- Curve（カーブ）
- Intersection（交差点）
- Roundabout（ラウンドアバウト）
- Tollgate（料金所）

---

### 2. 軽量・高速

**パフォーマンス**:
- **FPS**: 1000+ FPS（標準PC）
- **起動時間**: < 1秒
- **メモリ**: < 500MB

**最適化**:
- Panda3D（軽量3Dエンジン）
- オンデマンドレンダリング
- 効率的な物理計算

---

### 3. リアルな物理シミュレーション

**BulletPhysics**:
- 正確な車両ダイナミクス
- 衝突検出
- タイヤモデル

**センサー**:

| センサー | 説明 |
|---------|------|
| LiDAR | 点群（240点、範囲50m） |
| RGB Camera | 1Dビュー画像（84×84） |
| Depth Camera | 深度画像 |
| Semantic Camera | セマンティックセグメンテーション |
| Lane Detector | 車線検出器 |
| Side Detector | 側方検出器 |

---

## openpilotとの統合

### openpilot側のブリッジ

**場所**: `openpilot/tools/sim/bridge/metadrive/`

```
tools/sim/bridge/metadrive/
├── metadrive_bridge.py     # メインブリッジ
├── metadrive_world.py      # ワールド制御
├── metadrive_process.py    # プロセス管理
└── metadrive_common.py     # 共通定義
```

**動作**:
1. MetaDriveプロセス起動
2. openpilot controlsdからアクション受信
3. MetaDriveで実行
4. センサーデータをopenpilotに送信

---

### 使い方（openpilot sim）

```bash
# MetaDriveシミュレーション起動
cd openpilot/tools/sim
./launch_openpilot.sh --sim metadrive

# またはdirect
python bridge/metadrive/metadrive_bridge.py
```

**設定**:
```python
# metadrive_world.py
METADRIVE_CONFIG = {
    "use_render": True,       # 3D描画
    "manual_control": False,  # openpilot制御
    "traffic_density": 0.1,   # トラフィック密度
    "map": "SSSSSS",          # マップ構成
}
```

---

## 環境の種類

### MetaDriveEnv（基本）

**特徴**:
- ランダムマップ生成
- トラフィック車両
- ゴール到達タスク

```python
from metadrive import MetaDriveEnv

env = MetaDriveEnv(config={
    "num_scenarios": 100,
    "traffic_density": 0.1,
    "map": 3,  # ブロック数
    "horizon": 1000,  # 最大ステップ数
})

obs, info = env.reset()
for _ in range(1000):
    obs, reward, terminated, truncated, info = env.step(env.action_space.sample())
    if terminated or truncated:
        break
```

---

### SafeMetaDriveEnv（安全運転）

**特徴**:
- コスト関数（安全性評価）
- 複雑な障害物
- 安全運転学習

```python
from metadrive import SafeMetaDriveEnv

env = SafeMetaDriveEnv(config={
    "accident_prob": 0.5,  # 事故確率
    "crash_vehicle_cost": 1.0,
    "out_of_road_cost": 1.0,
})
```

---

### ScenarioEnv（実世界データ）

**特徴**:
- nuScenes/Waymoログ
- 実トラフィック再現
- リアルな挙動

```python
from metadrive import ScenarioEnv

env = ScenarioEnv(config={
    "data_directory": "/path/to/nuscenes",
    "num_scenarios": 1000,
    "reactive_traffic": True,  # トラフィック反応
})
```

---

### MultiAgent（マルチエージェント）

**特徴**:
- 複数車両同時制御
- 協調・競争タスク
- MARL研究

```python
from metadrive import MultiAgentRoundaboutEnv

env = MultiAgentRoundaboutEnv(config={
    "num_agents": 4,
})

obs, info = env.reset()
# obs: {agent_id: observation}
actions = {agent_id: env.action_space.sample() for agent_id in obs.keys()}
obs, rewards, terminateds, truncateds, infos = env.step(actions)
```

---

## 報酬・終了条件

### 報酬スキーム

```python
config = {
    # 成功報酬
    "success_reward": 10.0,
    
    # ペナルティ
    "out_of_road_penalty": 5.0,
    "crash_vehicle_penalty": 5.0,
    "crash_object_penalty": 5.0,
    
    # 継続報酬
    "driving_reward": 1.0,
    "speed_reward": 0.1,
}
```

**報酬計算**:
```python
reward = 0
reward += driving_reward * step_count
reward += speed_reward * velocity
if success:
    reward += success_reward
if crash:
    reward -= crash_penalty
```

---

### 終了条件

```python
config = {
    "out_of_road_done": True,      # 道路外で終了
    "crash_vehicle_done": True,    # 車両衝突で終了
    "crash_object_done": True,     # オブジェクト衝突で終了
    "horizon": 1000,               # 最大ステップ数
}
```

---

## 観測空間

### LiDAR観測（デフォルト）

```python
config = {
    "image_observation": False,  # LiDAR使用
}

# 観測: (240,) 各方向の距離
# 240点のLiDARスキャン、範囲50m
```

### 画像観測

```python
config = {
    "image_observation": True,
    "norm_pixel": True,  # 0-1正規化
    "stack_size": 3,     # 3フレームスタック
}

# 観測: (84, 84, 9) - RGB × 3フレーム
```

---

## アクション空間

### 連続アクション（デフォルト）

```python
# action: [steering, throttle/brake]
# steering: -1.0 ~ 1.0（左～右）
# throttle/brake: -1.0 ~ 1.0（ブレーキ～スロットル）

action = [0.5, 0.3]  # 右に軽くステア、軽くアクセル
```

### 離散アクション

```python
config = {
    "discrete_action": True,
    "discrete_steering_dim": 5,   # ステア: 5段階
    "discrete_throttle_dim": 5,   # スロットル: 5段階
}

# action: 0~24（5×5グリッド）
```

---

## インストール

### 基本インストール

```bash
# MetaDriveクローン
cd /home/takuya/work/comma/openpilot
git submodule update --init metadrive

# インストール
cd metadrive
pip install -e .
```

### 依存関係

```
- panda3d >= 1.10.13
- gymnasium >= 0.28.0
- numpy
- opencv-python
- Pillow
```

---

## 例：基本的な使い方

### 手動運転

```bash
python -m metadrive.examples.drive_in_single_agent_env
```

**操作**:
- `W/S`: 前進/後退
- `A/D`: 左/右ステア
- `Q`: 終了

---

### RL訓練例

```python
from metadrive import MetaDriveEnv
import numpy as np

env = MetaDriveEnv(config={
    "num_scenarios": 1000,
    "traffic_density": 0.1,
})

for episode in range(100):
    obs, info = env.reset()
    episode_reward = 0
    
    for step in range(1000):
        # ランダムポリシー
        action = env.action_space.sample()
        
        obs, reward, terminated, truncated, info = env.step(action)
        episode_reward += reward
        
        if terminated or truncated:
            break
    
    print(f"Episode {episode}: Reward = {episode_reward:.2f}")

env.close()
```

---

## パフォーマンス

### ベンチマーク

| 設定 | FPS | 用途 |
|------|-----|------|
| レンダリングなし | 1000+ | RL訓練 |
| トップダウン描画 | 500 | デバッグ |
| 3D描画 | 60 | デモ、手動運転 |

### 最適化Tips

```python
config = {
    # レンダリング無効化
    "use_render": False,
    
    # トラフィック削減
    "traffic_density": 0.05,
    
    # センサー無効化
    "show_lidar": False,
    "show_lane_line_detector": False,
}
```

---

## ベストプラクティス

### 1. シード固定（再現性）

```python
env = MetaDriveEnv(config={
    "start_seed": 0,
    "num_scenarios": 100,
})

# シナリオ0～99が常に同じ
```

### 2. マップ構成指定

```python
# 数値: ブロック数
config = {"map": 3}

# 文字列: ブロックシーケンス
# S: Straight, C: Curve, T: T-intersection, X: 4-way
config = {"map": "SCXTCS"}
```

### 3. トラフィック制御

```python
config = {
    "traffic_density": 0.1,        # 密度
    "traffic_mode": "Trigger",     # Trigger/Respawn
    "random_traffic": False,       # 決定論的
}
```

---

## トラブルシューティング

### 問題1: Panda3Dインポートエラー

**症状**:
```
ModuleNotFoundError: No module named 'panda3d'
```

**解決策**:
```bash
pip install panda3d>=1.10.13
```

---

### 問題2: 3D描画が表示されない

**症状**: ウィンドウが開かない

**解決策**:
```python
config = {
    "use_render": True,  # レンダリング有効化
    "manual_control": True,  # 手動制御
}
```

---

### 問題3: FPSが低い

**原因**: 3D描画のオーバーヘッド

**解決策**:
```python
config = {
    "use_render": False,  # 訓練時は無効化
}
```

---

## まとめ

MetaDriveのポイント:

1. **軽量**: 1000+ FPS、簡単インストール
2. **リアル**: BulletPhysics、センサー再現
3. **無限シーン**: ランダムマップ生成
4. **実世界対応**: nuScenes/Waymoデータ
5. **openpilot統合**: シミュレーション環境

---

## 詳細ドキュメント

- **[environments.md](environments.md)**: 環境定義の詳細（MetaDriveEnv、SafeMetaDriveEnv、ScenarioEnv、MultiAgent）
- **[engine.md](engine.md)**: シミュレーションエンジンの詳細（Panda3D、Physics、Rendering、Sensors）
- **[scenarios.md](scenarios.md)**: シナリオシステムの詳細（nuScenes、Waymo、実世界データ再現）

---

## 関連リンク

- **公式サイト**: https://metadrive-simulator.readthedocs.io
- **GitHub**: https://github.com/metadriverse/metadrive
- **論文**: https://arxiv.org/pdf/2109.12674.pdf
- **デモ**: https://www.youtube.com/embed/3ziJPqC_-T4

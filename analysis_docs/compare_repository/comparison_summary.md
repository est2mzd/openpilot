# openpilot比較レポート：ichiroブランチ vs 本家

## エグゼクティブサマリー

本レポートは、ichiroブランチ（Prius縦制御カスタム版）と本家openpilotの主要な違いを包括的に比較したものです。

### 対象リポジトリ
- **ichiroブランチ**: `/home/takuya/work/comma/other_repo/ichiro`
  - Toyota Priusの縦制御をopenpilot制御に切り替えたカスタムブランチ
  - ベース: openpilot v0.10.3～v0.10.4
  
- **本家openpilot**: `/home/takuya/work/comma/openpilot`
  - 公式openpilotリポジトリの最新版
  - 継続的に更新中

---

## 1. 縦制御（Longitudinal Control）の違い

### 概要

ichiroブランチと本家openpilotでは、**縦制御の基本アーキテクチャが大きく異なります**。

| 項目 | 本家openpilot | ichiro (Prius特化) |
|------|---------------|-------------------|
| **制御方式** | 加速度ターゲット（a_target）ベース | 速度ターゲット（v_target）ベース |
| **PIDフィードバック** | 加速度誤差 (a_target - a_ego) | 速度誤差 (v_pid - v_ego) + 加速度FF |
| **状態数** | 4状態 (off/stopping/starting/pid) | 3状態 (off/stopping/pid) |
| **アクチュエータ遅延補償** | なし | あり（upper/lower bound計算） |
| **デッドゾーン** | なし | あり（速度依存、チャタリング防止） |
| **オーバーシュート防止** | なし | あり（低速停止時） |
| **積分器凍結** | なし | あり（停止時の巻き上がり防止） |

### 主要な技術的違い

#### 本家: シンプル・モダン
```python
# 加速度誤差ベースの直接制御
error = a_target - CS.aEgo
output_accel = self.pid.update(error, speed=CS.vEgo, feedforward=a_target)
```

#### ichiro: 高度・カスタマイズ
```python
# アクチュエータ遅延補償
v_target_lower = interp(CP.longitudinalActuatorDelayLowerBound, T_IDXS, speeds)
a_target = 2 * (v_target - speeds[0]) / delay - long_plan.accels[0]

# 速度誤差 + FF制御 + 各種補償
output_accel = self.pid.update(
    self.v_pid, CS.vEgo,
    deadzone=deadzone,
    feedforward=a_target,
    freeze_integrator=freeze_integrator
)

# オーバーシュート防止
if prevent_overshoot:
    output_accel = min(output_accel, 0.0)
```

### Priusでのメリット
- アクチュエータ応答遅延の補償により、滑らかな加減速
- デッドゾーンによる微小振動の抑制
- 低速時のオーバーシュート防止で自然な停止感

**詳細**: [longitudinal_control_detail.md](longitudinal_control_detail.md)

---

## 2. UI（ユーザーインターフェース）の違い

### 概要

UIの実装方法とカスタマイズレベルに大きな違いがあります。

| 項目 | 本家openpilot | ichiro |
|------|---------------|--------|
| **ファイル構造** | モジュール化（onroad/下に分離） | モノリシック（onroad.cc 1ファイル） |
| **Alert更新** | updateState()で自動取得 | updateAlert()で外部から受信 |
| **速度調整ボタン** | なし | あり（⇧↓ボタン） |
| **追加情報表示** | なし | あり（standstill状態等） |
| **ナビゲーション** | 標準機能 | 独自navd実装 |
| **Python UI** | あり（レイリブ対応） | なし（C++/Qtのみ） |

### 主要な機能的違い

#### 1. 本家: モジュール化された構造
```
selfdrive/ui/qt/onroad/
├── alerts.cc/.h          # アラート専用
├── annotated_camera.cc/.h # カメラビュー専用
├── hud.cc/.h             # HUD専用
└── onroad_home.cc/.h     # 統合
```
保守性が高く、機能追加が容易。

#### 2. ichiro: 単一ファイル + カスタム機能
```
selfdrive/ui/qt/
└── onroad.cc/.h          # すべてを統合
    + 速度調整ボタン（⇧↓）
    + standstill状態表示
    + デバッグ情報表示
```
カスタマイズ優先、実用性重視。

### ユーザー体験の違い
- **本家**: 統一されたUI、シンプル
- **ichiro**: 走行中の直接操作（速度調整）、詳細情報表示

**詳細**: [ui_changes_detail.md](ui_changes_detail.md)

---

## 3. その他の主要な変更

### アーキテクチャと開発方針

| 項目 | 本家openpilot | ichiro |
|------|---------------|--------|
| **ベースバージョン** | 最新（継続更新） | v0.10.3～v0.10.4固定 |
| **対象車両** | 汎用（200+車種） | Prius特化 |
| **更新頻度** | 高頻度（毎日） | 低頻度（安定重視） |
| **Python管理** | pyproject.toml (PEP 518) | Pipfile (古い方式) |
| **CI/CD** | GitHub Actions完備 | 手動テスト中心 |
| **ドキュメント** | 充実 | 基本のみ |

### 独自機能（ichiroブランチ）

1. **ナビゲーション**: `selfdrive/ui/navd/` ディレクトリ（独自実装）
2. **追加UI**: `mui.cc`（独自）
3. **ダッシュカムモード**: `continue_dashcam.sh`
4. **WiFi UIカスタマイズ**: スクロールパディング調整等
5. **デバッグ表示**: standstill状態、内部変数の可視化

### コミット履歴からの特徴（ichiro）
```
* d3ab6d34b c4のEventName.resumeRequiredもバックをノーマルに。
* 1a59fee52 onroadに⇧、↓ボタンを追加。
* 72bc427da 追加情報表示
* 611f4a110 自分の車ではstandstillが機械スイッチなので...
```

実際の使用経験に基づいた細かいカスタマイズが多数。

**詳細**: [other_changes_detail.md](other_changes_detail.md)

---

## 比較マトリックス

### 開発哲学

```
本家openpilot:
- 汎用性重視
- 多車種対応
- 継続的な改善
- モダンなアーキテクチャ
- コミュニティ主導

ichiro:
- 実用性重視
- Prius特化
- 安定性優先
- カスタマイズ優先
- 個人最適化
```

### 技術スタック

| レイヤー | 本家 | ichiro |
|---------|------|--------|
| 言語 | Python 3.11+ / C++ | Python 3.8-3.10 / C++ |
| UI | Qt5 + Python (raylib) | Qt5のみ |
| 縦制御 | 加速度ベース（モダン） | 速度ベース（古典+改良） |
| パッケージ | pyproject.toml | Pipfile |
| テスト | 自動（GitHub Actions） | 手動 |

### 適用シーン

#### 本家openpilotが向いている場合
- 多種多様な車両で使用
- 最新機能を常に使いたい
- コミュニティサポートが欲しい
- モダンな開発環境を好む

#### ichiroブランチが向いている場合
- **Toyota Priusでの使用**
- より細かい制御調整が欲しい
- 安定した動作を優先
- 走行中の直接操作（速度調整ボタン）が欲しい
- デバッグ情報を表示したい

---

## 技術的ハイライト

### 1. 縦制御の洗練度（ichiro）

ichiroブランチの縦制御は、以下の点で本家より洗練されています：

```python
# アクチュエータ遅延の明示的補償
v_target_lower = interp(CP.longitudinalActuatorDelayLowerBound, T_IDXS, speeds)
a_target_lower = 2 * (v_target_lower - speeds[0]) / delay - accels[0]

# デッドゾーンによるチャタリング防止
deadzone = interp(CS.vEgo, CP.longitudinalTuning.deadzoneBP, deadzoneV)

# 積分器凍結によるワインドアップ防止
freeze_integrator = prevent_overshoot

# オーバーシュート防止
if prevent_overshoot and CS.vEgo < 1.5:
    output_accel = min(output_accel, 0.0)
```

これらは実車での経験に基づいた実用的な改良です。

### 2. UIのカスタマイズ性（ichiro）

走行中に速度を調整できるボタンは、実用性の大きな向上：

```cpp
// onroad.cc
// ⇧ボタン: 速度アップ
// ↓ボタン: 速度ダウン
// 判定範囲の調整、被り修正も実施済み
```

### 3. モジュール化（本家）

本家の新しいモジュール構造は、今後の拡張性が高い：

```
onroad/
├── alerts.cc      # アラート専用
├── hud.cc         # HUD専用
└── camera.cc      # カメラビュー専用
```

各コンポーネントが独立しており、バグ修正や機能追加が容易。

---

## 推奨事項

### ichiroブランチを使い続ける場合
1. ✅ 現状の安定性を享受
2. ✅ Prius特化の細かいチューニング
3. ⚠️ セキュリティアップデートは手動で取り込む必要
4. ⚠️ 新機能は自分で移植が必要

### 本家openpilotに移行する場合
1. ✅ 最新機能とセキュリティ
2. ✅ コミュニティサポート
3. ⚠️ ichiroの独自機能（速度調整ボタン等）の再実装が必要
4. ⚠️ 縦制御のチューニングやり直し

### ハイブリッドアプローチ
1. 本家ベースで運用
2. ichiroの縦制御ロジックを移植
3. UI機能を選択的に移植
4. 定期的に本家の更新を取り込む

---

## まとめ

### ichiroブランチの強み
- 🎯 **Prius特化の最適化**
- 🛠️ **実用的なカスタマイズ**（速度調整ボタン、デバッグ表示）
- 🎨 **洗練された縦制御**（遅延補償、デッドゾーン、オーバーシュート防止）
- 🔒 **安定したベース**（v0.10.3-0.10.4）

### 本家openpilotの強み
- 🚀 **最新技術とアルゴリズム**
- 🌐 **幅広い車種対応**
- 🏗️ **モダンなアーキテクチャ**
- 👥 **活発なコミュニティ**
- 🔄 **継続的な改善**

### 結論

**ichiroブランチは、Toyota Priusユーザーにとって実用性の高いカスタムブランチ**です。縦制御の細かいチューニングと便利なUI追加により、日常使用での満足度が高いと考えられます。

一方、**本家openpilotは、汎用性と最新性を重視するユーザー向け**です。多車種対応と継続的な改善により、長期的には本家の方が優位になる可能性があります。

どちらを選ぶかは、**実用性（ichiro）vs 最新性（本家）** のトレードオフです。

---

## 詳細ドキュメント

各トピックの詳細は以下のドキュメントを参照してください：

1. **縦制御の詳細**: [longitudinal_control_detail.md](longitudinal_control_detail.md)
   - 制御アルゴリズムの詳細比較
   - PID実装の違い
   - アクチュエータ遅延補償
   - 状態マシンの違い

2. **UI変更の詳細**: [ui_changes_detail.md](ui_changes_detail.md)
   - ファイル構造の違い
   - Alert/HUDシステムの比較
   - 独自機能（速度調整ボタン等）
   - レイアウトとスタイリング

3. **その他の変更**: [other_changes_detail.md](other_changes_detail.md)
   - バージョン管理
   - サブモジュール
   - ビルドシステム
   - テストとCI/CD
   - ドキュメント

---

## 付録：ファイルパス対応表

| 機能 | 本家 | ichiro |
|------|------|--------|
| 縦制御 | `selfdrive/controls/lib/longcontrol.py` | 同左（内容が異なる） |
| 縦プランナー | `selfdrive/controls/lib/longitudinal_planner.py` | 同左（内容が異なる） |
| Onroad UI | `selfdrive/ui/qt/onroad/*.cc` | `selfdrive/ui/qt/onroad.cc` |
| Alert | `selfdrive/ui/qt/onroad/alerts.cc` | `onroad.cc`内 |
| HUD | `selfdrive/ui/qt/onroad/hud.cc` | `onroad.cc`内 |
| Toyota I/F | `opendbc_repo/opendbc/car/toyota/interface.py` | `selfdrive/car/toyota/interface.py` |

---

**作成日**: 2025-12-20  
**比較対象**:
- ichiro: `/home/takuya/work/comma/other_repo/ichiro`
- 本家: `/home/takuya/work/comma/openpilot`

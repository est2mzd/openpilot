# sunnypilot と 本家openpilot の比較 - 概要

## はじめに

このドキュメントは、sunnypilot（/home/takuya/work/comma/other_repo/sunny）と本家openpilot（あなたのフォーク /home/takuya/work/comma/openpilot）の主要な違いをまとめたものです。

**sunnypilotとは**: 公式openpilotをベースに、より高度な機能とユーザーカスタマイズ性を追加したコミュニティフォークです。325車種以上をサポートし、2025年に完全リライトを実施しました。

---

## 比較対象

| 項目 | 本家openpilot（あなたのフォーク） | sunnypilot |
|------|--------------------------------|------------|
| **リポジトリ** | /home/takuya/work/comma/openpilot | /home/takuya/work/comma/other_repo/sunny |
| **ベース** | 公式openpilot最新版 | 公式openpilot v0.10.1 (2025年リライト) |
| **メンテナー** | est2mzd (GitHub) | sunnyhaibin/sunnypilot (GitHub) |
| **主な特徴** | 公式準拠 + metadriveサブモジュール | 大幅な機能拡張 + UI刷新 |
| **サポート車種** | 200+車種 | 325+車種 |
| **バージョン** | 最新版追従 | 2025.002.000 |

---

## 主要な違い - 3つの柱

### 1. 縦制御（Longitudinal Control）の高度化 🚗

**詳細**: [sunnypilot_longitudinal_control_detail.md](sunnypilot_longitudinal_control_detail.md)

sunnypilotは、本家の基本的な縦制御に以下の高度な機能を追加：

#### MADS (Modular Assistive Driving System)
- 横制御と縦制御を独立して有効化/無効化
- 一時停止（pause）状態のサポート
- より柔軟なエンゲージメント管理

#### DEC (Dynamic Experimental Control)
- 走行状況に応じて自動的にExperimentalモード切り替え
- カーブ、先行車距離、速度に基づく自動最適化

#### SLA (Speed Limit Assist)
- 車両CAN + OpenStreetMapから制限速度を取得
- 自動的にクルーズ速度を調整
- オフセット設定可能（制限速度 + X km/h）

#### ICBM (Intelligent Cruise Button Management)
- クルーズコントロールボタンコマンドで速度管理
- 車両ACCとの協調制御（ハイブリッド縦制御）

#### SCC-M / SCC-V (Smart Cruise Control)
- **SCC-M**: マップデータからカーブ情報を取得して事前減速
- **SCC-V**: モデルの視覚情報からカーブを検出して減速

| 機能 | 本家openpilot | sunnypilot |
|------|--------------|-----------|
| 基本縦制御 | ✅ MPC制御 | ✅ MPC + 拡張機能 |
| エンゲージメント | 一括ON/OFF | ✅ MADS（個別制御） |
| Experimentalモード | 手動切り替え | ✅ DEC（自動切り替え） |
| 制限速度対応 | ❌ | ✅ SLA（自動調整） |
| クルーズボタン制御 | ❌ | ✅ ICBM |
| カーブ速度制御 | 基本的 | ✅ SCC-M/V（高度） |

---

### 2. UIの完全刷新とカスタマイズ 🎨

**詳細**: [sunnypilot_ui_changes_detail.md](sunnypilot_ui_changes_detail.md)

sunnypilotは、UIを完全に再設計し、直感的で機能豊富な体験を提供：

#### オフロードUI（設定画面）
- **専用パネル**: Steering、Longitudinal、Vehicle、Models、Visuals、Display、Trips等
- **Advanced Controlsトグル**: 初心者/上級者モード切り替え
- **設定項目数**: 本家20項目 → sunnypilot 100+項目

#### Models パネル（革新的）
- 86+のドライビングモデルから選択
- GUIでダウンロード/削除/切り替え
- お気に入り機能、Fuzzy Search（あいまい検索）
- モデルフォルダー管理、キャッシュ更新

#### オンロードUI（運転中画面）
- **Rainbow Road Path** 🌈: Tesla風の虹色パス表示
- **On-Screen Turn Signals**: ウインカー表示
- **Blind Spot Indicators**: ブラインドスポット警告
- **Green Light Indicator**: 青信号表示
- **Lead Chevron Info**: 先行車情報詳細表示
- **Road Name Display**: 道路名表示（mapd統合）
- **Standstill Timer**: 停止時間タイマー

#### Display機能
- **Screen Off While Driving**: 運転中画面オフ（夜間の眩しさ軽減）
- **Brightness Controls**: 明るさ自動/手動調整
- **Custom Timeout**: タッチ後の消灯時間設定

| UIコンポーネント | 本家openpilot | sunnypilot |
|-----------------|--------------|-----------|
| 設定構造 | 1つのリスト | 専用パネルで整理 |
| モデル管理 | コマンドライン（SSH） | ✅ GUI（ダウンロード/削除） |
| オンロード情報 | 基本情報のみ | ✅ 豊富な情報表示 |
| ビジュアル | シンプル | ✅ カスタマイズ可能 |
| 画面制御 | 常時点灯 | ✅ 運転中オフ可能 |

---

### 3. システム統合と拡張機能 🔧

**詳細**: [sunnypilot_other_changes_detail.md](sunnypilot_other_changes_detail.md)

#### mapd統合（OpenStreetMap）
- オフラインマップデータのダウンロード
- 制限速度情報、道路名、カーブ情報を取得
- 検索機能で地域を簡単に指定

#### sunnylink（クラウド統合）
- 設定のバックアップ/リストア
- 複数デバイス間で設定同期
- 走行統計の収集
- 将来: リモートモデル切り替え（インフラ準備完了）

#### Vehicle Selector（車両選択）
- 自動フィンガープリント失敗時に手動選択可能
- 車種リストから選択
- より多くの車両をサポート

#### 外部ストレージサポート
- USBドライブ対応
- モデルや走行ログを外部保存

#### 開発環境強化
- 完全なDocker対応
- GitHub Runner Service
- 包括的テストスイート
- 自動化ワークフロー

| 機能 | 本家openpilot | sunnypilot |
|------|--------------|-----------|
| マップ統合 | ❌ | ✅ mapd（OpenStreetMap） |
| クラウド連携 | comma connect | ✅ sunnylink（設定同期） |
| 車両選択 | 自動のみ | ✅ 自動 + 手動 |
| 外部ストレージ | ❌ | ✅ USB対応 |
| モデル数 | 1モデル | ✅ 86+モデル |
| 登録要否 | 必須 | ✅ 不要 |

---

## 各機能の詳細ドキュメント

1. **[縦制御の詳細](sunnypilot_longitudinal_control_detail.md)**
   - MADS、DEC、SLA、ICBM、SCC-M/Vの詳細
   - 実装コード、動作原理
   - 使用例シナリオ

2. **[UIの詳細](sunnypilot_ui_changes_detail.md)**
   - オフロード/オンロードUIの完全解説
   - 新設パネルの機能
   - ビジュアルカスタマイズオプション

3. **[その他の変更](sunnypilot_other_changes_detail.md)**
   - mapd、sunnylink統合
   - モデル管理システム
   - 開発環境、テスト
   - リリース管理

---

## 使用ケース別の推奨

### 本家openpilot（あなたのフォーク）を推奨

- 公式の最新機能を常に使いたい
- シンプルな設定で使いたい
- 公式のサポートを重視
- 安定性を最優先

### sunnypilotを推奨

- **より高度な縦制御**が必要（MADS、SLA、DEC等）
- **豊富なUIカスタマイズ**が欲しい
- **複数のモデル**を試したい
- **制限速度への自動対応**が欲しい
- **マップデータ**を活用したい（オフライン）
- 細かい設定を調整したい

---

## 主要な違いマトリクス

| カテゴリ | 機能 | 本家 | sunnypilot |
|---------|------|------|------------|
| **縦制御** | MADS（個別制御） | ❌ | ✅ |
| | DEC（自動Experimental切替） | ❌ | ✅ |
| | SLA（制限速度アシスト） | ❌ | ✅ |
| | ICBM（ボタン制御） | ❌ | ✅ |
| | SCC-M/V（カーブ速度） | 基本的 | ✅ 高度 |
| **UI** | 専用設定パネル | ❌ | ✅ 9パネル |
| | モデル管理GUI | ❌ | ✅ 86+モデル |
| | Rainbow Road Path | ❌ | ✅ |
| | ウインカー画面表示 | ❌ | ✅ |
| | 画面オフ機能 | ❌ | ✅ |
| | Advanced Controls | ❌ | ✅ |
| **統合** | OpenStreetMap | ❌ | ✅ mapd |
| | クラウド設定同期 | ❌ | ✅ sunnylink |
| | 車両手動選択 | ❌ | ✅ |
| | 外部ストレージ | ❌ | ✅ USB |
| **開発** | Docker対応 | 基本 | ✅ 完全 |
| | CI/CD | 標準 | ✅ 大幅強化 |
| | テストスイート | 標準 | ✅ 包括的 |

---

## 技術的アーキテクチャ比較

### 本家openpilot

```
openpilot/
├── selfdrive/
│   ├── car/                    # 車両インターフェース
│   ├── controls/               # 制御系
│   │   ├── controlsd.py
│   │   └── longitudinal_planner.py
│   └── ui/                     # UI（シンプル）
├── cereal/                     # メッセージング
└── opendbc/                    # DBC定義
```

### sunnypilot

```
sunny/
├── sunnypilot/                 # ★sunnypilot独自機能
│   ├── mads/                   # MADS実装
│   ├── mapd/                   # mapd統合
│   ├── modeld/                 # モデルローダー
│   ├── models/                 # 86+モデル管理
│   ├── sunnylink/              # クラウド統合
│   └── selfdrive/
│       ├── car/                # 車両IF拡張
│       ├── controls/
│       │   ├── controlsd_ext.py       # 拡張制御
│       │   └── lib/
│       │       ├── speed_limit/       # SLA
│       │       ├── dec/               # DEC
│       │       ├── icbm/              # ICBM
│       │       └── scc/               # SCC
│       └── ui/                 # UI大幅拡張
│           ├── offroad/
│           │   └── panels/     # 専用パネル
│           └── onroad/         # 運転中UI強化
├── cereal_sp/                  # カスタムcereal
└── opendbc/                    # 拡張DBC
```

---

## バージョン情報

### 本家openpilot（あなたのフォーク）
- ベース: 公式openpilot最新版
- 最終更新: 継続的

### sunnypilot
- ベース: openpilot v0.10.1 (commit c9dbf97 - 2025年9月12日)
- バージョン: 2025.002.000 (2025年11月6日)
- 主要リライト: 2025年実施

**利用可能なブランチ**:
- `release` - 安定版 (https://release.sunnypilot.ai)
- `staging` - ベータ版 (https://staging.sunnypilot.ai)
- `dev` - 開発版 (https://dev.sunnypilot.ai)

---

## コミュニティとサポート

### 本家openpilot
- GitHub: https://github.com/est2mzd/openpilot
- 公式Discord参加可能

### sunnypilot
- GitHub: https://github.com/sunnypilot/sunnypilot
- ドキュメント: https://docs.sunnypilot.ai/
- コミュニティフォーラム: https://community.sunnypilot.ai/
- 車種サポート: 325+車種（docs/CARS.md）

---

## まとめ

### 本家openpilot（あなたのフォーク）の強み
✅ 公式最新版に追従  
✅ シンプルで信頼性が高い  
✅ 公式サポートとコミュニティ  
✅ 安定性重視  

### sunnypilotの強み
✅ 高度な縦制御機能（MADS、SLA、DEC等）  
✅ 豊富なUIカスタマイズ  
✅ 86+のモデル選択  
✅ マップ統合（オフライン対応）  
✅ 設定のクラウド同期  
✅ 325+車種サポート  
✅ 細かいカスタマイズが可能  

---

## 次のステップ

sunnypilotに興味がある場合：

1. **ドキュメントを読む**: [docs.sunnypilot.ai](https://docs.sunnypilot.ai/)
2. **詳細比較を確認**:
   - [縦制御の詳細](sunnypilot_longitudinal_control_detail.md)
   - [UIの詳細](sunnypilot_ui_changes_detail.md)
   - [その他の変更](sunnypilot_other_changes_detail.md)
3. **インストール**: 
   - Factory reset → Custom Software → `https://staging.sunnypilot.ai`
4. **フォーラム参加**: [community.sunnypilot.ai](https://community.sunnypilot.ai/)

本家openpilotを使い続ける場合：
- 公式の最新機能と安定性を享受
- 必要に応じてsunnypilotの特定機能を参考に独自実装も可能

---

**作成日**: 2025年12月21日  
**比較対象**:
- 本家openpilot: /home/takuya/work/comma/openpilot
- sunnypilot: /home/takuya/work/comma/other_repo/sunny (v2025.002.000)

# sunnypilot - その他の主要な変更

## 概要
縦制御とUI以外の、sunnypilotと本家openpilot（あなたのフォーク）の主要な違いをまとめます。

---

## 1. バージョンとベース

### 本家openpilot（あなたのフォーク）
- **ベースバージョン**: 公式openpilotの最新版ベース
- **メインブランチ**: `master`、`annotation`等
- **主な特徴**: 公式に独自機能を追加

### sunnypilot
- **ベースバージョン**: 公式openpilot v0.10.1ベース
  - master commit c9dbf97649a27117be6d5955a49e2d4253337288 (2025年9月12日)
- **リリースブランチ**: `release`、`staging`、`dev`
- **バージョン**: 2025.002.000 (2025年11月6日)
- **主な特徴**: 2025年に完全リライト実施

コミット履歴より：
```
* v2025.002.000 - 大規模リライト完了
* v2025.001.000 - リライト初版リリース
* 0.9.7.1 - レガシー版最終リリース
```

---

## 2. ディレクトリ構造の違い

### 本家openpilot独自のディレクトリ
```
openpilot/
├── selfdrive/ui/onroad/         # モジュール化されたUI（公式最新）
├── selfdrive/ui/layouts/        # レイアウト定義（公式最新）
├── openpilot/ (メタディレクトリ) # パッケージ化対応（公式最新）
└── metadrive/                   # Metadriveサブモジュール（あなたが追加）
```

### sunnypilot独自のディレクトリ
```
other_repo/sunny/
├── sunnypilot/                  # sunnypilot独自機能（★メイン）
│   ├── mads/                    # MADS実装
│   ├── mapd/                    # mapd統合
│   ├── modeld/                  # カスタムモデルローダー
│   ├── models/                  # モデル管理
│   ├── navd/                    # ナビゲーション
│   ├── sunnylink/               # クラウド統合
│   └── selfdrive/
│       ├── car/                 # 車両インターフェース拡張
│       ├── controls/            # 制御系拡張
│       │   └── lib/
│       │       ├── speed_limit/ # SLA実装
│       │       ├── dec/         # DEC実装
│       │       ├── icbm/        # ICBM実装
│       │       └── scc/         # SCC実装
│       └── ui/                  # UI拡張
├── Dockerfile.sunnypilot        # sunnypilot専用Docker
└── Dockerfile.sunnypilot_base   # ベースイメージ
```

---

## 3. サブモジュールの管理

### 共通のサブモジュール
両方とも以下のサブモジュールを使用：
- `panda` - CANインターフェースライブラリ
- `opendbc` - DBC（車両CAN定義）
- `cereal` - メッセージング
- `rednose` - カルマンフィルタ
- `msgq` - メッセージキュー
- `tinygrad` - ニューラルネットワークフレームワーク

### sunnypilot独自
- **カスタムcereal実装**
  - sunnypilot専用のイベント定義
  - カスタムcar parameters (CP_SP)
  - 拡張されたcar controls
  
```python
# カスタムcereal構造
cereal/
├── car_sp.capnp          # sunnypilot用car params
├── controls_sp.capnp     # sunnypilot用controls
└── events_sp.capnp       # sunnypilot用events
```

---

## 4. 車両サポートの違い

### 本家openpilot
公式と同様に幅広い車種をサポート（200車種以上）：
- Tesla
- Honda
- Toyota
- Hyundai
- GM
- Ford
- その他多数

### sunnypilot
さらに拡張されたサポート（325車種以上）：

**追加サポート**:
- Tesla Coop Steering（協調ステアリング）
- Honda Gas Interceptor対応強化
- Rivian 対応
- より多くのToyota SecOC車両

**ドキュメント**: `docs/CARS.md` に詳細なサポート車種リスト

---

## 5. モデルとAI

### 本家openpilot

```
models/
└── supercombo.onnx          # 最新のドライビングモデル（1つ）
```

公式の最新モデルを使用。モデル変更にはSSH経由でファイル置き換えが必要。

### sunnypilot

```
sunnypilot/models/
├── model_manager.py         # モデル管理システム
├── model_registry.json      # 86+モデルのレジストリ
└── downloaded_models/       # ダウンロード済みモデル
    ├── supercombo_latest.onnx
    ├── night_strike_oct2023.onnx
    ├── cool_people_oct2025.onnx
    └── ... (86+ models)
```

**モデル管理の革新**:

1. **Driving Model Manager**
   - 86+のモデルから選択可能
   - Night Strike (2023年10月) ～ The Cool People's Models (2025年10月)
   - GUIでダウンロード/削除/切り替え

2. **モデルバックエンドのモジュール化**
   - SNPE、thneed、tinygrad対応
   - 動的モデル入力サポート

3. **拡張モデル出力**
   - Turn desires（ターン意図）
   - より詳細なパス予測

4. **ライブパラメータ調整**
   - UIから遅延調整
   - ソフトウェア遅延制御

5. **モデルキャッシング**
   - 自動リフレッシュ
   - Shape inference（入力から形状を推論）

---

## 6. mapd統合（OpenStreetMap）

### 本家openpilot
マップデータの統合なし。

### sunnypilot

**ファイルパス**: `/home/takuya/work/comma/other_repo/sunny/sunnypilot/mapd/`

**機能**:
1. **オフラインマップダウンロード**
   - OpenStreetMapデータベースをダウンロード
   - 地域別にダウンロード可能
   - 検索機能で簡単に場所を見つける

2. **Speed Limit Assist (SLA)との統合**
   - マップから制限速度情報を取得
   - オフラインでも動作

3. **道路名表示**
   - 現在走行中の道路名を表示
   - ナビゲーション補助

4. **SCC-M (Smart Cruise Control - Map)**
   - マップデータからカーブ情報を取得
   - カーブ前に自動減速

**ダウンローダーUI**:
```
OpenStreetMap Downloader
─────────────────────────
Search: [Tokyo, Japan____]
─────────────────────────
📍 Tokyo, Japan (25 MB)
   [Download] [Delete]
```

---

## 7. sunnylink統合（クラウド連携）

### 本家openpilot

**comma connect**:
- 基本的なクラウド連携
- 走行ログのアップロード
- データ閲覧

### sunnypilot

**sunnylink** (`sunnypilot/sunnylink/`):

**機能**:
1. **設定のバックアップ/リストア**
   - クラウドに設定を保存
   - 複数デバイス間で設定を同期
   - 工場出荷リセット後も設定を復元可能

2. **統計情報**
   - 走行統計の収集
   - Novice tier以上で利用可能

3. **リモート管理（準備中）**
   - リモートでモデル切り替え（インフラ準備完了、今後有効化予定）
   - リモート設定変更

**アーキテクチャ**:
```python
# sunnylink/uploader.py
class SunnylinkUploader:
    def backup_settings(self):
        # 全設定をクラウドにアップロード
        settings = self.collect_all_settings()
        self.upload_to_cloud(settings)
    
    def restore_settings(self):
        # クラウドから設定をダウンロード
        settings = self.download_from_cloud()
        self.apply_settings(settings)
```

---

## 8. 外部ストレージサポート

### 本家openpilot
内蔵ストレージのみ。

### sunnypilot

**機能**:
- USBドライブ対応
- 外部ストレージにモデル保存
- 走行ログの外部保存

**設定**:
```
Settings > Device > Storage
─────────────────────────────
Internal Storage: 25GB free
External Storage: 128GB USB
─────────────────────────────
☐ Save models to external
☐ Save logs to external
```

---

## 9. Vehicle Selector（車両選択機能）

### 本家openpilot
自動フィンガープリントのみ。認識失敗時は使用不可。

### sunnypilot

**ファイルパス**: `sunnypilot/selfdrive/car/vehicle_selector.py`

**機能**:
- フィンガープリント失敗時に手動選択可能
- 車種リストから選択
- プラットフォーム選択も可能

**UI**:
```
Vehicle Selector
─────────────────────────────
Auto-detected: Unknown
─────────────────────────────
Manual Selection:
Make: [Toyota ▼]
Model: [Prius ▼]
Year: [2020 ▼]
─────────────────────────────
[Confirm Selection]
```

---

## 10. テストとCI/CD

### 本家openpilot
- GitHub Actions
- Jenkinsfile
- codecov.yml

### sunnypilot

**大幅強化**:

1. **完全なDocker対応**
   - `Dockerfile.sunnypilot`
   - `Dockerfile.sunnypilot_base`
   - 専用ビルド環境

2. **GitHub Runner Service**
   - 専用のCI/CDランナー
   - 自動テスト・ビルド・リリース

3. **包括的テストスイート**
   - opendbc, pandaテストスイート統合
   - リグレッションテスト
   - 安全性テストカバレッジ

4. **自動化ワークフロー**
   - モデルビルド・公開自動化
   - UIプレビュー生成
   - リリースドラフト自動作成
   - コード品質チェック

---

## 11. Python環境と依存関係

### 本家openpilot

```python
# pyproject.toml
[project]
dependencies = [
    "Cython",
    "PyQt5",
    "numpy",
    "opencv-python",
    # ...
]
```

Python 3.11以降推奨。

### sunnypilot

同様のpyproject.toml + 追加依存関係:

```python
dependencies = [
    # 本家と同じ
    "Cython",
    "PyQt5",
    # ...
    # sunnypilot追加
    "requests",      # sunnylink用
    "geopy",         # mapd用
    "osmium",        # OpenStreetMap解析
]
```

**uv.lock**:
- 最新のPythonパッケージマネージャー対応
- 高速な依存関係解決

---

## 12. ドキュメント

### 本家openpilot
- `README.md` - 基本情報
- `docs/` - 技術ドキュメント

### sunnypilot

**拡張ドキュメント**:
- `README.md` - インストール手順、ブランチ情報
- `CHANGELOG.md` - 詳細な変更履歴（1105行）
- `docs/CARS.md` - 325車種のサポートリスト
- `docs/SAFETY.md` - 安全性ポリシー
- **オンラインドキュメント**: https://docs.sunnypilot.ai/
- **コミュニティフォーラム**: https://community.sunnypilot.ai/

---

## 13. セキュリティと認証

### 本家openpilot
- comma Connect API
- デバイス登録必須

### sunnypilot

**変更点**:
- **登録不要**: デバイス登録なしでonroad可能
- comma Connect互換性維持
- sunnylink追加認証

---

## 14. Advanced Controls Toggle

### 本家openpilot
全ての設定が常に表示される。

### sunnypilot

**段階的な設定開示**:

```
Settings > Advanced Controls: [OFF]
─────────────────────────────────
[基本設定のみ表示]
- Enable openpilot
- Set cruise speed
- ...

Settings > Advanced Controls: [ON]
─────────────────────────────────
[全ての設定を表示]
- Enable openpilot
- Set cruise speed
- [Advanced] NNLC Fine Tuning ★
- [Advanced] Custom PID Gains ★
- [Advanced] Model Override ★
- ...
```

**メリット**:
- 初心者: シンプルな設定画面
- 上級者: 全てのオプションにアクセス
- 段階的な学習曲線

---

## 15. Convenience Features（便利機能）

### Exit Offroad Button
設定画面から運転モードへ素早く復帰。

### Always Offroad Mode
デバッグ用に常にオフロードモードを維持。

### Quiet Mode
アラート音を消音、視覚アラートのみ。

### Max Time Offroad
オフロード状態の最大時間を設定し、自動シャットダウン。

---

## 16. 翻訳とローカライゼーション

### 本家openpilot
基本的な多言語サポート。

### sunnypilot

**強化された翻訳**:
- 韓国語翻訳更新（Kirito3481氏）
- 自動翻訳管理システム
- コミュニティ翻訳サポート

---

## 17. 削除された機能（リライトにより）

sunnypilot 2025.001.000のリライトで一時的に削除された機能：

- **Navigate on openpilot (NoO)** - 公式が優先度変更のため
- Visuals: Rocket Fuel
- Visuals: Displaying Braking Status
- Toyota - Enforce Stock Longitudinal Control
- Subaru: Increase Steering Torque
- Longitudinal: Acceleration Personality
- UI: Display CPU Temperature
- Lateral: Block Lane Change with Road Edge Detection
- UI: Display DM Camera in Reverse Gear
- UI: Auto-hide Selected UI Elements
- Visuals: Display End-to-End Longitudinal Status
- Toyota: Stop and Go Hack (alpha)
- Visuals: Onroad Settings
- Honda: Serial Steering Support
- Volkswagen: Non-ACC Platforms Support
- Longitudinal: Dynamic Personality
- Honda Nidec: Allow Stock Longitudinal Control
- Lateral Planner: Dynamic Lane Profile
- Lateral Planner: Laneful Mode
- Lateral: Custom Camera and Path Offsets
- Toyota: Door Controls

**注**: これらは将来のリリースで復活する可能性があります。

---

## 18. パラメータストアキャッシング

### 本家openpilot
パラメータを毎回読み込み。

### sunnypilot

**高速化**:
```python
# params_cache.py
class ParamsCache:
    def __init__(self):
        self.cache = {}
        self.load_cache()
    
    def load_cache(self):
        # 起動時にキャッシュをロード
        # 起動時間を大幅短縮
```

**ライブ更新対応**:
- キャッシュを使用しながらも
- パラメータ変更時は即座に反映

---

## 19. CLion IDE統合

### sunnypilot独自

```
.idea/
├── workspace.xml
├── inspectionProfiles/
└── runConfigurations/
```

**開発者向け機能**:
- CLion IDE設定
- 外部ツール統合
- デバッグ設定
- コード品質ツール

---

## 20. リリース管理

### 本家openpilot
- `RELEASES.md` - リリースノート

### sunnypilot

**複数ブランチ戦略**:

| ブランチ | 用途 | URL |
|---------|------|-----|
| `release` | 安定版 | https://release.sunnypilot.ai |
| `staging` | ベータ版 | https://staging.sunnypilot.ai |
| `dev` | 開発版 | https://dev.sunnypilot.ai |
| カスタム | 任意 | https://install.sunnypilot.ai/{branch_name} |

**レガシーブランチ（非推奨）**:
- `release-c3`
- `staging-c3`
- `dev-c3`

---

## 21. Panda Firmware Checks

### sunnypilot強化

- 改善されたファームウェアチェック
- 非推奨Pandaデバイスの優雅な処理
- より詳細なエラーメッセージ

---

## 主要な違いまとめ

| 項目 | 本家openpilot（あなたのフォーク） | sunnypilot |
|------|--------------------------------|------------|
| **ベースバージョン** | 公式最新版ベース | v0.10.1ベース（2025年リライト） |
| **モデル管理** | 1モデル（SSH置き換え） | 86+モデル（GUI管理） |
| **マップ統合** | なし | mapd（OpenStreetMap） |
| **クラウド連携** | comma connect | sunnylink（設定同期等） |
| **車両選択** | 自動のみ | 自動 + 手動選択 |
| **外部ストレージ** | なし | USB対応 |
| **Advanced Controls** | なし | あり（段階的設定開示） |
| **ドキュメント** | 基本 | 充実（専用サイト、フォーラム） |
| **CI/CD** | 標準 | 大幅強化（Docker、自動化） |
| **登録要否** | 必須 | 不要 |
| **設定項目数** | 約20 | 100+ |
| **カスタマイズ性** | 中 | 非常に高 |
| **コミュニティ** | GitHub | フォーラム + GitHub |
| **リリース頻度** | 公式に準拠 | 独自ペース |

---

## まとめ

sunnypilotは、本家openpilotをベースに以下を実現：

1. **モデル管理の革新**: 86+モデルのGUI管理
2. **マップ統合**: OpenStreetMapでオフライン機能強化
3. **クラウド連携**: sunnylink で設定同期
4. **ユーザビリティ**: Advanced Controls、Vehicle Selector等
5. **開発環境**: 充実したCI/CD、Docker対応
6. **コミュニティ**: 専用フォーラムとドキュメント
7. **柔軟性**: 複数ブランチ、外部ストレージ対応

本家が公式最新版への追従を重視する一方、sunnypilotは機能の豊富さとカスタマイズ性を重視しています。

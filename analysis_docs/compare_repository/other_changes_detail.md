# その他の主要な変更

## 概要
縦制御とUI以外の、ichiroブランチと本家openpilotの主要な違いをまとめます。

## 1. バージョンとベース

### 本家openpilot（あなたのフォーク）
- **ベースバージョン**: 公式openpilotの最新版ベース
- **メインブランチ**: `master`、`annotation`等
- **主な特徴**: 公式に独自機能を追加

### ichiroブランチ
- **ベースバージョン**: 公式openpilot v0.10.3～v0.10.4ベース
- **カスタマイズブランチ**: `master-cit02`
- **主な特徴**: 旧バージョンベースにPrius向けカスタマイズ

コミット履歴より：
```
* e8d51d450 AGNOS16, 公式0.10.3リリースベースのベースブランチ
* 1c2f9e619 bump version to 0.10.4
* dfd7a8c8d AGNOS 16 (#36915)
```

## 2. ディレクトリ構造の違い

### 本家openpilot独自のディレクトリ
```
openpilot/
├── selfdrive/ui/onroad/         # モジュール化されたUI（公式最新）
├── selfdrive/ui/layouts/        # レイアウト定義（公式最新）
├── selfdrive/ui/__pycache__/    # Python キャッシュ
├── openpilot/ (メタディレクトリ) # パッケージ化対応（公式最新）
└── metadrive/                   # Metadriveサブモジュール（あなたが追加）
```

### ichiroブランチ独自のディレクトリ
```
other_repo/ichiro/
├── selfdrive/ui/navd/           # ナビゲーション機能（ichiro独自）
├── selfdrive/ui/mui.cc          # 追加UI（ichiro独自）
└── laika/                       # GPSライブラリ（シンボリックリンク、ichiro独自）
```

## 3. サブモジュールの管理

### 共通のサブモジュール
両方とも以下のサブモジュールを使用：
- `panda` - CANインターフェースライブラリ
- `opendbc` - DBC（車両CAN定義）
- `cereal` - メッセージング
- `rednose` - カルマンフィルタ
- `msgq` - メッセージキュー

### 本家openpilot独自
- `metadrive` - Metadriveシミュレーター（あなたが追加したサブモジュール）
  ```
  [submodule "metadrive"]
      active = true
      url = https://github.com/est2mzd/metadrive.git
  ```

## 4. 車両サポートの違い

### 本家openpilot
公式と同様に幅広い車種をサポート（200車種以上）：
- Tesla
- Honda
- Toyota
- Hyundai
- GM
- Volkswagen
- その他多数

### ichiroブランチ
主にToyota Priusに特化したカスタマイズ：
```python
# selfdrive/car/toyota/interface.py
if candidate == CAR.PRIUS:
    stop_and_go = True
    ret.wheelbase = 2.70
    ret.steerRatio = 15.74
    tire_stiffness_factor = 0.6371
    ret.mass = 3045. * CV.LB_TO_KG + STD_CARGO_KG
    set_lat_tune(ret.lateralTuning, LatTunes.INDI_PRIUS)
    ret.steerActuatorDelay = 0.3
    # Prius特有のチューニング
```

## 5. コンパイルと依存関係

### 本家openpilot
```python
# pyproject.toml で依存関係を管理
[project]
dependencies = [
    "Cython",
    "PyQt5",
    "numpy",
    "opencv-python",
    # ...多数
]
```

Python パッケージとして管理され、`pip install -e .` でインストール可能。

### ichiroブランチ
- Pipfileで依存関係管理（古い方式）
- カスタムビルドスクリプト

## 6. Git LFS（Large File Storage）設定

### 本家openpilot (.git/config)
```ini
[lfs]
    repositoryformatversion = 0
    url = https://github.com/est2mzd/openpilot.git/info/lfs
    concurrenttransfers = 4
    pushurl = https://github.com/est2mzd/openpilot.git/info/lfs

[lfs "transfer"]
    enableSSH = false
    maxretries = 1

[lfs "https://github.com/est2mzd/openpilot.git/info/lfs"]
    access = basic
```

あなたのリポジトリに詳細なGit LFS設定が追加されています。

### ichiroブランチ
標準的なGit LFS設定。

## 7. テストとCI/CD

### 本家openpilot
- **CI**: GitHub Actions + Jenkinsfile
- **テストカバレッジ**: codecov.yml で設定
- **自動テスト**: 
  - `selfdrive/test/` - 統合テスト
  - `selfdrive/test/process_replay/` - プロセスリプレイテスト
  - `selfdrive/test/profiling/` - パフォーマンステスト

### ichiroブランチ
- テストファイルは存在するが、CI/CD設定はカスタマイズされている可能性
- 主に手動テスト中心

## 8. ドキュメント

### 本家openpilot
- `README.md` - 包括的なドキュメント
- `docs/` - 詳細なドキュメント
- `mkdocs.yml` - ドキュメントサイト設定
- `RELEASES.md` - リリースノート
- `SECURITY.md` - セキュリティポリシー

### ichiroブランチ
- 基本的なドキュメントのみ
- カスタマイズに関するドキュメントは少ない

## 9. インストーラーとセットアップ

### 本家openpilot
```
selfdrive/ui/installer/
├── installers/                # 各種インストーラー
├── inter-ascii.ttf           # フォント
└── continue_openpilot.sh     # 継続スクリプト
```

### ichiroブランチ
```
selfdrive/ui/installer/
├── continue_dashcam.sh       # ダッシュカム継続（独自）
├── installer.h               # インストーラーヘッダ（独自）
└── continue_openpilot.sh     # カスタマイズ版
```

`continue_dashcam.sh` はichiro独自のダッシュカムモード用スクリプト。

## 10. Python環境と互換性

### 本家openpilot
- **Python**: 3.11以降推奨
- **パッケージ管理**: pyproject.toml（PEP 518準拠）
- **仮想環境**: venv推奨

### ichiroブランチ
- **Python**: 3.8～3.10
- **パッケージ管理**: Pipfile（古い方式）
- **仮想環境**: Pipenvまたはvenv

## 11. モデルとAI

### 本家openpilot
```
models/
└── supercombo.onnx          # 最新のドライビングモデル
```

頻繁にモデルが更新され、パフォーマンスが向上。

### ichiroブランチ
コミット履歴より一部モデルをrevert：
```
5d66ad168 -piY , ボタンCG追加
e8d51d450 AGNOS16, 公式0.10.3リリースベースのベースブランチ（このあと走行モデルとかいくつかrevertしてる）
```

特定のモデルバージョンを維持している可能性。

## 12. セキュリティと認証

### 本家openpilot
- `common/api.py` - Comma Connect API
- OAuth2認証
- デバイス認証

### ichiroブランチ
- カスタマイズされたAPI設定
- 独自の認証フロー（可能性）

## 13. 多言語サポート

### 本家openpilot
公式と同様、多言語サポートが継続的に更新されています。

### ichiroブランチ
基本的な多言語サポートのみ（v0.10.3-0.10.4時点）。

## 14. ハードウェア対応

### 本家openpilot
- Comma 3/3X 完全対応
- AGNOS 16 対応
- 新しいハードウェア機能のサポート

### ichiroブランチ
- Comma 3/3X対応
- AGNOS 16対応
- 一部ハードウェア機能はカスタマイズ

コミット履歴：
```
dfd7a8c8d AGNOS 16 (#36915)
```

## 15. デバッグとロギング

### 本家openpilot
```python
# common/swaglog.py - 高度なロギング
# selfdrive/controls/lib/drive_helpers.py - ドライブヘルパー
```

詳細なログとデバッグ機能。

### ichiroブランチ
コミット履歴より追加情報表示：
```
72bc427da 追加情報表示
e20700acf standstillの状態を確認。
```

デバッグ用の追加表示が実装されています。

## 16. WiFi UIの改善

### 本家openpilot
公式の継続的な改善を含む。

### ichiroブランチ
コミット履歴より：
```
66b7a2cbf Wifi選択のスクロール上下パディング調整。
```

WiFi UIに独自のカスタマイズが加えられています。

## 17. アラートとエラーハンドリング

### 本家openpilot
公式と同様、エラーメッセージの継続的な改善。

### ichiroブランチ
コミット履歴より：
```
611f4a110 自分の車ではstandstillが機械スイッチなので、その確認にはplannerの意思を見ない方がいい。
```

Prius固有の動作に合わせたカスタマイズ。

## 18. イベント処理

### 本家openpilot
公式の最新のマウス/タッチイベント処理を含む。

### ichiroブランチ
コミット履歴より：
```
7d313cc2c ボタン判定範囲被り修正。
```

独自の速度調整ボタンに関するタッチイベント調整。

## 主要な違いまとめ

| 項目 | 本家openpilot（あなたのフォーク） | ichiro |
|------|--------------------------------|--------|
| ベースバージョン | 公式最新版ベース | v0.10.3～v0.10.4固定 |
| 車両特化 | 汎用（200+車種） | Prius特化 |
| UI構造 | モジュール化（最新） | モノリシック（旧構造） |
| ナビゲーション | 標準 | 独自navd |
| 独自サブモジュール | metadrive（あなたが追加） | laika等 |
| Git LFS設定 | 詳細設定あり | 標準設定 |
| Python管理 | pyproject.toml（最新） | Pipfile（旧） |
| カスタマイズ | 公式ベース+独自機能 | Prius向け大幅カスタマイズ |
| 更新頻度 | 公式に追従 | 低頻度（安定重視） |

ichiroブランチは、旧バージョンをベースに特定の車両（Prius）に特化した実用的なカスタマイズを重視したブランチです。
一方、あなたの本家フォークは公式最新版ベースで、metadriveサブモジュール追加等の独自機能を含んでいます。
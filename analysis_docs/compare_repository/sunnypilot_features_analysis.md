# sunnypilot 機能分析と比較

## 要約

本ドキュメントは、sunnypilotの主要機能と標準openpilotとの違いについて包括的に分析したものです。分析は `/home/takuya/work/comma/other_repo/sunny` にあるコードベース（sunnypilot rewriteバージョン2025.001.000）に基づいています。

sunnypilotは、comma.aiのopenpilotの主要なフォークであり、comma.aiの安全ポリシーに準拠しながら、拡張された運転支援機能を提供します。2025.001.000バージョンは、上流のテストスイートと安全コンプライアンスメカニズムを完全に採用した完全な書き直しを表しています。

---

## 1. 縦方向制御機能

### 1.1 MADS (Modular Assistive Driving System)

**場所:** `/home/takuya/work/comma/other_repo/sunny/sunnypilot/mads/`

**説明:** MADSは、標準openpilotと比較してより柔軟なエンゲージメントモデルを提供する完全な運転支援フレームワークです。

**主要な実装ファイル:**
- `sunnypilot/mads/mads.py` - メインMADSコントローラー
- `sunnypilot/mads/state.py` - ステートマシン実装
- `sunnypilot/mads/helpers.py` - ヘルパー関数と定数

**コア機能:**

1. **ステートマシン** (`cereal/custom.capnp`で定義された状態):
   - `disabled` - システム非アクティブ
   - `enabled` - 横方向および縦方向の完全制御がアクティブ
   - `paused` - 横方向制御が一時停止、再開可能
   - `softDisabling` - 無効状態への段階的な移行
   - `overriding` - ユーザーが横方向制御をオーバーライド中

2. **エンゲージメントモード:**
   - **標準モード:** ACCメインクルーズボタンの押下が必要
   - **統合エンゲージメントモード:** ACCメインボタンなしでのエンゲージメントを許可
   - **メインクルーズなしモード:** ACCメインボタンがない車両用（Tesla、特定のHyundai/Kia）

3. **ブレーキ時のステアリングモード:**
   - **一時停止:** ブレーキが押されたときにステアリングを一時停止
   - **解除:** ブレーキが押されたときに完全に解除
   - `MadsSteeringMode`パラメータで設定

4. **主要パラメータ:**
   - `Mads` - MADSの有効/無効（デフォルト: 有効）
   - `MadsMainCruiseAllowed` - ACCメインクルーズ要件を許可（デフォルト: 有効）
   - `MadsSteeringMode` - ブレーキ時のステアリング動作（0=一時停止、1=解除）
   - `MadsUnifiedEngagementMode` - 統合エンゲージメントを有効化（デフォルト: 有効）

**標準openpilotとの違い:**
- 標準openpilotはエンゲージメントにクルーズコントロールのアクティブ化が必要
- 標準openpilotはブレーキで常に完全解除
- 標準openpilotには一時停止状態がない
- MADSは複数のエンゲージメント戦略を提供
- MADSは「サイレントLKAS」シナリオをサポート（クルーズコントロールなしの横方向制御のみ）

**イベント処理:**

MADSは標準のCarEventに対して「サイレント」バリアントを導入:
- `softDisableNoEntryBrake` → `softDisableNoEntryBrakeSilent`
- `preEnableStandstill` → `preEnableStandstillSilent`
- `manualRestart` → `manualRestartSilent`

これらのサイレントイベントは警告音を鳴らさずシステムを一時停止状態に移行させます。

**主要な関数:**

`mads.py`から:
- `update()` - MADSの状態を更新し、イベントを処理
- `can_engage()` - システムがエンゲージ可能かチェック
- `should_pause()` - 一時停止に移行すべきか判定
- `should_resume()` - 一時停止から再開すべきか判定

`state.py`から:
- `compute_state()` - 現在の条件に基づいてMADS状態を計算
- `handle_events()` - CarEventsを処理し適切なアクションを決定

**統合ポイント:**

MADSは以下と統合されています:
- `selfdrived.py` - メイン制御ループ
- `controlsd.py` - アクチュエーションロジック
- すべての縦方向プランナー（DEC、SLA、ICBM、SCC）
- UI要素（エンゲージメント状態表示）

---
### 1.2 DEC (Dynamic Experimental Control)

**場所:** `/home/takuya/work/comma/other_repo/sunny/sunnypilot/selfdrive/controls/lib/dec/`

**説明:** DECは、ドライビング条件に基づいてACCモードとブレンドモード（実験的）を自動的に切り替えるシステムです。

**主要な実装ファイル:**
- `dec.py` - メインDECコントローラー
- `constants.py` - 定数としきい値

**コア機能:**

1. **モード切り替えロジック:**
   - **ACCモード:** 高速道路/安定走行向け - 従来型のアダプティブクルーズコントロール
   - **ブレンドモード:** 複雑な運転向け - ニューラルネットワーク主導の縦方向計画

2. **カルマンフィルター:**
   DECは2つのカルマンフィルターを使用して意思決定を行います:
   - **ジャーク予測:** 将来のジャークを予測し、粗い運転を検出
   - **鉛車両距離:** 鉛車両までの距離の傾向を追跡

3. **モード遷移:**
   
   **ACCからブレンドへの切り替え（以下の場合）:**
   - 予測されるジャークが高い（> 1.5 m/s³）
   - 鉛車両の距離が急速に減少している
   - 速度が低い（< 15 m/s）で混雑した交通
   - 頻繁な停止/発進の運転
   
   **ブレンドからACCへの切り替え（以下の場合）:**
   - 予測されるジャークが低い（< 0.8 m/s³）
   - 安定した鉛車両の距離
   - より高い速度（> 20 m/s）
   - スムーズな高速道路走行

4. **ヒステリシス:**
   - モードフラッピング（頻繁な切り替え）を防ぐため、切り替えに異なるしきい値を使用
   - モード変更には5秒のクールダウン期間
   - 両モードで安定性を確保するため統合/フィルタリング時間

5. **主要パラメータ:**
   - `DynamicExperimentalControl` - DECの有効/無効（デフォルト: 有効）
   - `DynamicExperimentalControlOverride` - 強制的にブレンドモードに設定（デフォルト: 無効）

**主要な定数:**

`constants.py`から:
```python
JERK_THRESHOLD_BLEND = 1.5    # ACCからブレンドへの切り替えのジャーク
JERK_THRESHOLD_ACC = 0.8      # ブレンドからACCへの切り替えのジャーク
MIN_SPEED_BLEND = 15.0        # ブレンドモードの最小速度
MIN_SPEED_ACC = 20.0          # ACCモードの最小速度
MODE_CHANGE_COOLDOWN = 5.0    # モード変更間の秒数
```

**標準openpilotとの違い:**
- 標準openpilotは手動での実験モード切り替えのみ提供
- 標準openpilotは条件に基づく自動切り替えなし
- 標準openpilotはモード選択にカルマンフィルターを使用しない
- DECは最適な安全性とスムーズさのためにモードを自動最適化

**パフォーマンスの利点:**
- 高速道路での予測可能なACCの動作
- 都市部での人間らしいブレンド制御
- ユーザーの手動介入の削減
- スムーズなモード遷移

---

### 1.3 SLA (Speed Limit Assist)

**場所:** `/home/takuya/work/comma/other_repo/sunny/sunnypilot/selfdrive/controls/lib/speed_limit/`

**説明:** SLAは、複数のソースから速度制限を検出し、オプションで自動的にクルーズ速度を調整する包括的なシステムです。

**主要な実装ファイル:**
- `speed_limit_assist.py` - メインSLAコントローラー
- `speed_limit_resolver.py` - 複数のソースから速度制限を解決
- `common.py` - 共通のデータ構造と列挙型
- `helpers.py` - ヘルパー関数とユーティリティ

**コア機能:**

1. **速度制限ソース:**
   - **車両ソース:** CANバスから（車がサポートしている場合）
   - **マップソース:** mapd統合からのOpenStreetMap
   - **ビジョンソース:** モデルが標識を検出（将来の機能）
   - **ナビゲーションソース:** ナビゲーションアプリから（将来の機能）

2. **速度制限リゾルバー:**
   
   複数のソースが利用可能な場合のポリシー:
   - **車優先:** 車のデータを優先、マップで補完
   - **マップ優先:** マップデータを優先、車で補完
   - **最小:** 利用可能なすべてのソースの最小値を使用
   - **最大:** 利用可能なすべてのソースの最大値を使用
   
   パラメータ: `SpeedLimitValuePriority` (0=車、1=マップ、2=最小、3=最大)

3. **制御状態:**
   
   **状態列挙型:**
   - `inactive` - SLAは何もしていない
   - `adapting` - 新しい速度制限に適応中
   - `active` - アクティブに速度制限を維持
   - `preparing` - 今後の速度制限変更を準備中
   
4. **速度オフセット:**
   - `SpeedLimitOffset` - 検出された制限へのオフセット（mph）
   - `SpeedLimitOffsetType` - 固定またはパーセンテージベースのオフセット
   - 例: 制限が65 mphで、オフセット+5 = 70 mphに設定

5. **自動調整:**
   - `SpeedLimitControl` - 自動速度調整の有効/無効
   - 有効にすると、速度制限が変わったときにクルーズ速度を自動設定
   - ICBMと連携して実際のクルーズ設定を変更

6. **視覚的フィードバック:**
   - UIに現在の速度制限を表示
   - 今後の速度制限変更を表示（マップデータが利用可能な場合）
   - 速度制限変更までの距離
   - 速度制限ソース（車/マップ）の表示

**主要な関数:**

`speed_limit_resolver.py`から:
- `resolve()` - すべてのソースから速度制限を解決
- `get_map_speed_limit()` - mapdから速度制限を取得
- `get_car_speed_limit()` - 車のCANから速度制限を取得
- `apply_policy()` - 設定されたポリシーに基づいて最終的な制限を選択

`speed_limit_assist.py`から:
- `update()` - SLA状態を更新し、適応を管理
- `should_adapt()` - 新しい制限に適応すべきか判定
- `calculate_target()` - オフセットを含む目標速度を計算

**主要パラメータ:**
- `SpeedLimitAssist` - SLAの有効/無効（デフォルト: 無効）
- `SpeedLimitControl` - 自動速度調整（デフォルト: 無効）
- `SpeedLimitValuePriority` - ソース優先順位（デフォルト: 車優先）
- `SpeedLimitOffset` - 速度オフセット値（デフォルト: 0）
- `SpeedLimitOffsetType` - オフセットタイプ（デフォルト: 固定）
- `SpeedLimitEngageType` - エンゲージタイプ（全て/ユーザーのみ/停止時なし）
- `SpeedLimitConfirm` - 自動調整前に確認が必要（デフォルト: 無効）

**標準openpilotとの違い:**
- 標準openpilotには速度制限検出がない
- 標準openpilotには自動速度調整がない
- 標準openpilotにはマップベースの速度制限統合がない
- SLAは完全な速度制限認識と制御エコシステムを提供

---
### 1.4 ICBM (Intelligent Cruise Button Management)

**場所:** `/home/takuya/work/comma/other_repo/sunny/sunnypilot/selfdrive/car/intelligent_cruise_button_management/`

**説明:** ICBMは、openpilotの縦方向制御をネイティブでサポートしていない車両向けのハイブリッド縦方向制御システムです。車両の組み込みACCを制御しながら、openpilotが意思決定を行います。

**主要な実装ファイル:**
- `controller.py` - メインICBMコントローラー
- `helpers.py` - ヘルパー関数とユーティリティ

**コア機能:**

1. **ハイブリッド制御モデル:**
   - openpilotが縦方向計画を計算
   - ICBMが車のクルーズボタンコマンドに変換
   - 車の組み込みACCが実際のアクチュエーションを実行
   - フィードバックループが車の応答を監視

2. **ボタンコマンド:**
   - `none` - コマンドなし
   - `increase` - クルーズ速度を上げる
   - `decrease` - クルーズ速度を下げる
   - コマンドは車のCANバスに送信

3. **状態追跡:**
   - `target_velocity` - 目標クルーズ速度
   - `current_car_cruise_speed` - 車の現在のクルーズ設定
   - `command_sent` - 最後に送信されたコマンド
   - `last_command_time` - コマンドのタイミング

4. **目標速度の計算:**
   - openpilotの縦方向計画から取得
   - SLA調整を含む（有効な場合）
   - DEC選択を考慮
   - 車の制限に従う

5. **コマンド生成ロジック:**
   ```python
   if target > car_cruise + threshold:
       command = INCREASE
   elif target < car_cruise - threshold:
       command = DECREASE
   else:
       command = NONE
   ```

6. **レートリミット:**
   - 車のシステムを過負荷にしないため、ボタンコマンドをレートリミット
   - 一般的なレート: 200msごとに1コマンド
   - 連続コマンド間のクールダウン
   - 車のACCによって強制されるハードウェア制限

7. **主要パラメータ:**
   - `IntelligentCruiseButtonManagement` - ICBMの有効/無効（デフォルト: 有効）
   - サポートされている車両でのみ利用可能（CarParamsSPで判定）

**車両サポート:**

ICBMは以下の車両で利用可能です:
- openpilot縦方向制御をサポートしていないHyundai/Kia/Genesis車両
- クルーズボタンがCANを介して制御可能な一部のToyota車両
- 車両サポートは `CarParamsSP.intelligentCruiseButtonManagement.available` で判定

**統合ポイント:**

ICBMは以下と統合されています:
- `longitudinal_planner.py` - 目標速度を取得
- `car_interface.py` - ボタンコマンドを送信
- SLA - 速度制限調整
- DEC - モード選択の調整

**標準openpilotとの違い:**
- 標準openpilotは直接アクチュエーションまたはサポートなしのいずれか
- 標準openpilotには非ネイティブ車両向けのハイブリッド制御なし
- ICBMはより多くの車両でopenpilot縦方向機能を利用可能に
- openpilotのインテリジェンスと車のACCハードウェアを組み合わせ

**制限事項:**
- 車の組み込みACCの応答時間に依存
- openpilot直接制御よりも遅いループ
- 車のACCの制限に制約される
- すべての車両モデルで利用可能ではない

---

### 1.5 SCC-M/V (Smart Cruise Control Map/Vision)

**場所:** `/home/takuya/work/comma/other_repo/sunny/sunnypilot/selfdrive/controls/lib/smart_cruise_control/`

**説明:** SCC-M/Vは、ビジョン（モデル予測）とマップデータの両方を使用してカーブで安全な速度を計算する高度なカーブ速度制御システムです。

**主要な実装ファイル:**
- `smart_cruise_control.py` - メインSCCコントローラー
- `vision_controller.py` - ビジョンベースのカーブ速度
- `map_controller.py` - マップベースのカーブ速度

**コア機能:**

1. **SCC-V (Smart Cruise Control Vision):**
   
   **ビジョンベースの計算:**
   - 運転モデルの経路予測を使用
   - カーブの曲率を分析
   - 快適さの制約に基づいて最大安全速度を計算
   - 先読み時間: 一般的に3-6秒
   
   **曲率から速度への変換:**
   ```python
   max_speed = sqrt(max_lateral_accel / curvature)
   ```
   
   **主要パラメータ:**
   - `VisionTurnControl` - ビジョンベースの制御を有効化（デフォルト: 有効）
   - `VisionTurnVMax` - ビジョンの最大速度制限（デフォルト: 90 mph）
   - `VisionTurnLookahead` - 先読み時間（デフォルト: 4秒）

2. **SCC-M (Smart Cruise Control Map):**
   
   **マップベースの計算:**
   - mapdからのOSMカーブデータを使用
   - カーブの半径とタイプを分析
   - 道路の種類を考慮（高速道路 vs 地方道）
   - より長い先読み距離（最大500m）
   
   **主要パラメータ:**
   - `MapTurnControl` - マップベースの制御を有効化（デフォルト: 有効）
   - `MapTurnVMax` - マップの最大速度制限（デフォルト: 90 mph）
   - `MapTurnLookahead` - 先読み距離（デフォルト: 300m）

3. **統合制御:**
   
   両方が有効な場合:
   - ビジョンとマップの予測を計算
   - より保守的な（低い）速度を使用
   - 両方の情報源からの信頼度を考慮
   - スムーズな遷移のためにブレンド

4. **速度計算:**
   
   **横方向加速度制限:**
   - 快適: 1.8 m/s²
   - スポーツ: 2.5 m/s²（より攻撃的な運転を希望するユーザー向け）
   - パラメータ: `CurvatureMaxLatAccel`
   
   **計算:**
   ```python
   for each_point in lookahead:
       curvature = calculate_curvature(point)
       safe_speed = sqrt(max_lat_accel / curvature)
       min_speed = min(min_speed, safe_speed)
   return min_speed
   ```

5. **速度の適用:**
   - カーブ速度を縦方向プランナーに送信
   - 他の速度制約とブレンド（鉛車両、速度制限）
   - スムーズな減速/加速プロファイル
   - カーブの前に速度を下げるための事前適応

**視覚的フィードバック:**
- UIにカーブ速度を表示
- 今後のカーブの表示
- カーブまでの距離
- カーブ速度ソース（ビジョン/マップ）の表示

**統合ポイント:**

SCC-M/Vは以下と統合されています:
- `longitudinal_planner.py` - 速度制約を適用
- `model_parser.py` - ビジョン予測
- `mapd` - マップカーブデータ
- DEC - モード選択の調整

**標準openpilotとの違い:**
- 標準openpilotはモデル予測のみを使用
- 標準openpilotにはマップベースのカーブ制御なし
- 標準openpilotには明示的なカーブ速度制限なし
- SCC-M/Vはビジョンとマップからのベストなデータでカーブをより安全に処理

**パフォーマンスの利点:**
- カーブでのより安全な速度
- ビジョンの制限を超えたより長い先読み（マップ使用時）
- カーブへのよりスムーズなアプローチ
- 急ブレーキイベントの削減
- 馴染みのない道路でのより快適な乗り心地

---
## 2. UI機能

### 2.1 完全なUIリデザイン

**説明:** sunnypilotは標準openpilotのUIに対して広範なビジュアルカスタマイゼーションを提供します。

**主要なビジュアルカスタマイゼーション:**

1. **レインボーロード:**
   - パラメータ: `RainbowPath`
   - 経路を虹色のグラデーションで表示
   - カスタマイズ可能な色スキーム
   - 視覚的魅力のためのオプション機能

2. **ターンシグナル表示:**
   - パラメータ: `TurnSignalsBlindspot`
   - ターンシグナルがアクティブなときにアニメーション矢印を表示
   - 車両の周囲のビジュアルインジケーター
   - ユーザーの行動認識を向上

3. **ブラインドスポット表示:**
   - パラメータ: `BlindSpotPath`
   - 車両がブラインドスポットにいるときに視覚的警告
   - 車両検出と統合
   - 車線変更の安全性を向上

4. **ドライビング中の画面オフ:**
   - パラメータ: `ScreenOff`、`ScreenOffTimer`
   - タイマー後に画面をオフ（1分、3分、5分、カスタム）
   - 電力節約と気を散らす要素の削減
   - タッチまたはイベントで画面を復帰

5. **カスタム画面の明るさ:**
   - パラメータ: `Brightness`、`BrightnessControl`
   - 手動の明るさ制御（0-100%）
   - 自動または手動モード
   - 日中/夜間の独立した設定

6. **道路名表示:**
   - パラメータ: `RoadNameUI`
   - mapdから現在の道路名を表示
   - ナビゲーション認識を支援
   - マップデータが利用可能な場合のみ

7. **鉛車両情報の強化:**
   - 4レベルの詳細度（オフ、レベル1-3）
   - 鉛車両の距離を表示
   - 相対速度を表示
   - パラメータ: `ShowLeadCarIndicator`

8. **青信号アラート:**
   - パラメータ: `GreenLightAlert`
   - 信号が青になったときに通知
   - 停車時の注意維持に役立つ
   - 視覚的および聴覚的な警告

9. **停車タイマー:**
   - パラメータ: `StandstillTimer`
   - 停車時間を表示
   - 交通パターンの認識を向上
   - オプション機能

**標準openpilotとの違い:**
- 標準openpilotは最小限のUIカスタマイゼーション
- 標準openpilotにはレインボーロード、ターンシグナル表示、ブラインドスポット表示なし
- 標準openpilotは基本的な画面の明るさ制御のみ
- sunnypilotは広範なビジュアルパーソナライゼーションを提供

---

### 2.2 モデルパネル

**場所:** 設定 > モデル

**説明:** 86以上の運転モデルを管理、ダウンロード、選択するための専用パネル。

**機能:**

1. **モデルブラウジング:**
   - 利用可能なすべてのモデルのリスト
   - モデル名、バージョン、リリース日を表示
   - 各モデルの説明
   - ファジー検索機能

2. **検索とフィルター:**
   - モデル名で検索
   - お気に入りでフィルター
   - リリース日でソート
   - パラメータ: `ShowModelManagerFavorites`

3. **ダウンロード管理:**
   - ダウンロードボタン（キャッシュされていないモデル用）
   - 進捗バーとパーセンテージ
   - ダウンロード完了までの推定時間
   - 中断されたダウンロードの再開機能

4. **モデルアクティベーション:**
   - ダウンロード済みモデルから選択
   - アクティベーションを確認
   - 次回のドライブで有効化
   - 以前のモデルはキャッシュに残る

5. **ストレージ管理:**
   - キャッシュされたモデルのストレージ使用量を表示
   - キャッシュをクリアしてスペースを解放
   - 個別のモデル削除
   - 外部ストレージサポート

6. **お気に入りシステム:**
   - 頻繁に使用するモデルをお気に入りに登録
   - お気に入りビューでクイックアクセス
   - モデルごとのお気に入り切り替え

**標準openpilotとの違い:**
- 標準openpilotはリリースごとに1つのモデル
- 標準openpilotにはモデル選択インターフェースなし
- 標準openpilotは新しいモデルにOSアップデート全体が必要
- sunnypilotはアプリストアのようなモデル体験を提供

---

### 2.3 設定パネルの整理

**場所:** 設定

**説明:** sunnypilotは設定を機能別に整理された7つの主要パネルに編成しています。

**パネル:**

1. **デバイスパネル:**
   - デバイス管理
   - ストレージオプション
   - システム設定
   - アップデート管理

2. **ネットワークパネル:**
   - WiFi設定
   - テザリングオプション
   - sunnylink接続
   - データ使用

3. **切り替えパネル:**
   - 主要機能の切り替え（MADS、DEC、SLA、NNLC等）
   - 機能の有効/無効
   - 簡単なオン/オフコントロール

4. **カスタマイゼーションパネル:**
   - UI視覚カスタマイゼーション
   - レインボーロード、ターンシグナル、ブラインドスポット
   - 画面の明るさとタイムアウト
   - 表示オプション

5. **車両パネル:**
   - 車両固有の設定
   - トルクラテラルコントロールパラメータ
   - 車両セレクター
   - キャリブレーション設定

6. **コントロールパネル:**
   - 縦方向制御設定（DEC、SLA、ICBM、SCC）
   - 横方向制御設定（NNLC）
   - インタラクティビティのタイムアウト
   - 速度表示オプション
   - 開発者UIオプション

7. **トリップパネル:**
   - トリップ統計
   - 履歴データ
   - パフォーマンスメトリクス

**高度なコントロール切り替え:**
- パラメータ: `ShowAdvancedControls`
- 初心者向けに高度な設定を非表示
- 複雑さの段階的開示
- 最初の圧倒感を削減

**標準openpilotとの違い:**
- 標準openpilotはフラットな設定リスト
- 標準openpilotは機能別にカテゴリー分けなし
- 標準openpilotはすべての設定を一度に表示
- sunnypilotは整理された段階的なインターフェースを提供

---
## 3. モデル機能

### 3.1 Driving Model Manager

**場所:** `/home/takuya/work/comma/other_repo/sunny/sunnypilot/models/`

**説明:** 86以上の運転モデルを管理、ダウンロード、選択するための完全なシステム。

**主要な実装ファイル:**
- `manager.py` - モデルのダウンロードと状態管理
- `fetcher.py` - ネットワークフェッチとキャッシング
- `helpers.py` - モデルの検証とユーティリティ
- `default_model.py` - デフォルトモデル設定
- `split_model_constants.py` - モデルアーキテクチャ定数

**コア機能:**

1. **モデルリポジトリ:**
   - 執筆時点で86以上のモデル（2023年10月 - 2025年10月）
   - Night Strike（2023年10月）からThe Cool People's Models（2025年10月）までの範囲
   - 各モデルは固有の特性とトレーニングデータを持つ
   - モデルはダウンロードURIとSHA256ハッシュを含むJSONメタデータに保存

2. **ダウンロード管理:**
   - aiohttpによる非同期ダウンロード
   - 効率的なストリーミングのための128 KBチャンクサイズ
   - 進捗追跡（パーセンテージとETA）
   - ダウンロード後のSHA256検証
   - ダウンロード前の自動キャッシュチェック
   - 中断されたダウンロードの再開機能

3. **モデル状態:**
   - `pending` - ダウンロードされていない
   - `downloading` - 現在ダウンロード中
   - `cached` - すでにダウンロードされ検証済み
   - `failed` - ダウンロードまたは検証が失敗

4. **モデルバンドル構造:**
   ```
   ModelBundle:
     - name: モデル名
     - version: モデルバージョン
     - fileName: ディスク上のファイル名
     - downloadProgress: 進捗状態
     - downloadUri: SHA256を含むダウンロードURL
     - releaseDate: モデルがリリースされた日
     - description: モデルの説明
   ```

5. **アクティブモデル管理:**
   - 再起動なしでアクティブモデルを切り替え
   - モデルの互換性チェック
   - モデルが失敗した場合の自動フォールバック
   - ライブモデルパラメータの更新

**主要な関数:**

`manager.py`から:
- `_download_file()` - 進捗を含む非同期ファイルダウンロード
- `_process_artifact()` - モデルアーティファクトのダウンロードと検証
- `_calculate_eta()` - 進捗に基づくダウンロードETAの計算
- cerealメッセージングによるモデル状態の公開

`helpers.py`から:
- `verify_file()` - SHA256ハッシュ検証
- `get_active_bundle()` - 現在アクティブなモデルを取得
- モデルパスの解決と検証

**標準openpilotとの違い:**
- 標準openpilotはリリースごとに単一モデルを含む
- 標準openpilotにはダウンロードインターフェースなし
- 標準openpilotは新しいモデルのために完全なソフトウェアアップデートが必要
- 標準openpilotにはモデル選択やお気に入り機能なし
- sunnypilotは完全なモデルマーケットプレイス体験を提供

---
### 3.2 NNLC (Neural Network Lateral Control)

**場所:** `/home/takuya/work/comma/other_repo/sunny/sunnypilot/selfdrive/controls/lib/nnlc/`

**説明:** 高度なニューラルネットワークベースの横方向制御システム（旧NNFF - Neural Network FeedForward）。改善されたステアリングパフォーマンスのための学習トルク補正を提供します。

**主要な実装ファイル:**
- `nnlc.py` - メインNNLCコントローラー
- `model.py` - ニューラルネットワークトルクモデル
- `helpers.py` - ヘルパー関数とフィンガープリント
- `tests/test_fingerprint.py` - モデルフィンガープリントテスト

**コア機能:**

1. **ニューラルネットワークモデル:**
   - 現在のv_ego、横加速度、横加速度/ジャーク誤差、ロール、ピッチを取得
   - 履歴データを使用（過去0.3、0.2、0.1秒）
   - 将来の計画データを使用（0.3、0.6、1.0、1.5秒先）
   - トルク補正値を出力

2. **モデル入力:**
   - 現在の速度と加速度
   - 目標と実際の横加速度
   - ロールとピッチ（重力補正済み）
   - 履歴横加速度軌跡
   - 計画された将来の横加速度
   - セットポイントと測定値の誤差

3. **トルク制御との統合:**
   - `LatControlTorqueExtBase`を拡張
   - トルク空間でフィードフォワードを計算
   - 摩擦補正を適用
   - 低速係数調整（0 m/sで12倍、30+ m/sで0倍）

4. **ロール-ピッチ補正:**
   - ピッチに基づいてロール値を調整: `roll * cos(pitch)`
   - 重力ベクトルの回転を考慮
   - より正確な横加速度計算

5. **状態追跡:**
   - 過去の値のためのdequeを維持:
     - `lateral_accel_desired_deque` - 履歴目標横加速度
     - `roll_deque` - 履歴ロール値
     - `error_deque` - 履歴追跡誤差
   - 30フレームの履歴（100 Hzで0.3秒）

6. **モデル選択:**
   - 車両固有のモデルフィンガープリント
   - 車両に基づいて適切なNNモデルをロード
   - 一致しない場合はMOCKモデルにフォールバック
   - モデルパスは`CarParamsSP.neuralNetworkLateralControl.model`から

**主要パラメータ:**
- `NeuralNetworkLateralControl` - NNLCの有効/無効
- CarParamsSPのモデルパス（フィンガープリントによって設定）

**車両サポート:**
- トルクベースの横方向制御でのみ利用可能
- アングルベースのプラットフォーム（Ford、Nissan、Tesla）では無効
- 車両に一致するNNモデルが必要

**標準openpilotとの違い:**
- 標準openpilotは横方向制御に単純なPID + フィードフォワードを使用
- 標準openpilotはステアリングにニューラルネットワークを使用しない
- 標準openpilotは条件に適応しない固定パラメータを持つ
- NNLCは車両固有の運転データから学習
- NNLCはよりスムーズで自然なステアリングフィールを提供
- 車両ダイナミクスと路面条件のより良い補正

**パフォーマンスの利点:**
- トルク振動の削減
- カーブでのより良い追跡
- 様々な路面のより良い処理
- より人間らしいステアリング動作
- 車両負荷と条件への適応

---

### 3.3 モデル選択とダウンロード管理

**ユーザーインターフェースフロー:**

1. **モデルをブラウズ:**
   - 設定のモデルパネルに移動
   - メタデータ付きの利用可能なモデルのリストを表示
   - ファジーマッチングを使用して検索
   - お気に入りでフィルター

2. **モデルをダウンロード:**
   - リストからモデルを選択
   - ダウンロードボタンをタップ
   - パーセンテージとETAを含む進捗バーを表示
   - 検証は自動的に行われる

3. **モデルをアクティベート:**
   - ダウンロード済みモデルを選択
   - アクティベーションを確認
   - 次のドライブでモデルがアクティブになる
   - 以前のモデルはキャッシュに残る

4. **モデルを管理:**
   - クイックアクセスのためにお気に入りをマーク
   - スペースを解放するためにキャッシュをクリア
   - 新しいリリースのためにモデルリストを更新
   - ストレージ使用量を表示

**バックエンドプロセス:**

`manager.py`から:
```python
async def _process_artifact(artifact, destination_path):
    # すでにキャッシュされているかチェック
    if exists and verify_hash:
        set_status_cached()
        return
    
    # 進捗追跡でダウンロード
    await _download_file(url, path, artifact)
    
    # ダウンロード後に検証
    if not verify_hash:
        raise Exception("Verification failed")
```

**モデルメタデータ構造:**
```json
{
  "name": "Model Name",
  "version": "v1.0.0",
  "fileName": "model.thneed",
  "downloadUri": {
    "uri": "https://...",
    "sha256": "hash..."
  },
  "releaseDate": "2025-01-01",
  "description": "Model description"
}
```

**ストレージ管理:**
- モデルは永続的な場所に保存
- 起動時のキャッシュ検証
- 破損したファイルの自動クリーンアップ
- SHA256整合性チェック

**標準openpilotとの違い:**
- 標準openpilotにはモデル選択なし
- 標準openpilotは新しいモデルのために完全なOSアップデートが必要
- 標準openpilotはリリースごとに単一モデル
- sunnypilotはアプリストアのようなモデル体験を提供

---
## 4. その他の主要機能

### 4.1 sunnylink統合

**場所:** `/home/takuya/work/comma/other_repo/sunny/sunnypilot/sunnylink/`

**説明:** 設定のバックアップ/リストアとリモート管理のためのクラウド接続サービス。

**主要ファイル:**
- `api.py` - sunnylink APIクライアント
- `statsd.py` - 統計追跡
- `params_metadata.json` - パラメータメタデータと説明

**コア機能:**

1. **設定のバックアップ/リストア:**
   - 永続的なパラメータの自動バックアップ
   - 設定のクラウドストレージ
   - 新しいデバイスで設定をリストア
   - バージョン追跡

2. **ユーザー管理:**
   - sunnylink dongle ID登録
   - ユーザーロールと権限
   - マルチデバイスサポート
   - オフラインアクセス用のキャッシュ

3. **統計収集:**
   - ドライビング統計のアップロード
   - 機能使用状況の追跡
   - パフォーマンスメトリクス
   - ユーザーの同意が必要

4. **リモート管理（保留中）:**
   - リモート設定のためのインフラストラクチャ準備済み
   - 将来: リモートモデル切り替え
   - 将来: リモートパラメータ更新
   - 利用可能性のアナウンスは保留中

**主要パラメータ:**
- `SunnylinkEnabled` - sunnylinkの有効/無効（デフォルト: 有効）
- `EnableSunnylinkUploader` - 統計アップロードを有効化
- `SunnylinkDongleId` - 固有のデバイス識別子
- `SunnylinkCache_Roles` - キャッシュされたユーザーロール
- `SunnylinkCache_Users` - キャッシュされたユーザーリスト
- `LastSunnylinkPingTime` - 最後の接続時刻
- `SunnylinkTempFault` - 一時的な接続障害フラグ

**バックアップマネージャー:**
- パラメータ:
  - `BackupManager_CreateBackup` - バックアップ作成をトリガー
  - `BackupManager_RestoreVersion` - リストアするバージョン
- すべてのPERSISTENT | BACKUPフラグ付きパラメータをバックアップ
- デバイス間で設定をリストア
- ロールバック用のバージョン管理

**標準openpilotとの違い:**
- 標準comma.aiはドライブデータ用のcomma connectを持つ
- 標準openpilotには設定のバックアップ/リストアなし
- 標準openpilotにはリモート管理インフラストラクチャなし
- sunnylinkは完全なクラウド設定エコシステムを追加

---

### 4.2 mapd統合

**場所:** `/home/takuya/work/comma/other_repo/sunny/third_party/mapd_pfeiferj/`

**説明:** オフラインOpenStreetMapデータアクセスのためのpfeiferjのmapd統合。

**ソース:** https://github.com/pfeiferj/openpilot-mapd

**コア機能:**

1. **オフラインマップデータベース:**
   - 地域のOpenStreetMapデータをダウンロード
   - デバイスにローカル保存
   - インターネット接続なしでアクセス
   - 効率性のためgzipで圧縮

2. **速度制限データ:**
   - OSMからの道路レベルの速度制限
   - マップ変更の更新が利用可能
   - 次の速度制限変更までの距離
   - 複数の速度制限タイプ（標準、推奨等）

3. **データベース管理:**
   - 地域ダウンロード（マップ上でエリアを選択）
   - 既存のデータベースを更新
   - 内部または外部ストレージに保存
   - バージョン追跡

4. **統合ポイント:**
   - Speed Limit Resolver（マップ速度制限を使用）
   - Smart Cruise Control Map（カーブデータ）
   - 道路名表示
   - ナビゲーション支援

**主要パラメータ:**
- `MapdVersion` - インストールされたmapdバージョン
- `MapSpeedLimit` - 現在のマップ速度制限
- `NextMapSpeedLimit` - 次の速度制限変更（JSON）
- `MapAdvisorySpeedLimit` - 推奨速度制限
- `OSMDownloadBounds` - ダウンロードエリア境界
- `OSMDownloadLocations` - ダウンロード済みの場所（JSON）
- `OsmDownloadedDate` - 最後のダウンロード日
- `OSMDownloadProgress` - ダウンロード進捗（JSON）
- `OsmLocal` - ローカルOSMデータベースを使用
- `OsmLocationName` - 選択された場所名
- `Offroad_OSMUpdateRequired` - 更新通知
- `OsmDbUpdatesCheck` - 更新をチェック
- `RoadName` - 現在の道路名

**UI機能:**
- 場所/地域を検索
- 利用可能なダウンロードを表示
- ダウンロードの進捗追跡
- ストレージ使用量の表示
- データベース更新通知

**標準openpilotとの違い:**
- 標準openpilotにはオフラインマップデータなし
- 標準openpilotにはマップからの速度制限検出なし
- 標準openpilotは車の速度制限が利用可能な場合のみそれに依存
- mapdは包括的なオフラインナビゲーションデータを提供

---
### 4.3 Vehicle Selector

**説明:** 自動的にフィンガープリントされない車両のための手動車両選択。

**コア機能:**

1. **プラットフォーム選択:**
   - サポートされている車両のリストをブラウズ
   - メーカー/モデル/年で検索
   - 自動フィンガープリントが失敗したときに選択
   - 自動選択をオーバーライド

2. **プラットフォームバンドル:**
   - パラメータ: `CarPlatformBundle`（JSON）
   - 選択されたプラットフォーム情報を保存
   - 車が自動検出されないときに使用
   - エッジケースの車両でsunnypilotを使用可能に

3. **フィンガープリントのオーバーライド:**
   - 手動プラットフォーム選択
   - 開発に便利
   - 異なる車インターフェースのテスト
   - カスタム/改造車両のサポート

**UIフロー:**
1. サポートされていない/検出されない車で起動
2. システムが手動選択を促す
3. ユーザーが車両を検索
4. システムが適切な車インターフェースをロード
5. 将来の起動のために設定が保存される

**標準openpilotとの違い:**
- 標準openpilotは正確なフィンガープリントマッチが必要
- 標準openpilotには手動選択なし
- 標準openpilotはマッチなしでは起動しない
- Vehicle Selectorはフォールバックオプションを提供

---

### 4.4 外部ストレージサポート

**説明:** ダウンロードとデータのために外部ストレージを使用する機能。

**機能:**

1. **ストレージの場所:**
   - 内部デバイスストレージ
   - USB外部ドライブ
   - SDカード（サポートされているハードウェア上）

2. **使用例:**
   - モデルストレージ（大きなモデルファイル）
   - OSMデータベースストレージ
   - ログと録画
   - 拡張容量

3. **管理:**
   - 外部ストレージの自動検出
   - ストレージ場所間のデータ移行
   - ストレージスペースの監視
   - クリーンアップユーティリティ

**標準openpilotとの違い:**
- 標準openpilotは内部ストレージのみを使用
- 標準openpilotは限られたストレージ容量
- 外部ストレージにより、はるかに大きなモデルライブラリが可能
- 地域全体のオフラインマップデータが可能

---

### 4.5 高度なコントロール切り替え

**説明:** 設定の複雑さのための段階的開示システム。

**パラメータ:** `ShowAdvancedControls`

**機能:**

1. **基本モード（高度オフ）:**
   - 必須設定のみを表示
   - 新規ユーザー向けの簡素化されたインターフェース
   - 複雑なチューニングパラメータを非表示
   - 認知負荷の削減

2. **高度モード（高度オン）:**
   - 利用可能なすべての設定を表示
   - チューニングパラメータが可視
   - エキスパートレベルの制御
   - 完全なカスタマイゼーションアクセス

3. **影響を受ける設定:**
   - トルク横方向制御パラメータ
   - モデル固有のチューニング
   - 開発者オプション
   - 実験的機能

**標準openpilotとの違い:**
- 標準openpilotはすべてのユーザーにすべての設定を表示
- 標準openpilotには複雑さの管理なし
- 高度な切り替えはより良いオンボーディングを提供
- 新規ユーザーの圧倒感を削減

---
## 5. 実装アーキテクチャ

### 5.1 cerealメッセージ定義

sunnypilotはopenpilotのcerealスキーマをカスタムメッセージで拡張しています:

**場所:** `cereal/custom.capnp`

**主要な構造:**

1. **ModularAssistiveDrivingSystem:**
   - 状態列挙型（disabled、enabled、paused等）
   - 設定フラグ
   - イベント処理

2. **IntelligentCruiseButtonManagement:**
   - ステートマシン状態
   - ボタンコマンド（none、increase、decrease）
   - 目標速度追跡

3. **LongitudinalPlanSP:**
   - ソース（acc、blended、e2e）
   - DEC状態とステータス
   - 速度制限情報
   - Smart Cruise Control状態
   - 目標速度と加速度

4. **CarParamsSP:**
   - sunnypilot固有の車パラメータ
   - 機能利用可能性フラグ
   - 車両固有のチューニング

**メッセージフロー:**
```
selfdriveState -> MADS -> longitudinalPlanSP
                      ├─> DEC
                      ├─> SLA
                      ├─> SCC-M/V
                      └─> ICBM
```

---

### 5.2 パラメータシステム

**場所:** `openpilot/common/params_keys.h`

**パラメータフラグ:**
- `PERSISTENT` - 再起動後も残る
- `BACKUP` - sunnylinkバックアップに含まれる
- `CLEAR_ON_MANAGER_START` - マネージャー起動時にリセット
- `CLEAR_ON_ONROAD_TRANSITION` - オンロード移行時にリセット
- `CLEAR_ON_OFFROAD_TRANSITION` - オフロード移行時にリセット

**タイプ:**
- `BOOL` - ブール値true/false
- `INT` - 整数値
- `FLOAT` - 浮動小数点
- `STRING` - 文字列値
- `JSON` - JSON構造化データ
- `BYTES` - バイナリデータ

**整理:**
- 標準openpilotパラメータ
- sunnypilot一般パラメータ（135-178行）
- MADSパラメータ（180-184行）
- Model Managerパラメータ（186-191行）
- NNLCパラメータ（194行）
- sunnylinkパラメータ（197-205行）
- Backup managerパラメータ（207-209行）
- 車両固有パラメータ（211-215行）
- モデル固有パラメータ（218-223行）
- mapdパラメータ（227-243行）
- 速度制限パラメータ（246-249行）
- SCCパラメータ（252-253行）
- トルクパラメータ（256-262行）

---

### 5.3 標準openpilotとの統合

sunnypilotは標準openpilotアーキテクチャとの互換性を維持しています:

1. **安全性コンプライアンス:**
   - すべての上流安全テストを採用
   - ドライバー監視を保持
   - アクチュエーションチェックを維持
   - 安全ポリシーに従う

2. **コアコンポーネント:**
   - 拡張機能付きの標準controlsdを使用
   - 標準longitudinal_plannerに統合
   - ラテラルコントローラーを拡張するが置き換えない
   - 標準ステートマシンを保持

3. **拡張ポイント:**
   - `controlsd_ext.py` - 拡張コントロール
   - `LongitudinalPlannerSP` - 拡張ロングプランナー
   - `LatControlTorqueExtBase` - 拡張ラテラルコントロール
   - `CarParamsSP` - 拡張車パラメータ

4. **メッセージング:**
   - 追加cerealメッセージ（custom.capnp）
   - すべての標準メッセージを保持
   - 拡張機能はオプション/グレースフルデグラデーション

---

## 6. 比較サマリーテーブル

### 6.1 縦方向制御の比較

| 機能 | 標準openpilot | sunnypilot |
|------|--------------|-----------|
| 基本ACC | ✅ 単一モード | ✅ マルチモード（MADS、DEC） |
| 実験モード | ✅ 手動切り替え | ✅ 自動（DEC） |
| 速度制限検出 | ❌ なし | ✅ 車 + マップ + 組み合わせ（SLA） |
| クルーズボタン管理 | ❌ なし | ✅ 完全自動化（ICBM） |
| カーブ速度制御 | ⚠️ モデルのみ | ✅ ビジョン + マップ（SCC-V/M） |
| 速度ソース優先順位 | N/A | ✅ 設定可能なポリシー |
| オフラインマップ統合 | ❌ なし | ✅ OSM付きmapd |

### 6.2 UIカスタマイゼーションの比較

| 機能 | 標準openpilot | sunnypilot |
|------|--------------|-----------|
| 設定の整理 | ⚠️ フラットリスト | ✅ カテゴリー別パネル |
| 高度/基本モード | ❌ すべて表示 | ✅ 段階的開示 |
| レインボーロード | ❌ なし | ✅ オプション |
| ターンシグナル表示 | ❌ なし | ✅ オプション |
| ブラインドスポット表示 | ❌ なし | ✅ オプション |
| ドライビング中画面オフ | ❌ なし | ✅ 設定可能 |
| 道路名表示 | ❌ なし | ✅ マップデータから |
| 鉛車両情報 | ⚠️ 基本 | ✅ 強化版（4レベル） |
| 青信号アラート | ❌ なし | ✅ オプション |
| 停車タイマー | ❌ なし | ✅ オプション |
| 明るさ制御 | ⚠️ 自動のみ | ✅ 手動 + 自動 |

### 6.3 モデル管理の比較

| 機能 | 標準openpilot | sunnypilot |
|------|--------------|-----------|
| モデル選択 | ❌ リリース毎に1つ | ✅ 86以上のモデル |
| モデルダウンロード | ❌ OSアップデートのみ | ✅ アプリ内ダウンロード |
| モデルお気に入り | ❌ なし | ✅ 完全なシステム |
| モデル検索 | ❌ なし | ✅ ファジー検索 |
| ダウンロード進捗 | N/A | ✅ 進捗 + ETA |
| モデル検証 | ⚠️ 暗黙的 | ✅ 明示的SHA256 |
| ストレージ管理 | N/A | ✅ キャッシュ管理 |

### 6.4 エンゲージメントモデルの比較

| 機能 | 標準openpilot | sunnypilot（MADS） |
|------|--------------|------------------|
| エンゲージメント状態 | ✅ 有効/無効 | ✅ 有効/一時停止/無効/オーバーライド中/ソフト無効化中 |
| ブレーキ動作 | ❌ 常に解除 | ✅ 一時停止または解除（設定可能） |
| サイレントLKAS | ❌ なし | ✅ 一時停止状態サポート |
| 統合モード | ❌ ACCメインが必要 | ✅ オプションで直接エンゲージメント |
| メインクルーズなし | ❌ 必須 | ✅ サポート（Tesla等） |
| イベント処理 | ⚠️ 基本 | ✅ 一時停止状態用のサイレントバリアント |

---

## 7. ファイル構造の比較

### 7.1 主要ディレクトリ（sunnypilot追加分）

```
sunnypilot/
├── mads/                          # MADS実装
│   ├── mads.py
│   ├── state.py
│   └── helpers.py
├── selfdrive/
│   ├── car/
│   │   └── intelligent_cruise_button_management/  # ICBM
│   │       ├── controller.py
│   │       └── helpers.py
│   └── controls/
│       └── lib/
│           ├── dec/               # Dynamic Experimental Control
│           │   ├── dec.py
│           │   └── constants.py
│           ├── speed_limit/       # Speed Limit Assist
│           │   ├── speed_limit_assist.py
│           │   ├── speed_limit_resolver.py
│           │   ├── common.py
│           │   └── helpers.py
│           ├── smart_cruise_control/  # SCC-M/V
│           │   ├── smart_cruise_control.py
│           │   ├── vision_controller.py
│           │   └── map_controller.py
│           └── nnlc/              # Neural Network Lateral Control
│               ├── nnlc.py
│               ├── model.py
│               └── helpers.py
├── models/                        # Model Manager
│   ├── manager.py
│   ├── fetcher.py
│   ├── helpers.py
│   └── default_model.py
├── sunnylink/                     # sunnylink統合
│   ├── api.py
│   ├── statsd.py
│   └── params_metadata.json
└── mapd/                          # mapd統合ラッパー
```

### 7.2 変更された標準ファイル

**拡張されたが置き換えられていない:**
- `selfdrive/controls/lib/longitudinal_planner.py` → `LongitudinalPlannerSP`でラップ
- `selfdrive/controls/lib/latcontrol_torque.py` → `LatControlTorqueExt`で拡張
- `selfdrive/car/interfaces.py` → sunnypilot初期化で拡張
- `selfdrive/selfdrived/selfdrived.py` → MADS、ICBM統合を追加

**新しいcerealメッセージ:**
- `cereal/custom.capnp` - すべてのsunnypilot固有のメッセージタイプ

**拡張されたパラメータ:**
- `common/params_keys.h` - すべてのsunnypilotパラメータ

---
## 8. 重要なポイント

### 8.1 哲学の違い

**標準openpilot:**
- フォーカス: プロダクション対応、最小限の機能、最大限の安全性
- アプローチ: 保守的、遅い機能展開
- カスタマイゼーション: 最小限、ワンサイズフィットモスト
- モデル: リリースごとに慎重にテストされた単一モデル
- UI: 機能的、最小限のカスタマイゼーション

**sunnypilot:**
- フォーカス: 機能豊富、ユーザーの選択、安全性の維持
- アプローチ: 迅速な機能開発、広範なテスト
- カスタマイゼーション: 最大限のユーザーコントロールとパーソナライゼーション
- モデル: 大規模なライブラリからのユーザー選択
- UI: 洗練され、広範なカスタマイゼーションオプション

### 8.2 安全性へのアプローチ

両方とも安全性を維持していますが、方法が異なります:

**標準openpilot:**
- 最小限の機能 = 潜在的な問題が少ない
- 単一のテスト済み構成
- 保守的な制限

**sunnypilot:**
- より多くの機能 = より多くのテストが必要
- 複数の構成をサポート
- 標準の安全テストを維持
- 追加の検証レイヤーを追加
- 構成選択のユーザー責任

### 8.3 対象ユーザー

**標準openpilot:**
- 一般ユーザー
- 「ただ動く」体験を求める
- カスタマイゼーションが不要
- comma.aiの決定を信頼

**sunnypilot:**
- 愛好家ユーザー
- 動作のコントロールを求める
- 設定を行う意欲がある
- 最新機能を迅速に求める
- 選択肢とカスタマイゼーションを評価

---

## 9. 結論

sunnypilotは、安全性コンプライアンスを維持しながら標準openpilotに対する重要な強化を表しています。主な差別化要因は次のとおりです:

1. **MADS** - 一時停止状態を含むより柔軟なエンゲージメントモデル
2. **DEC** - ACCモードとブレンドモード間のインテリジェントな自動切り替え
3. **SLA** - 完全な速度制限検出と自動クルーズ調整
4. **ICBM** - 非openpilot縦制御車両向けハイブリッド縦方向制御
5. **SCC-M/V** - ビジョンとマップデータを使用した高度なカーブ速度制御
6. **NNLC** - よりスムーズなステアリングのためのニューラルネットワークベース横方向制御
7. **Model Manager** - アプリ内ダウンロードと選択が可能な86以上のモデル
8. **UIカスタマイゼーション** - 広範なビジュアルオプションと設定の整理
9. **sunnylink** - クラウド設定バックアップと将来のリモート管理
10. **mapd** - 速度制限とナビゲーション用のオフラインマップデータ

コードベースは、標準とsunnypilot固有のコード間の明確な分離で適切に整理されています。拡張機能は変更ではなく継承とコンポジションを通じて実装されており、メンテナンス可能で、上流の更新を容易にしています。

比較ドキュメントでは、以下に焦点を当ててください:
- 柔軟なエンゲージメントの基盤としてのMADS
- 高度な縦方向スイートとしてのDEC/SLA/ICBM/SCC
- 改善された横方向制御のためのNNLC
- ユーザーの選択のためのModel Manager
- より良いユーザー体験のためのUI強化
- 標準をクリーンに拡張する方法を示す統合アーキテクチャ

すべての機能は、標準openpilotよりも大幅に多くの機能とユーザーコントロールを提供しながら、安全性第一のアプローチを維持しています。

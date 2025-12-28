# sunnypilot - 縦制御の詳細比較

## 概要
sunnypilotと本家openpilot（あなたのフォーク）の縦制御（加減速制御）の違いを詳しく比較します。

sunnypilotは公式openpilotをベースに、より高度な縦制御機能とユーザーカスタマイズ性を追加したフォークです。

---

## 1. MADS (Modular Assistive Driving System)

### sunnypilot独自の統合制御システム

**ファイルパス**: `/home/takuya/work/comma/other_repo/sunny/sunnypilot/mads/`

MADSは、sunnypilotの中核をなすモジュール化されたアシスト運転システムです。

#### 主な特徴

1. **柔軟なエンゲージメント**
   - 横制御と縦制御を独立して有効化/無効化可能
   - 一時停止状態（pause）をサポート
   - 従来のopenpilotより細かい制御が可能

2. **状態管理**
```python
# sunnypilot/mads/mads.py
class MADSState:
    disabled = 0
    enabled = 1
    paused = 2  # 一時停止状態（sunnypilot独自）
```

3. **エンゲージメントロジック**
```python
# 横制御と縦制御を個別に管理
if mads.active:
    if mads.lateral_enabled:
        # 横制御を実行
    if mads.longitudinal_enabled:
        # 縦制御を実行
```

#### 本家との違い

| 項目 | 本家openpilot | sunnypilot (MADS) |
|------|--------------|-------------------|
| エンゲージ | 一括ON/OFF | 横/縦を個別制御 |
| 一時停止 | なし | あり（pause状態） |
| 復帰方法 | 完全リセット | pause状態から即復帰 |
| カスタマイズ性 | 低 | 高 |

---

## 2. DEC (Dynamic Experimental Control)

### 動的な実験モード切り替え

**ファイルパス**: `/home/takuya/work/comma/other_repo/sunny/sunnypilot/selfdrive/controls/lib/dec/`

DECは、走行状況に応じて自動的にExperimentalモード（e2e縦制御）を切り替えるシステムです。

#### 動作原理

```python
# dec_controller.py
class DECController:
    def update(self, v_ego, curve_speed, lead_distance):
        # 条件に基づいてexperimentalモードを自動切り替え
        if self.should_enable_experimental(v_ego, curve_speed, lead_distance):
            return True  # Experimental ON
        else:
            return False  # Experimental OFF
```

#### 切り替え条件（例）

1. **カーブ検出時**
   - カーブ速度が閾値以下 → Experimental ON
   - 直線道路 → Experimental OFF

2. **先行車との距離**
   - 近距離 → Experimental ON（細かい制御）
   - 遠距離 → Experimental OFF（効率的な制御）

3. **速度範囲**
   - 低速（< 50 km/h） → Experimental ON
   - 高速（> 80 km/h） → Experimental OFF

#### 本家との違い

| 項目 | 本家openpilot | sunnypilot (DEC) |
|------|--------------|------------------|
| Experimentalモード | 手動切り替え | 自動切り替え |
| 切り替え基準 | ユーザー判断 | AI/状況判断 |
| 最適化 | なし | 走行状況に応じて自動最適化 |

---

## 3. SLA (Speed Limit Assist)

### 速度制限アシスト

**ファイルパス**: `/home/takuya/work/comma/other_repo/sunny/sunnypilot/selfdrive/controls/lib/speed_limit/`

SLAは、制限速度情報を取得してクルーズ速度を自動調整する機能です。

#### コンポーネント

1. **Speed Limit Resolver** (`speed_limit_resolver.py`)
   - 複数のソースから制限速度を取得
   - 優先順位: 車両CAN > マップデータ > 組み合わせ

```python
class SpeedLimitResolver:
    def __init__(self):
        self.limit_solutions = {}  # 各ソースの制限速度
        self.distance_solutions = {}  # 制限速度開始地点までの距離
    
    def resolve(self):
        # 最適な制限速度を選択
        if self.car_limit:
            return self.car_limit
        elif self.map_limit:
            return self.map_limit
        else:
            return None
```

2. **Speed Limit Assist** (`speed_limit_assist.py`)
   - クルーズ速度を自動調整

```python
# 制限速度に応じてクルーズ速度を調整
if sla_active:
    if current_speed > speed_limit + offset:
        # 減速
        target_speed = speed_limit + offset
    else:
        # 現在の速度を維持
        target_speed = current_speed
```

#### SLAの状態

```python
class SpeedLimitAssistState:
    disabled = 0       # 無効
    inactive = 1       # 非アクティブ（制限速度情報なし）
    preActive = 2      # 準備中（制限速度検出）
    adapting = 3       # 調整中（速度を変更中）
    active = 4         # アクティブ（制限速度に追従）
```

#### データソース

1. **車両CAN**
   - 車両の標識認識システムから取得
   - リアルタイム性が高い

2. **マップデータ（mapd統合）**
   - OpenStreetMapの制限速度情報
   - オフラインでも利用可能
   - カバレッジが広い

3. **組み合わせモード**
   - 車両とマップの両方を使用
   - より正確な情報を選択

#### 調整ロジック

```python
# speed_limit_assist.py
LIMIT_ADAPT_ACC = -1.0  # m/s^2 減速時の理想加速度

def calculate_target_speed(current_limit, distance_to_limit):
    # 制限速度開始地点までの距離に応じて調整
    if distance_to_limit < 100:  # 100m以内
        return current_limit  # すぐに制限速度に合わせる
    else:
        # 徐々に減速
        return interpolate(current_speed, current_limit, distance_to_limit)
```

#### 本家との違い

| 項目 | 本家openpilot | sunnypilot (SLA) |
|------|--------------|------------------|
| 制限速度取得 | なし | 車両CAN + マップデータ |
| 自動調整 | なし | あり（自動クルーズ速度調整） |
| オフラインマップ | なし | あり（mapd統合） |
| UI表示 | なし | 制限速度表示 + 警告 |
| カスタマイズ | なし | オフセット調整可能 |

---

## 4. ICBM (Intelligent Cruise Button Management)

### インテリジェント・クルーズボタン管理

**ファイルパス**: `/home/takuya/work/comma/other_repo/sunny/sunnypilot/selfdrive/controls/lib/icbm/`

ICBMは、車両のクルーズコントロールボタンコマンドを管理して速度を制御するシステムです。

#### 動作原理

```python
# icbm_controller.py
class ICBMController:
    def send_cruise_button_command(self, target_speed, current_speed):
        if target_speed > current_speed:
            # 加速ボタンを送信
            return CruiseButton.ACCEL_SET
        elif target_speed < current_speed:
            # 減速ボタンを送信
            return CruiseButton.DECEL_SET
        else:
            # ボタン送信なし
            return CruiseButton.CANCEL
```

#### 使用ケース

1. **ハイブリッド縦制御**
   - 車両のACCは有効
   - openpilotがボタンコマンドで速度調整
   - 完全な縦制御より安全性が高い

2. **制限速度への自動調整**
   - SLAと組み合わせて使用
   - 制限速度に応じて自動でボタン送信

#### 本家との違い

| 項目 | 本家openpilot | sunnypilot (ICBM) |
|------|--------------|-------------------|
| ボタンコマンド | なし | あり |
| ハイブリッド制御 | なし | あり（ACC + ボタン制御） |
| 速度調整 | アクセル/ブレーキ直接制御 | クルーズボタン経由 |
| 安全性 | 高（完全制御） | より高（車両ACC併用） |

---

## 5. SCC-M / SCC-V (Smart Cruise Control - Map / Vision)

### カーブ速度制御

**ファイルパス**: `/home/takuya/work/comma/other_repo/sunny/sunnypilot/selfdrive/controls/lib/scc/`

SCC-MとSCC-Vは、カーブ進入時に速度を自動調整する機能です。

#### SCC-M (Map-based)

```python
# scc_map.py
class SCCMap:
    def get_curve_speed(self, location, heading):
        # マップデータからカーブ情報を取得
        curve_radius = self.get_curve_radius_from_map(location, heading)
        
        # カーブ半径から適切な速度を計算
        safe_speed = calculate_safe_curve_speed(curve_radius)
        return safe_speed
```

#### SCC-V (Vision-based)

```python
# scc_vision.py
class SCCVision:
    def get_curve_speed(self, model_output):
        # モデルのパス予測からカーブを検出
        path_curvature = model_output.path.curvature
        
        # 曲率から適切な速度を計算
        safe_speed = calculate_safe_curve_speed_from_curvature(path_curvature)
        return safe_speed
```

#### 統合制御

```python
# scc_controller.py
def update(self, v_ego, map_curve_speed, vision_curve_speed):
    # 両方の情報を使用して最適な速度を決定
    curve_speed = min(map_curve_speed, vision_curve_speed)
    
    if v_ego > curve_speed:
        # カーブ前に減速
        target_accel = calculate_decel_for_curve(v_ego, curve_speed)
        return target_accel
    else:
        # 通常の加速度
        return normal_accel
```

#### 本家との違い

| 項目 | 本家openpilot | sunnypilot (SCC-M/V) |
|------|--------------|---------------------|
| カーブ検出 | モデルのみ | マップ + モデル |
| カーブ速度調整 | 基本的 | 高度（事前減速） |
| マップ活用 | なし | あり（mapd統合） |
| 快適性 | 標準 | 向上（スムーズな減速） |

---

## 6. 縦制御パラメータのカスタマイズ

### sunnypilot独自のチューニング機能

**ファイルパス**: `/home/takuya/work/comma/other_repo/sunny/sunnypilot/selfdrive/car/interfaces.py`

#### カスタムパラメータ（CP_SP）

```python
# interfaces.py
class CarInterfaceBase:
    def get_params(self, candidate, fingerprint):
        # 標準パラメータ（本家互換）
        ret = super().get_params(candidate, fingerprint)
        
        # sunnypilot拡張パラメータ
        ret_sp = CarParamsSP()
        ret_sp.longitudinal.accelMin = -3.5  # カスタム最小加速度
        ret_sp.longitudinal.accelMax = 2.5   # カスタム最大加速度
        
        return ret, ret_sp
```

#### 利用可能なカスタマイズ

1. **加速度リミット**
   - 最小/最大加速度の調整
   - 車種ごとに最適化可能

2. **ジャーク制御**
   - 加速度の変化率を制限
   - より快適な加減速

3. **停止制御**
   - 停止時のブレーキ圧調整
   - スムーズな停止

---

## 7. 縦制御の実装構造比較

### アーキテクチャの違い

#### 本家openpilot

```
controlsd.py
  ├─ longitudinal_planner.py
  │    └─ 目標速度/加速度計算
  ├─ long_mpc.py
  │    └─ MPC（モデル予測制御）
  └─ car_controller.py
       └─ アクチュエーター制御
```

#### sunnypilot

```
controlsd.py + controlsd_ext.py (sunnypilot拡張)
  ├─ MADS (mads/)
  │    ├─ エンゲージメント管理
  │    └─ 状態遷移制御
  │
  ├─ DEC (dec/)
  │    └─ 動的Experimentalモード切り替え
  │
  ├─ SLA (speed_limit/)
  │    ├─ speed_limit_resolver.py (制限速度取得)
  │    └─ speed_limit_assist.py (速度調整)
  │
  ├─ ICBM (icbm/)
  │    └─ クルーズボタンコマンド管理
  │
  ├─ SCC (scc/)
  │    ├─ SCC-M (マップベース)
  │    └─ SCC-V (ビジョンベース)
  │
  ├─ longitudinal_planner.py
  │    └─ 上記機能を統合した目標速度計算
  │
  └─ car_controller.py
       └─ アクチュエーター制御
```

---

## 8. 主要な違いまとめ

| 機能 | 本家openpilot | sunnypilot |
|------|--------------|-----------|
| **基本縦制御** | 標準MPC制御 | MPC + 拡張機能 |
| **エンゲージメント** | 一括ON/OFF | MADS（個別制御） |
| **Experimentalモード** | 手動切り替え | DEC（自動切り替え） |
| **制限速度対応** | なし | SLA（自動調整） |
| **クルーズボタン制御** | なし | ICBM |
| **カーブ速度制御** | 基本的 | SCC-M/V（高度） |
| **マップ活用** | なし | あり（mapd統合） |
| **カスタマイズ性** | 低 | 高（多数のパラメータ） |
| **UI設定** | 少ない | 豊富（専用パネル） |
| **安全性** | 高 | より高（複数の制御モード） |

---

## 9. 使用例シナリオ

### シナリオ1: 高速道路走行

**本家openpilot**:
1. ACCをON
2. openpilotがエンゲージ
3. 一定速度で巡航
4. 前車に追従

**sunnypilot**:
1. ACCをON（MADSモード）
2. 横制御と縦制御を個別にエンゲージ可能
3. SLAが制限速度を検出
4. 制限速度に応じて自動調整
5. DECが状況に応じてExperimentalモード切り替え
6. SCC-Mがカーブを検出して事前減速

### シナリオ2: 市街地走行

**本家openpilot**:
1. 低速追従
2. 停止/発進を繰り返し

**sunnypilot**:
1. MADSで細かい制御
2. SLAが頻繁な速度制限変更に対応
3. SCC-Vがカーブを検出
4. ICBMで車両ACCと協調制御（より安全）

---

## 10. 設定とカスタマイズ

### sunnypilotの設定UI

**パス**: Settings > Longitudinal パネル

利用可能な設定:
- MADS有効化/無効化
- DEC感度調整
- SLAオフセット設定（制限速度 + X km/h）
- SLA有効化/無効化
- ICBM有効化/無効化
- SCC-M/V有効化/無効化
- カスタム加速度リミット

### 本家openpilot

基本的な設定のみ:
- Experimentalモード ON/OFF
- 車間距離設定

---

## まとめ

sunnypilotは、本家openpilotの縦制御機能を大幅に拡張し、以下を実現しています：

1. **より細かい制御**: MADS、DEC、ICBMによる柔軟な制御
2. **インテリジェントな自動化**: SLA、SCC-M/Vによる自動速度調整
3. **安全性の向上**: 複数の制御モードと車両ACCとの協調
4. **ユーザー体験の向上**: 豊富なカスタマイズオプションと直感的なUI

本家openpilotがシンプルで信頼性の高い縦制御を提供する一方、sunnypilotはより高度で快適な運転体験を提供します。

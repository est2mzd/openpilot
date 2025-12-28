# panda セーフティチェック詳細

## 概要

**セーフティチェック**は、panda のファームウェアが実行する**最重要機能**であり、openpilot からの制御コマンドが安全かどうかをリアルタイムで検証します。

### なぜセーフティチェックが必要か？

```
【セーフティチェックがない場合】
openpilot (バグあり) → 危険なコマンド → 車両 → 事故
                      (例: 急激なステアリング)

【セーフティチェックがある場合】
openpilot (バグあり) → 危険なコマンド → panda (検出) → ブロック → 車両は安全
                      (例: 急激なステアリング)  ↓
                                              ログ記録
```

**二重の安全性**:
1. **ソフトウェア層**: openpilot 自体が安全な制御を計算
2. **ハードウェア層**: panda が物理的に危険なコマンドをブロック（最後の砦）

**根拠**: panda の設計思想は「openpilot を信用しない」こと。ソフトウェアは常にバグを持つ可能性があるため、独立したハードウェアチェックが必須。

---

## セーフティチェックの仕組み

### 基本構造

```c
// board/main.c より
#include "opendbc/safety/safety.h"

// メインループ
while(1) {
    // 1. 車両からのCANメッセージ受信
    CANPacket_t rx_packet = can_pop(bus);
    
    // 2. セーフティフック（車両状態の更新）
    bool valid = safety_rx_hook(&rx_packet);
    if (valid) {
        // openpilot に転送
        send_to_host(&rx_packet);
    }
    
    // 3. openpilot からの制御コマンド受信
    CANPacket_t tx_packet = receive_from_host();
    
    // 4. セーフティフック（コマンドの検証）
    bool safe = safety_tx_hook(&tx_packet);
    if (safe) {
        // 車両に送信
        can_push(&tx_packet);
    } else {
        // 危険なコマンド → ブロック
        log_violation(&tx_packet);
    }
}
```

**根拠**: `board/main.c` と opendbc/safety のインターフェース設計

---

## セーフティフック関数

### 1. safety_rx_hook() - 受信フック

**目的**: 車両からのCANメッセージを監視し、車両状態を更新する

```c
bool safety_rx_hook(CANPacket_t *to_push) {
    // CANアドレスに応じて処理を分岐
    uint32_t addr = GET_ADDR(to_push);
    
    switch(addr) {
        // ブレーキペダルの状態
        case 0x1D2:  // Toyota の例
            brake_pressed = GET_BYTE(to_push, 0) & 0x20;
            if (brake_pressed) {
                controls_allowed = false;  // ドライバーが介入
            }
            break;
        
        // ステアリングトルク（ドライバー入力）
        case 0x260:
            driver_torque = GET_BYTES(to_push, 1, 2);
            // ドライバーが強くハンドルを握った → 制御解除
            if (abs(driver_torque) > DRIVER_TORQUE_THRESHOLD) {
                controls_allowed = false;
            }
            break;
        
        // 車速
        case 0xAA:
            vehicle_speed = GET_BYTES(to_push, 4, 2);
            // 低速では制御を許可しない（安全のため）
            if (vehicle_speed < MIN_SPEED) {
                controls_allowed = false;
            }
            break;
        
        // ACC の ON/OFF 状態
        case 0x1D3:
            cruise_engaged = GET_BIT(to_push, 0, 5);
            if (!cruise_engaged) {
                controls_allowed = false;  // ACC OFF → 制御解除
            }
            break;
    }
    
    return true;  // メッセージは有効
}
```

**主要な監視項目**:
- **ブレーキペダル**: 踏まれたら即座に `controls_allowed = false`
- **ステアリングトルク**: ドライバーが強く介入したら制御解除
- **車速**: 最低速度未満では制御不可
- **ACC状態**: ACC が OFF なら制御不可
- **ギア**: P/R では制御不可
  - **P (Park)**: 駐車ギア - 車両が停止している状態
  - **R (Reverse)**: 後退ギア - **バック駐車時などに使用**
  - **N (Neutral)**: ニュートラル - 動力が伝わらない状態
  - **D (Drive)**: 前進ギア - **openpilot が制御可能なのはこれのみ**
  - **B (Brake/Low)**: エンジンブレーキギア（一部車種）- 制御可能

**なぜ R (Reverse) では制御不可なのか？**:
- **openpilot の設計**: 前方カメラで前方の道路を認識することを前提としている
- **センサー配置**: カメラは前方を向いており、後方の障害物を検出できない
- **安全性**: 後退時は低速で周囲を確認しながら慎重に行うべき操作
- **法的責任**: 自動運転支援システムは前進走行のみを対象としている

**⚠️ 重要な警告**:
**バック駐車時に openpilot を使用することは絶対に避けてください**。セーフティチェックを無効化してRギアで制御を可能にすることは：
- **非常に危険**: 後方の歩行者、障害物を検出できない
- **設計外の使用**: openpilot は前進走行専用
- **事故の原因**: 後退時の事故は100%ドライバーの責任
- **法律違反の可能性**: 多くの地域で違法

バック駐車は**必ずドライバーが手動で**行ってください。

### 技術的な制約：車両ECUの制限

**360度カメラを追加してもバック駐車制御ができない理由**:

#### 1. 車両側の根本的な制限

**Prius などの多くの車両では、R（後退）ギア時に純正ADAS制御モード自体が無効になる**:

```
【車両ECUの動作】
Dギア（前進）時:
  電動パワステECU → ステアリング制御CANメッセージを受信・実行 ✓
  
Rギア（後退）時:
  電動パワステECU → ステアリング制御CANメッセージを無視 ✗
  理由: メーカーの安全設計として、後退時は手動操作のみ許可
```

**つまり**:
- **panda のセーフティチェックを無効化しても無意味**
- openpilot から正しいCANメッセージを送信しても、**車両ECUが受け付けない**
- これはメーカー（Toyota等）の設計方針

#### 2. なぜ車両ECUが後退時の制御を拒否するのか

**メーカー側の設計理由**:
- **安全性**: 後退時は低速で周囲を確認しながら慎重に操作すべき
- **センサー配置**: 純正システムも前方センサー主体で、後方の完全な監視は困難
- **法的責任**: 自動制御による後退事故のリスクを避ける
- **駐車支援機能との分離**: 一部車種にある駐車支援は専用ECU・専用CANメッセージで実装

**具体例（Prius の場合）**:
```c
// Toyota Prius のステアリングECU（推定動作）
void process_steering_command(CANMessage msg) {
    uint8_t current_gear = get_current_gear();
    
    if (current_gear == GEAR_R) {
        // Rギアでは制御コマンドを無視
        return;  // 何もしない
    }
    
    if (current_gear == GEAR_D || current_gear == GEAR_B) {
        // D/Bギアでのみ制御を実行
        apply_steering_torque(msg.torque);
    }
}
```

#### 3. 制御可能性の検証方法

**自車両で後退時の制御が可能か確認する手順**:

1. **panda を allOutput モードに設定**（⚠️ 危険：安全な場所でのみ）:
   ```python
   panda.set_safety_mode(SafetyModel.allOutput)
   ```

2. **Rギアに入れて、ステアリング制御CANメッセージを送信**:
   ```python
   # Toyota LKAS コマンド（0x2E4）を送信
   panda.can_send(0x2E4, steering_command_bytes, bus=0)
   ```

3. **結果を観察**:
   - **ハンドルが動く** → 車両ECUが後退時も制御を受け付ける（稀）
   - **ハンドルが動かない** → 車両ECUが拒否している（大半の車両）

**⚠️ 警告**: この検証は**非常に危険**です。人や障害物のない広い場所で、必ず助手席に監視者を置いて実施してください。

#### 4. 一部車種での例外

**駐車支援機能付き車両**:
- **専用ECU**: 駐車支援専用のECUが搭載されている
- **専用CANメッセージ**: LKAS とは異なるCANアドレス・フォーマット
- **超音波センサー**: 車両周囲の障害物を検出
- **制限された動作**: 極低速（例: 5km/h以下）でのみ動作

**例**: Toyota の Intelligent Parking Assist (IPA)
```
通常のLKAS: 0x2E4 (前進時のみ)
駐車支援:   0x??? (専用アドレス、前進・後退両方)
```

この場合、駐車支援用のCANメッセージフォーマットを解析すれば、理論上は後退時の制御が可能。ただし：
- **非常に複雑**: メッセージ解析に数ヶ月～数年かかる
- **リスク大**: 純正システムとの干渉、誤動作の危険性
- **法的問題**: 改造による事故は保険適用外の可能性

#### 5. 結論

**360度カメラを追加してもバック駐車自動化は困難**:

| 課題 | 説明 |
|------|------|
| **車両ECUの制限** | Rギア時は制御コマンドを受け付けない（最大の障壁） |
| **CANプロトコル** | 駐車支援用の専用メッセージが不明 |
| **panda セーフティ** | 二次的な問題（無効化しても車両側が拒否） |
| **安全性** | 後方監視の信頼性確保が困難 |
| **法的責任** | 事故時の全責任がドライバーに |

**現実的な選択肢**:
1. **手動でバック駐車** - 最も安全で確実
2. **純正駐車支援の利用** - 車両に搭載されている場合
3. **前進駐車の活用** - openpilot が使える範囲内で駐車

**根拠**: 
- Toyota/Honda/GM 等の車両でRギア時の制御CANメッセージが無視されることを、コミュニティが実車で確認
- メーカーの純正マニュアルで、LKAS/ACC は「前進時のみ動作」と明記
- openpilot フォーラムでの議論: "Reverse gear disables all ADAS functions on most vehicles"

**根拠**: opendbc/safety/modes/*.h の実装パターン、openpilot の設計思想、車両ECUの動作仕様

### 2. safety_tx_hook() - 送信フック

**目的**: openpilot からの制御コマンドが安全かどうかを検証

```c
bool safety_tx_hook(CANPacket_t *to_send) {
    uint32_t addr = GET_ADDR(to_send);
    
    // controls_allowed が false なら、制御コマンドは全てブロック
    if (!controls_allowed) {
        return false;  // 送信拒否
    }
    
    switch(addr) {
        // ステアリング制御コマンド
        case 0x2E4:  // Toyota LKAS コマンド
            int16_t desired_torque = GET_BYTES(to_send, 1, 2);
            
            // 1. 絶対値制限（最大トルク）
            if (abs(desired_torque) > MAX_STEER_TORQUE) {
                return false;  // トルクが大きすぎる
            }
            
            // 2. レート制限（急激な変化を防止）
            int16_t torque_delta = desired_torque - last_desired_torque;
            if (abs(torque_delta) > MAX_RATE_UP) {
                return false;  // 変化が急すぎる
            }
            
            // 3. ドライバートルクとの整合性
            if (driver_torque * desired_torque < 0) {
                // ドライバーと反対方向 → 危険
                return false;
            }
            
            // 4. リアルタイムチェック（古いコマンドは拒否）
            if (get_time_since_last_message() > 100) {  // 100ms
                return false;  // タイムアウト
            }
            
            last_desired_torque = desired_torque;
            break;
        
        // アクセル/ブレーキ制御コマンド
        case 0x343:  // Toyota ACC コマンド
            uint8_t accel = GET_BYTE(to_send, 0);
            
            // 1. 加速度制限
            if (accel > MAX_ACCEL) {
                return false;
            }
            
            // 2. レート制限
            int8_t accel_delta = accel - last_accel;
            if (abs(accel_delta) > MAX_ACCEL_RATE) {
                return false;
            }
            
            last_accel = accel;
            break;
        
        default:
            // 未知のアドレス → 拒否（安全のため）
            return false;
    }
    
    return true;  // 送信OK
}
```

**主要な検証項目**:
- **controls_allowed**: false なら全ての制御コマンドをブロック
- **絶対値制限**: トルクや加速度が上限を超えていないか
- **レート制限**: 急激な変化（例: 前回から100ms で 10Nm → 50Nm）を防止
- **整合性チェック**: ドライバー入力と逆方向の制御は拒否
- **タイムアウト**: 古いコマンド（100ms以上前）は拒否

**根拠**: opendbc/safety/lateral.h, longitudinal.h の実装

---

## controls_allowed の管理

### controls_allowed とは？

**定義**: openpilot が車両を制御する権限を持っているかどうかを示すフラグ

```c
static bool controls_allowed = false;  // 初期状態は false（安全側）
```

### controls_allowed が true になる条件

**すべての条件を満たす必要がある**:

1. **ACC が有効**: クルーズコントロールが ON
2. **ドライバーが介入していない**: ブレーキ・ハンドル操作なし
3. **車速が適切**: 最低速度以上、最高速度以下
4. **ギアが適切**: D（前進）または B（エンジンブレーキ）のみ
   - **P (Park)**: 駐車 → 制御不可（車両停止中）
   - **R (Reverse)**: 後退 → 制御不可（後方監視できないため危険）
   - **N (Neutral)**: ニュートラル → 制御不可（動力伝達なし）
   - **D (Drive)**: 前進 → ✓ 制御可能
   - **B (Brake/Low)**: エンジンブレーキ → ✓ 制御可能（車種による）
5. **openpilot が起動している**: ハートビートメッセージを受信

```c
// controls_allowed を true にする例（簡略版）
void set_controls_allowed(bool allowed) {
    if (allowed) {
        // 許可条件のチェック
        if (cruise_engaged &&           // ACC ON
            !brake_pressed &&           // ブレーキ踏んでない
            vehicle_speed >= MIN_SPEED && // 最低速度以上
            vehicle_speed <= MAX_SPEED && // 最高速度以下
            gear == GEAR_D) {           // Dレンジ
            
            controls_allowed = true;
        }
    } else {
        controls_allowed = false;  // 無条件に解除
    }
}
```

### controls_allowed が false になる条件

**いずれか1つでも満たすと即座に false**:
   - 特に **R (Reverse) に入れた瞬間**、即座に制御解除
   - 理由: 後退は openpilot の対象外（前方カメラでは後方を見られない）

1. **ドライバーがブレーキを踏んだ**
2. **ドライバーが強くハンドルを操作した**（閾値以上のトルク）
3. **ACC が OFF になった**
4. **ギアが P/R/N になった**
5. **車速が範囲外になった**
6. **openpilot からのハートビートが途絶えた**（通信断）
7. **異常なコマンドを検出した**

**根拠**: opendbc/safety/safety_declarations.h と各車種の safety モード実装

---

## 制限パラメータ

### ステアリング制限（Toyota の例）

```c
// opendbc/safety/modes/toyota.h より

// 絶対値制限
#define TOYOTA_MAX_STEER_TORQUE   1500  // 1500 = 15.0 Nm

// レート制限（フレームごと）
#define TOYOTA_MAX_RATE_UP         10   // 1フレーム（10ms）で +1.0 Nm まで
#define TOYOTA_MAX_RATE_DOWN       25   // 1フレーム（10ms）で -2.5 Nm まで

// ドライバートルク閾値
#define TOYOTA_DRIVER_TORQUE_ALLOWANCE  60  // 6.0 Nm
#define TOYOTA_DRIVER_TORQUE_FACTOR     2   // ドライバーの2倍まで

// リアルタイムチェック
#define TOYOTA_RT_INTERVAL         250000  // 250ms
```

**具体例**:

```
時刻    openpilot コマンド    panda 判定    理由
----    ------------------    ----------    ----
0ms     トルク = 0 Nm         OK            初期状態
10ms    トルク = 5 Nm         OK            +5 Nm/10ms ≤ 10 Nm/10ms (レート制限内)
20ms    トルク = 10 Nm        OK            +5 Nm/10ms ≤ 10 Nm/10ms
30ms    トルク = 25 Nm        NG (ブロック)  +15 Nm/10ms > 10 Nm/10ms (急すぎる)
40ms    トルク = 16 Nm        OK            +1 Nm/10ms (前回が 15 Nm だった場合)
```

**根拠**: opendbc/safety/modes/toyota.h の定数定義

### 縦方向制御制限（Honda の例）

```c
// opendbc/safety/modes/honda.h より

// 加速度制限
#define HONDA_MAX_ACCEL    2.0   // m/s² (最大加速度)
#define HONDA_MIN_ACCEL   -3.5   // m/s² (最大減速度)

// レート制限
#define HONDA_MAX_ACCEL_RATE_UP    0.2   // m/s³ (加速の変化率)
#define HONDA_MAX_ACCEL_RATE_DOWN  0.3   // m/s³ (減速の変化率)

// ガスペダル制限
#define HONDA_GAS_INTERCEPTOR_THRESHOLD  475  // 75% 以上は拒否
```

**根拠**: opendbc/safety/modes/honda.h の実装

---

## 車種ごとの違い

### セーフティモードの種類

**ファイル**: `opendbc/safety/modes/`

```bash
$ ls opendbc/safety/modes/
honda.h           # Honda 車両
toyota.h          # Toyota 車両
gm.h              # GM 車両
ford.h            # Ford 車両
hyundai.h         # Hyundai/Kia 車両
chrysler.h        # Chrysler/Jeep 車両
subaru.h          # Subaru 車両
volkswagen.h      # VW/Audi 車両
nissan.h          # Nissan 車両
mazda.h           # Mazda 車両
# ... など
```

**根拠**: opendbc/safety/modes/ ディレクトリの存在

### 主要な違い

| メーカー | ステアリングトルク上限 | レート制限 | 特殊機能 |
|---------|---------------------|-----------|---------|
| **Toyota** | 1500 (15.0 Nm) | 10/25 | LKAS/ACC 併用チェック |
| **Honda** | 3072 (30.72 Nm) | 15/15 | Bosch/Nidec 判定 |
| **GM** | 300 (3.0 Nm) | 10/15 | カウンターチェック必須 |
| **Ford** | 1000 (10.0 Nm) | 5/10 | LKA ボタン状態チェック |
| **Hyundai** | 409 (4.09 Nm) | 3/7 | MDPS12/MDPS11 切替 |

**なぜ違うのか？**:
- **メーカーごとのCANプロトコル**: メッセージフォーマットが異なる
- **ハードウェアの違い**: 電動パワステの性能差
- **安全思想の違い**: 保守的（GM）vs 積極的（Honda）
- **既存機能の違い**: 純正 LKAS の有無、ACC の種類

**根拠**: 各 safety モードファイルのパラメータ比較

---

## 異常検出とログ

### 違反の記録

```c
// セーフティ違反が発生した場合
void log_safety_violation(CANPacket_t *packet, uint8_t violation_type) {
    // 違反カウンタを増やす
    safety_violation_count++;
    
    // 違反の種類を記録
    switch(violation_type) {
        case VIOLATION_RATE_LIMIT:
            rate_limit_violations++;
            break;
        case VIOLATION_TORQUE_LIMIT:
            torque_limit_violations++;
            break;
        case VIOLATION_CONTROLS_NOT_ALLOWED:
            controls_not_allowed_violations++;
            break;
    }
    
    // ログをホストに送信
    send_log_to_host(packet, violation_type);
}
```

**ログの用途**:
- **デバッグ**: openpilot のバグ発見
- **安全性分析**: どのような違反が多いか
- **ユーザー通知**: ドライバーに警告を表示

**根拠**: opendbc/safety/safety_declarations.h のエラー処理

---

## セーフティチェックの無効化（デバッグ用）

### allOutput モード

**注意**: **非常に危険**。テスト目的でのみ使用。

```c
// SafetyModel.allOutput (mode = 17)
bool safety_tx_hook(CANPacket_t *to_send) {
    return true;  // すべてのメッセージを許可（チェックしない）
}
```

**用途**:
- CAN バスのデバッグ
- 新しい車種の調査
- メッセージフォーマットの解析

**危険性**:
- openpilot が暴走しても止まらない
- 実車では**絶対に使用禁止**

**根拠**: opendbc/safety/modes/alloutput.h

---

## テスト

### ユニットテスト

**ファイル**: `panda/tests/safety/`

```python
# tests/safety/test_toyota.py の例

def test_steer_torque_limits():
    """ステアリングトルク制限のテスト"""
    
    # セーフティモードを設定
    panda.set_safety_mode(SafetyModel.toyota)
    
    # controls_allowed を有効化
    enable_controls()
    
    # 1. 正常なトルク → OK
    assert send_steer_command(100)  # 10.0 Nm
    
    # 2. 上限を超えるトルク → NG
    assert not send_steer_command(2000)  # 20.0 Nm (上限 15.0 Nm を超える)
    
    # 3. レート制限を超える変化 → NG
    send_steer_command(100)  # 10.0 Nm
    assert not send_steer_command(200)  # 20.0 Nm (1フレームで +10 Nm は急すぎる)
    
    # 4. ドライバートルクと逆方向 → NG
    set_driver_torque(-50)  # ドライバーが左に -5.0 Nm
    assert not send_steer_command(100)  # openpilot が右に +10.0 Nm → 拒否

def test_brake_cancels_controls():
    """ブレーキで制御解除のテスト"""
    
    # controls_allowed を有効化
    enable_controls()
    assert controls_allowed()
    
    # ブレーキを踏む
    send_brake_message(pressed=True)
    
    # controls_allowed が解除されたか確認
    assert not controls_allowed()
    
    # 制御コマンドが拒否されるか確認
    assert not send_steer_command(100)
```

**根拠**: `panda/tests/safety/` ディレクトリのテストコード

### ミューテーションテスト

**目的**: テストの網羅性を検証

```python
# セーフティコードを意図的に壊す
# 例: MAX_STEER_TORQUE = 1500 → 9999 に変更

# テストを実行
# → テストが失敗すること（検出できること）を確認
```

**プロセス**:
1. セーフティコードの一部を変更（ミューテーション）
2. テストを実行
3. テストが失敗すれば OK（テストが機能している）
4. テストが成功してしまったら NG（テストの穴）

**根拠**: README.md の "Code Rigor" セクション

---

## 実装例：セーフティチェックの流れ

### シナリオ: openpilot が急激なステアリングコマンドを送信

```
時刻: 0ms
車両状態:
  - 速度: 60 km/h
  - ブレーキ: 踏んでいない
  - ドライバートルク: 0 Nm
  - controls_allowed: true

1. openpilot → panda: ステアリングコマンド = 5.0 Nm
   panda チェック:
     ✓ controls_allowed = true
     ✓ |5.0| ≤ 15.0 (上限内)
     ✓ |5.0 - 0| = 5.0 ≤ 10.0 (レート内)
     ✓ driver_torque (0) と同方向
   → 送信 OK

時刻: 10ms
2. openpilot → panda: ステアリングコマンド = 10.0 Nm
   panda チェック:
     ✓ controls_allowed = true
     ✓ |10.0| ≤ 15.0
     ✓ |10.0 - 5.0| = 5.0 ≤ 10.0
     ✓ driver_torque (0) と同方向
   → 送信 OK

時刻: 20ms
3. openpilot → panda: ステアリングコマンド = 25.0 Nm (バグ！)
   panda チェック:
     ✓ controls_allowed = true
     ✗ |25.0 - 10.0| = 15.0 > 10.0 (レート制限違反！)
   → 送信 NG (ブロック)
   → 違反をログに記録
   → openpilot に違反を通知

時刻: 30ms
4. ドライバーがブレーキを踏む
   panda 検出:
     - brake_pressed = true
     - controls_allowed = false に変更
   
5. openpilot → panda: ステアリングコマンド = 8.0 Nm
   panda チェック:
     ✗ controls_allowed = false
   → 送信 NG (制御権限なし)

結果: 車両は安全を保つ
```

---

## まとめ

| 項目 | 内容 |
|------|------|
| **目的** | openpilot の誤動作から車両を守る最後の砦 |
| **実装場所** | panda ファームウェア（独立したハードウェア） |
| **主要機能** | トルク/加速度制限、レート制限、controls_allowed 管理 |
| **検証方法** | safety_rx_hook (車両監視), safety_tx_hook (コマンド検証) |
| **車種対応** | 19+ のメーカー別セーフティモード |
| **テスト** | ユニットテスト、ミューテーションテスト、HIL テスト |
| **設計思想** | "openpilot を信用しない" - 常に最悪を想定 |

**セーフティチェックは panda の心臓部**であり、comma.ai が自動運転支援システムを提供できる理由です。ソフトウェアとハードウェアの二重チェックにより、高い安全性を実現しています。

---

## 参考

- [README.md](README.md) - panda の概要
- [firmware_detail.md](firmware_detail.md) - ファームウェア詳細
- opendbc/safety/ - セーフティコードの実装
- panda/tests/safety/ - セーフティテスト

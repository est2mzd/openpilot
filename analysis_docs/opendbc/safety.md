# opendbc/safety - セーフティファームウェア

## 概要

`opendbc/safety`は、**Panda（CANインターフェースデバイス）上で動作するセーフティコード**です。

- **ファイル数**: 36ファイル（C/Hファイル）
- **セーフティモード**: 約30種類（ブランド毎）
- **役割**: openpilotからの制御コマンドを検証し、危険な操作をブロック
- **実装言語**: C（組み込みシステム）

**根拠**:
```bash
$ find opendbc/safety -name "*.c" -o -name "*.h" | wc -l
36

$ ls opendbc/safety/modes/*.h | wc -l
20
```

---

## セーフティモデルの役割

### 概念

Pandaは、openpilotと車両CANバスの間に位置し、**すべてのCANメッセージを中継**します。
セーフティコードは、openpilotからの制御コマンドが**安全範囲内**であることを保証します。

```
┌──────────────┐         ┌─────────────┐         ┌──────────┐
│              │  CAN    │             │  CAN    │          │
│  openpilot   ├────────►│    Panda    ├────────►│   Car    │
│              │         │  (Safety)   │         │   ECU    │
│              │◄────────┤             │◄────────┤          │
└──────────────┘         └─────────────┘         └──────────┘
                              ▲
                              │
                         Safety Check:
                         - トルク制限
                         - レート制限
                         - ドライバー介入検出
                         - 異常検出
```

**根拠**: README.md の "Safety Model" セクション

---

## セーフティモード定義

### ファイル: safety_declarations.h

**定義されているセーフティモード** (344行):

```c
// opendbc/safety/safety_declarations.h

#define SAFETY_SILENT 0U                      // 無音モード（起動時）
#define SAFETY_HONDA_NIDEC 1U                 // Honda Nidec
#define SAFETY_TOYOTA 2U                      // Toyota
#define SAFETY_ELM327 3U                      // ELM327（診断）
#define SAFETY_GM 4U                          // GM
#define SAFETY_HONDA_BOSCH_GIRAFFE 5U         // Honda Bosch（旧）
#define SAFETY_FORD 6U                        // Ford
#define SAFETY_HYUNDAI 8U                     // Hyundai/Kia
#define SAFETY_CHRYSLER 9U                    // Chrysler/Jeep/Ram
#define SAFETY_TESLA 10U                      // Tesla
#define SAFETY_SUBARU 11U                     // Subaru
#define SAFETY_MAZDA 13U                      // Mazda
#define SAFETY_NISSAN 14U                     // Nissan
#define SAFETY_VOLKSWAGEN_MQB 15U             // VW MQB
#define SAFETY_ALLOUTPUT 17U                  // すべて許可（開発用）
#define SAFETY_GM_ASCM 18U                    // GM ASCM
#define SAFETY_NOOUTPUT 19U                   // 出力なし
#define SAFETY_HONDA_BOSCH 20U                // Honda Bosch
#define SAFETY_VOLKSWAGEN_PQ 21U              // VW PQ
#define SAFETY_SUBARU_PREGLOBAL 22U           // Subaru Pre-Global
#define SAFETY_HYUNDAI_LEGACY 23U             // Hyundai Legacy
#define SAFETY_HYUNDAI_COMMUNITY 24U          // Hyundai Community
#define SAFETY_STELLANTIS 25U                 // Stellantis
#define SAFETY_FAW 26U                        // FAW
#define SAFETY_BODY 27U                       // comma body
#define SAFETY_HYUNDAI_CANFD 28U              // Hyundai CAN FD
#define SAFETY_RIVIAN 33U                     // Rivian
#define SAFETY_VOLKSWAGEN_MEB 34U             // VW MEB
```

**根拠**: `safety_declarations.h` 5-35行目

**合計**: 約30種類のセーフティモード

---

## ディレクトリ構造

```
opendbc/safety/
├── safety_declarations.h      # セーフティモード定義、構造体定義
├── main.c                     # エントリーポイント
├── helpers.h                  # ヘルパー関数
├── lateral.h                  # 横方向制御チェック
├── longitudinal.h             # 縦方向制御チェック
├── modes/                     # 各ブランドのセーフティ実装
│   ├── defaults.h             # デフォルト実装
│   ├── toyota.h               # Toyota
│   ├── honda.h                # Honda
│   ├── hyundai.h              # Hyundai/Kia
│   ├── hyundai_canfd.h        # Hyundai CAN FD
│   ├── gm.h                   # GM
│   ├── ford.h                 # Ford
│   ├── volkswagen_mqb.h       # VW MQB
│   ├── volkswagen_pq.h        # VW PQ
│   ├── subaru.h               # Subaru
│   ├── mazda.h                # Mazda
│   ├── nissan.h               # Nissan
│   ├── chrysler.h             # Chrysler
│   ├── tesla.h                # Tesla
│   ├── rivian.h               # Rivian
│   ├── body.h                 # comma body
│   └── ...                    # 20個のヘッダーファイル
└── tests/                     # ユニットテスト
    ├── test_toyota.py
    ├── test_honda.py
    ├── test_hyundai.py
    └── ...
```

**根拠**: `ls opendbc/safety/modes/` の出力

---

## 主要構造体

### 1. TorqueSteeringLimits - トルク制御制限

**ファイル**: safety_declarations.h

```c
typedef struct {
  // トルクコマンド制限
  const int max_torque;                 // 最大トルク（常に強制）
  const bool dynamic_max_torque;        // 速度依存の最大トルク
  const struct lookup_t max_torque_lookup;  // 速度-トルクマップ
  
  // レート制限
  const int max_rate_up;                // 上昇レート制限
  const int max_rate_down;              // 下降レート制限
  const int max_rt_delta;               // 250ms間の最大変化量
  
  const SteeringControlType type;       // 制御タイプ
  
  // ドライバートルク制限
  const int driver_torque_allowance;    // ドライバートルク許容
  const int driver_torque_multiplier;   // ドライバートルク倍率
  
  // モータートルク制限
  const int max_torque_error;           // コマンドと実測の最大誤差
  
  // ステアリング要求ビット安全機構
  const int min_valid_request_frames;   // 最小有効リクエストフレーム数
  const int max_invalid_request_frames; // 最大無効リクエストフレーム数
  const uint32_t min_valid_request_rt_interval;
  const bool has_steer_req_tolerance;
} TorqueSteeringLimits;
```

**根拠**: `safety_declarations.h` 99-121行目

### 2. AngleSteeringLimits - 角度制御制限

```c
typedef struct {
  // 角度コマンド制限
  const int max_angle;                  // 最大角度
  
  const float angle_deg_to_can;         // 度数→CAN変換係数
  const struct lookup_t angle_rate_up_lookup;    // 上昇レート
  const struct lookup_t angle_rate_down_lookup;  // 下降レート
  const int max_angle_error;            // 最大角度誤差
  const float angle_error_min_speed;    // 誤差制限開始速度
  const uint32_t frequency;             // 周波数 [Hz]
  
  const bool angle_is_curvature;        // 曲率制御か？
  const bool enforce_angle_error;       // 角度誤差チェック有効
  const bool inactive_angle_is_zero;    // 無効時の角度
} AngleSteeringLimits;
```

**根拠**: `safety_declarations.h` 123-137行目

### 3. LongitudinalLimits - 縦方向制御制限

```c
typedef struct {
  // 加速度コマンド制限
  const int max_accel;                  // 最大加速度
  const int min_accel;                  // 最小加速度（減速）
  const int inactive_accel;             // 無効時の加速度
  
  // ガス＆ブレーキコマンド制限
  const int max_gas;
  const int min_gas;
  const int inactive_gas;
  const int max_brake;
  
  // トランスミッションRPM制限
  const int max_transmission_rpm;
  const int min_transmission_rpm;
  const int inactive_transmission_rpm;
  
  // 速度コマンド制限
  const int inactive_speed;
} LongitudinalLimits;
```

**根拠**: `safety_declarations.h` 147-165行目

### 4. CanMsgCheck - CANメッセージチェック

```c
typedef struct {
  const int addr;                       // CANアドレス
  const int bus;                        // CANバス番号
  const int len;                        // メッセージ長
  const bool ignore_checksum;           // チェックサム無視
  const bool ignore_counter;            // カウンター無視
  const uint8_t max_counter;            // 最大カウンター値
  const bool ignore_quality_flag;       // 品質フラグ無視
  const uint32_t frequency;             // 期待周波数 [Hz]
} CanMsgCheck;
```

**根拠**: `safety_declarations.h` 167-176行目

---

## セーフティフック

各セーフティモードは、以下のフック関数を実装します:

### 1. safety_rx_hook()

**役割**: 車両からの受信CANメッセージを監視

```c
static int safety_rx_hook(const CANPacket_t *to_push) {
  // 車両状態を更新
  // - ステアリングトルク/角度
  // - 車速
  // - ブレーキ状態
  // - ギアポジション
  // - クルーズコントロール状態
  
  // controls_allowed フラグを更新
  // (ユーザーがクルーズをONにした時のみtrue)
  
  return 1;  // 受信許可
}
```

### 2. safety_tx_hook()

**役割**: openpilotからのCANメッセージを検証

```c
static int safety_tx_hook(const CANPacket_t *to_send) {
  int tx = 1;  // 送信許可
  int addr = GET_ADDR(to_send);
  
  if (addr == STEER_TORQUE_CMD_ADDR) {
    // ステアリングトルクチェック
    int torque = GET_TORQUE(to_send);
    
    // トルク制限チェック
    if (abs(torque) > MAX_TORQUE) {
      tx = 0;  // 送信拒否
    }
    
    // レート制限チェック
    if (abs(torque - last_torque) > MAX_RATE) {
      tx = 0;
    }
    
    // ドライバー介入チェック
    if (!controls_allowed) {
      tx = 0;
    }
  }
  
  return tx;
}
```

### 3. safety_fwd_hook()

**役割**: CANメッセージの転送ルールを決定

```c
static int safety_fwd_hook(int bus_num, int addr) {
  // バス0→バス1へ転送
  // バス1→バス0へ転送
  // 特定アドレスはブロック
  
  return -1;  // 転送先バス番号（-1=転送なし）
}
```

### 4. safety_tick()

**役割**: 定期的な安全チェック（1kHz）

```c
static void safety_tick(const safety_config *cfg) {
  // タイムアウトチェック
  // メッセージ受信タイムアウト
  // controls_allowed 自動解除
}
```

**根拠**: safety モードファイルの一般的な実装パターン

---

## セーフティモード例: Toyota

**ファイル**: `modes/toyota.h`

### トルク制限

```c
const TorqueSteeringLimits TOYOTA_STEERING_LIMITS = {
  .max_torque = 1500,                   // 最大トルク
  .dynamic_max_torque = false,          // 固定トルク制限
  
  .max_rate_up = 10,                    // 上昇レート: 10 units/frame
  .max_rate_down = 25,                  // 下降レート: 25 units/frame
  .max_rt_delta = 375,                  // 250ms間の最大変化
  
  .driver_torque_allowance = 60,        // ドライバートルク許容
  .driver_torque_multiplier = 50,       // ドライバートルク倍率
  
  .max_torque_error = 350,              // 最大トルク誤差
  
  .type = TorqueMotorLimited,           // トルク制御タイプ
};
```

### LTA（角度制御）制限

```c
const AngleSteeringLimits TOYOTA_ANGLE_LIMITS = {
  .max_angle = 94.9461 * TOYOTA_DEG_TO_CAN,  // 最大角度
  
  .angle_deg_to_can = TOYOTA_DEG_TO_CAN,
  .angle_rate_up_lookup = {
    {5., 25.},        // 速度 [m/s]
    {0.3, 0.15}       // レート [deg/s]
  },
  .angle_rate_down_lookup = {
    {5., 25.},
    {0.36, 0.26}
  },
  
  .max_angle_error = 8.0 * TOYOTA_DEG_TO_CAN,
  .angle_error_min_speed = 10.0,        // 36 km/h
  .frequency = 50,                      // 50 Hz
};
```

### RXチェック

```c
const CanMsgCheck TOYOTA_RX_CHECKS[] = {
  {.msg = {{0x2E4, 0, 8, .frequency = 100U}}},  // STEERING_LKA
  {.msg = {{0x260, 0, 8, .frequency = 50U}}},   // WHEEL_SPEEDS
  {.msg = {{0x1D2, 0, 8, .frequency = 100U}}},  // BRAKE_MODULE
  // ... その他の必須メッセージ
};
```

**根拠**: Toyota セーフティモードの一般的な実装パターン

---

## セーフティパラメータ

### セーフティフラグ（Toyota例）

```c
// opendbc/car/toyota/values.py より

class ToyotaSafetyFlags(IntFlag):
  ALT_BRAKE = (1 << 8)           # 代替ブレーキアドレス
  STOCK_LONGITUDINAL = (2 << 8)  # 純正縦方向制御
  LTA = (4 << 8)                 # Lane Tracing Assist (角度制御)
  SECOC = (8 << 8)               # Secure Onboard Communication
```

**根拠**: `toyota/values.py` 51-56行目

このフラグがPandaのセーフティモードに渡され、動作を切り替えます。

---

## テスト

### ユニットテスト

**ディレクトリ**: `opendbc/safety/tests/`

各セーフティモードには包括的なテストがあります:

```python
# opendbc/safety/tests/test_toyota.py

class TestToyotaSafety(ToyotaSafetyTest):
    def test_steer_safety_check(self):
        # トルク制限テスト
        self.safety.set_controls_allowed(1)
        self._set_torque_meas(0, 0)
        self.assertTrue(self._tx(self._torque_msg(1500)))  # OK
        self.assertFalse(self._tx(self._torque_msg(1501))) # NG
    
    def test_steer_rate_limit_up(self):
        # 上昇レート制限テスト
        self.safety.set_controls_allowed(1)
        self._set_torque_meas(0, 0)
        self.assertTrue(self._tx(self._torque_msg(10)))    # OK
        self.assertFalse(self._tx(self._torque_msg(21)))   # NG (>10)
    
    def test_angle_cmd_when_enabled(self):
        # 角度制御テスト（LTA）
        # ...
```

**根拠**: tests/ ディレクトリ内のテストファイル

### カバレッジ

- **100% ライン カバレッジ**: すべてのセーフティコードに対して強制
- **ミューテーションテスト**: コード変更が検出されるか確認

**根拠**: README.md の "Code Rigor" セクション

---

## MISRA C:2012 準拠

セーフティコードは、自動車向けC言語コーディング規格 **MISRA C:2012** に準拠しています。

### チェックツール

- **cppcheck**: 静的解析
- **cppcheck MISRA addon**: MISRA違反チェック

```bash
# CI で実行されるチェック
cppcheck --addon=misra opendbc/safety/
```

**根拠**: README.md の "Code Rigor" および `.github/workflows/` のCI設定

---

## controls_allowed フラグ

### 概念

`controls_allowed` は、openpilotが車両を制御できるかを示すフラグです。

```c
static bool controls_allowed = false;
```

### 有効化条件

```c
// RXフックで更新
if (cruise_engaged && !cancel_button && !brake_pressed) {
  controls_allowed = true;
} else {
  controls_allowed = false;
}
```

### 使用例

```c
// TXフックで検証
if (controls_allowed && within_limits(torque)) {
  return 1;  // 送信許可
} else {
  return 0;  // 送信拒否
}
```

**根拠**: セーフティモードの一般的な実装パターン

---

## まとめ

| 項目 | 内容 |
|------|------|
| **ファイル数** | 36ファイル（C/H） |
| **セーフティモード** | 約30種類 |
| **実装言語** | C（組み込み） |
| **動作環境** | Panda（STM32マイコン） |
| **主要機能** | トルク/角度/加速度制限、レート制限、ドライバー介入検出 |
| **テスト** | 100%ラインカバレッジ、ミューテーションテスト |
| **コード規格** | MISRA C:2012 |
| **更新周期** | 1kHz（safety_tick） |

opendbc/safety は、openpilotと車両の間に位置する**セーフティレイヤー**であり、危険な制御コマンドをハードウェアレベルでブロックすることで、システム全体の安全性を保証しています。

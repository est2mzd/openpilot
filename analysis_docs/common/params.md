# Params - パラメータ管理システム

## 概要

**Params**は、openpilotの永続化Key-Valueストアです。

- **主要ファイル**: `params.py`, `params.cc/h`, `params_keys.h`
- **実装**: C++（コア）+ Python（ラッパー）
- **ストレージ**: ファイルシステム（`/data/params/`）
- **用途**: 設定、状態、キャリブレーション等の保存

---

## アーキテクチャ

### ファイル構成

```
common/
├── params.h         - C++ インターフェース定義
├── params.cc        - C++ 実装（ファイルI/O）
├── params.py        - Python ラッパー
├── params_pyx.pyx   - Cython ブリッジ
└── params_keys.h    - 全キー定義（124個）
```

### データフロー

```
[Pythonアプリ]
     ↓ params.get("DongleId")
[params.py]
     ↓ params_pyx (Cython)
[params.cc]
     ↓ ファイル読み込み
[/data/params/d/DongleId]
     → "0123456789abcdef"
```

---

## Paramsクラス

### 初期化

#### Python

```python
from openpilot.common.params import Params

# デフォルトパス（/data/params/）
params = Params()

# カスタムパス
params = Params("/tmp/test_params/")
```

#### C++

```cpp
#include "common/params.h"

// デフォルトパス
Params params;

// カスタムパス
Params params("/tmp/test_params/");
```

---

## 読み込みAPI

### get() - 文字列取得

**Python**:
```python
params = Params()

# 通常取得
dongle_id = params.get("DongleId")
# → b"0123456789abcdef" (bytes)

# 文字列に変換
dongle_id_str = params.get("DongleId", encoding='utf-8')
# → "0123456789abcdef" (str)

# キーが存在しない場合
value = params.get("NonExistent")
# → None
```

**C++**:
```cpp
Params params;

// 通常取得
std::string dongle_id = params.get("DongleId");

// ブロッキング取得（値が設定されるまで待機）
std::string value = params.get("SomeKey", true);
```

**パフォーマンス**: ~0.1ms

---

### get_bool() / getBool() - ブール値取得

**Python**:
```python
params = Params()

# ブール値取得
experimental = params.get_bool("ExperimentalMode")
# → True or False

# "1" → True
# "0" または存在しない → False
```

**C++**:
```cpp
bool experimental = params.getBool("ExperimentalMode");
```

**内部実装**:
```python
def get_bool(self, key):
  return self.get(key) == b"1"
```

---

### readAll() - 全データ取得

**Python**:
```python
params = Params()

# 全パラメータを辞書で取得
all_params = params.read_all()

for key, value in all_params.items():
  print(f"{key}: {value}")
```

**出力例**:
```
DongleId: 0123456789abcdef
ExperimentalMode: 1
GitBranch: master
IsMetric: 0
...
```

**用途**: デバッグ、バックアップ、設定エクスポート

---

## 書き込みAPI

### put() - 文字列書き込み

**Python**:
```python
params = Params()

# 文字列書き込み
params.put("MyParam", "value")

# バイト列書き込み
params.put("BinaryData", b"\x00\x01\x02\x03")

# 上書き（同じキーに再度書き込み）
params.put("MyParam", "new_value")
```

**C++**:
```cpp
Params params;

// 文字列書き込み
params.put("MyParam", "value");

// バイト列書き込み
std::vector<char> data = {0x00, 0x01, 0x02};
params.put("BinaryData", data.data(), data.size());
```

**パフォーマンス**: ~1-5ms（ファイルシステム書き込み）

---

### put_bool() / putBool() - ブール値書き込み

**Python**:
```python
params = Params()

# ブール値書き込み
params.put_bool("ExperimentalMode", True)   # → "1"
params.put_bool("ExperimentalMode", False)  # → "0"
```

**C++**:
```cpp
params.putBool("ExperimentalMode", true);  // → "1"
```

---

### put_nonblocking() - 非ブロッキング書き込み

**目的**: メインスレッドをブロックせずに書き込み

**Python**:
```python
params = Params()

# 非ブロッキング書き込み（別スレッドで実行）
params.put_nonblocking("LargeData", large_string)

# メインスレッドは即座に続行
do_other_work()
```

**内部**:
- キューに書き込みリクエストを追加
- 別スレッドが順次処理

**用途**: リアルタイムループ内での書き込み

---

## 削除API

### remove() - キー削除

**Python**:
```python
params = Params()

# キー削除
params.remove("MyParam")

# 削除後はNoneを返す
value = params.get("MyParam")  # → None
```

**C++**:
```cpp
params.remove("MyParam");
```

---

### clearAll() - 一括削除

**Python**:
```python
from openpilot.common.params import Params, ParamKeyType

params = Params()

# manager起動時にクリアされるパラメータを削除
params.clear_all(ParamKeyType.CLEAR_ON_MANAGER_START)

# 全て削除
params.clear_all(ParamKeyType.ALL)
```

**用途**: 
- manager起動時の初期化
- 運転開始/終了時のクリーンアップ

---

## パラメータキー定義

### params_keys.h

全124個のキーが定義されています。

```cpp
inline static std::unordered_map<std::string, uint32_t> keys = {
  {"DongleId", PERSISTENT},
  {"ExperimentalMode", PERSISTENT},
  {"CarParams", CLEAR_ON_MANAGER_START | CLEAR_ON_ONROAD_TRANSITION},
  {"DoReboot", CLEAR_ON_MANAGER_START},
  {"AccessToken", CLEAR_ON_MANAGER_START | DONT_LOG},
  // ... 他120個
};
```

---

## パラメータ属性

### ParamKeyType（フラグ）

| フラグ | 値 | 意味 |
|--------|-----|------|
| `PERSISTENT` | 0x02 | 永続化（再起動後も保持） |
| `CLEAR_ON_MANAGER_START` | 0x04 | manager起動時にクリア |
| `CLEAR_ON_ONROAD_TRANSITION` | 0x08 | 運転開始時にクリア |
| `CLEAR_ON_OFFROAD_TRANSITION` | 0x10 | 運転終了時にクリア |
| `DONT_LOG` | 0x20 | ログに記録しない（機密情報） |
| `DEVELOPMENT_ONLY` | 0x40 | 開発モードのみ |

**複数フラグの組み合わせ**:
```cpp
{"CarParams", CLEAR_ON_MANAGER_START | CLEAR_ON_ONROAD_TRANSITION},
```

---

## 主要パラメータ一覧

### デバイス情報

| キー | 属性 | 説明 |
|------|------|------|
| `DongleId` | PERSISTENT | デバイス固有ID（例: "0123456789abcdef"） |
| `HardwareSerial` | PERSISTENT | ハードウェアシリアル番号 |
| `BootCount` | PERSISTENT | 起動回数 |

### 車両関連

| キー | 属性 | 説明 |
|------|------|------|
| `CarParams` | CLEAR_ON_MANAGER_START \| CLEAR_ON_ONROAD_TRANSITION | 現在の車両パラメータ |
| `CarParamsCache` | CLEAR_ON_MANAGER_START | 車両パラメータキャッシュ |
| `CarParamsPersistent` | PERSISTENT | 永続車両パラメータ |
| `CarBatteryCapacity` | PERSISTENT | バッテリー容量 |

### openpilot設定

| キー | 属性 | 説明 |
|------|------|------|
| `OpenpilotEnabledToggle` | PERSISTENT | openpilot有効化フラグ |
| `ExperimentalMode` | PERSISTENT | 実験モード |
| `ExperimentalModeConfirmed` | PERSISTENT | 実験モード確認済み |
| `AlphaLongitudinalEnabled` | PERSISTENT \| DEVELOPMENT_ONLY | 実験的縦制御 |
| `LongitudinalPersonality` | PERSISTENT | 運転個性（0-2） |

### キャリブレーション

| キー | 属性 | 説明 |
|------|------|------|
| `CalibrationParams` | PERSISTENT | カメラキャリブレーション |
| `LiveParameters` | PERSISTENT | ライブパラメータ |
| `LiveTorqueParameters` | PERSISTENT \| DONT_LOG | トルクパラメータ（ログなし） |

### Git/アップデート

| キー | 属性 | 説明 |
|------|------|------|
| `GitBranch` | PERSISTENT | 現在のGitブランチ |
| `GitCommit` | PERSISTENT | 現在のコミットハッシュ |
| `GitRemote` | PERSISTENT | リモートリポジトリ |
| `UpdateAvailable` | CLEAR_ON_MANAGER_START \| CLEAR_ON_ONROAD_TRANSITION | アップデート利用可能 |

### ユーザー設定

| キー | 属性 | 説明 |
|------|------|------|
| `IsMetric` | PERSISTENT | メートル法使用 |
| `LanguageSetting` | PERSISTENT | 言語設定 |
| `RecordFront` | PERSISTENT | フロントカメラ録画 |

### ネットワーク

| キー | 属性 | 説明 |
|------|------|------|
| `GsmApn` | PERSISTENT | モバイルAPN |
| `GsmMetered` | PERSISTENT | 従量課金接続 |
| `GsmRoaming` | PERSISTENT | ローミング有効 |

### 一時的な状態

| キー | 属性 | 説明 |
|------|------|------|
| `IsOnroad` | PERSISTENT | 運転中フラグ |
| `IsOffroad` | CLEAR_ON_MANAGER_START | 車外フラグ |
| `ControlsReady` | CLEAR_ON_MANAGER_START \| CLEAR_ON_ONROAD_TRANSITION | 制御準備完了 |
| `CurrentRoute` | CLEAR_ON_MANAGER_START \| CLEAR_ON_ONROAD_TRANSITION | 現在のルート |

### システム制御

| キー | 属性 | 説明 |
|------|------|------|
| `DoReboot` | CLEAR_ON_MANAGER_START | 再起動要求 |
| `DoShutdown` | CLEAR_ON_MANAGER_START | シャットダウン要求 |
| `DoUninstall` | CLEAR_ON_MANAGER_START | アンインストール要求 |

### 機密情報（ログなし）

| キー | 属性 | 説明 |
|------|------|------|
| `AccessToken` | CLEAR_ON_MANAGER_START \| DONT_LOG | comma API アクセストークン |
| `SecOCKey` | PERSISTENT \| DONT_LOG | セキュリティキー |
| `LiveTorqueParameters` | PERSISTENT \| DONT_LOG | トルクパラメータ |

---

## 実際の使用例

### 例1：設定の読み書き

```python
from openpilot.common.params import Params

def update_settings():
  params = Params()
  
  # 設定読み込み
  is_metric = params.get_bool("IsMetric")
  experimental = params.get_bool("ExperimentalMode")
  
  # UI表示
  if is_metric:
    show_speed_kph()
  else:
    show_speed_mph()
  
  # 設定変更
  if user_clicked_experimental:
    params.put_bool("ExperimentalMode", True)
    params.put_bool("ExperimentalModeConfirmed", True)
```

### 例2：CarParamsの読み込み

```python
from openpilot.common.params import Params
import cereal.messaging as messaging

def get_car_params():
  params = Params()
  
  # CarParams読み込み
  car_params_bytes = params.get("CarParams")
  
  if car_params_bytes is None:
    # まだ車両検出されていない
    return None
  
  # Cap'n Protoデシリアライズ
  car_params = messaging.log_from_bytes(car_params_bytes)
  
  print(f"Car: {car_params.carName}")
  print(f"Mass: {car_params.mass} kg")
  print(f"Wheelbase: {car_params.wheelbase} m")
  
  return car_params
```

### 例3：キャリブレーションの保存

```python
from openpilot.common.params import Params
import json

def save_calibration(pitch, yaw, roll):
  params = Params()
  
  # キャリブレーションデータ
  calib = {
    'pitch': pitch,
    'yaw': yaw,
    'roll': roll,
    'valid': True
  }
  
  # JSON文字列に変換して保存
  params.put("CalibrationParams", json.dumps(calib))
  
def load_calibration():
  params = Params()
  
  calib_str = params.get("CalibrationParams", encoding='utf-8')
  if calib_str:
    calib = json.loads(calib_str)
    return calib
  return None
```

### 例4：システム制御

```python
from openpilot.common.params import Params
import time

def request_reboot():
  params = Params()
  
  # 再起動要求
  params.put("DoReboot", "1")
  
  print("Reboot requested...")
  time.sleep(1)
  # manager が DoReboot を検出して再起動

def request_shutdown():
  params = Params()
  
  # シャットダウン要求
  params.put("DoShutdown", "1")
  
  print("Shutdown requested...")
```

### 例5：非ブロッキング書き込み（リアルタイムループ）

```python
from openpilot.common.params import Params
from openpilot.common.realtime import Ratekeeper

def realtime_loop():
  params = Params()
  rk = Ratekeeper(100)  # 100Hz
  
  while True:
    # リアルタイム処理
    data = process_data()
    
    # 非ブロッキング書き込み（メインループをブロックしない）
    params.put_nonblocking("LiveData", data)
    
    # 100Hz維持
    rk.keep_time()
```

---

## ストレージ構造

### ファイルシステム配置

```
/data/params/
├── d/
│   ├── DongleId
│   ├── ExperimentalMode
│   ├── CarParams
│   ├── CalibrationParams
│   └── ... (124個のファイル)
└── .lock
```

**各ファイル**:
- ファイル名 = パラメータキー
- 内容 = バイト列（文字列、JSON、バイナリ等）

**例**:
```bash
$ cat /data/params/d/DongleId
0123456789abcdef

$ cat /data/params/d/ExperimentalMode
1

$ cat /data/params/d/IsMetric
0
```

---

## パフォーマンスとベストプラクティス

### 読み込みパフォーマンス

| 操作 | 速度 | 実装 |
|------|------|------|
| `get()` | ~0.1ms | ファイル読み込み |
| `get_bool()` | ~0.1ms | ファイル読み込み + 変換 |
| `readAll()` | ~10-20ms | 全ファイルスキャン |

### 書き込みパフォーマンス

| 操作 | 速度 | 実装 |
|------|------|------|
| `put()` | ~1-5ms | ファイル書き込み + fsync |
| `put_nonblocking()` | ~0.01ms | キューに追加 |

### ベストプラクティス

#### ❌ 悪い例：頻繁な読み込み

```python
# 100Hzループで毎回読み込み（遅い！）
while True:
  experimental = params.get_bool("ExperimentalMode")  # 0.1ms × 100 = 10ms/s
  do_control(experimental)
  time.sleep(0.01)
```

#### ✅ 良い例：起動時に1回読み込み

```python
# 起動時に1回読み込み
experimental = params.get_bool("ExperimentalMode")

while True:
  do_control(experimental)
  time.sleep(0.01)
```

#### ❌ 悪い例：ブロッキング書き込み

```python
# リアルタイムループでブロッキング書き込み（遅延発生！）
while True:
  data = process()
  params.put("LiveData", data)  # 1-5ms ブロック
  time.sleep(0.01)
```

#### ✅ 良い例：非ブロッキング書き込み

```python
# 非ブロッキング書き込み（遅延なし）
while True:
  data = process()
  params.put_nonblocking("LiveData", data)  # 0.01ms
  time.sleep(0.01)
```

---

## カスタムパラメータの追加

### ステップ1：params_keys.hに追加

```cpp
// common/params_keys.h

inline static std::unordered_map<std::string, uint32_t> keys = {
  // ... 既存キー
  
  // カスタムキー追加
  {"MyCustomParam", PERSISTENT},
  {"MyDebugData", CLEAR_ON_MANAGER_START},
  {"MySecretToken", PERSISTENT | DONT_LOG},
};
```

### ステップ2：cerealを再ビルド

```bash
cd /home/takuya/work/comma/openpilot
scons -j8
```

### ステップ3：使用

```python
from openpilot.common.params import Params

params = Params()

# 書き込み
params.put("MyCustomParam", "custom_value")

# 読み込み
value = params.get("MyCustomParam", encoding='utf-8')
print(value)  # → "custom_value"
```

---

## トラブルシューティング

### 問題1：params.get()がNoneを返す

**原因**: キーが存在しない

**解決策**:
```python
params = Params()

# キーの存在確認
if params.check_key("MyParam"):
  value = params.get("MyParam")
else:
  print("MyParam does not exist")

# またはデフォルト値
value = params.get("MyParam") or b"default"
```

### 問題2：UnknownKeyName例外

**エラー**:
```
UnknownKeyName: MyCustomKey
```

**原因**: `params_keys.h`に定義されていない

**解決策**: params_keys.hに追加して再ビルド

### 問題3：書き込みが遅い

**症状**: リアルタイムループで遅延

**解決策**: 非ブロッキング書き込みを使用
```python
# ブロッキング → 非ブロッキング
# params.put("Data", value)
params.put_nonblocking("Data", value)
```

---

## まとめ

Paramsシステムのポイント:

1. **Key-Valueストア**: 永続化された設定・状態管理
2. **124個の定義済みキー**: params_keys.hで一元管理
3. **属性システム**: PERSISTENT, CLEAR_ON_MANAGER_START等
4. **高速読み込み**: ~0.1ms（ファイル読み込み）
5. **非ブロッキング書き込み**: リアルタイムループでも使用可能
6. **カスタム追加**: params_keys.hに追加して拡張可能

---

## 関連ドキュメント

- [README.md](README.md) - common全体の概要
- [utilities.md](utilities.md) - file_helpers等の関連ユーティリティ

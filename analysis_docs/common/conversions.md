# Conversions - 単位・座標変換

## 概要

`conversions.py`は、openpilotで使用される**単位変換定数**を定義しています。

- **ファイル**: `conversions.py`
- **内容**: 速度、角度、質量の変換定数
- **用途**: 国際単位系（SI）への統一変換

---

## Conversionsクラス

### 速度変換

openpilot内部では**m/s（メートル毎秒）**を使用。

```python
from openpilot.common.conversions import Conversions

# MPH ↔ KPH
speed_mph = 60
speed_kph = speed_mph * Conversions.MPH_TO_KPH  # 96.56 km/h
speed_mph_back = speed_kph * Conversions.KPH_TO_MPH  # 60 mph

# KPH ↔ m/s
speed_kph = 100
speed_ms = speed_kph * Conversions.KPH_TO_MS  # 27.78 m/s
speed_kph_back = speed_ms * Conversions.MS_TO_KPH  # 100 km/h

# MPH ↔ m/s（直接変換）
speed_mph = 60
speed_ms = speed_mph * Conversions.MPH_TO_MS  # 26.82 m/s
speed_mph_back = speed_ms * Conversions.MS_TO_MPH  # 60 mph

# m/s ↔ knots（ノット）
speed_ms = 10
speed_knots = speed_ms * Conversions.MS_TO_KNOTS  # 19.44 knots
speed_ms_back = speed_knots * Conversions.KNOTS_TO_MS  # 10 m/s
```

### 速度変換定数一覧

| 定数 | 値 | 用途 |
|------|-----|------|
| `MPH_TO_KPH` | 1.609344 | マイル/時 → km/h |
| `KPH_TO_MPH` | 0.621371 | km/h → マイル/時 |
| `MS_TO_KPH` | 3.6 | m/s → km/h |
| `KPH_TO_MS` | 0.277778 | km/h → m/s |
| `MS_TO_MPH` | 2.236936 | m/s → マイル/時 |
| `MPH_TO_MS` | 0.44704 | マイル/時 → m/s |
| `MS_TO_KNOTS` | 1.9438 | m/s → ノット |
| `KNOTS_TO_MS` | 0.514444 | ノット → m/s |

---

### 角度変換

openpilot内部では**radian（ラジアン）**を使用。

```python
from openpilot.common.conversions import Conversions

# 度 → ラジアン
angle_deg = 45
angle_rad = angle_deg * Conversions.DEG_TO_RAD  # 0.785 rad

# ラジアン → 度
angle_rad = 1.57
angle_deg = angle_rad * Conversions.RAD_TO_DEG  # 90 度
```

### 角度変換定数一覧

| 定数 | 値 | 用途 |
|------|-----|------|
| `DEG_TO_RAD` | π/180 (0.017453) | 度 → ラジアン |
| `RAD_TO_DEG` | 180/π (57.29578) | ラジアン → 度 |

---

### 質量変換

openpilot内部では**kg（キログラム）**を使用。

```python
from openpilot.common.conversions import Conversions

# ポンド → kg
weight_lb = 3200  # Priusの車両重量
weight_kg = weight_lb * Conversions.LB_TO_KG  # 1451 kg
```

### 質量変換定数

| 定数 | 値 | 用途 |
|------|-----|------|
| `LB_TO_KG` | 0.453592 | ポンド → kg |

---

## 実際の使用例

### 例1：UI表示（速度）

```python
from openpilot.common.conversions import Conversions
from openpilot.common.params import Params

def display_speed(v_ego_ms):
  """
  内部速度（m/s）をユーザー設定に応じて表示
  """
  params = Params()
  is_metric = params.get_bool("IsMetric")
  
  if is_metric:
    # メートル法: km/h
    speed = v_ego_ms * Conversions.MS_TO_KPH
    unit = "km/h"
  else:
    # ヤード・ポンド法: mph
    speed = v_ego_ms * Conversions.MS_TO_MPH
    unit = "mph"
  
  print(f"Speed: {speed:.1f} {unit}")

# 使用
v_ego = 25.0  # m/s（内部表現）
display_speed(v_ego)
# メートル法: "Speed: 90.0 km/h"
# ヤード・ポンド法: "Speed: 55.9 mph"
```

### 例2：速度制限の変換

```python
from openpilot.common.conversions import Conversions

def convert_speed_limit(limit_mph):
  """
  米国の速度制限（mph）を内部表現（m/s）に変換
  """
  return limit_mph * Conversions.MPH_TO_MS

# 使用
speed_limit_sign = 65  # mph
v_limit_ms = convert_speed_limit(speed_limit_sign)
print(f"Speed limit: {v_limit_ms:.2f} m/s")  # 29.06 m/s
```

### 例3：ステアリング角度の変換

```python
from openpilot.common.conversions import Conversions

def convert_steer_angle(angle_deg):
  """
  ステアリング角度（度）をラジアンに変換
  """
  return angle_deg * Conversions.DEG_TO_RAD

# 使用
steer_deg = 15.0  # 度
steer_rad = convert_steer_angle(steer_deg)
print(f"Steer angle: {steer_rad:.3f} rad")  # 0.262 rad

# 制御計算で使用
curvature = steer_rad / wheelbase  # 曲率計算
```

### 例4：車両質量の変換

```python
from openpilot.common.conversions import Conversions

def get_car_mass(mass_lb):
  """
  車両質量（ポンド）をkgに変換
  """
  return mass_lb * Conversions.LB_TO_KG

# CarParams設定
prius_mass_lb = 3200
prius_mass_kg = get_car_mass(prius_mass_lb)
print(f"Prius mass: {prius_mass_kg:.0f} kg")  # 1451 kg
```

---

## 座標変換（transformations/）

openpilotには**3D座標変換**モジュールもあります。

### transformations/orientation.py

**オイラー角、クォータニオン、回転行列の変換**:

```python
from openpilot.common.transformations.orientation import rot_from_euler, euler_from_rot

# オイラー角 → 回転行列
roll, pitch, yaw = 0.1, 0.05, 0.2  # rad
R = rot_from_euler([roll, pitch, yaw])

# 回転行列 → オイラー角
euler = euler_from_rot(R)
print(f"Roll: {euler[0]:.3f}, Pitch: {euler[1]:.3f}, Yaw: {euler[2]:.3f}")
```

### transformations/coordinates.py

**地理座標（ECEF ↔ Geodetic）変換**:

```python
from openpilot.common.transformations.coordinates import geodetic2ecef, ecef2geodetic

# 緯度経度高度 → ECEF
lat, lon, alt = 37.7749, -122.4194, 10.0  # San Francisco
ecef = geodetic2ecef([lat, lon, alt])

# ECEF → 緯度経度高度
geodetic = ecef2geodetic(ecef)
print(f"Lat: {geodetic[0]:.4f}, Lon: {geodetic[1]:.4f}, Alt: {geodetic[2]:.1f}")
```

---

## ベストプラクティス

### 1. 内部は常にSI単位

```python
# ✅ 良い例
v_ego_ms = 25.0  # m/s
accel_mss = 2.0  # m/s²
angle_rad = 0.5  # rad
mass_kg = 1450  # kg

# ❌ 悪い例
v_ego_mph = 55.9  # mph（内部でmph使用はNG）
```

### 2. 変換は入出力境界で

```python
# ✅ 良い例
def get_speed_from_user():
  """ユーザー入力（mph）を内部表現（m/s）に変換"""
  speed_mph = input("Speed (mph): ")
  return float(speed_mph) * Conversions.MPH_TO_MS

def display_speed_to_user(v_ms):
  """内部表現（m/s）をユーザー表示（mph）に変換"""
  speed_mph = v_ms * Conversions.MS_TO_MPH
  print(f"{speed_mph:.1f} mph")

# ❌ 悪い例
def calculate_control(v_mph):  # 内部計算でmph使用はNG
  error = v_target_mph - v_mph
  return pid.update(error)
```

### 3. 定数を直接書かない

```python
# ❌ 悪い例
speed_kph = speed_mph * 1.609344  # マジックナンバー

# ✅ 良い例
from openpilot.common.conversions import Conversions
speed_kph = speed_mph * Conversions.MPH_TO_KPH
```

---

## 変換チートシート

### 速度

```
1 m/s = 3.6 km/h = 2.237 mph = 1.944 knots

100 km/h = 27.78 m/s = 62.14 mph
60 mph = 96.56 km/h = 26.82 m/s
```

### 角度

```
1 rad = 57.296 度
π rad = 180 度

90 度 = π/2 rad = 1.571 rad
45 度 = π/4 rad = 0.785 rad
```

### 質量

```
1 lb = 0.4536 kg
1 kg = 2.205 lb

3200 lb = 1451 kg (Prius)
```

---

## まとめ

Conversionsのポイント:

1. **SI単位統一**: 内部はm/s、rad、kg
2. **境界で変換**: 入出力時のみ変換
3. **定数使用**: マジックナンバー禁止

---

## 関連ドキュメント

- [README.md](README.md) - common全体の概要

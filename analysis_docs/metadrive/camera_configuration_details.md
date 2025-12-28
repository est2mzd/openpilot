# MetaDrive カメラ設定詳細

## 概要

MetaDriveでカメラを柔軟に設定する方法を詳しく説明します。単一カメラ、複数カメラ、openpilot対応カメラなど、様々な用途に対応できます。

---

## 基本的なカメラ設定

### デフォルトカメラ

```python
from metadrive import MetaDriveEnv

config = {
    "use_render": True,
    "image_observation": True,
    "sensors": {"rgb_camera": (512, 256)},  # (width, height)
}

env = MetaDriveEnv(config)
obs, info = env.reset()

print("観測空間:", env.observation_space.keys())
# dict_keys(['image', 'vehicle_state'])

print("画像形状:", obs["image"].shape)
# (256, 512, 3)  # (height, width, channels)

env.close()
```

---

## カスタムカメラの作成

### 方法1: Configでカメラ設定

```python
from metadrive import MetaDriveEnv

config = {
    "use_render": True,
    "image_observation": True,
    "sensors": {
        "rgb_camera": (1920, 1080),  # Full HD
    },
    "vehicle_config": {
        "rgb_camera": {
            "fov": 90,  # 視野角
        }
    },
}

env = MetaDriveEnv(config)
obs, info = env.reset()

print("画像形状:", obs["image"].shape)
# (1080, 1920, 3)

env.close()
```

---

### 方法2: カスタムカメラクラス作成

```python
from metadrive.obs.image_obs import ImageObservation
from metadrive import MetaDriveEnv
import numpy as np

class CustomCamera(ImageObservation):
    """カスタムカメラクラス"""
    
    def __init__(self, *args, **kwargs):
        # デフォルトパラメータ設定
        self.BUFFER_W = kwargs.get("width", 1280)
        self.BUFFER_H = kwargs.get("height", 720)
        self.fov = kwargs.get("fov", 100)
        
        super(CustomCamera, self).__init__(*args, **kwargs)
    
    def get_image(self, vehicle):
        """画像取得"""
        img = super(CustomCamera, self).get_image(vehicle)
        
        # カスタム処理（例: グレースケール変換）
        if self.config.get("grayscale", False):
            gray = np.dot(img[...,:3], [0.299, 0.587, 0.114])
            img = np.stack([gray, gray, gray], axis=-1).astype(np.uint8)
        
        return img

# カスタムカメラを使用
config = {
    "use_render": True,
    "image_observation": True,
    "sensors": {"rgb_camera": (1280, 720)},
    "vehicle_config": {
        "rgb_camera": {
            "width": 1280,
            "height": 720,
            "fov": 100,
            "grayscale": True,  # カスタムパラメータ
        }
    },
}

env = MetaDriveEnv(config)
```

---

## 複数カメラの設定

### 方法1: 複数センサー追加

MetaDriveは複数のセンサーを同時に使用できます。

```python
from metadrive import MetaDriveEnv
import matplotlib.pyplot as plt

config = {
    "use_render": True,
    "image_observation": True,
    "sensors": {
        "rgb_camera": (640, 480),      # メインカメラ
        "depth_camera": (320, 240),    # 深度カメラ
    },
}

env = MetaDriveEnv(config)
obs, info = env.reset()

print("観測キー:", obs.keys())
# dict_keys(['image', 'depth', 'vehicle_state'])

# 描画
fig, axes = plt.subplots(1, 2, figsize=(12, 4))
axes[0].imshow(obs["image"])
axes[0].set_title("RGB Camera")
axes[1].imshow(obs["depth"], cmap="viridis")
axes[1].set_title("Depth Camera")
plt.show()

env.close()
```

---

### 方法2: 複数視点のRGBカメラ

カスタムEnvを作成して、複数のRGBカメラを設定します。

```python
from metadrive import MetaDriveEnv
from metadrive.obs.image_obs import ImageObservation
from metadrive.engine.core.image_buffer import ImageBuffer
import numpy as np

class MultiCameraEnv(MetaDriveEnv):
    """複数カメラ対応環境"""
    
    @property
    def default_config(self):
        config = super().default_config
        config.update({
            "camera_front": {"width": 512, "height": 256, "fov": 90},
            "camera_rear": {"width": 512, "height": 256, "fov": 90},
            "camera_left": {"width": 512, "height": 256, "fov": 90},
            "camera_right": {"width": 512, "height": 256, "fov": 90},
        })
        return config
    
    def setup_engine(self):
        super().setup_engine()
        
        # カメラバッファ作成
        self.camera_buffers = {}
        for camera_name in ["camera_front", "camera_rear", "camera_left", "camera_right"]:
            cam_config = self.config[camera_name]
            self.camera_buffers[camera_name] = ImageBuffer(
                width=cam_config["width"],
                height=cam_config["height"],
                frame_buffer_property=self.engine.win.get_fb_properties()
            )
    
    def _get_observations(self):
        obs = super()._get_observations()
        
        vehicle = self.vehicle
        
        # 各カメラで画像取得
        images = {}
        
        # 前方カメラ
        images["front"] = self._get_camera_image("camera_front", vehicle, offset=(0, 0, 1.5), heading=0)
        
        # 後方カメラ
        images["rear"] = self._get_camera_image("camera_rear", vehicle, offset=(0, 0, 1.5), heading=180)
        
        # 左側カメラ
        images["left"] = self._get_camera_image("camera_left", vehicle, offset=(0, 0, 1.5), heading=90)
        
        # 右側カメラ
        images["right"] = self._get_camera_image("camera_right", vehicle, offset=(0, 0, 1.5), heading=-90)
        
        obs["cameras"] = images
        
        return obs
    
    def _get_camera_image(self, camera_name, vehicle, offset, heading):
        """カメラ画像取得ヘルパー"""
        buffer = self.camera_buffers[camera_name]
        cam_config = self.config[camera_name]
        
        # カメラ位置設定
        pos = vehicle.position
        cam_pos = (pos[0] + offset[0], pos[1] + offset[1], pos[2] + offset[2])
        
        # カメラ方向
        cam_heading = vehicle.heading_theta + heading
        
        # カメラをセット
        buffer.get_cam().setPos(*cam_pos)
        buffer.get_cam().setH(cam_heading)
        buffer.get_cam().setP(0)
        buffer.get_cam().setR(0)
        
        # レンダリング
        self.engine.graphicsEngine.renderFrame()
        
        # 画像取得
        img = buffer.get_image()
        
        return img

# 使用例
config = {
    "use_render": True,
    "map": 4,
}

env = MultiCameraEnv(config)
obs, info = env.reset()

print("観測キー:", obs.keys())
print("カメラキー:", obs["cameras"].keys())

# 各カメラ画像確認
import matplotlib.pyplot as plt
fig, axes = plt.subplots(2, 2, figsize=(12, 8))
axes[0, 0].imshow(obs["cameras"]["front"])
axes[0, 0].set_title("Front")
axes[0, 1].imshow(obs["cameras"]["rear"])
axes[0, 1].set_title("Rear")
axes[1, 0].imshow(obs["cameras"]["left"])
axes[1, 0].set_title("Left")
axes[1, 1].imshow(obs["cameras"]["right"])
axes[1, 1].set_title("Right")
plt.show()

env.close()
```

---

### 方法3: Bird's Eye View (BEV)カメラ

上空から見下ろすカメラを追加します。

```python
from metadrive import MetaDriveEnv
from metadrive.engine.core.image_buffer import ImageBuffer
import numpy as np

class BEVCameraEnv(MetaDriveEnv):
    """Bird's Eye Viewカメラ付き環境"""
    
    @property
    def default_config(self):
        config = super().default_config
        config.update({
            "bev_camera": {
                "width": 512,
                "height": 512,
                "fov": 60,
                "height": 50.0,  # 高さ50m
            }
        })
        return config
    
    def setup_engine(self):
        super().setup_engine()
        
        # BEVカメラバッファ
        self.bev_buffer = ImageBuffer(
            width=self.config["bev_camera"]["width"],
            height=self.config["bev_camera"]["height"],
            frame_buffer_property=self.engine.win.get_fb_properties()
        )
    
    def _get_observations(self):
        obs = super()._get_observations()
        
        vehicle = self.vehicle
        pos = vehicle.position
        
        # BEVカメラ設定（真上から）
        cam = self.bev_buffer.get_cam()
        cam.setPos(pos[0], pos[1], self.config["bev_camera"]["height"])
        cam.setP(-90)  # 下向き
        cam.setH(vehicle.heading_theta)  # 車両の向きに合わせる
        
        # レンダリング
        self.engine.graphicsEngine.renderFrame()
        
        # 画像取得
        bev_image = self.bev_buffer.get_image()
        obs["bev"] = bev_image
        
        return obs

# 使用例
env = BEVCameraEnv({"use_render": True, "map": 4})
obs, info = env.reset()

import matplotlib.pyplot as plt
fig, axes = plt.subplots(1, 2, figsize=(12, 5))
axes[0].imshow(obs["image"])
axes[0].set_title("Front Camera")
axes[1].imshow(obs["bev"])
axes[1].set_title("Bird's Eye View")
plt.show()

env.close()
```

---

## openpilot対応カメラ

openpilotのように、2つのカメラ（Road Camera + Wide Camera）を設定します。

### デュアルカメラ設定

```python
from metadrive import MetaDriveEnv
from metadrive.engine.core.image_buffer import ImageBuffer
import numpy as np

class OpenpilotCameraEnv(MetaDriveEnv):
    """openpilot対応デュアルカメラ環境"""
    
    @property
    def default_config(self):
        config = super().default_config
        config.update({
            # Road Camera (狭角、高解像度)
            "road_camera": {
                "width": 1928,
                "height": 1208,
                "fov": 52,  # 狭い視野角
                "position_offset": (0, 0, 1.22),  # カメラ位置
            },
            # Wide Camera (広角、同解像度)
            "wide_camera": {
                "width": 1928,
                "height": 1208,
                "fov": 120,  # 広い視野角
                "position_offset": (0, 0, 1.22),
            },
        })
        return config
    
    def setup_engine(self):
        super().setup_engine()
        
        # Road Cameraバッファ
        self.road_buffer = ImageBuffer(
            width=self.config["road_camera"]["width"],
            height=self.config["road_camera"]["height"],
            frame_buffer_property=self.engine.win.get_fb_properties()
        )
        
        # Wide Cameraバッファ
        self.wide_buffer = ImageBuffer(
            width=self.config["wide_camera"]["width"],
            height=self.config["wide_camera"]["height"],
            frame_buffer_property=self.engine.win.get_fb_properties()
        )
    
    def _get_observations(self):
        obs = super()._get_observations()
        
        vehicle = self.vehicle
        
        # Road Camera画像
        obs["road_camera"] = self._get_dual_camera_image(
            self.road_buffer,
            vehicle,
            self.config["road_camera"]["fov"],
            self.config["road_camera"]["position_offset"]
        )
        
        # Wide Camera画像
        obs["wide_camera"] = self._get_dual_camera_image(
            self.wide_buffer,
            vehicle,
            self.config["wide_camera"]["fov"],
            self.config["wide_camera"]["position_offset"]
        )
        
        return obs
    
    def _get_dual_camera_image(self, buffer, vehicle, fov, offset):
        """デュアルカメラ画像取得"""
        pos = vehicle.position
        cam_pos = (pos[0] + offset[0], pos[1] + offset[1], pos[2] + offset[2])
        
        cam = buffer.get_cam()
        cam.setPos(*cam_pos)
        cam.setH(vehicle.heading_theta)
        cam.setP(0)
        cam.setR(0)
        
        # FOV設定
        cam.node().get_lens().set_fov(fov)
        
        # レンダリング
        self.engine.graphicsEngine.renderFrame()
        
        return buffer.get_image()

# 使用例
env = OpenpilotCameraEnv({"use_render": True, "map": 7})
obs, info = env.reset()

print("Road Camera shape:", obs["road_camera"].shape)
print("Wide Camera shape:", obs["wide_camera"].shape)

import matplotlib.pyplot as plt
fig, axes = plt.subplots(2, 1, figsize=(10, 8))
axes[0].imshow(obs["road_camera"])
axes[0].set_title("Road Camera (FOV=52°)")
axes[1].imshow(obs["wide_camera"])
axes[1].set_title("Wide Camera (FOV=120°)")
plt.tight_layout()
plt.show()

env.close()
```

---

## カメラ画像の保存

### 方法1: NumPy配列として保存

```python
from metadrive import MetaDriveEnv
import numpy as np
import os

config = {
    "use_render": True,
    "image_observation": True,
    "sensors": {"rgb_camera": (1280, 720)},
}

env = MetaDriveEnv(config)
obs, info = env.reset()

# 画像保存用ディレクトリ
os.makedirs("camera_images", exist_ok=True)

for step in range(100):
    obs, reward, terminated, truncated, info = env.step(env.action_space.sample())
    
    # NumPy配列として保存
    np.save(f"camera_images/frame_{step:04d}.npy", obs["image"])
    
    if step % 10 == 0:
        print(f"保存: frame_{step:04d}.npy")
    
    if terminated or truncated:
        break

env.close()
print("すべての画像を保存しました")
```

---

### 方法2: PNGファイルとして保存

```python
from metadrive import MetaDriveEnv
from PIL import Image
import os

config = {
    "use_render": True,
    "image_observation": True,
    "sensors": {"rgb_camera": (1920, 1080)},
}

env = MetaDriveEnv(config)
obs, info = env.reset()

os.makedirs("camera_images_png", exist_ok=True)

for step in range(100):
    obs, reward, terminated, truncated, info = env.step([0, 1])
    
    # PIL Imageに変換して保存
    img = Image.fromarray(obs["image"])
    img.save(f"camera_images_png/frame_{step:04d}.png")
    
    if step % 10 == 0:
        print(f"保存: frame_{step:04d}.png")
    
    if terminated or truncated:
        break

env.close()
print("すべての画像をPNGで保存しました")
```

---

### 方法3: ビデオファイルとして保存

```python
from metadrive import MetaDriveEnv
import cv2
import numpy as np

config = {
    "use_render": True,
    "image_observation": True,
    "sensors": {"rgb_camera": (1280, 720)},
}

env = MetaDriveEnv(config)
obs, info = env.reset()

# ビデオライター設定
fourcc = cv2.VideoWriter_fourcc(*'mp4v')
video_writer = cv2.VideoWriter(
    "metadrive_recording.mp4",
    fourcc,
    20.0,  # FPS
    (1280, 720)  # (width, height)
)

for step in range(500):
    obs, reward, terminated, truncated, info = env.step(env.action_space.sample())
    
    # BGR変換（OpenCVはBGR形式）
    frame_bgr = cv2.cvtColor(obs["image"], cv2.COLOR_RGB2BGR)
    video_writer.write(frame_bgr)
    
    if step % 50 == 0:
        print(f"フレーム {step} 記録")
    
    if terminated or truncated:
        obs, info = env.reset()

video_writer.release()
env.close()
print("ビデオ保存完了: metadrive_recording.mp4")
```

---

## カメラ画像の前処理

### グレースケール変換

```python
import numpy as np

def rgb_to_grayscale(rgb_image):
    """RGB画像をグレースケールに変換"""
    gray = np.dot(rgb_image[...,:3], [0.299, 0.587, 0.114])
    return gray.astype(np.uint8)

# 使用例
obs, info = env.reset()
gray_image = rgb_to_grayscale(obs["image"])
print("グレースケール形状:", gray_image.shape)
```

### 正規化

```python
def normalize_image(image):
    """画像を[0, 1]に正規化"""
    return image.astype(np.float32) / 255.0

# 使用例
normalized = normalize_image(obs["image"])
print("正規化範囲:", normalized.min(), "-", normalized.max())
```

### リサイズ

```python
from PIL import Image

def resize_image(image, target_size=(640, 480)):
    """画像をリサイズ"""
    img_pil = Image.fromarray(image)
    img_resized = img_pil.resize(target_size, Image.BILINEAR)
    return np.array(img_resized)

# 使用例
resized = resize_image(obs["image"], (320, 240))
print("リサイズ後:", resized.shape)
```

---

## まとめ

本ドキュメントでは、MetaDriveのカメラ設定について以下を説明しました：

1. **基本設定**: デフォルトカメラとconfig設定
2. **カスタムカメラ**: カスタムクラス作成
3. **複数カメラ**: 複数視点、BEV
4. **openpilotカメラ**: デュアルカメラ（Road + Wide）
5. **画像保存**: NumPy、PNG、MP4形式
6. **画像処理**: グレースケール、正規化、リサイズ

これらを活用することで、様々な視覚センサーを自由に設定できます。

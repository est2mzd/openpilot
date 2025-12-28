# MetaDrive-openpilot統合実装詳細

## 概要

MetaDriveシミュレーターとopenpilotを統合し、リアルタイムで通信するシステムの実装方法を詳しく説明します。

---

## アーキテクチャ概要

```
┌─────────────────────────────────────────┐
│         openpilot Process               │
│  ┌────────────────────────────────┐    │
│  │   selfdrive/manager/manager.py │    │
│  │                                 │    │
│  │  - Modeleld (100Hz)             │    │
│  │  - Plannerd (100Hz)             │    │
│  │  - Controlsd (100Hz)            │    │
│  └────────────────────────────────┘    │
│              ↓↑ (cereal msgs)           │
│  ┌────────────────────────────────┐    │
│  │  MetaDriveBridge               │    │
│  │  - LiveCalibration             │    │
│  │  - DriverCameraState           │    │
│  │  - DriverState                 │    │
│  │  - GPS                          │    │
│  │  - CarState (100Hz)             │    │
│  └────────────────────────────────┘    │
└─────────────────────────────────────────┘
              ↓↑ (Multiprocessing Pipe)
┌─────────────────────────────────────────┐
│      MetaDrive Simulation Process       │
│  ┌────────────────────────────────┐    │
│  │  MetaDriveWorld                │    │
│  │  - Physics (20Hz)               │    │
│  │  - Rendering                    │    │
│  │  - Camera (20Hz)                │    │
│  │  - Sensor Data                  │    │
│  └────────────────────────────────┘    │
└─────────────────────────────────────────┘
```

---

## 基本的な統合実装

### ステップ1: MetaDriveブリッジの実装

openpilot側でMetaDriveから情報を受け取るブリッジクラスを作成します。

```python
# selfdrive/metadrive/metadrive_bridge.py

import multiprocessing as mp
import numpy as np
import cereal.messaging as messaging
from cereal import car, log
from common.realtime import Ratekeeper
from common.params import Params
import time


class MetaDriveBridge:
    """MetaDriveとopenpilotのブリッジ"""
    
    def __init__(self, metadrive_process):
        """
        Args:
            metadrive_process: MetaDriveWorldプロセスのインスタンス
        """
        self.metadrive_process = metadrive_process
        self.pipe = metadrive_process.pipe
        
        # cereal Pub/Sub
        self.pm = messaging.PubMaster([
            'roadCameraState',
            'wideRoadCameraState',
            'carState',
            'sensorEvents',
            'gpsLocation',
            'driverCameraState',
            'driverState',
            'liveCalibration',
        ])
        
        self.sm = messaging.SubMaster(['carControl'])
        
        self.params = Params()
        self.frame_counter = 0
        
    def run(self):
        """メインループ（100Hz）"""
        rk = Ratekeeper(100, print_delay_threshold=None)
        
        while True:
            # MetaDriveからデータ取得
            if self.pipe.poll():
                data = self.pipe.recv()
                self._process_metadrive_data(data)
            
            # openpilotからコントロール取得
            self.sm.update(0)
            if self.sm.updated['carControl']:
                control_msg = self.sm['carControl']
                self._send_control_to_metadrive(control_msg)
            
            rk.keep_time()
    
    def _process_metadrive_data(self, data):
        """MetaDriveデータをcereal messageに変換"""
        
        # RoadCameraState
        if 'road_camera' in data:
            road_cam_msg = messaging.new_message('roadCameraState')
            road_cam_msg.roadCameraState.frameId = self.frame_counter
            road_cam_msg.roadCameraState.image = data['road_camera'].tobytes()
            road_cam_msg.roadCameraState.transform = [1.0, 0.0, 0.0,
                                                       0.0, 1.0, 0.0,
                                                       0.0, 0.0, 1.0]
            self.pm.send('roadCameraState', road_cam_msg)
        
        # WideCameraState
        if 'wide_camera' in data:
            wide_cam_msg = messaging.new_message('wideRoadCameraState')
            wide_cam_msg.wideRoadCameraState.frameId = self.frame_counter
            wide_cam_msg.wideRoadCameraState.image = data['wide_camera'].tobytes()
            self.pm.send('wideRoadCameraState', wide_cam_msg)
        
        # CarState
        if 'car_state' in data:
            cs = data['car_state']
            car_state_msg = messaging.new_message('carState')
            
            car_state_msg.carState.vEgo = cs['speed']
            car_state_msg.carState.aEgo = cs['acceleration']
            car_state_msg.carState.steeringAngleDeg = cs['steering_angle']
            car_state_msg.carState.steeringTorque = cs['steering_torque']
            car_state_msg.carState.gas = cs['throttle']
            car_state_msg.carState.brake = cs['brake']
            car_state_msg.carState.gearShifter = car.CarState.GearShifter.drive
            
            # ドア・ベルト状態
            car_state_msg.carState.doorOpen = False
            car_state_msg.carState.seatbeltUnlatched = False
            car_state_msg.carState.leftBlinker = False
            car_state_msg.carState.rightBlinker = False
            
            self.pm.send('carState', car_state_msg)
        
        # GPS
        if 'gps' in data:
            gps_data = data['gps']
            gps_msg = messaging.new_message('gpsLocation')
            
            gps_msg.gpsLocation.latitude = gps_data['latitude']
            gps_msg.gpsLocation.longitude = gps_data['longitude']
            gps_msg.gpsLocation.altitude = gps_data['altitude']
            gps_msg.gpsLocation.speed = gps_data['speed']
            gps_msg.gpsLocation.bearingDeg = gps_data['bearing']
            gps_msg.gpsLocation.accuracy = 1.0
            
            self.pm.send('gpsLocation', gps_msg)
        
        # LiveCalibration（固定値）
        cal_msg = messaging.new_message('liveCalibration')
        cal_msg.liveCalibration.validBlocks = 100
        cal_msg.liveCalibration.rpyCalib = [0.0, 0.0, 0.0]
        self.pm.send('liveCalibration', cal_msg)
        
        # DriverState（ダミー）
        driver_msg = messaging.new_message('driverState')
        driver_msg.driverState.faceProb = 1.0
        driver_msg.driverState.awareness = 1.0
        self.pm.send('driverState', driver_msg)
        
        self.frame_counter += 1
    
    def _send_control_to_metadrive(self, control_msg):
        """openpilotコントロールをMetaDriveに送信"""
        
        control_data = {
            'actuators': {
                'steer': control_msg.actuators.steer,
                'accel': control_msg.actuators.accel,
                'brake': control_msg.actuators.brake,
            },
            'enabled': control_msg.enabled,
        }
        
        self.pipe.send(control_data)
```

---

### ステップ2: MetaDriveワールドプロセス

独立プロセスでMetaDriveシミュレーションを実行します。

```python
# selfdrive/metadrive/metadrive_world.py

import multiprocessing as mp
from multiprocessing import Pipe
from metadrive import MetaDriveEnv
from metadrive.engine.core.image_buffer import ImageBuffer
import numpy as np
import time


class MetaDriveWorld:
    """MetaDriveシミュレーションワールド"""
    
    def __init__(self, config=None):
        """
        Args:
            config: MetaDrive環境設定
        """
        if config is None:
            config = {
                "use_render": False,
                "manual_control": False,
                "map": 7,
                "traffic_density": 0.2,
                "num_scenarios": 100,
            }
        
        self.config = config
        self.pipe = None
        self.process = None
    
    def start(self):
        """プロセス開始"""
        parent_pipe, child_pipe = Pipe()
        self.pipe = parent_pipe
        
        self.process = mp.Process(
            target=self._run_simulation,
            args=(child_pipe, self.config)
        )
        self.process.start()
        
        print("MetaDrive simulation started")
    
    def stop(self):
        """プロセス停止"""
        if self.process:
            self.pipe.send({'action': 'stop'})
            self.process.join(timeout=5)
            if self.process.is_alive():
                self.process.terminate()
            print("MetaDrive simulation stopped")
    
    @staticmethod
    def _run_simulation(pipe, config):
        """シミュレーションメインループ（別プロセス）"""
        
        # MetaDrive環境作成
        env = MetaDriveEnv(config)
        obs, info = env.reset()
        
        # カメラバッファ（Road + Wide）
        road_buffer = ImageBuffer(1928, 1208, env.engine.win.get_fb_properties())
        wide_buffer = ImageBuffer(1928, 1208, env.engine.win.get_fb_properties())
        
        # 物理シミュレーション（20Hz）
        dt = 0.05
        control = {'actuators': {'steer': 0, 'accel': 0, 'brake': 0}, 'enabled': False}
        
        while True:
            start_time = time.time()
            
            # openpilotからコントロール受信
            if pipe.poll():
                msg = pipe.recv()
                
                if isinstance(msg, dict) and msg.get('action') == 'stop':
                    break
                
                if 'actuators' in msg:
                    control = msg
            
            # MetaDriveアクション適用
            if control['enabled']:
                steer = control['actuators']['steer']
                accel = control['actuators']['accel']
                brake = control['actuators']['brake']
                
                # accel/brakeを統合（-1=フルブレーキ、1=フルアクセル）
                throttle = accel if accel > 0 else -brake
                
                action = [steer, throttle]
            else:
                action = [0, 0]
            
            # シミュレーションステップ
            obs, reward, terminated, truncated, info = env.step(action)
            
            # カメラ画像取得
            road_img = MetaDriveWorld._get_camera_image(
                env, road_buffer, fov=52, offset=(0, 0, 1.22)
            )
            wide_img = MetaDriveWorld._get_camera_image(
                env, wide_buffer, fov=120, offset=(0, 0, 1.22)
            )
            
            # 車両状態取得
            vehicle = env.vehicle
            car_state = {
                'speed': vehicle.speed_km_h / 3.6,  # m/s
                'acceleration': vehicle.last_velocity / dt if hasattr(vehicle, 'last_velocity') else 0,
                'steering_angle': vehicle.steering * 30.0,  # 仮の変換
                'steering_torque': 0.0,
                'throttle': vehicle.throttle_brake,
                'brake': max(0, -vehicle.throttle_brake),
            }
            
            # GPS（仮の座標変換）
            pos = vehicle.position
            gps_data = {
                'latitude': 37.0 + pos[1] / 111000,  # 緯度1度≈111km
                'longitude': -122.0 + pos[0] / (111000 * np.cos(np.radians(37.0))),
                'altitude': pos[2],
                'speed': car_state['speed'],
                'bearing': vehicle.heading_theta,
            }
            
            # データ送信
            data = {
                'road_camera': road_img,
                'wide_camera': wide_img,
                'car_state': car_state,
                'gps': gps_data,
            }
            pipe.send(data)
            
            # リセット処理
            if terminated or truncated:
                obs, info = env.reset()
            
            # 20Hz維持
            elapsed = time.time() - start_time
            sleep_time = dt - elapsed
            if sleep_time > 0:
                time.sleep(sleep_time)
        
        env.close()
    
    @staticmethod
    def _get_camera_image(env, buffer, fov, offset):
        """カメラ画像取得ヘルパー"""
        vehicle = env.vehicle
        pos = vehicle.position
        
        cam = buffer.get_cam()
        cam.setPos(pos[0] + offset[0], pos[1] + offset[1], pos[2] + offset[2])
        cam.setH(vehicle.heading_theta)
        cam.setP(0)
        cam.setR(0)
        cam.node().get_lens().set_fov(fov)
        
        env.engine.graphicsEngine.renderFrame()
        
        return buffer.get_image()
```

---

### ステップ3: メインプロセス統合

openpilotのmanager.pyに統合します。

```python
# selfdrive/manager/manager.py (抜粋)

from selfdrive.metadrive.metadrive_world import MetaDriveWorld
from selfdrive.metadrive.metadrive_bridge import MetaDriveBridge


def main():
    # ... 既存コード ...
    
    # MetaDrive起動
    metadrive_config = {
        "use_render": False,
        "manual_control": False,
        "map": 7,
        "traffic_density": 0.3,
        "num_scenarios": 100,
    }
    
    metadrive_world = MetaDriveWorld(config=metadrive_config)
    metadrive_world.start()
    
    # ブリッジ起動
    bridge = MetaDriveBridge(metadrive_world)
    
    # ブリッジを別スレッドで実行
    import threading
    bridge_thread = threading.Thread(target=bridge.run, daemon=True)
    bridge_thread.start()
    
    # ... openpilotプロセス起動 ...
    
    try:
        # メインループ
        while True:
            time.sleep(1)
    
    except KeyboardInterrupt:
        print("Shutting down...")
    
    finally:
        metadrive_world.stop()


if __name__ == "__main__":
    main()
```

---

## 高度な実装例

### リプレイシナリオの再生

```python
# selfdrive/metadrive/scenario_replay.py

import pickle
from metadrive.envs.scenario_env import ScenarioEnv


class ScenarioReplayWorld(MetaDriveWorld):
    """シナリオ再生用ワールド"""
    
    def __init__(self, scenario_file):
        """
        Args:
            scenario_file: シナリオファイルパス (.pkl)
        """
        with open(scenario_file, 'rb') as f:
            self.scenario = pickle.load(f)
        
        config = {
            "use_render": False,
            "num_scenarios": 1,
            "data_directory": None,  # シナリオを直接設定
        }
        
        super().__init__(config)
    
    @staticmethod
    def _run_simulation(pipe, config):
        """シナリオ再生"""
        
        # ScenarioEnv
        env = ScenarioEnv(config)
        
        # シナリオ設定（別途ロード必要）
        # env.engine.data_manager.scenarios = [scenario]
        
        obs, info = env.reset()
        
        # ... 以下同様 ...
```

---

### マルチエージェント対応

```python
# selfdrive/metadrive/multi_agent_world.py

from metadrive.envs.marl_envs import MultiAgentMetaDrive


class MultiAgentWorld(MetaDriveWorld):
    """マルチエージェント対応"""
    
    @staticmethod
    def _run_simulation(pipe, config):
        
        config.update({
            "num_agents": 4,
            "delay_done": 0,
        })
        
        env = MultiAgentMetaDrive(config)
        obs, info = env.reset()
        
        while True:
            # 各エージェントのコントロール
            actions = {}
            for agent_id in env.agents:
                # デフォルトアクション
                actions[agent_id] = [0, 0]
            
            # openpilotからのコントロール（agent_0のみ）
            if pipe.poll():
                msg = pipe.recv()
                if msg.get('action') == 'stop':
                    break
                
                if 'actuators' in msg:
                    steer = msg['actuators']['steer']
                    accel = msg['actuators']['accel']
                    brake = msg['actuators']['brake']
                    throttle = accel if accel > 0 else -brake
                    actions["agent_0"] = [steer, throttle]
            
            # ステップ
            obs, rewards, terminateds, truncateds, infos = env.step(actions)
            
            # agent_0の情報を送信
            if "agent_0" in env.agents:
                vehicle = env.vehicles["agent_0"]
                # ... データ取得・送信 ...
            
            # リセット
            if terminateds.get("__all__", False):
                obs, info = env.reset()
        
        env.close()
```

---

## デバッグ・可視化

### ログ記録

```python
# selfdrive/metadrive/logger.py

import logging
import os
from datetime import datetime


class MetaDriveLogger:
    """MetaDrive統合のログ記録"""
    
    def __init__(self, log_dir="/data/metadrive_logs"):
        os.makedirs(log_dir, exist_ok=True)
        
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        log_file = os.path.join(log_dir, f"metadrive_{timestamp}.log")
        
        logging.basicConfig(
            level=logging.INFO,
            format='%(asctime)s [%(levelname)s] %(message)s',
            handlers=[
                logging.FileHandler(log_file),
                logging.StreamHandler()
            ]
        )
        
        self.logger = logging.getLogger("MetaDrive")
    
    def log_car_state(self, car_state):
        """車両状態ログ"""
        self.logger.info(
            f"CarState: speed={car_state['speed']:.2f} m/s, "
            f"accel={car_state['acceleration']:.2f} m/s^2, "
            f"steer={car_state['steering_angle']:.2f} deg"
        )
    
    def log_control(self, control):
        """コントロール入力ログ"""
        self.logger.info(
            f"Control: steer={control['actuators']['steer']:.3f}, "
            f"accel={control['actuators']['accel']:.3f}, "
            f"brake={control['actuators']['brake']:.3f}"
        )
```

### 可視化ツール

```python
# tools/metadrive/visualize.py

import matplotlib.pyplot as plt
import numpy as np
from collections import deque


class MetaDriveVisualizer:
    """リアルタイム可視化"""
    
    def __init__(self, history_length=100):
        self.history_length = history_length
        
        self.speed_history = deque(maxlen=history_length)
        self.accel_history = deque(maxlen=history_length)
        self.steer_history = deque(maxlen=history_length)
        
        plt.ion()
        self.fig, self.axes = plt.subplots(3, 1, figsize=(10, 8))
    
    def update(self, car_state, control):
        """データ更新・描画"""
        
        self.speed_history.append(car_state['speed'])
        self.accel_history.append(car_state['acceleration'])
        self.steer_history.append(control['actuators']['steer'])
        
        # プロット
        self.axes[0].clear()
        self.axes[0].plot(self.speed_history)
        self.axes[0].set_ylabel("Speed (m/s)")
        self.axes[0].grid(True)
        
        self.axes[1].clear()
        self.axes[1].plot(self.accel_history)
        self.axes[1].set_ylabel("Acceleration (m/s^2)")
        self.axes[1].grid(True)
        
        self.axes[2].clear()
        self.axes[2].plot(self.steer_history)
        self.axes[2].set_ylabel("Steering")
        self.axes[2].set_xlabel("Time Steps")
        self.axes[2].grid(True)
        
        plt.pause(0.01)
```

---

## まとめ

本ドキュメントでは、MetaDriveとopenpilotの統合実装を説明しました：

1. **アーキテクチャ**: マルチプロセス構成（openpilot ↔ MetaDrive）
2. **MetaDriveBridge**: cereal messagingでデータ変換
3. **MetaDriveWorld**: 独立プロセスでシミュレーション実行
4. **統合**: manager.pyでの起動
5. **高度な機能**: シナリオ再生、マルチエージェント
6. **デバッグ**: ログ記録、可視化

これにより、openpilotをMetaDrive上で動作させることができます。

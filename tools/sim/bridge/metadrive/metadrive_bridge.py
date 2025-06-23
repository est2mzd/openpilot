import math
from multiprocessing import Queue

from metadrive.component.sensors.base_camera import _cuda_enable
from metadrive.component.map.pg_map import MapGenerateMethod

from openpilot.tools.sim.bridge.common import SimulatorBridge
from openpilot.tools.sim.bridge.metadrive.metadrive_common import RGBCameraRoad, RGBCameraWide
from openpilot.tools.sim.bridge.metadrive.metadrive_world import MetaDriveWorld
from openpilot.tools.sim.lib.camerad import W, H

"""

"""
import copy
from metadrive.envs.base_env import BASE_DEFAULT_CONFIG

print(f"_cuda_enable = {_cuda_enable}")

my_vehicle_config_2 = copy.deepcopy(BASE_DEFAULT_CONFIG["vehicle_config"])
my_vehicle_config_2.update(dict(
  enable_reverse=False,
  render_vehicle=False,
  image_source="rgb_road",
  spawn_lane_index=("S", "E", 0),
  destination=("E", "X", 0)
))

my_vehicle_config_1 = {
  "enable_reverse": False,
  #"render_vehicle": False,
  "image_source": "rgb_road"
}

my_vehicle_config_3 = {
  "vehicle_model": "s",
  "enable_reverse": False,
  "image_source": "rgb_road",
  "spawn_longitude": 15
}

#----------------------------------------------------------#

def straight_block(length):
  return {
    "id": "S",
    "pre_block_socket_index": 0,
    "length": length
  }

def curve_block(length, angle=45, direction=0):
  return {
    "id": "C",
    "pre_block_socket_index": 0,
    "length": length,
    "radius": length,
    "angle": angle,
    "dir": direction
  }

def create_map(track_size=200):
  curve_len = track_size * 2
  return dict(
    type=MapGenerateMethod.PG_MAP_FILE,
    lane_num=2,
    lane_width=4.5,
    config=[
      None,
      straight_block(track_size),
      curve_block(curve_len, 90),
      straight_block(track_size),
      curve_block(curve_len, 90),
      straight_block(track_size),
      curve_block(curve_len, 90),
      straight_block(track_size),
      curve_block(curve_len, 90),
    ]
  )


class MetaDriveBridge(SimulatorBridge):
  TICKS_PER_FRAME = 5

  def __init__(self, dual_camera, high_quality, test_duration=math.inf, test_run=False):
    super().__init__(dual_camera, high_quality)

    self.should_render = False
    self.test_run = test_run
    self.test_duration = test_duration if self.test_run else math.inf

  def spawn_world(self, queue: Queue):
    sensors = {
      "rgb_road": (RGBCameraRoad, W, H, )
    }

    if self.dual_camera:
      sensors["rgb_wide"] = (RGBCameraWide, W, H)

    config = dict(
      use_render=self.should_render,
      vehicle_config=my_vehicle_config_1,
      sensors=sensors,
      image_on_cuda=_cuda_enable,
      image_observation=True,
      interface_panel=[],
      out_of_route_done=False,
      on_continuous_line_done=False,
      crash_vehicle_done=False,
      crash_object_done=False,
      arrive_dest_done=False,
      traffic_density=0.02, # traffic is incredibly expensive
      map_config=create_map(),
      decision_repeat=1,
      physics_world_step_size=self.TICKS_PER_FRAME/100,
      preload_models=False,
      show_logo=False,
      #anisotropic_filtering=False # TODO-MOD-BY-KOBA
    )

    return MetaDriveWorld(queue, config, self.test_duration, self.test_run, self.dual_camera)

"""
vehicle_config=dict(
  enable_reverse=False,
  render_vehicle=False,
  image_source="rgb_road",
),
"""
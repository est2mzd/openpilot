# MetaDrive Engine 詳細ドキュメント

## 目次
1. [概要](#概要)
2. [エンジンアーキテクチャ](#エンジンアーキテクチャ)
3. [EngineCore](#enginecore)
4. [BaseEngine](#baseengine)
5. [物理シミュレーション](#物理シミュレーション)
6. [レンダリングシステム](#レンダリングシステム)
7. [センサーシステム](#センサーシステム)
8. [アセット管理](#アセット管理)
9. [使用例](#使用例)

---

## 概要

MetaDriveのエンジンは、**Panda3D**ゲームエンジンをベースにした高性能なドライビングシミュレータのコアです。

### 主要機能

- **Panda3D統合**: ShowBase継承による3Dレンダリング
- **BulletPhysics**: リアルタイム物理シミュレーション (9.81 m/s² gravity)
- **高性能レンダリング**: SimplePBR, マルチスレッドレンダリング対応
- **多様なセンサー**: LiDAR, RGB/Depth/Semantic Camera, Distance Detector
- **シングルトンパターン**: エンジンインスタンスは1つのみ生成可能
- **カリキュラム学習**: レベルベースの段階的学習サポート

### ファイル構成

```
metadrive/engine/
├── base_engine.py          # BaseEngine (タスク管理、オブジェクト管理)
├── core/
│   ├── engine_core.py      # EngineCore (Panda3D/物理/レンダリング)
│   ├── physics_world.py    # PhysicsWorld (BulletWorld wrapper)
│   ├── image_buffer.py     # ImageBuffer (カメラ画像バッファ)
│   ├── light.py            # Light (照明システム)
│   ├── sky_box.py          # SkyBox (空の描画)
│   ├── terrain.py          # Terrain (地形レンダリング)
│   └── force_fps.py        # ForceFPS (FPS制御)
├── asset_loader.py         # アセット読み込み
├── top_down_renderer.py    # トップダウンレンダラ
└── interface.py            # 外部インターフェース
```

**コード規模**: engine/ ディレクトリ全体で **約3,500行** (Python)

---

## エンジンアーキテクチャ

MetaDriveエンジンは2層構造になっています:

```
┌─────────────────────────────────────┐
│        BaseEngine (タスク層)         │
│  ・オブジェクト管理 (spawn/clear)     │
│  ・ポリシー管理 (add_policy)         │
│  ・エピソード管理 (record/replay)    │
│  ・カリキュラム学習                  │
└─────────────────────────────────────┘
                  ↓
┌─────────────────────────────────────┐
│       EngineCore (レンダリング層)     │
│  ・Panda3D ShowBase                 │
│  ・BulletPhysics統合                │
│  ・レンダリングパイプライン           │
│  ・センサー管理                      │
└─────────────────────────────────────┘
```

### シングルトンパターン

```python
class BaseEngine(EngineCore, Randomizable):
    singleton = None  # シングルトンインスタンス
    
    def __init__(self, global_config):
        # エンジンは1度しか生成できない
        BaseEngine.singleton = self
```

**理由**: Panda3Dの制約により、複数のShowBaseインスタンスを同時に生成できない

---

## EngineCore

**ファイル**: [metadrive/engine/core/engine_core.py](../openpilot/metadrive/metadrive/engine/core/engine_core.py)  
**行数**: 618行

### 継承関係

```python
class EngineCore(ShowBase.ShowBase):
    """
    Panda3D ShowBaseを継承したコアエンジン
    """
```

### 初期化パラメータ

```python
def __init__(self, global_config):
    self.mode = global_config["_render_mode"]  # ONSCREEN/OFFSCREEN/NONE
    self.global_config = global_config
```

#### レンダリングモード

| モード | 説明 | 用途 |
|--------|------|------|
| `RENDER_MODE_ONSCREEN` | ウィンドウ表示 | デバッグ、デモ |
| `RENDER_MODE_OFFSCREEN` | オフスクリーンレンダリング | 画像観測、学習 |
| `RENDER_MODE_NONE` | レンダリング無効 | LiDAR専用、高速シミュレーション |

### Panda3D設定

```python
# ウィンドウ設定
loadPrcFileData("", "window-title MetaDrive")
loadPrcFileData("", "win-size {} {}".format(*config["window_size"]))

# 物理エンジン設定
loadPrcFileData("", "bullet-filter-algorithm groups-mask")

# アンチエイリアシング
if is_mac():
    loadPrcFileData("", "framebuffer-multisample 1")
    loadPrcFileData("", "multisamples 4")  # macOS: 4x MSAA
else:
    loadPrcFileData("", "multisamples 8")  # その他: 8x MSAA
```

### マルチスレッドレンダリング

```python
if config["multi_thread_render"]:
    loadPrcFileData("", "threading-model {}".format(
        config["multi_thread_render_mode"]
    ))
```

**オプション**:
- `"Cull/Draw"`: カリング/描画を別スレッド化 (推奨)
- `"App/Draw"`: アプリ/描画を別スレッド化

### SimplePBR統合

```python
if self.use_render_pipeline:
    from metadrive.render_pipeline.rpcore import RenderPipeline
    self.render_pipeline = RenderPipeline()
    self.render_pipeline.pre_showbase_init()
```

**SimplePBR**: 物理ベースレンダリング (PBR) を実現するシェーダーシステム

### デバッグ機能

```python
if config["debug"]:
    self.accept("1", self.toggleDebug)      # BulletDebug表示切替
    self.accept("2", self.toggleWireframe)  # ワイヤーフレーム表示
    self.accept("3", self.toggleTexture)    # テクスチャ表示切替
    self.accept("4", self.toggleAnalyze)    # シーングラフ解析
    self.accept("5", self.reload_shader)    # シェーダーリロード
```

### パフォーマンス監視 (pstats)

```python
if config["pstats"]:
    loadPrcFileData("", "want-pstats 1")
    self.pstats_process = subprocess.Popen(['pstats'])
```

**pstats**: Panda3D標準のプロファイリングツール (port 5185で動作)

---

## BaseEngine

**ファイル**: [metadrive/engine/base_engine.py](../openpilot/metadrive/metadrive/engine/base_engine.py)  
**行数**: 828行

### 主要コンポーネント

```python
class BaseEngine(EngineCore, Randomizable):
    def __init__(self, global_config):
        EngineCore.__init__(self, global_config)
        Randomizable.__init__(self, self.global_random_seed)
        
        # タスク管理
        self.task_manager = self.taskMgr  # Panda3D TaskMgr
        self._managers = OrderedDict()    # カスタムマネージャー
        
        # オブジェクト管理
        self._spawned_objects = dict()    # 生成済みオブジェクト
        self._dying_objects = dict()      # オブジェクトプール
        self._object_policies = dict()    # ポリシー (AI制御)
        self._object_tasks = dict()       # タスク
        
        # インターフェース
        self.interface = Interface(self)
        
        # メインカメラ
        self.main_camera = self.setup_main_camera()
        
        # トップダウンレンダラ
        self.top_down_renderer = None
```

### カラーマネージメント

```python
def generate_distinct_rgb_values():
    # 16x16x16 = 4,096個の識別可能なRGB値を生成
    r = np.linspace(16, 256-16, 16).astype(int)
    g = np.linspace(16, 256-16, 16).astype(int)
    b = np.linspace(16, 256-16, 16).astype(int)
    rgbs = np.array(np.meshgrid(r, g, b)).T.reshape(-1, 3)
    return tuple(tuple(round(vv, 5) for vv in v) for v in rgbs / 255.0)

COLOR_SPACE = generate_distinct_rgb_values()
```

**用途**: Semantic Cameraでオブジェクトごとに異なる色を割り当て

### オブジェクト管理

#### spawn_object - オブジェクト生成

```python
def spawn_object(self, object_class, force_spawn=False, 
                 auto_fill_random_seed=True, **kwargs):
    """
    オブジェクトを生成 (オブジェクトプール利用)
    
    Args:
        object_class: 生成するクラス
        force_spawn: プールを使わず強制的に新規生成
        auto_fill_random_seed: ランダムシード自動設定
        **kwargs: オブジェクト初期化パラメータ
        
    Returns:
        生成されたオブジェクト
    """
    if auto_fill_random_seed and "random_seed" not in kwargs:
        kwargs["random_seed"] = self.generate_seed()
    
    # オブジェクトプールから再利用
    if not force_spawn and object_class.__name__ in self._dying_objects:
        pool = self._dying_objects[object_class.__name__]
        if len(pool) > 0:
            obj = pool.pop()
            obj.reset(**kwargs)  # 再初期化
            return obj
    
    # 新規生成
    obj = object_class(**kwargs)
    self._spawned_objects[obj.id] = obj
    return obj
```

**オブジェクトプール**: メモリアロケーションを減らし、パフォーマンスを向上

#### clear_objects - オブジェクト削除

```python
def clear_objects(self, objects: List[str]):
    """
    オブジェクトをクリア (プールに戻す)
    
    Args:
        objects: クリアするオブジェクトIDのリスト
    """
    for obj_id in objects:
        if obj_id in self._spawned_objects:
            obj = self._spawned_objects.pop(obj_id)
            
            # ポリシー削除
            if obj_id in self._object_policies:
                self._object_policies.pop(obj_id)
            
            # オブジェクトプールに追加
            class_name = obj.__class__.__name__
            if class_name not in self._dying_objects:
                self._dying_objects[class_name] = []
            self._dying_objects[class_name].append(obj)
            
            # リセット
            obj.destroy()
```

### ポリシー管理

```python
def add_policy(self, object_id, policy_class, *args, **kwargs):
    """
    オブジェクトにポリシー (AI制御) を追加
    
    Args:
        object_id: 対象オブジェクトID
        policy_class: ポリシークラス
        
    Returns:
        生成されたポリシー
    """
    policy = policy_class(*args, **kwargs)
    self._object_policies[object_id] = policy
    
    if self.record_episode:
        self.record_manager.add_policy_info(
            object_id, policy_class, *args, **kwargs
        )
    
    return policy

def get_policy(self, object_id):
    """ポリシー取得"""
    return self._object_policies.get(object_id, None)
```

**ポリシー**: 車両やエージェントの行動を制御するAI (例: IDMPolicy, ReplayPolicy)

### カリキュラム学習

```python
def __init__(self, global_config):
    # カリキュラムレベル設定
    self._max_level = global_config.get("curriculum_level", 1)
    self._current_level = 0
    self._num_scenarios_per_level = int(
        global_config.get("num_scenarios", 1) / self._max_level
    )

def curriculum_reset(self):
    """
    カリキュラム学習用リセット
    レベルを段階的に上げる
    """
    self._current_level = min(
        self._current_level + 1, 
        self._max_level - 1
    )
```

**カリキュラム学習**: 簡単なシナリオから徐々に難易度を上げる学習手法

### エピソード記録/再生

```python
def __init__(self, global_config):
    # 記録/再生モード
    self.record_episode = False  # エピソード記録
    self.replay_episode = False  # エピソード再生
```

**用途**: 
- **record**: エージェントの行動を記録
- **replay**: 記録した行動を再生 (デバッグ、可視化)

---

## 物理シミュレーション

**ファイル**: [metadrive/engine/core/physics_world.py](../openpilot/metadrive/metadrive/engine/core/physics_world.py)  
**行数**: 約100行

### PhysicsWorld

```python
class PhysicsWorld:
    def __init__(self, debug=False, disable_collision=False):
        # 動的世界 (移動/相互作用するオブジェクト)
        self.dynamic_world = BulletWorld()
        self.dynamic_world.setGravity(Vec3(0, 0, -9.81))  # 重力設定
        
        # 静的世界 (位置クエリ専用、物理計算なし)
        self.static_world = BulletWorld() if not debug else self.dynamic_world
        
        # 衝突ルール設定
        CollisionGroup.set_collision_rule(
            self.dynamic_world, 
            disable_collision=disable_collision
        )
```

### 動的世界 vs 静的世界

| 世界 | 用途 | 物理計算 | オブジェクト例 |
|------|------|----------|----------------|
| **dynamic_world** | 移動・衝突検出 | あり | 車両、歩行者、動的障害物 |
| **static_world** | 位置クエリ | なし | 道路、建物、静的障害物 |

### 重力設定

```python
self.dynamic_world.setGravity(Vec3(0, 0, -9.81))  # m/s²
```

**値**: -9.81 m/s² (地球の重力加速度)

### 衝突グループ

```python
class CollisionGroup:
    @staticmethod
    def set_collision_rule(world, disable_collision=False):
        """
        衝突ルールを設定
        
        - Vehicle vs Vehicle: 検出
        - Vehicle vs Terrain: 検出
        - LiDAR vs 全て: 検出
        - Camera vs 全て: 無視
        """
        pass
```

**CollisionGroup**: オブジェクト種別ごとの衝突検出ON/OFF制御

### 物理ステップ実行

```python
def step_physics(self, dt):
    """
    物理シミュレーションを1ステップ進める
    
    Args:
        dt: タイムステップ (秒)
    """
    self.dynamic_world.doPhysics(dt)
```

**dt**: 通常 0.02秒 (50Hz) または 0.01秒 (100Hz)

---

## レンダリングシステム

### レンダリングパイプライン

```
┌──────────────┐
│ シーン構築    │ (Panda3D SceneGraph)
└──────────────┘
        ↓
┌──────────────┐
│ カリング      │ (視錐台カリング、オクルージョンカリング)
└──────────────┘
        ↓
┌──────────────┐
│ 描画          │ (OpenGL / DirectX)
└──────────────┘
        ↓
┌──────────────┐
│ ポストプロセス│ (トーンマッピング、SSAO等)
└──────────────┘
```

### SimplePBR (Physical Based Rendering)

**ファイル**: `metadrive/third_party/simplepbr/`

```python
from metadrive.third_party.simplepbr import init

# PBRシェーダー初期化
init(self)
```

**特徴**:
- **リアルな質感**: 金属、プラスチック、布等
- **環境マッピング**: 反射/屈折
- **シャドウマッピング**: PSSM (Parallel-Split Shadow Maps)

### 照明システム

**ファイル**: `metadrive/engine/core/light.py`

```python
class Light:
    """
    太陽光 + 環境光
    """
    def __init__(self, engine):
        # 太陽光 (Directional Light)
        self.sun = DirectionalLight("sun")
        self.sun.setColor((1, 1, 1, 1))
        
        # 環境光 (Ambient Light)
        self.ambient = AmbientLight("ambient")
        self.ambient.setColor((0.4, 0.4, 0.4, 1))
```

### 空の描画

**ファイル**: `metadrive/engine/core/sky_box.py`

```python
class SkyBox:
    """
    スカイボックス (6面体の背景画像)
    """
    def __init__(self, engine):
        self.skybox = loader.loadModel("models/skybox.egg")
        self.skybox.setScale(10000)  # 巨大なボックス
        self.skybox.reparentTo(render)
```

### 地形レンダリング

**ファイル**: `metadrive/engine/core/terrain.py`

```python
class Terrain:
    """
    道路・地面の描画
    """
    @staticmethod
    def make_render_state(engine, vert_shader, frag_shader):
        """
        地形専用のシェーダーを設定
        
        Args:
            vert_shader: 頂点シェーダー (terrain.vert.glsl)
            frag_shader: フラグメントシェーダー (terrain.frag.glsl)
        """
        pass
```

### トップダウンレンダラ

**ファイル**: [metadrive/engine/top_down_renderer.py](../openpilot/metadrive/metadrive/engine/top_down_renderer.py)

```python
class TopDownRenderer:
    """
    鳥瞰図 (Top-Down View) レンダラ
    """
    def __init__(self, engine, width=512, height=512):
        self.cam = Camera("top_down_camera")
        self.cam.setPos(0, 0, 100)  # 上空から見下ろす
        self.cam.lookAt(0, 0, 0)
```

**用途**: 
- 可視化 (デバッグ、デモ)
- ミニマップ生成
- 観測空間 (鳥瞰画像観測)

---

## センサーシステム

**ディレクトリ**: [metadrive/component/sensors/](../openpilot/metadrive/metadrive/component/sensors/)

### BaseSensor

**ファイル**: [base_sensor.py](../openpilot/metadrive/metadrive/component/sensors/base_sensor.py)

```python
class BaseSensor:
    """
    全センサーの基底クラス
    """
    def perceive(self, *args, **kwargs):
        """
        センサー出力を取得
        
        Returns:
            センサーデータ (画像、点群、距離等)
        """
        raise NotImplementedError
```

### センサー一覧

| センサー | ファイル | 出力 | 用途 |
|---------|---------|------|------|
| **LiDAR** | `lidar.py` | 240点の距離配列 | 障害物検出、SLAM |
| **RGB Camera** | `rgb_camera.py` | RGB画像 (84x84x3) | 視覚認識、エンドツーエンド学習 |
| **Depth Camera** | `depth_camera.py` | 深度画像 (84x84) | 距離推定、3D再構成 |
| **Semantic Camera** | `semantic_camera.py` | セマンティックセグメンテーション画像 | オブジェクト識別 |
| **Instance Camera** | `instance_camera.py` | インスタンスセグメンテーション画像 | 個別オブジェクト追跡 |
| **Dashboard** | `dashboard.py` | 速度計、燃料計等のUI | 状態表示 |
| **MiniMap** | `mini_map.py` | ミニマップ画像 | ナビゲーション |
| **Distance Detector** | `distance_detector.py` | 前方距離 | 追従制御、衝突回避 |

### LiDAR

**ファイル**: [lidar.py](../openpilot/metadrive/metadrive/component/sensors/lidar.py)  
**行数**: 212行

```python
class Lidar(DistanceDetector):
    DEFAULT_HEIGHT = 1.2  # センサー高さ (m)
    
    def perceive(self, base_vehicle, physics_world, 
                 num_lasers, distance, height=None, show=False):
        """
        LiDAR計測
        
        Args:
            base_vehicle: 車両オブジェクト
            physics_world: 物理世界
            num_lasers: レーザー本数 (通常240本)
            distance: 最大検出距離 (m)
            height: センサー高さ (m)
            show: レーザー可視化
            
        Returns:
            距離配列 (shape: (num_lasers,))
            検出オブジェクトリスト
        """
        pass
```

#### LiDAR仕様

| パラメータ | デフォルト値 | 説明 |
|-----------|-------------|------|
| **num_lasers** | 240 | レーザー本数 (360°を240分割) |
| **distance** | 50.0 (m) | 最大検出距離 |
| **height** | 1.2 (m) | 地面からの高さ |
| **angle_resolution** | 1.5° | 角度分解能 (360/240) |

#### レーザー検出方法

```python
def _get_lidar_mask(self, vehicle, num_lasers, distance):
    """
    BulletPhysicsのレイキャスト (光線投射) を使用
    
    1. 車両中心から放射状にレイを発射
    2. 衝突オブジェクトまでの距離を計測
    3. 距離配列を返す
    """
    pass
```

**検出可能オブジェクト**:
- 他の車両
- 道路境界
- 建物
- 静的障害物

#### 周辺車両取得

```python
@staticmethod
def get_surrounding_vehicles(detected_objects) -> Set:
    """
    検出オブジェクトから車両のみを抽出
    
    Returns:
        Set[BaseVehicle]: 周辺車両の集合
    """
    from metadrive.component.vehicle.base_vehicle import BaseVehicle
    vehicles = set()
    for obj in detected_objects:
        if isinstance(obj, BaseVehicle):
            vehicles.add(obj)
    return vehicles
```

### RGB Camera

**ファイル**: [rgb_camera.py](../openpilot/metadrive/metadrive/component/sensors/rgb_camera.py)

```python
class RGBCamera(BaseCamera):
    BUFFER_W = 84  # 画像幅
    BUFFER_H = 84  # 画像高さ
    CAM_MASK = CamMask.RgbCam
    
    def __init__(self, width, height, engine, *, cuda=False):
        self.BUFFER_W, self.BUFFER_H = width, height
        super(RGBCamera, self).__init__(engine, cuda)
```

#### トーンマッピング

```python
def _setup_effect(self):
    """
    SimplePBRエフェクト設定
    
    1. HDRレンダリング (16bit float RGBA)
    2. トーンマッピング (post.vert.glsl, tonemap.frag.glsl)
    3. 露出調整 (exposure=1.0)
    """
    self.scene_tex = p3d.Texture()
    self.scene_tex.set_format(p3d.Texture.F_rgba16)
    self.scene_tex.set_component_type(p3d.Texture.T_float)
    
    # トーンマッピングシェーダー
    tonemap_shader = p3d.Shader.make(
        p3d.Shader.SL_GLSL,
        vertex=post_vert_str,
        fragment=post_frag_str,
    )
    self.tonemap_quad.set_shader(tonemap_shader)
    self.tonemap_quad.set_shader_input('exposure', 1.0)
```

#### MSAA (Multi-Sample Anti-Aliasing)

```python
fbprops = p3d.FrameBufferProperties()
if is_mac():
    fbprops.set_multisamples(4)   # macOS: 4x MSAA
else:
    fbprops.set_multisamples(16)  # その他: 16x MSAA
```

### Depth Camera

**ファイル**: `depth_camera.py`

```python
class DepthCamera(BaseCamera):
    """
    深度カメラ (シーン深度を0-1にマッピング)
    """
    def perceive(self):
        """
        Returns:
            深度画像 (shape: (height, width), dtype: float32)
            値範囲: [0, 1] (0=near, 1=far)
        """
        pass
```

### Semantic Camera

**ファイル**: `semantic_camera.py`

```python
class SemanticCamera(BaseCamera):
    """
    セマンティックセグメンテーションカメラ
    オブジェクトクラスごとに異なる色を割り当て
    """
    def perceive(self):
        """
        Returns:
            セマンティック画像 (shape: (height, width, 3))
            各オブジェクトクラスに固有のRGB色が割り当てられる
        """
        pass
```

**カラーマッピング**:
- Vehicle (車両): Red (1, 0, 0)
- Lane (車線): Green (0, 1, 0)
- Sidewalk (歩道): Blue (0, 0, 1)
- Building (建物): Yellow (1, 1, 0)

### BaseCamera

**ファイル**: `base_camera.py`

```python
class BaseCamera(BaseSensor):
    """
    全カメラセンサーの基底クラス
    """
    def __init__(self, engine, cuda=False):
        self.engine = engine
        self.cuda = cuda  # CUDA加速 (GPU転送高速化)
        
        # カメラバッファ作成
        self.buffer = self._create_buffer(
            self.BUFFER_W, 
            self.BUFFER_H, 
            None
        )
        
        # カメラノード作成
        self.cam = self.buffer.makeCamera()
    
    def perceive(self):
        """
        画像データ取得
        
        Returns:
            np.ndarray: 画像配列 (shape: (H, W, C))
        """
        # GPU → CPU転送
        self.buffer.trigger_copy()
        tex = self.buffer.getTexture()
        
        # Texture → NumPy配列
        data = tex.getRamImage()
        img = np.frombuffer(data, dtype=np.uint8)
        img = img.reshape((self.BUFFER_H, self.BUFFER_W, -1))
        
        return img
```

#### CUDA最適化

```python
if self.cuda:
    # GPU → GPU転送 (CPU経由なし)
    self.buffer.setActive(True)
    self.buffer.trigger_copy()
```

**効果**: CPU-GPU間のデータ転送を削減し、高速化

---

## アセット管理

**ファイル**: [metadrive/engine/asset_loader.py](../openpilot/metadrive/metadrive/engine/asset_loader.py)

### AssetLoader

```python
def initialize_asset_loader(engine):
    """
    アセットローダー初期化
    
    1. アセットディレクトリ検索
    2. モデル/テクスチャ/サウンド読み込み
    3. キャッシュ設定
    """
    pass

def pull_asset():
    """
    リモートからアセットをダウンロード
    
    - 3Dモデル (.egg, .bam, .gltf)
    - テクスチャ (.png, .jpg)
    - スカイボックス画像
    """
    pass
```

### アセット種別

| 種別 | 拡張子 | 用途 |
|------|--------|------|
| **3Dモデル** | .egg, .bam, .gltf | 車両、建物、道路 |
| **テクスチャ** | .png, .jpg | 地面、壁、道路マーキング |
| **シェーダー** | .vert.glsl, .frag.glsl | カスタムレンダリング |
| **サウンド** | .wav, .ogg | エンジン音、環境音 |

### モデルキャッシュ

```python
from panda3d.core import ModelPool, TexturePool

# モデルキャッシュサイズ設定
loadPrcFileData("", "model-cache-compressed-textures 1")
loadPrcFileData("", "textures-power-2 none")
```

**効果**: 同じモデルを複数回読み込まず、メモリ使用量を削減

---

## 使用例

### 基本的なエンジン初期化

```python
from metadrive.engine.base_engine import BaseEngine
from metadrive.constants import RENDER_MODE_ONSCREEN

# 設定
config = {
    "_render_mode": RENDER_MODE_ONSCREEN,
    "window_size": (800, 600),
    "use_render": True,
    "debug": True,
    "show_interface": True,
    "sensors": {
        "main_camera": (RGBCamera, 800, 600),
    },
}

# エンジン生成
engine = BaseEngine(config)
```

### オブジェクト生成と物理シミュレーション

```python
from metadrive.component.vehicle.base_vehicle import BaseVehicle

# 車両生成
vehicle = engine.spawn_object(
    BaseVehicle,
    random_seed=0,
    position=(0, 0, 0),
    heading=0,
)

# ポリシー追加 (手動制御)
from metadrive.policy.manual_control_policy import ManualControlPolicy
engine.add_policy(vehicle.id, ManualControlPolicy, engine)

# 物理シミュレーションループ
dt = 0.02  # 50Hz
for step in range(1000):
    # ポリシー実行
    action = engine.get_policy(vehicle.id).act()
    vehicle.apply_action(action)
    
    # 物理ステップ
    engine.physics_world.dynamic_world.doPhysics(dt)
    
    # レンダリング
    engine.taskMgr.step()
```

### LiDAR計測

```python
from metadrive.component.sensors.lidar import Lidar

# LiDAR生成
lidar = Lidar(engine)

# 計測
distances, objects = lidar.perceive(
    base_vehicle=vehicle,
    physics_world=engine.physics_world,
    num_lasers=240,
    distance=50.0,
    height=1.2,
    show=True  # レーザー可視化
)

print("距離配列:", distances.shape)  # (240,)
print("検出オブジェクト数:", len(objects))
```

### RGB Camera画像取得

```python
from metadrive.component.sensors.rgb_camera import RGBCamera

# カメラ生成
camera = RGBCamera(width=84, height=84, engine=engine)

# 画像取得
img = camera.perceive()
print("画像shape:", img.shape)  # (84, 84, 3)

# 画像保存
from PIL import Image
Image.fromarray(img).save("camera_view.png")
```

### トップダウンレンダリング

```python
from metadrive.engine.top_down_renderer import TopDownRenderer

# トップダウンレンダラ生成
top_down = TopDownRenderer(engine, width=512, height=512)

# 鳥瞰図レンダリング
img = top_down.render()
print("鳥瞰図shape:", img.shape)  # (512, 512, 3)
```

### カリキュラム学習

```python
# カリキュラムレベル設定
config = {
    "curriculum_level": 5,      # 5段階
    "num_scenarios": 1000,      # 1000シナリオ
}

engine = BaseEngine(config)

# エピソードごとにレベルアップ
for episode in range(100):
    engine.reset()
    
    if episode % 20 == 0:
        engine.curriculum_reset()  # レベルアップ
        print(f"Level {engine._current_level + 1}")
```

### エピソード記録と再生

```python
# 記録モード
config["record_episode"] = True
engine = BaseEngine(config)

# エピソード実行 (自動的に記録される)
for step in range(1000):
    action = policy.act()
    vehicle.apply_action(action)
    engine.step()

# 記録保存
engine.record_manager.save("episode_001.pkl")

# 再生モード
config["replay_episode"] = True
engine = BaseEngine(config)
engine.replay_manager.load("episode_001.pkl")

for step in range(1000):
    engine.step()  # 記録された行動を再生
```

### マルチスレッドレンダリング

```python
config = {
    "multi_thread_render": True,
    "multi_thread_render_mode": "Cull/Draw",  # カリング/描画を並列化
}

engine = BaseEngine(config)
# レンダリングが自動的に別スレッドで実行される
```

### デバッグ機能

```python
config["debug"] = True
config["debug_panda3d"] = True  # Panda3D詳細ログ
engine = BaseEngine(config)

# デバッグキー
# 1: BulletDebug表示 (衝突形状、重心等)
# 2: ワイヤーフレーム表示
# 3: テクスチャON/OFF
# 4: シーングラフ解析
# 5: シェーダーリロード
```

---

## まとめ

MetaDriveのエンジンは、以下の特徴を持つ高性能シミュレータです:

### 主要特徴

1. **Panda3Dベース**: 強力な3Dゲームエンジン上に構築
2. **BulletPhysics**: リアルタイム物理シミュレーション (9.81 m/s² gravity)
3. **SimplePBR**: 物理ベースレンダリング (リアルな質感)
4. **多様なセンサー**: LiDAR, RGB/Depth/Semantic Camera等
5. **高性能**: マルチスレッドレンダリング、オブジェクトプール
6. **柔軟性**: 3つのレンダリングモード (ONSCREEN/OFFSCREEN/NONE)
7. **カリキュラム学習**: 段階的な学習サポート
8. **記録/再生**: エピソードの記録と再生機能

### パフォーマンス

- **1000+ FPS**: レンダリングなし (RENDER_MODE_NONE)
- **200-500 FPS**: オフスクリーンレンダリング (84x84画像)
- **60-120 FPS**: ウィンドウ表示 (800x600)

### 適用例

- **強化学習**: Gymnasium環境との統合
- **自動運転研究**: センサーフュージョン、経路計画
- **データ生成**: 合成データ生成 (画像、LiDAR点群)
- **シナリオ評価**: 実世界ログ再生 (nuScenes/Waymo)

MetaDriveエンジンは、openpilotのシミュレーション環境として、リアルタイム性能とリアリティのバランスが取れた設計になっています。

# Manager詳細

このドキュメントでは、openpilotのManager（マネージャー）の実装詳細を説明します。

> **📖 関連ドキュメント**
> - [実行フロー概要](../execution-flow-ja.md) - 全体の流れ
> - [msgq詳細](msgq-details.md) - プロセス間通信
> - [プロセス管理詳細](#3-プロセス管理の詳細) - 下部セクション


**ファイル**: [system/manager/manager.py](../../system/manager/manager.py)

Managerはopenpilotの**中央管理プロセス**で、以下の役割を担います：

- **プロセス起動・監視・停止**
- **走行状態の監視**（onroad/offroad）
- **設定管理**（Params）
- **ログシステムの初期化**
- **プロセス状態の配信**

## 2. 初期化処理（manager_init）

### 2.1 Paramsの初期化

```python
params = Params()
params.clear_all(ParamKeyType.CLEAR_ON_MANAGER_START)
```

**Paramsとは？**
- openpilotの設定管理システム
- C++で実装され、Cythonを通じてPythonから利用
- キー・バリュー型のストレージ
- 複数のプロセス間で設定を共有

**関連ファイル**:
- [common/params.h](../../common/params.h) - C++ヘッダー
- [common/params.cc](../../common/params.cc) - C++実装
- [common/params_pyx.pyx](../../common/params_pyx.pyx) - Pythonバインディング

**ParamKeyTypeの種類**:
- `CLEAR_ON_MANAGER_START`: Manager起動時にクリア
- `CLEAR_ON_ONROAD_TRANSITION`: 走行開始時にクリア
- `CLEAR_ON_OFFROAD_TRANSITION`: 走行終了時にクリア
- `PERSISTENT`: 永続的に保持

### 2.2 デバイス登録

```python
serial = HARDWARE.get_serial()
reg_res = register(show_spinner=True)
dongle_id = reg_res
```

**処理内容**:
1. ハードウェアのシリアル番号取得
2. comma.aiクラウドへの登録リクエスト
3. dongle_id（デバイス識別子）の取得

**dongle_idの用途**:
- デバイスの一意識別
- ログのアップロード先識別
- Athena通信の認証
- OTAアップデート管理

### 2.3 ログシステムの初期化

#### Sentry（エラー監視）

```python
if os.getenv("SENTRY"):
    from system.sensord.sentry import init_sentry
    init_sentry(manager=True)
```

**Sentry**の役割:
- リアルタイムのエラー監視
- クラッシュレポートの収集
- スタックトレースの記録
- パフォーマンス監視

**有効化条件**:
- 本家comma.aiの公式ビルド
- 実機（TICI）での動作
- `SENTRY`環境変数の設定

#### cloudlog（独自ロギング）

```python
cloudlog.bind_global(
    version=version,
    origin=origin,
    branch=branch,
    commit=commit[:7],
    device=HARDWARE.get_device_type(),
    dongle_id=dongle_id,
    dirty=dirty
)
```

**cloudlog**の役割:
- openpilot独自のロギングシステム
- グローバルメタデータの管理
- プロセス間でのログ情報共有
- コンテキスト情報の自動付与

**バインドされる情報**:
- **version**: openpilotバージョン
- **origin**: Gitリモート（fork元）
- **branch**: Gitブランチ
- **commit**: コミットハッシュ
- **device**: ハードウェアタイプ（TICI、PC等）
- **dongle_id**: デバイス識別子
- **dirty**: Git作業ディレクトリの変更有無

### 2.4 プロセスの事前準備

```python
for p in managed_processes.values():
    p.prepare()
```

**prepare()の役割**:
- プロセスの起動可能性チェック
- 依存ファイルの存在確認
- 環境変数の設定
- ビルドが必要な場合のビルド実行

**例**: ネイティブプロセスの場合
```python
class NativeProcess:
    def prepare(self):
        # バイナリファイルの存在確認
        if not os.path.isfile(self.exe_path):
            raise FileNotFoundError(f"{self.exe_path} not found")

        # 実行権限のチェック
        if not os.access(self.exe_path, os.X_OK):
            raise PermissionError(f"{self.exe_path} not executable")
```

## 3. メインループ（manager_thread）

### 3.1 Pub/Sub通信の初期化

```python
sm = messaging.SubMaster(['deviceState', 'carParams'], poll='deviceState')
pm = messaging.PubMaster(['managerState'])
```

**SubMaster**:
- 複数のメッセージトピックを購読
- `poll='deviceState'`: deviceStateの更新を優先的に監視
- 内部でリングバッファからデータを読み取り

**PubMaster**:
- メッセージの配信
- `managerState`: Managerの状態情報を配信
- 他のプロセスがManager状態を監視可能

### 3.2 監視ループの詳細

```python
while True:
    sm.update(1000)  # 1000msタイムアウト
    started = sm['deviceState'].started

    # 走行状態の変化を検出
    if started and not started_prev:
        # offroad → onroad 遷移
        params.clear_all(ParamKeyType.CLEAR_ON_ONROAD_TRANSITION)
        cloudlog.event("onroad transition")
    elif not started and started_prev:
        # onroad → offroad 遷移
        params.clear_all(ParamKeyType.CLEAR_ON_OFFROAD_TRANSITION)
        cloudlog.event("offroad transition")

    started_prev = started

    # 必要なプロセスを起動・維持
    ensure_running(managed_processes.values(), started, params=params,
                   CP=sm['carParams'], not_run=ignore)

    # プロセス状態の収集
    running = {p.name: p.proc.exitcode is None for p in managed_processes.values() if p.proc}

    # managerStateの配信
    msg = messaging.new_message('managerState')
    msg.managerState.processes = running
    pm.send('managerState', msg)
```

**処理の流れ**:

1. **メッセージ更新**（`sm.update(1000)`）
   - 1000msタイムアウトでブロッキング
   - deviceStateの更新を待機
   - 新しいメッセージが来たら即座に処理

2. **走行状態の検出**
   - `started = sm['deviceState'].started`
   - `True`: 走行中（エンジンON、ギアがD/R等）
   - `False`: 停車中（パーキング、エンジンOFF等）

3. **状態遷移処理**
   - onroad遷移: 一時的な設定をクリア（前回の走行データ等）
   - offroad遷移: 走行中のみ必要な設定をクリア

4. **プロセス管理**（`ensure_running`）
   - 各プロセスの起動条件を評価
   - 必要なプロセスを起動
   - 不要なプロセスを停止
   - クラッシュしたプロセスを再起動

5. **状態配信**
   - 全プロセスの実行状態を収集
   - `managerState`として配信
   - 他のプロセスがManager状態を監視可能

### 3.3 ensure_running関数の詳細

```python
def ensure_running(processes, started, params=None, CP=None, not_run=None):
    not_run = not_run or set()

    for p in processes:
        # プロセスをスキップすべきか判定
        if p.name in not_run:
            continue

        # 起動条件を評価
        should_run = p.enabled and p.should_run(started, params, CP)

        # プロセスの状態を確認
        is_running = p.proc and p.proc.exitcode is None

        # 起動・停止の判断
        if should_run and not is_running:
            # 起動が必要
            p.start()
        elif not should_run and is_running:
            # 停止が必要
            p.stop()
```

**判定ロジック**:
- `p.enabled`: プラットフォーム依存の有効フラグ
- `p.should_run(started, params, CP)`: 動的な起動条件関数
- `p.proc.exitcode is None`: プロセスが実行中か

**起動条件の例**:
```python
# 走行中のみ実行
def only_onroad(started, params, CP):
    return started

# 常時実行
def always_run(started, params, CP):
    return True

# 走行中かつ実車の場合のみ実行
def iscar(started, params, CP):
    return started and not CP.notCar
```

## 4. クリーンアップ（manager_cleanup）

```python
def manager_cleanup() -> None:
    # 第1段階: 非同期で全プロセスに停止シグナル送信
    cloudlog.info("manager_cleanup: stopping processes")
    for p in managed_processes.values():
        p.stop(block=False)

    # 第2段階: ブロッキングで完全停止を待機
    cloudlog.info("manager_cleanup: waiting for processes to stop")
    for p in managed_processes.values():
        p.stop(block=True)

    cloudlog.info("manager_cleanup: all processes stopped")
```

**2段階停止の理由**:

### なぜ2段階？

**1段階目（非同期）**:
- 全プロセスに`SIGTERM`を送信
- ブロックせずに即座に次のプロセスへ
- 全プロセスが同時に終了処理を開始

**2段階目（ブロッキング）**:
- 各プロセスの終了を待機（`proc.wait()`）
- タイムアウト処理（通常10秒）
- 強制終了（`SIGKILL`）が必要な場合に実行

**メリット**:
```
同期的停止（悪い例）:
Process1.stop() [10秒] → Process2.stop() [10秒] → Process3.stop() [10秒]
合計: 30秒

並列停止（良い例）:
Process1.stop(block=False) [即座]
Process2.stop(block=False) [即座]
Process3.stop(block=False) [即座]
↓
全プロセス並列に終了処理 [10秒]
合計: 10秒
```

### プロセス停止の詳細

```python
class ManagerProcess:
    def stop(self, block=True):
        if self.proc is None:
            return

        # SIGTERM送信
        if self.proc.exitcode is None:
            self.signal(signal.SIGTERM)

        # ブロッキングモードの場合
        if block:
            try:
                self.proc.wait(timeout=10)
            except subprocess.TimeoutExpired:
                # タイムアウト: 強制終了
                cloudlog.warning(f"{self.name}: timeout, sending SIGKILL")
                self.signal(signal.SIGKILL)
                self.proc.wait()
```

## 5. プロセス管理の詳細

### 5.1 プロセスタイプ

**PythonProcess**:
```python
class PythonProcess(ManagerProcess):
    def __init__(self, name, module, should_run):
        super().__init__(name, should_run)
        self.module = module

    def start(self):
        self.proc = subprocess.Popen(
            ["python", "-m", self.module],
            env=os.environ.copy()
        )
```

**NativeProcess**:
```python
class NativeProcess(ManagerProcess):
    def __init__(self, name, cwd, cmdline, should_run):
        super().__init__(name, should_run)
        self.cwd = cwd
        self.cmdline = cmdline

    def start(self):
        self.proc = subprocess.Popen(
            self.cmdline,
            cwd=self.cwd,
            env=os.environ.copy()
        )
```

**DaemonProcess**:
```python
class DaemonProcess(ManagerProcess):
    def __init__(self, name, module, param_name):
        super().__init__(name, always_run)
        self.module = module
        self.param_name = param_name  # PIDを保存するParamsキー

    def start(self):
        # デーモンプロセスを起動し、PIDをParamsに保存
        self.proc = subprocess.Popen(
            ["python", "-m", self.module],
            start_new_session=True  # 新しいセッションを作成
        )
        params.put(self.param_name, str(self.proc.pid))
```

### 5.2 プロセス定義の例

[system/manager/process_config.py](../../system/manager/process_config.py):

```python
procs = [
    # 制御系
    PythonProcess("controlsd", "selfdrive.controls.controlsd",
                 and_(not_joystick, iscar)),
    PythonProcess("plannerd", "selfdrive.controls.plannerd",
                 and_(not_joystick, only_onroad)),

    # 認識系
    PythonProcess("modeld", "selfdrive.modeld.modeld", only_onroad),
    NativeProcess("camerad", "system/camerad", ["./camerad"],
                 or_(driverview, only_onroad_not_sim), enabled=not WEBCAM),

    # システム系
    PythonProcess("pandad", "selfdrive.pandad.pandad", always_run),
    DaemonProcess("manage_athenad", "system.athena.manage_athenad", "AthenadPid"),
]

managed_processes = {p.name: p for p in procs}
```

### 5.3 起動条件関数の組み合わせ

```python
# AND条件
and_(not_joystick, iscar)
# → not_joystick かつ iscar

# OR条件
or_(driverview, only_onroad_not_sim)
# → driverview または only_onroad_not_sim

# 複雑な条件
and_(not_joystick, or_(only_onroad, driverview))
# → not_joystick かつ (only_onroad または driverview)
```

## 6. デバッグ・開発のヒント

### 6.1 環境変数での制御

**プロセスのブロック**:
```bash
export BLOCK="modeld,plannerd"
./launch_openpilot.sh
```
→ modeldとplannerdを起動しない

**Pandaなし起動**:
```bash
export NOBOARD=1
./launch_openpilot.sh
```
→ Pandaが接続されていない環境でテスト

**シミュレーションモード**:
```bash
export SIMULATION=1
./launch_openpilot.sh
```
→ 一部のハードウェア依存機能を無効化

### 6.2 ログの確認

```bash
# Managerのログ
tail -f /tmp/launch_log

# 個別プロセスのログ
journalctl -u comma -f

# cloudlogの出力先
ls -l /data/community/crashes/
```

### 6.3 プロセスの手動起動

```bash
# 個別プロセスを手動で起動してテスト
python -m selfdrive.controls.controlsd

# デバッグモード
export DEBUG=1
python -m selfdrive.modeld.modeld
```

### 6.4 プロセスの追加方法

1. **process_config.pyに追加**:
```python
PythonProcess("myprocess", "mymodule.myprocess", only_onroad)
```

2. **モジュールの実装**:
```python
# mymodule/myprocess.py
import cereal.messaging as messaging

def main():
    pm = messaging.PubMaster(['myTopic'])
    sm = messaging.SubMaster(['carState'])

    while True:
        sm.update()
        # 処理
        msg = messaging.new_message('myTopic')
        msg.myTopic.data = 42
        pm.send('myTopic', msg)

if __name__ == "__main__":
    main()
```

3. **メッセージ定義の追加** ([cereal/log.capnp](../../cereal/log.capnp)):
```capnp
struct MyTopic {
  data @0 :Int32;
}
```

## 7. まとめ

Managerはopenpilotの心臓部として以下を実現：

- **柔軟なプロセス管理**: 起動条件関数による動的制御
- **高速なシャットダウン**: 並列停止による時間短縮
- **状態の可視化**: managerStateによる監視
- **拡張性**: 新しいプロセスの追加が容易

この設計により、openpilotは複雑な自動運転システムを安定的に実行できます。

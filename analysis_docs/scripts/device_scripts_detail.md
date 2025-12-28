
# デバイス制御スクリプト詳細（初心者向け解説）

このドキュメントでは、openpilotの `scripts/` ディレクトリにある「デバイス制御」用スクリプトについて、初心者向けに技術的な背景も含めて丁寧に解説します。

---

## 背景・目的

openpilotは車載デバイス上で動作するため、電源管理やアップデート、車種ごとの起動設定など、ハードウェア制御が重要です。これらの操作を手作業で行うのは難しいため、初心者でも安全・確実に操作できるスクリプトを用意しています。

---


## disable-powersave.py

- **用途**: デバイスの省電力モード（スリープや自動停止）を無効化します。
- **技術解説**:
  - openpilotのHARDWARE APIを使い、`set_power_save(False)`を呼び出すことで、デバイスが勝手にスリープしないようにします。
  - 長時間のデバッグや実験時に便利です。
- **使い方**:
  ```bash
  python3 disable-powersave.py
  ```
- **注意点**:
  - 省電力を無効にするとバッテリー消費が増えるので、必要な時だけ使いましょう。
- **根拠**: [scripts/disable-powersave.py](../../scripts/disable-powersave.py#L1-L5)

### 【全行解説】disable-powersave.py
```python
#!/usr/bin/env python3
# Python3で実行することを指定
from openpilot.system.hardware import HARDWARE
# openpilotのハードウェア制御APIをインポート

if __name__ == "__main__":
  HARDWARE.set_power_save(False)
# スクリプトが直接実行された場合、省電力モードを無効化
```

---


## stop_updater.sh

- **用途**: openpilotのアップデート用プロセスを安全に停止し、未完了のアップデート情報を削除します。
- **技術解説**:
  - `pkill -2 -f system.updated.updated`でアップデートプロセスにSIGINT（割り込み）を送信します。
  - `/data/safe_staging/finalized/.overlay_consistent`を削除し、アップデートの一時ファイルを消します。
  - アップデートの不具合時や強制停止したい時に使います。
- **使い方**:
  ```bash
  ./stop_updater.sh
  ```
- **根拠**: [scripts/stop_updater.sh](../../scripts/stop_updater.sh#L1-L8)

### 【全行解説】stop_updater.sh
```bash
#!/usr/bin/env sh
# このスクリプトがshで実行されることを指定

# Stop updater
pkill -2 -f system.updated.updated
# system.updated.updatedプロセスにSIGINT（割り込み）を送信して停止

# Remove pending update
rm -f /data/safe_staging/finalized/.overlay_consistent
# アップデートの一時ファイルを削除
```

---


## update_now.sh

- **用途**: アップデート用プロセスにSIGHUP（ハングアップ信号）を送り、即時アップデートを促します。
- **技術解説**:
  - `pkill -1 -f system.updated`でsystem.updatedプロセスにSIGHUPを送信します。
  - 通常は自動で行われるアップデートを、手動で即時実行したい時に使います。
- **使い方**:
  ```bash
  ./update_now.sh
  ```
- **根拠**: [scripts/update_now.sh](../../scripts/update_now.sh#L1-L6)

### 【全行解説】update_now.sh
```bash
#!/usr/bin/env sh
# このスクリプトがshで実行されることを指定

# Send SIGHUP to updater
pkill -1 -f system.updated
# system.updatedプロセスにSIGHUP（ハングアップ信号）を送信
```

---


## launch_corolla.sh

- **用途**: トヨタCorolla用のopenpilotを起動します。
- **技術解説**:
  - スクリプト内で環境変数`FINGERPRINT`や`SKIP_FW_QUERY`を設定し、Corolla専用の設定で`launch_openpilot.sh`を呼び出します。
  - 車種ごとの違いを意識せず、簡単に起動できます。
- **使い方**:
  ```bash
  ./launch_corolla.sh
  ```
- **根拠**: [scripts/launch_corolla.sh](../../scripts/launch_corolla.sh#L1-L8)

### 【全行解説】launch_corolla.sh
```bash
#!/usr/bin/env bash
# このスクリプトがbashで実行されることを指定

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null && pwd)"
# スクリプト自身のディレクトリを取得

export FINGERPRINT="TOYOTA_COROLLA_TSS2"
# FINGERPRINT環境変数にCorolla用の値を設定
export SKIP_FW_QUERY="1"
# SKIP_FW_QUERY環境変数を1に設定（ファームウェアクエリをスキップ）
$DIR/../launch_openpilot.sh
# 上位ディレクトリのlaunch_openpilot.shを実行
```

---



# ユーティリティスクリプト詳細（初心者向け解説）

このドキュメントでは、openpilotの `scripts/` ディレクトリにある「ユーティリティ」用スクリプトについて、初心者向けに技術的な背景も含めて丁寧に解説します。

---

## 背景・目的

ユーティリティスクリプトは、日常的な作業やトラブルシュート、開発補助を簡単にするための便利ツールです。openpilotの開発や運用でよく使うコマンドやテスト、ネットワーク設定などを自動化します。

---

## reporter.py

- `onnx`パッケージでAIモデルファイルを読み込み、`model_checkpoint`メタデータを抽出します。
- masterブランチとPRブランチのモデルを比較し、違いをMarkdown表で出力します。
- モデルの更新や差分確認に便利です。

```bash
python3 reporter.py
```

### 【全行解説】reporter.py

```python
#!/usr/bin/env python3
# Python3で実行することを指定
import os
import glob
import onnx
# 必要な標準・外部モジュールをインポート

BASEDIR = os.path.abspath(os.path.join(os.path.dirname(os.path.realpath(__file__)), "../"))
# スクリプトのベースディレクトリを取得
MASTER_PATH = os.getenv("MASTER_PATH", BASEDIR)
# 環境変数MASTER_PATHがあればそれを、なければBASEDIRを使う
MODEL_PATH = "/selfdrive/modeld/models/"
# モデルファイルのパス


    model = onnx.load(f)
    metadata = {prop.key: prop.value for prop in model.metadata_props}
    return metadata['model_checkpoint'].split('/')[0]
# ONNXファイルからcheckpoint情報を取得

if __name__ == "__main__":
    print("| | master | PR branch |")
    print("|-| -----  | --------- |")
    for f in glob.glob(BASEDIR + MODEL_PATH + "/*.onnx"):
        # TODO: add checkpoint to DM
        if "dmonitoring" in f:
            continue
        fn = os.path.basename(f)
        master = get_checkpoint(MASTER_PATH + MODEL_PATH + fn)
        pr = get_checkpoint(BASEDIR + MODEL_PATH + fn)
        print(
            "|", fn, "|",
            f"[{master}](https://reporter.comma.life/experiment/{master})", "|",
            f"[{pr}](https://reporter.comma.life/experiment/{pr})", "|"
        )
# masterとPRのモデルのcheckpointを比較し、Markdown表で出力
```

---

## retry.sh

- 任意のコマンドを最大3回まで実行し、失敗時は5秒待って再試行します。
- 3回失敗した場合はエラー終了します。
- ネットワーク不安定時や一時的な失敗に強い自動化スクリプトです。

```bash
./retry.sh <コマンド>
```

### 【全行解説】retry.sh

```bash
#!/usr/bin/env bash
# このスクリプトがbashで実行されることを指定

function fail {
    echo $1 >&2
    exit 1
}
# エラー時のメッセージ出力と終了

function retry {
    local n=1
    local max=3 # 3 retries before failure
    local delay=5 # delay between retries, 5 seconds
    while true; do
        echo "Running command '$@' with retry, attempt $n/$max"
        "$@" && break || {
            if [[ $n -lt $max ]]; then
                ((n++))
                sleep $delay;
            else
                fail "The command has failed after $n attempts."
            fi
        }
    done
}
# 指定コマンドを最大3回までリトライ

if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
        retry "$@"
fi
# スクリプトが直接実行された場合、retry関数でコマンドを実行
```

---

## usb.sh

- `nmcli`はLinuxのネットワーク管理ツールです。
- `nmcli connection modify --temporary esim ipv4.route-metric 1 ipv6.route-metric 1`でルート優先度を設定し、`nmcli con up esim`で接続を有効化します。
- USB経由のネットワークやesimの設定に便利です。

```bash
./usb.sh
```

### 【全行解説】usb.sh

```bash
#!/usr/bin/env bash
# このスクリプトがbashで実行されることを指定

nmcli connection modify --temporary esim ipv4.route-metric 1 ipv6.route-metric 1
# esim接続のIPv4/IPv6ルートメトリックを一時的に1に設定
nmcli con up esim
# esim接続を有効化
```

---

## waste.c

- C言語で書かれており、各CPUコアにプロセスを割り当て、128MBのメモリを確保します。
- ARM NEON命令でベクトル演算を繰り返し、MEM定義時はmemsetでRAMも負荷をかけます。
- ハードウェアのストレステストやベンチマークに使います。

```bash
gcc -O2 waste.c -lpthread -o waste
./waste
```

### 【全行解説】waste.c

```c
// gcc -O2 waste.c -lpthread -owaste
// gcc -O2 waste.c -lpthread -owaste -DMEM
// ビルド方法の例

#define _GNU_SOURCE
#include <stdio.h>
#include <math.h>
#include <sched.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <arm_neon.h>
#include <sys/sysinfo.h>
#include "../common/timing.h"
// 必要なヘッダをインクルード

int get_nprocs(void);
double *ttime, *oout;
# コア数取得用関数とグローバル変数

void waste(int pid) {
    cpu_set_t my_set;
    CPU_ZERO(&my_set);
    CPU_SET(pid, &my_set);
    int ret = sched_setaffinity(0, sizeof(cpu_set_t), &my_set);
    printf("set affinity to %d: %d\n", pid, ret);
    // 指定コアにプロセスを割り当て

    // 128 MB
    float32x4_t *tmp = (float32x4_t *)malloc(0x800000*sizeof(float32x4_t));
    // 128MB分のメモリ確保

    // comment out the memset for CPU only and not RAM
    // otherwise we need this to avoid the zero page
#ifdef MEM
    memset(tmp, 0xaa, 0x800000*sizeof(float32x4_t));
#endif
    // MEM定義時はRAMも初期化

    float32x4_t out;

    double sec = seconds_since_boot();
    while (1) {
        for (int i = 0; i < 0x10; i++) {
            for (int j = 0; j < 0x800000; j+=0x20) {
                out = vmlaq_f32(out, tmp[j+0], tmp[j+1]);
                out = vmlaq_f32(out, tmp[j+2], tmp[j+3]);
                out = vmlaq_f32(out, tmp[j+4], tmp[j+5]);
                out = vmlaq_f32(out, tmp[j+6], tmp[j+7]);
                out = vmlaq_f32(out, tmp[j+8], tmp[j+9]);
                out = vmlaq_f32(out, tmp[j+10], tmp[j+11]);
                out = vmlaq_f32(out, tmp[j+12], tmp[j+13]);
                out = vmlaq_f32(out, tmp[j+14], tmp[j+15]);
                out = vmlaq_f32(out, tmp[j+16], tmp[j+17]);
                out = vmlaq_f32(out, tmp[j+18], tmp[j+19]);
                out = vmlaq_f32(out, tmp[j+20], tmp[j+21]);
                out = vmlaq_f32(out, tmp[j+22], tmp[j+23]);
                out = vmlaq_f32(out, tmp[j+24], tmp[j+25]);
                out = vmlaq_f32(out, tmp[j+26], tmp[j+27]);
                out = vmlaq_f32(out, tmp[j+28], tmp[j+29]);
                out = vmlaq_f32(out, tmp[j+30], tmp[j+31]);
            }
        }
        double nsec = seconds_since_boot();
        ttime[pid] = nsec-sec;
        oout[pid] = out[0] + out[1] + out[2] + out[3];
        sec = nsec;
    }
}

int main() {
    int CORES = get_nprocs();
    ttime = (double *)malloc(CORES*sizeof(double));
    oout = (double *)malloc(CORES*sizeof(double));

    pthread_t waster[CORES];
    for (long i = 0; i < CORES; i++) {
        ttime[i] = NAN;
        pthread_create(&waster[i], NULL, (void *(*)(void *))waste, (void*)i);
    }
    while (1) {
        double avg = 0.0;
        double iavg = 0.0;
        for (int i = 0; i < CORES; i++) {
            avg += ttime[i];
            iavg += 1/ttime[i];
            printf("%4.2f ", ttime[i]);
        }
        double mb_per_sec = (16.*0x800000/(1024*1024))*sizeof(float32x4_t)*iavg;
        printf("-- %4.2f -- %.2f MB/s \n", avg/CORES, mb_per_sec);
        sleep(1);
    }
}
# 各コアごとの処理時間やメモリ帯域を1秒ごとに表示
```

---

## waste.py

- 各CPUコアごとにプロセスを生成し、numpyで行列積を繰り返し計算します。
- setproctitleでプロセス名を表示し、進捗や計算時間を標準出力します。
- Python環境で手軽にCPU負荷テストができます。

```bash
python3 waste.py
```

### 【全行解説】waste.py

```python
#!/usr/bin/env python3
# Python3で実行することを指定
import os
import time
import numpy as np
from multiprocessing import Process
from setproctitle import setproctitle
# 必要な標準・外部モジュールをインポート

def waste(core):
    os.sched_setaffinity(0, [core,])
    # プロセスを指定コアに割り当て
    m1 = np.zeros((200, 200)) + 0.8
    m2 = np.zeros((200, 200)) + 1.2
    # 行列を初期化
    i = 1
    st = time.monotonic()
    j = 0
    while 1:
        if (i % 100) == 0:
            setproctitle(f"{core:3d}: {i:8d}")
            lt = time.monotonic()
            print(f"{core:3d}: {i:8d} {lt-st:f}  {j:.2f}")
            st = lt
        i += 1
        j = np.sum(np.matmul(m1, m2))
    # 100回ごとに進捗を表示しつつ、行列積を計算し続ける


    print("1-2 seconds is baseline")
    for i in range(os.cpu_count()):
        p = Process(target=waste, args=(i,))
        p.start()
# CPUコア数分だけプロセスを立ち上げてwaste関数を実行

if __name__ == "__main__":
    main()
# スクリプトが直接実行された場合、main()を呼ぶ
```

---

## cell.sh

- XDG_CACHE_HOMEを/data/tinycacheに設定し、tinygrad_repo/examplesディレクトリで`beautiful_cartpole.py`を無限ループ実行します。
- tinygradの動作確認やGPUの連続テストに使います。

```bash
./cell.sh
```

### 【全行解説】cell.sh

```bash
#!/usr/bin/env bash
# このスクリプトがbashで実行されることを指定

# testing the GPU box

export XDG_CACHE_HOME=/data/tinycache
# tinygradのキャッシュディレクトリを指定
mkdir -p $XDG_CACHE_HOME
# キャッシュディレクトリを作成

cd /data/openpilot/tinygrad_repo/examples
# tinygradのexamplesディレクトリに移動
while true; do
  AMD=1 AMD_IFACE=usb python ./beautiful_cartpole.py
  sleep 1
done
# beautiful_cartpole.pyを無限ループで実行し、1秒待機を繰り返す
```

---


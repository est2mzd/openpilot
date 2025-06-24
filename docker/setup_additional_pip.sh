#!/bin/bash
set -e

# metadrive の _cuda_enableを Trueにするのに必要

# image_on_cuda=True

python -m ensurepip --upgrade
which pip3

pip3 install cupy-cuda12x # OK. https://qiita.com/maueki/items/c78f04c4f3742c36bcb0
pip3 install PyOpenGL PyOpenGL_accelerate #OK . https://qiita.com/ousttrue/items/cf8cfbbfadf686b6c338
pip3 install nvidia-cuda-runtime-cu12
pip3 install cuda-python

#==========================================
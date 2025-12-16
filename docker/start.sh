#!/bin/bash

source ./common.sh

xhost +local:docker

docker run \
    --gpus all \
    --net=host \
    --ipc=host \
    --env DISPLAY=$DISPLAY \
    --env DEVICE=CUDA \
    --env LD_LIBRARY_PATH=/usr/local/cuda/lib64 \
    -e NVIDIA_VISIBLE_DEVICES=all \
    -e NVIDIA_DRIVER_CAPABILITIES=all \
    -itd \
    --shm-size=8G \
    -v /tmp/.X11-unix:/tmp/.X11-unix \
    -v "${GRANDPARENT_DIR}/openpilot":/home/${USER_NAME}/openpilot \
    -v "${GRANDPARENT_DIR}/others":/home/${USER_NAME}/others \
    -v "${GRANDPARENT_DIR}/data":/home/${USER_NAME}/data \
    --cap-add=NET_ADMIN \
    --device /dev/net/tun \
    --privileged \
    --name $CONTAINER_NAME \
    $IMAGE_NAME
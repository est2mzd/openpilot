#!/bin/bash

IMAGE_NAME=openpilot:202505

# コンテナ名
CONTAINER_NAME=openpilot_${HOSTNAME}

# フルパスの取得
CURRENT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PARENT_DIR="$(dirname "$CURRENT_DIR")"
GRANDPARENT_DIR="$(dirname "$PARENT_DIR")"

# ホストのユーザー情報
USER_NAME=$(whoami)
USER_UID=$(id -u) #USER_ID=1001
USER_GID=$(id -g)

# コンテナの作業ディレクトリ
WORK_DIR=${PARENT_DIR}

echo "---------- common.sh ------------"
echo "Image Name     = ${IMAGE_NAME}"
echo "Container Name = ${CONTAINER_NAME}"
echo "WORK_DIR       = ${WORK_DIR}"
echo "USER_NAME  = ${USER_NAME}"
echo "USER_UID   = ${USER_UID}"
echo "USER_GID   = ${USER_GID}"
echo "---------------------------------"


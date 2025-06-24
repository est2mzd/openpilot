#!/bin/bash

source ./common.sh

DOCKERFILEPATH="${CURRENT_DIR}/Dockerfile.openpilot_base_2"

echo "DOCKERFILEPATH = ${DOCKERFILEPATH}"

docker build \
    -f ${DOCKERFILEPATH} \
    --build-arg USER_NAME=${USER_NAME} \
    --build-arg USER_ID=${USER_ID} \
    --build-arg GROUP_ID=${USER_GID} \
    -t $IMAGE_NAME .

# --no-cache \


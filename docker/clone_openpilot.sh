
source ./common.sh
set -e
cd ${PARENT_DIR}/
git clone https://github.com/est2mzd/openpilot.git
git config --global --add safe.directory /home/takuya/work/comma/openpilot
cd ${PARENT_DIR}/openpilot
git submodule update --init --recursive
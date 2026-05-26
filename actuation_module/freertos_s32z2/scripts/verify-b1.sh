#!/usr/bin/env bash
# Run on the development host.
# Build, flash, and validate the B-1 bring-up.
set -euo pipefail

: "${S32_RTD_PATH:?Set S32_RTD_PATH to the extracted NXP RTD root}"
: "${WEST_FLASH_RUNNER:?Set WEST_FLASH_RUNNER (e.g. pyocd, openocd)}"

REPO_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)
cd "${REPO_ROOT}"

rm -rf build-s32z2
cmake -S actuation_module/freertos_s32z2 -B build-s32z2 \
    -DCMAKE_TOOLCHAIN_FILE=actuation_module/freertos_s32z2/cmake/arm-cortex-r52.cmake
cmake --build build-s32z2 -j

west flash --runner "${WEST_FLASH_RUNNER}" \
    --elf-file build-s32z2/actuation_freertos_s32z2.elf

LOG=/tmp/freertos-s32z2-b1-uart.log
rm -f "${LOG}"
timeout 10s tio -b 115200 /dev/ttyACM0 2>/dev/null | tee "${LOG}" || true

grep -F "FreeRTOS on S32Z2 starting" "${LOG}"
count=$(grep -cF "actuation alive ticks=" "${LOG}")
if [ "${count}" -lt 5 ]; then
    echo "B-1 verification FAILED: only ${count} heartbeats (need >= 5)"
    exit 1
fi
echo "B-1 verification OK (${count} heartbeats)"

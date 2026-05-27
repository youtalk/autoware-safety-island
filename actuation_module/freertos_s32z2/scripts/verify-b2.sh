#!/usr/bin/env bash
# Full B-2 verification: build, flash, start edge ECU peer, run DDS round-trip.
set -euo pipefail

: "${S32_RTD_PATH:?Set S32_RTD_PATH}"
: "${WEST_FLASH_RUNNER:?Set WEST_FLASH_RUNNER}"

REPO_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)
cd "${REPO_ROOT}"

# Reuse verify-b1.sh's build/flash up to and including the heartbeat check
# is overkill here; rebuild and reflash directly.
rm -rf build-s32z2
cmake -S actuation_module/freertos_s32z2 -B build-s32z2 \
    -DCMAKE_TOOLCHAIN_FILE=actuation_module/freertos_s32z2/cmake/arm-cortex-r52.cmake
cmake --build build-s32z2 -j

./actuation_module/freertos_s32z2/scripts/build-edge-ecu-peer.sh

west flash --runner "${WEST_FLASH_RUNNER}" \
    --elf-file build-s32z2/actuation_freertos_s32z2.elf

CYCLONEDDS_XML=demo/cyclonedds.xml
[ -f demo/cyclonedds-s32z2.xml ] && CYCLONEDDS_XML=demo/cyclonedds-s32z2.xml
export CYCLONEDDS_URI=file://$(pwd)/${CYCLONEDDS_XML}

EDGE_PUB_LOG=/tmp/freertos-s32z2-b2-edge-pub.log
EDGE_SUB_LOG=/tmp/freertos-s32z2-b2-edge-sub.log
UART_LOG=/tmp/freertos-s32z2-b2-uart.log
rm -f "${EDGE_PUB_LOG}" "${EDGE_SUB_LOG}" "${UART_LOG}"

./build-edge-ecu-peer/edge_ecu_pub > "${EDGE_PUB_LOG}" 2>&1 &
PUB_PID=$!
trap 'kill $PUB_PID 2>/dev/null || true' EXIT

( timeout 30s tio -b 115200 /dev/ttyACM0 2>/dev/null | tee "${UART_LOG}" || true ) &
UART_PID=$!
trap 'kill $PUB_PID $UART_PID 2>/dev/null || true' EXIT

timeout 30s ./build-edge-ecu-peer/edge_ecu_sub > "${EDGE_SUB_LOG}" 2>&1 || true
wait $UART_PID 2>/dev/null || true

errors=0
grep -F "Controller Node Started"         "${UART_LOG}" || errors=$((errors+1))
grep -F "Actuation Safety Island is Live"  "${UART_LOG}" || errors=$((errors+1))
count=$(grep -cF "STEERING REPORT" "${EDGE_SUB_LOG}")
if [ "${count}" -lt 2 ]; then
    echo "STEERING REPORT count is ${count} (need >= 2)"
    errors=$((errors+1))
fi
if grep -qF "actuation_main returned" "${UART_LOG}"; then
    echo "actuation_main returned — controller loop died during verification"
    errors=$((errors+1))
fi

if [ "${errors}" -eq 0 ]; then
    echo "B-2 verification OK"
    exit 0
else
    echo "B-2 verification FAILED (${errors} check failures)"
    exit 1
fi

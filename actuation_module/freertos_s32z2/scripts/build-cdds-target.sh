#!/usr/bin/env bash
# Cross-build CycloneDDS as a static library for the S32Z2 (Cortex-R52).
# Run on the development host.
set -euo pipefail

REPO_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)
cd "${REPO_ROOT}"

# The host idlc from the FreeRTOS POSIX simulator pipeline is reused.
if [ ! -x build-freertos/cdds_host_out/bin/idlc ]; then
    echo "Host idlc not found at build-freertos/cdds_host_out/bin/idlc."
    echo "Build it first via the FreeRTOS POSIX simulator's Phase 1 cmake (see"
    echo "actuation_module/freertos/CMakeLists.txt header comments)."
    exit 1
fi

rm -rf build-s32z2/cdds_target
cmake -S cyclonedds -B build-s32z2/cdds_target \
    -DCMAKE_TOOLCHAIN_FILE="${REPO_ROOT}/actuation_module/freertos_s32z2/cmake/arm-cortex-r52.cmake" \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_IDLC=OFF \
    -DBUILD_DDSPERF=OFF \
    -DBUILD_EXAMPLES=OFF \
    -DBUILD_TESTING=OFF \
    -DENABLE_SECURITY=OFF \
    -DENABLE_SSL=OFF \
    -DENABLE_SHM=OFF \
    -DENABLE_IPV6=OFF \
    -DENABLE_NETWORK_PARTITIONS=OFF \
    -DCMAKE_INSTALL_PREFIX="${REPO_ROOT}/build-s32z2/cdds_target_out" \
    -DCMAKE_BUILD_TYPE=Release

cmake --build build-s32z2/cdds_target --target install -j
test -f build-s32z2/cdds_target_out/lib/libddsc.a
echo "CycloneDDS target library built: build-s32z2/cdds_target_out/lib/libddsc.a"

# Copyright (c) 2026, Arm Limited and contributors.
# SPDX-License-Identifier: Apache-2.0
#
# FreeRTOS on NXP S32Z2 (Cortex-R52) build entry point.
# Build with:
#   S32_RTD_PATH=/opt/nxp/<rtd-version> \
#     cmake -S actuation_module/freertos_s32z2 -B build-s32z2 \
#       -DCMAKE_TOOLCHAIN_FILE=actuation_module/freertos_s32z2/cmake/arm-cortex-r52.cmake
#   cmake --build build-s32z2 -j

cmake_minimum_required(VERSION 3.22)
project(actuation_module_freertos_s32z2 LANGUAGES C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- NXP RTD location ----
if(NOT DEFINED ENV{S32_RTD_PATH})
    message(FATAL_ERROR
        "S32_RTD_PATH is not set. Point it to the extracted NXP S32 RTD root, "
        "e.g. S32_RTD_PATH=/opt/nxp/RTD_4.0.x . The RTD is downloaded manually "
        "from the NXP Software Licensing portal (see actuation_module/freertos_s32z2/README.md).")
endif()
set(S32_RTD_PATH $ENV{S32_RTD_PATH})

if(NOT IS_DIRECTORY "${S32_RTD_PATH}")
    message(FATAL_ERROR "S32_RTD_PATH=${S32_RTD_PATH} is not a directory.")
endif()

# Locate the FreeRTOS Cortex-R52 port shipped in the RTD. The exact subdirectory
# depends on the RTD version; engineer must inspect the archive (Task 0 Step 2)
# and adjust the PATHS line if the layout differs.
find_path(S32Z2_FREERTOS_PORT_DIR
    NAMES portmacro.h port.c
    PATHS ${S32_RTD_PATH}/FreeRTOS/portable/GCC/ARM_Cortex_R52
          ${S32_RTD_PATH}/freertos/portable/GCC/ARM_Cortex_R52
    DOC "NXP RTD-provided FreeRTOS Cortex-R52 port directory"
    REQUIRED NO_DEFAULT_PATH)

find_path(S32Z2_FREERTOS_KERNEL_DIR
    NAMES FreeRTOS.h
    PATHS ${S32_RTD_PATH}/FreeRTOS/include
          ${S32_RTD_PATH}/freertos/include
    DOC "NXP RTD-provided FreeRTOS kernel include directory"
    REQUIRED NO_DEFAULT_PATH)

message(STATUS "S32Z2 FreeRTOS port directory: ${S32Z2_FREERTOS_PORT_DIR}")
message(STATUS "S32Z2 FreeRTOS kernel headers: ${S32Z2_FREERTOS_KERNEL_DIR}")

# ---- Platform definitions ----
add_compile_definitions(PLATFORM_FREERTOS=1 PLATFORM_FREERTOS_S32Z2=1)

# ---- Application executable (skeleton; expands in later tasks) ----
add_executable(actuation_freertos_s32z2
    freertos_main.cpp
    board_init.c
    startup.S
)

target_link_options(actuation_freertos_s32z2 PRIVATE
    -T${CMAKE_CURRENT_SOURCE_DIR}/linker.ld
)

# Pull in RTD driver objects (Mcu, Port, Platform). The engineer must add
# explicit paths once the RTD layout is known. Reference template:
# file(GLOB _rtd_drivers
#     ${S32_RTD_PATH}/MCAL/Mcu_TS_T40D34M40I0R0/src/Mcu*.c
#     ${S32_RTD_PATH}/MCAL/Port_TS_T40D34M40I0R0/src/Port*.c
#     ${S32_RTD_PATH}/MCAL/Platform_TS_T40D34M40I0R0/src/Platform*.c)
# target_sources(actuation_freertos_s32z2 PRIVATE ${_rtd_drivers})

target_include_directories(actuation_freertos_s32z2 PRIVATE
    ${CMAKE_SOURCE_DIR}/../include
    ${CMAKE_SOURCE_DIR}/../include/platform/freertos/s32z2
    ${S32Z2_FREERTOS_KERNEL_DIR}
    ${S32Z2_FREERTOS_PORT_DIR}
)

set_target_properties(actuation_freertos_s32z2 PROPERTIES SUFFIX ".elf")

file(GLOB _freertos_core
    ${S32Z2_FREERTOS_KERNEL_DIR}/../tasks.c
    ${S32Z2_FREERTOS_KERNEL_DIR}/../queue.c
    ${S32Z2_FREERTOS_KERNEL_DIR}/../list.c
    ${S32Z2_FREERTOS_KERNEL_DIR}/../timers.c
    ${S32Z2_FREERTOS_KERNEL_DIR}/../event_groups.c
    ${S32Z2_FREERTOS_KERNEL_DIR}/../stream_buffer.c
    ${S32Z2_FREERTOS_KERNEL_DIR}/../portable/MemMang/heap_4.c
)
target_sources(actuation_freertos_s32z2 PRIVATE
    ${_freertos_core}
    ${S32Z2_FREERTOS_PORT_DIR}/port.c
    ${S32Z2_FREERTOS_PORT_DIR}/portASM.S
)
target_include_directories(actuation_freertos_s32z2 PRIVATE
    ${S32Z2_FREERTOS_PORT_DIR}
)

# ---- lwIP (from NXP RTD) ----
find_path(LWIP_DIR
    NAMES src/api/sockets.c
    PATHS ${S32_RTD_PATH}/third_party/lwip
          ${S32_RTD_PATH}/middleware/lwip
    DOC "NXP RTD-provided lwIP source root"
    REQUIRED NO_DEFAULT_PATH)
message(STATUS "lwIP root: ${LWIP_DIR}")

file(GLOB _lwip_core
    ${LWIP_DIR}/src/api/*.c
    ${LWIP_DIR}/src/core/*.c
    ${LWIP_DIR}/src/core/ipv4/*.c
    ${LWIP_DIR}/src/netif/*.c
)
target_sources(actuation_freertos_s32z2 PRIVATE ${_lwip_core})
target_include_directories(actuation_freertos_s32z2 PRIVATE
    ${LWIP_DIR}/src/include
)

# ---- NETC driver (from RTD MCAL) ----
file(GLOB _netc_sources
    ${S32_RTD_PATH}/MCAL/Eth_43_NETC_TS_*/src/*.c
    ${S32_RTD_PATH}/MCAL/Eth_43_NETC_TS_*/src/*/*.c
)
target_sources(actuation_freertos_s32z2 PRIVATE ${_netc_sources})

target_sources(actuation_freertos_s32z2 PRIVATE
    lwip_bringup.c
)

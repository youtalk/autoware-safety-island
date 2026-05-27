# Copyright (c) 2026, Arm Limited and contributors.
# SPDX-License-Identifier: Apache-2.0
#
# FreeRTOS on NXP S32Z2 (Cortex-R52) build entry point.
#
# Three NXP packages from the Flexnet portal must be extracted and exported as
# environment variables before invoking cmake:
#
#   export S32_RTD_PATH=~/nxp/RTD_S32ZE_2.0.1/_jar_family/S32DS/software/PlatformSDK_S32ZE
#   export FREERTOS_PATH=~/nxp/FreeRTOS_S32ZE_4.0.0/_jar/S32DS/software/PlatformSDK_S32ZE/FreeRTOS
#   export LWIP_PATH=~/nxp/TCPIP_S32ZE_3.0.0/_jar/S32DS/software/PlatformSDK_S32ZE/stacks/tcpip
#
# Then:
#   cmake -S actuation_module/freertos_s32z2 -B build-s32z2 \
#         -DCMAKE_TOOLCHAIN_FILE=actuation_module/freertos_s32z2/cmake/arm-cortex-r52.cmake
#   cmake --build build-s32z2 -j

cmake_minimum_required(VERSION 3.22)
project(actuation_module_freertos_s32z2 LANGUAGES C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Mirror the POSIX FreeRTOS build's anchor variable so actuation_core.cmake
# and the autoware component .cmake files resolve their relative paths.
set(ACTUATION_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(REPO_ROOT ${ACTUATION_DIR}/..)

# ---- NXP package locations (three roots) ----
foreach(_var IN ITEMS S32_RTD_PATH FREERTOS_PATH LWIP_PATH)
    if(NOT DEFINED ENV{${_var}})
        message(FATAL_ERROR
            "${_var} is not set. Point it to the extracted NXP package root. "
            "See actuation_module/freertos_s32z2/README.md for the layout.")
    endif()
    set(${_var} $ENV{${_var}})
    if(NOT IS_DIRECTORY "${${_var}}")
        message(FATAL_ERROR "${_var}=${${_var}} is not a directory.")
    endif()
endforeach()

# ---- RTD Config-Tools-generated PB configurations ----
# These come from S32 Config Tools (S32DS) and live OUTSIDE this repository
# because they carry the NXP Confidential and Proprietary licence header.
# Point S32CT_GENERATED_DIR at an S32CT project root that has the layout:
#   $S32CT_GENERATED_DIR/
#     board/Siul2_Port_Ip_Cfg.{c,h}
#     generate/include/*.h
#     generate/src/*.c
# Typical value: ~/workspaceS32DS.3.6.2/lwip_S32Z27X_FreeRTOS_R52
if(NOT DEFINED ENV{S32CT_GENERATED_DIR})
    message(FATAL_ERROR
        "S32CT_GENERATED_DIR is not set. Generate the RTD PB configurations "
        "in S32 Design Studio's S32 Configuration Tools (Update Code on a "
        ".mex import of $LWIP_PATH/examples/.../lwip_S32Z27X_FreeRTOS_R52) "
        "then point S32CT_GENERATED_DIR at that project root. See "
        "actuation_module/freertos_s32z2/README.md.")
endif()
set(S32CT_GENERATED_DIR $ENV{S32CT_GENERATED_DIR})
if(NOT IS_DIRECTORY "${S32CT_GENERATED_DIR}/generate/src")
    message(FATAL_ERROR
        "S32CT_GENERATED_DIR=${S32CT_GENERATED_DIR} does not contain a "
        "generate/src subdirectory (expected output of S32CT Update Code).")
endif()
message(STATUS "S32CT generated configs: ${S32CT_GENERATED_DIR}")

# Reference FreeRTOS example (provides startup, linker, and demo board config).
set(S32Z2_FREERTOS_EXAMPLE
    ${FREERTOS_PATH}/examples/S32DS/FreeRTOS_Toggle_Led_Example_S32Z2XX_R52)

# Locate the FreeRTOS Cortex-R52 port (ARM_CR52_GIC, self-contained in port.c).
find_path(S32Z2_FREERTOS_PORT_DIR
    NAMES portmacro.h port.c
    PATHS ${FREERTOS_PATH}/Source/portable/GCC/ARM_CR52_GIC
    DOC "NXP FreeRTOS Cortex-R52 port directory"
    REQUIRED NO_DEFAULT_PATH)

find_path(S32Z2_FREERTOS_KERNEL_DIR
    NAMES FreeRTOS.h
    PATHS ${FREERTOS_PATH}/Source/include
    DOC "NXP FreeRTOS kernel include directory"
    REQUIRED NO_DEFAULT_PATH)

set(S32Z2_FREERTOS_SOURCE_DIR ${FREERTOS_PATH}/Source)

message(STATUS "S32Z2 FreeRTOS port directory: ${S32Z2_FREERTOS_PORT_DIR}")
message(STATUS "S32Z2 FreeRTOS kernel headers: ${S32Z2_FREERTOS_KERNEL_DIR}")
message(STATUS "S32Z2 FreeRTOS example:        ${S32Z2_FREERTOS_EXAMPLE}")

# ---- Configurable values ----
# Mirror the POSIX FreeRTOS build's Zephyr-Kconfig-equivalent knobs so the
# shared platform_config.h works unchanged.
set(CONFIG_LOG_LEVEL 1 CACHE STRING "Log level (0=off, 1=info, 2=debug)")
set(CONFIG_LOG_THROTTLE_RATE 3 CACHE STRING "Log throttle rate in seconds")
set(CONFIG_DDS_DOMAIN_ID 2 CACHE STRING "DDS domain ID")
set(CONFIG_DDS_NETWORK_INTERFACE "" CACHE STRING "DDS network interface name (empty=auto)")
set(CONFIG_DDS_LOG_LEVEL 0 CACHE STRING "DDS log level (0=off, 1=warn, 2=debug)")
set(CONFIG_THREAD_STACK_SIZE 262144 CACHE STRING "Thread stack size in bytes")
set(CONFIG_NET_DHCPV4_INITIAL_DELAY_MAX 0 CACHE STRING "DHCP delay")
set(CONFIG_ENABLE_SNTP OFF CACHE BOOL "Enable SNTP time sync")

set(CONFIG_CONTROL_CMD_OUTPUT_MODE "DDS_ONLY" CACHE STRING "Control command output mode: DDS_ONLY, CAN_ONLY, DDS_AND_CAN")
set_property(CACHE CONFIG_CONTROL_CMD_OUTPUT_MODE PROPERTY STRINGS "DDS_ONLY" "CAN_ONLY" "DDS_AND_CAN")

set(CONFIG_CONTROL_CMD_OUTPUT_DDS_ONLY 0)
set(CONFIG_CONTROL_CMD_OUTPUT_CAN_ONLY 0)
set(CONFIG_CONTROL_CMD_OUTPUT_DDS_AND_CAN 0)
set(CONFIG_CONTROL_CMD_CAN_OUTPUT 0)
set(CONFIG_CONTROL_CMD_DDS_OUTPUT 0)

if(CONFIG_CONTROL_CMD_OUTPUT_MODE STREQUAL "DDS_ONLY")
    set(CONFIG_CONTROL_CMD_OUTPUT_DDS_ONLY 1)
    set(CONFIG_CONTROL_CMD_DDS_OUTPUT 1)
elseif(CONFIG_CONTROL_CMD_OUTPUT_MODE STREQUAL "CAN_ONLY")
    set(CONFIG_CONTROL_CMD_OUTPUT_CAN_ONLY 1)
    set(CONFIG_CONTROL_CMD_CAN_OUTPUT 1)
elseif(CONFIG_CONTROL_CMD_OUTPUT_MODE STREQUAL "DDS_AND_CAN")
    set(CONFIG_CONTROL_CMD_OUTPUT_DDS_AND_CAN 1)
    set(CONFIG_CONTROL_CMD_CAN_OUTPUT 1)
    set(CONFIG_CONTROL_CMD_DDS_OUTPUT 1)
else()
    message(FATAL_ERROR "Unsupported CONFIG_CONTROL_CMD_OUTPUT_MODE=${CONFIG_CONTROL_CMD_OUTPUT_MODE}. Use DDS_ONLY, CAN_ONLY, or DDS_AND_CAN.")
endif()

configure_file(
    ${ACTUATION_DIR}/include/platform/freertos/freertos_config_generated.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/freertos_config_generated.h
    @ONLY
)

# ---- Platform definitions ----
add_compile_definitions(PLATFORM_FREERTOS=1 PLATFORM_FREERTOS_S32Z2=1)

# ---- Application executable ----
# startup.s / linker_ram_r52.ld are NXP Confidential and Proprietary; they are
# referenced in-place from the extracted FreeRTOS example and never copied
# into this repository.
set(_s32z2_startup_asm
    ${S32Z2_FREERTOS_EXAMPLE}/Project_Settings/Startup_Code/startup.s
    ${S32Z2_FREERTOS_EXAMPLE}/Project_Settings/Startup_Code/Vector_Table.s
    ${S32Z2_FREERTOS_EXAMPLE}/Project_Settings/Startup_Code/core_utility.s
    ${S32Z2_FREERTOS_EXAMPLE}/Project_Settings/Startup_Code/exceptions_stack.s
)
# The NXP startup .s files use C preprocessor macros (#ifdef __ghs__ etc.)
# to switch toolchains and define THUMB_CODE. GNU `as` does not preprocess
# lowercase .s files by default; force cpp preprocessing so the GCC branch
# of the conditionals is taken.
set_source_files_properties(${_s32z2_startup_asm} PROPERTIES
    COMPILE_OPTIONS "-x;assembler-with-cpp"
)

add_executable(actuation_freertos_s32z2
    freertos_main.cpp
    board_init.c
    ${_s32z2_startup_asm}
    ${S32Z2_FREERTOS_EXAMPLE}/Project_Settings/Startup_Code/core.c
    ${S32Z2_FREERTOS_EXAMPLE}/Project_Settings/Startup_Code/exceptions.c
    ${S32Z2_FREERTOS_EXAMPLE}/Project_Settings/Startup_Code/gic500.c
    ${S32Z2_FREERTOS_EXAMPLE}/Project_Settings/Startup_Code/startup_init.c
    ${S32Z2_FREERTOS_EXAMPLE}/Project_Settings/Startup_Code/system.c
)

target_link_options(actuation_freertos_s32z2 PRIVATE
    -T${S32Z2_FREERTOS_EXAMPLE}/Project_Settings/Linker_Files/linker_ram_r52.ld
)

target_include_directories(actuation_freertos_s32z2 PRIVATE
    ${CMAKE_SOURCE_DIR}/../include
    ${CMAKE_SOURCE_DIR}/../include/platform/freertos/s32z2
    ${CMAKE_CURRENT_BINARY_DIR}/generated
    # pbcfg_shims/ is searched BEFORE generate/include so our sentinel-only
    # versions of Mcu_VS_0_PBcfg.h / Power_Ip_VS_0_PBcfg.h /
    # Ram_Ip_VS_0_PBcfg.h override the empty stubs S32CT left behind.
    ${CMAKE_SOURCE_DIR}/pbcfg_shims
    ${S32Z2_FREERTOS_KERNEL_DIR}
    ${S32Z2_FREERTOS_PORT_DIR}
    ${S32Z2_FREERTOS_EXAMPLE}/include
    ${S32Z2_FREERTOS_EXAMPLE}/RTD/include
    ${S32CT_GENERATED_DIR}/generate/include
    ${S32CT_GENERATED_DIR}/board
    ${S32_RTD_PATH}/RTD/BaseNXP_TS_T31D53M20I1R0/header
    ${S32_RTD_PATH}/RTD/BaseNXP_TS_T31D53M20I1R0/include
    ${S32_RTD_PATH}/RTD/Mcl_TS_T31D53M20I1R0/include
    ${S32_RTD_PATH}/RTD/Mcu_TS_T31D53M20I1R0/include
    ${S32_RTD_PATH}/RTD/Port_TS_T31D53M20I1R0/include
    ${S32_RTD_PATH}/RTD/Platform_TS_T31D53M20I1R0/include
    ${S32_RTD_PATH}/RTD/Platform_TS_T31D53M20I1R0/startup/include
    ${S32_RTD_PATH}/RTD/Uart_TS_T31D53M20I1R0/include
    ${S32_RTD_PATH}/RTD/Gpt_TS_T31D53M20I1R0/include
    ${S32_RTD_PATH}/RTD/Eth_43_NETC_TS_T31D53M20I1R0/include
    ${S32_RTD_PATH}/RTD/EthSwt_43_NETC_TS_T31D53M20I1R0/include
    ${S32_RTD_PATH}/RTD/Det_TS_T31D53M20I1R0/include
    ${S32_RTD_PATH}/RTD/Rte_TS_T31D53M20I1R0/include
)

# Pull all generated PB/Cfg sources into the target. The Mcu_VS_0_PBcfg.c is
# an empty placeholder in the lwip example variant so glob-include is fine.
file(GLOB _s32ct_generated_src
    ${S32CT_GENERATED_DIR}/generate/src/*.c
    ${S32CT_GENERATED_DIR}/board/*.c)
target_sources(actuation_freertos_s32z2 PRIVATE ${_s32ct_generated_src})

set_target_properties(actuation_freertos_s32z2 PROPERTIES SUFFIX ".elf")

# ---- FreeRTOS kernel + ARM_CR52_GIC port ----
# The ARM_CR52_GIC port is self-contained in port.c (no portASM.S).
file(GLOB _freertos_core
    ${S32Z2_FREERTOS_SOURCE_DIR}/tasks.c
    ${S32Z2_FREERTOS_SOURCE_DIR}/queue.c
    ${S32Z2_FREERTOS_SOURCE_DIR}/list.c
    ${S32Z2_FREERTOS_SOURCE_DIR}/timers.c
    ${S32Z2_FREERTOS_SOURCE_DIR}/event_groups.c
    ${S32Z2_FREERTOS_SOURCE_DIR}/stream_buffer.c
    ${S32Z2_FREERTOS_SOURCE_DIR}/portable/MemMang/heap_4.c
)
target_sources(actuation_freertos_s32z2 PRIVATE
    ${_freertos_core}
    ${S32Z2_FREERTOS_PORT_DIR}/port.c
)

# common/'s `#include <pthread.h>` is resolved by the hand-rolled shim at
# actuation_module/include/platform/freertos/s32z2/pthread.h (that path is
# already first on the include search list). The shim maps pthread_mutex_*
# to FreeRTOS semaphores and pthread_create/join/cancel to xTaskCreate,
# avoiding FreeRTOS-Plus-POSIX and its header layering problems.

# ---- lwIP (from NXP TCP/IP Stack package) ----
find_path(LWIP_DIR
    NAMES src/api/sockets.c
    PATHS ${LWIP_PATH}/lwip
    DOC "NXP-provided lwIP source root"
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

# ---- NXP NETC <-> lwIP glue (eth_port.c) and RTD NETC driver ----
target_sources(actuation_freertos_s32z2 PRIVATE
    ${LWIP_PATH}/code/ports/netif/ethif/rtd/generic/eth_port.c
)
target_include_directories(actuation_freertos_s32z2 PRIVATE
    ${LWIP_PATH}/code/ports/netif/ethif/rtd/generic
)
file(GLOB _netc_sources
    ${S32_RTD_PATH}/RTD/Eth_43_NETC_TS_T31D53M20I1R0/src/*.c
)
target_sources(actuation_freertos_s32z2 PRIVATE ${_netc_sources})

target_sources(actuation_freertos_s32z2 PRIVATE
    lwip_bringup.c
)

# ---- Pre-built CycloneDDS target library ----
set(CDDS_HOST_PREFIX   ${CMAKE_SOURCE_DIR}/../../build-freertos/cdds_host_out CACHE PATH "")
set(CDDS_TARGET_PREFIX ${CMAKE_SOURCE_DIR}/../../build-s32z2/cdds_target_out CACHE PATH "")

if(NOT EXISTS ${CDDS_HOST_PREFIX}/bin/idlc)
    message(FATAL_ERROR
        "Host idlc not found at ${CDDS_HOST_PREFIX}/bin/idlc. "
        "Run the FreeRTOS POSIX simulator phase 1 build first.")
endif()
if(NOT EXISTS ${CDDS_TARGET_PREFIX}/lib/libddsc.a)
    message(FATAL_ERROR
        "Cross-built CycloneDDS not found at ${CDDS_TARGET_PREFIX}/lib/libddsc.a. "
        "Run actuation_module/freertos_s32z2/scripts/build-cdds-target.sh first.")
endif()

add_library(cdds STATIC IMPORTED GLOBAL)
set_target_properties(cdds PROPERTIES
    IMPORTED_LOCATION ${CDDS_TARGET_PREFIX}/lib/libddsc.a)

set(CYCLONEDDS_INCLUDE_DIR ${CDDS_TARGET_PREFIX}/include)

# Expose host idlc on PATH so autoware_msgs configure finds it.
set(ENV{PATH}            "${CDDS_HOST_PREFIX}/bin:$ENV{PATH}")
set(ENV{LD_LIBRARY_PATH} "${CDDS_HOST_PREFIX}/lib:$ENV{LD_LIBRARY_PATH}")
set(CycloneDDS_DIR ${CDDS_HOST_PREFIX}/lib/cmake/CycloneDDS)

# ---- Eigen3 (host package, header-only) ----
find_package(Eigen3 REQUIRED)

# ---- Autoware Messages (IDL -> C via host idlc) ----
add_subdirectory(${CMAKE_SOURCE_DIR}/../src/autoware/autoware_msgs
                 ${CMAKE_CURRENT_BINARY_DIR}/autoware_msgs)

# ---- Autoware controller core (shared with POSIX simulator) ----
include(${CMAKE_SOURCE_DIR}/../cmake/actuation_core.cmake)

# Rename actuation src main() to actuation_main(); the FreeRTOS wrapper
# provides the real main() in freertos_main.cpp.
add_compile_definitions(main=actuation_main)

target_sources(actuation_freertos_s32z2 PRIVATE
    ${ACTUATION_DIR}/src/main.cpp
    ${APP_SOURCES}
)

# Undo the main rename for the FreeRTOS wrapper itself.
set_source_files_properties(freertos_main.cpp PROPERTIES
    COMPILE_DEFINITIONS "main=main"
)

target_include_directories(actuation_freertos_s32z2 PRIVATE
    ${CYCLONEDDS_INCLUDE_DIR}
    ${EIGEN3_INCLUDE_DIR}
    ${APP_INCLUDE_DIRS}
)
target_link_libraries(actuation_freertos_s32z2 PRIVATE
    cdds
    autoware_msgs
)

// Copyright (c) 2026, Arm Limited and contributors.
// SPDX-License-Identifier: Apache-2.0
//
// B-2: FreeRTOS entry — bring up the board, configure the network, run the
// actuation controller.

#include <cstdio>

#include "FreeRTOS.h"
#include "task.h"

#include "platform/freertos/s32z2/board_init.h"

// actuation_main is main.cpp's main() renamed via -Dmain=actuation_main.
// CMake adds a -Wl,--defsym alias so the symbol is reachable under the
// unmangled C name that NXP's startup.s expects.
extern "C" int actuation_main(void);

static void actuation_task(void *pvParameters) {
    (void)pvParameters;
    // Network bring-up is owned by actuation_main() (main.cpp's configure_network),
    // which runs it once before constructing the Controller. Don't call it here
    // too -- a second lwip_bring_up_blocking() re-inits tcpip/netif and hangs.
    int ret = actuation_main();
    printf("actuation_main returned %d\n", ret);
    vTaskDelete(nullptr);
}

// Static idle / timer task allocations (unchanged from B-1).
static StaticTask_t xIdleTaskTCB;
static StackType_t  xIdleStack[configMINIMAL_STACK_SIZE];
static StaticTask_t xTimerTaskTCB;
static StackType_t  xTimerStack[configTIMER_TASK_STACK_DEPTH];

extern "C" void vApplicationGetIdleTaskMemory(
    StaticTask_t **ppxIdleTaskTCB, StackType_t **ppxIdleTaskStack,
    configSTACK_DEPTH_TYPE *pulIdleTaskStackSize)
{
    *ppxIdleTaskTCB       = &xIdleTaskTCB;
    *ppxIdleTaskStack     = xIdleStack;
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

extern "C" void vApplicationGetTimerTaskMemory(
    StaticTask_t **ppxTimerTaskTCB, StackType_t **ppxTimerTaskStack,
    configSTACK_DEPTH_TYPE *pulTimerTaskStackSize)
{
    *ppxTimerTaskTCB       = &xTimerTaskTCB;
    *ppxTimerTaskStack     = xTimerStack;
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}

extern "C" void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    (void)xTask; (void)pcTaskName;
    for (;;) {}
}

int main(void) {
    board_init();
    printf("FreeRTOS S32Z2 actuation starting...\n");

    // actuation_task is only the launcher: it brings up the network, then
    // constructs the Controller node and blocks in wait_for_completion().
    // The deep MPC/PID/Eigen control work runs on the node's own 256 KiB
    // node_stack (controller_node.cpp, off-heap StaticTask). The launcher's
    // own peak is bounded by CycloneDDS participant/reader/writer creation,
    // so it does not need the POSIX simulator's 32768-word (128 KiB) stack.
    // That value was copied from the 4 MiB-heap POSIX build; here the whole
    // FreeRTOS heap is only configTOTAL_HEAP_SIZE (96 KiB) because int_sram_dram
    // is already ~500/512 KiB full (node_stack + ucHeap + lwIP ram_heap), so a
    // 128 KiB stack can never be allocated. 8192 words = 32 KiB fits and leaves
    // the rest of the heap for CycloneDDS' internal threads and objects.
    // xTaskCreateFpu (not xTaskCreate) so the ARM_CR52_GIC port reserves a
    // per-task FPU context and records its TLS pointer. The port disables
    // FPEXC.EN per task and re-enables it lazily in vPortUndefinedInstruction
    // (wired to the undef vector in cp15_arm.S); that handler needs TLS[0] to
    // point at this task's FP save area, which only xTaskCreateFpu sets up.
    TaskHandle_t actuation_handle = nullptr;
    BaseType_t rc = xTaskCreateFpu(
        actuation_task, "actuation", 8192, nullptr,
        configMAX_PRIORITIES - 2, &actuation_handle);
    if (rc != pdPASS) {
        printf("xTaskCreate failed: %ld\n", (long)rc);
        for (;;) {}
    }

    vTaskStartScheduler();
    for (;;) {}
    return 1;
}

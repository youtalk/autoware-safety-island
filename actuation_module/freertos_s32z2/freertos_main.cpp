// Copyright (c) 2026, Arm Limited and contributors.
// SPDX-License-Identifier: Apache-2.0
//
// B-2: FreeRTOS entry — bring up the board, configure the network, run the
// actuation controller.

#include <cstdio>

#include "FreeRTOS.h"
#include "task.h"

#include "platform/freertos/s32z2/board_init.h"
#include "platform/platform_network.h"

extern "C" int actuation_main(void);   // renamed from main() via -Dmain=actuation_main

static void actuation_task(void *pvParameters) {
    (void)pvParameters;
    if (configure_network() != 0) {
        printf("network bring-up failed\n");
        vTaskDelete(nullptr);
        return;
    }
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

    // 32768 stack words (32-bit) = 128 KiB stack; matches the POSIX simulator.
    BaseType_t rc = xTaskCreate(
        actuation_task, "actuation", 32768, nullptr,
        configMAX_PRIORITIES - 2, nullptr);
    if (rc != pdPASS) {
        printf("xTaskCreate failed: %ld\n", (long)rc);
        for (;;) {}
    }

    vTaskStartScheduler();
    for (;;) {}
    return 1;
}

// Copyright (c) 2026, Arm Limited and contributors.
// SPDX-License-Identifier: Apache-2.0
//
// B-1: FreeRTOS bring-up — print "actuation alive ticks=N" from a FreeRTOS task.

#include <cstdio>

#include "FreeRTOS.h"
#include "task.h"

#include "platform/freertos/s32z2/board_init.h"

static void hello_task(void *pvParameters) {
    (void)pvParameters;
    TickType_t n = 0;
    for (;;) {
        ++n;
        printf("actuation alive ticks=%lu\n", (unsigned long)n);
        vTaskDelay(pdMS_TO_TICKS(150));
    }
}

// Static idle / timer task allocations (required because
// configSUPPORT_STATIC_ALLOCATION=1).
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
    printf("FreeRTOS on S32Z2 starting...\n");

    BaseType_t rc = xTaskCreate(
        hello_task, "hello", 4096, nullptr,
        configMAX_PRIORITIES - 2, nullptr);
    if (rc != pdPASS) {
        printf("xTaskCreate failed: %ld\n", (long)rc);
        for (;;) {}
    }

    vTaskStartScheduler();

    // Unreachable.
    for (;;) {}
    return 1;
}

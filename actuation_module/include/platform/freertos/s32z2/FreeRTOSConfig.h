// Copyright (c) 2026, Arm Limited and contributors.
// SPDX-License-Identifier: Apache-2.0
//
// FreeRTOS kernel configuration for the NXP S32Z2 (Cortex-R52) target.
// Companion to actuation_module/freertos_s32z2/board_init.c.

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <stdint.h>

// Substitute the value observed in Task 5 Step 1 — the clock Mcu_Init programs.
#define configCPU_CLOCK_HZ                  ((uint32_t)240000000UL)

#define configUSE_PREEMPTION                1
#define configUSE_IDLE_HOOK                 0
#define configUSE_TICK_HOOK                 0
#define configTICK_RATE_HZ                  ((TickType_t)1000)
#define configMAX_PRIORITIES                16
#define configMINIMAL_STACK_SIZE            ((unsigned short)512)
#define configMAX_TASK_NAME_LEN             32
#define configUSE_16_BIT_TICKS              0
#define configIDLE_SHOULD_YIELD             1
#define configUSE_TIME_SLICING              1

#define configTOTAL_HEAP_SIZE               ((size_t)(16U * 1024U * 1024U))
#define configSUPPORT_STATIC_ALLOCATION     1
#define configSUPPORT_DYNAMIC_ALLOCATION    1
#define configAPPLICATION_ALLOCATED_HEAP    0

#define configUSE_MUTEXES                   1
#define configUSE_RECURSIVE_MUTEXES         1
#define configUSE_COUNTING_SEMAPHORES       1
#define configQUEUE_REGISTRY_SIZE           20

#define configUSE_TIMERS                    1
#define configTIMER_TASK_PRIORITY           (configMAX_PRIORITIES - 1)
#define configTIMER_QUEUE_LENGTH            20
#define configTIMER_TASK_STACK_DEPTH        (configMINIMAL_STACK_SIZE * 4)

#define configUSE_TASK_NOTIFICATIONS        1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES 3

#define configCHECK_FOR_STACK_OVERFLOW      2
#define configUSE_TRACE_FACILITY            0
#define configGENERATE_RUN_TIME_STATS       0
#define configUSE_CO_ROUTINES               0

#define INCLUDE_vTaskDelete                 1
#define INCLUDE_vTaskDelay                  1
#define INCLUDE_vTaskDelayUntil             1
#define INCLUDE_vTaskSuspend                1
#define INCLUDE_xTaskGetSchedulerState      1
#define INCLUDE_xTaskGetCurrentTaskHandle   1
#define INCLUDE_uxTaskGetStackHighWaterMark 0
#define INCLUDE_xTimerPendFunctionCall      1

// PIT-backed tick. board_init.c implements these.
void s32z2_pit_setup_tick_interrupt(void);
void s32z2_pit_clear_tick_interrupt(void);
#define configSETUP_TICK_INTERRUPT()        s32z2_pit_setup_tick_interrupt()
#define configCLEAR_TICK_INTERRUPT()        s32z2_pit_clear_tick_interrupt()

#endif  // FREERTOS_CONFIG_H

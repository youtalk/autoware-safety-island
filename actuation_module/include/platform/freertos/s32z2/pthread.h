// Copyright (c) 2026, Arm Limited and contributors.
// SPDX-License-Identifier: Apache-2.0
//
// Minimal hand-rolled pthread shim for FreeRTOS on Cortex-R52.
//
// Replaces newlib's <pthread.h> (which on arm-none-eabi only ships type
// declarations, no working implementation). The common/ code only uses a
// small slice of pthread (mutex_lock/unlock, create/join/cancel,
// attr_init/setstack/destroy, PTHREAD_MUTEX_INITIALIZER, usleep), so this
// header stays small enough to read in one screen.
//
// pthread_t / pthread_mutex_t / pthread_attr_t typedefs come from newlib's
// <sys/_pthreadtypes.h> so we don't fight the typedefs that <chrono> and
// <time.h> pull in transitively. Backing storage:
//   - pthread_mutex_t (uint32_t) holds a SemaphoreHandle_t pointer-cast;
//     PTHREAD_MUTEX_INITIALIZER == 0 triggers lazy creation on first use.
//   - pthread_t (uint32_t) holds a pointer to a pthread_internal_t we
//     allocate in pthread_create. The wrapped FreeRTOS task is stored
//     there along with a binary semaphore used by pthread_join.

#ifndef PLATFORM_FREERTOS_S32Z2_PTHREAD_H_
#define PLATFORM_FREERTOS_S32Z2_PTHREAD_H_

#include <stddef.h>
#include <stdint.h>
#include <sys/_pthreadtypes.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

#undef PTHREAD_MUTEX_INITIALIZER
#define PTHREAD_MUTEX_INITIALIZER ((pthread_mutex_t)0)

typedef struct pthread_internal_s {
    void *(*entry)(void *);
    void *arg;
    TaskHandle_t task;
    SemaphoreHandle_t done;
} pthread_internal_t;

static inline int pthread_mutex_lock(pthread_mutex_t *mutex)
{
    if ((SemaphoreHandle_t)(uintptr_t)*mutex == NULL) {
        taskENTER_CRITICAL();
        if ((SemaphoreHandle_t)(uintptr_t)*mutex == NULL) {
            *mutex = (pthread_mutex_t)(uintptr_t)xSemaphoreCreateMutex();
        }
        taskEXIT_CRITICAL();
    }
    SemaphoreHandle_t s = (SemaphoreHandle_t)(uintptr_t)*mutex;
    if (s == NULL) {
        return -1;
    }
    return xSemaphoreTake(s, portMAX_DELAY) == pdPASS ? 0 : -1;
}

static inline int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
    SemaphoreHandle_t s = (SemaphoreHandle_t)(uintptr_t)*mutex;
    if (s == NULL) {
        return -1;
    }
    return xSemaphoreGive(s) == pdPASS ? 0 : -1;
}

static inline int pthread_attr_init(pthread_attr_t *attr)
{
    if (attr == NULL) {
        return -1;
    }
    attr->is_initialized = 1;
    attr->stackaddr = NULL;
    attr->stacksize = 0;
    return 0;
}

static inline int pthread_attr_setstack(pthread_attr_t *attr,
                                        void *stackaddr,
                                        size_t stacksize)
{
    if (attr == NULL) {
        return -1;
    }
    attr->stackaddr = stackaddr;
    attr->stacksize = (int)stacksize;
    return 0;
}

static inline int pthread_attr_destroy(pthread_attr_t *attr)
{
    if (attr == NULL) {
        return -1;
    }
    attr->is_initialized = 0;
    return 0;
}

static inline void pthread_s32z2_entry_(void *p)
{
    pthread_internal_t *info = (pthread_internal_t *)p;
    info->entry(info->arg);
    xSemaphoreGive(info->done);
    vTaskSuspend(NULL);
}

static inline int pthread_create(pthread_t *thread,
                                 const pthread_attr_t *attr,
                                 void *(*start_routine)(void *),
                                 void *arg)
{
    if (thread == NULL || start_routine == NULL) {
        return -1;
    }
    pthread_internal_t *info = (pthread_internal_t *)pvPortMalloc(sizeof(*info));
    if (info == NULL) {
        return -1;
    }
    info->entry = start_routine;
    info->arg = arg;
    info->done = xSemaphoreCreateBinary();
    if (info->done == NULL) {
        vPortFree(info);
        return -1;
    }
    BaseType_t ok = pdFAIL;
    if (attr != NULL && attr->stackaddr != NULL && attr->stacksize > 0) {
        StaticTask_t *tcb = (StaticTask_t *)pvPortMalloc(sizeof(StaticTask_t));
        if (tcb != NULL) {
            info->task = xTaskCreateStatic(
                pthread_s32z2_entry_,
                "pthread",
                (configSTACK_DEPTH_TYPE)((size_t)attr->stacksize / sizeof(StackType_t)),
                info,
                tskIDLE_PRIORITY + 1,
                (StackType_t *)attr->stackaddr,
                tcb);
            ok = (info->task != NULL) ? pdPASS : pdFAIL;
        }
    } else {
        configSTACK_DEPTH_TYPE depth = (configSTACK_DEPTH_TYPE)configMINIMAL_STACK_SIZE;
        if (attr != NULL && attr->stacksize > 0) {
            depth = (configSTACK_DEPTH_TYPE)((size_t)attr->stacksize / sizeof(StackType_t));
        }
        ok = xTaskCreate(pthread_s32z2_entry_, "pthread", depth, info,
                         tskIDLE_PRIORITY + 1, &info->task);
    }
    if (ok != pdPASS) {
        vSemaphoreDelete(info->done);
        vPortFree(info);
        return -1;
    }
    *thread = (pthread_t)(uintptr_t)info;
    return 0;
}

static inline int pthread_join(pthread_t thread, void **retval)
{
    pthread_internal_t *info = (pthread_internal_t *)(uintptr_t)thread;
    if (info == NULL) {
        return -1;
    }
    xSemaphoreTake(info->done, portMAX_DELAY);
    if (retval != NULL) {
        *retval = NULL;
    }
    if (info->task != NULL) {
        vTaskDelete(info->task);
    }
    vSemaphoreDelete(info->done);
    vPortFree(info);
    return 0;
}

static inline int pthread_cancel(pthread_t thread)
{
    pthread_internal_t *info = (pthread_internal_t *)(uintptr_t)thread;
    if (info == NULL || info->task == NULL) {
        return -1;
    }
    TaskHandle_t t = info->task;
    info->task = NULL;
    xSemaphoreGive(info->done);
    vTaskDelete(t);
    return 0;
}

static inline int usleep(unsigned int useconds)
{
    if (useconds == 0) {
        taskYIELD();
        return 0;
    }
    uint64_t ticks = ((uint64_t)useconds * (uint64_t)configTICK_RATE_HZ
                      + 999999ULL) / 1000000ULL;
    if (ticks == 0) {
        ticks = 1;
    }
    vTaskDelay((TickType_t)ticks);
    return 0;
}

#ifdef __cplusplus
}
#endif

#endif  // PLATFORM_FREERTOS_S32Z2_PTHREAD_H_

// Copyright (c) 2026, Arm Limited and contributors.
// SPDX-License-Identifier: Apache-2.0
//
// Wrapper so common/ code's `#include <pthread.h>` resolves to
// FreeRTOS-Plus-POSIX on this target without putting Plus-POSIX's
// include/FreeRTOS_POSIX directory on the firmware -I path (which
// would shadow newlib's <time.h>, <errno.h>, <ctime>).

#ifndef PLATFORM_FREERTOS_S32Z2_PTHREAD_H_
#define PLATFORM_FREERTOS_S32Z2_PTHREAD_H_

#include "FreeRTOS_POSIX.h"
#include "FreeRTOS_POSIX/pthread.h"

#endif  // PLATFORM_FREERTOS_S32Z2_PTHREAD_H_

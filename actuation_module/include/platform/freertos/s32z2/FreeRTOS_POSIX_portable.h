// Copyright (c) 2026, Arm Limited and contributors.
// SPDX-License-Identifier: Apache-2.0
//
// Port-specific FreeRTOS+POSIX configuration for S32Z2 (Cortex-R52).
// Included from FreeRTOS_POSIX.h before FreeRTOS_POSIX_portable_default.h,
// so anything left undefined here picks up the upstream default.
//
// On bare-metal arm-none-eabi we run with newlib. newlib's <sys/types.h>
// transitively pulls in <sys/_pthreadtypes.h> and <sys/sched.h>, which
// declare pthread_t / pthread_mutex_t / struct sched_param. Plus-POSIX
// must own those types (PTHREAD_MUTEX_INITIALIZER's expansion is a struct
// literal that targets Plus-POSIX's pthread_mutex_internal_t, not the
// opaque newlib long-unsigned-int). The build pre-defines the newlib
// include guards _SYS__PTHREADTYPES_H_ and _SYS_SCHED_H_ globally so
// newlib's pthread / sched headers become empty wherever they are pulled
// in (e.g. via <chrono> -> <time.h>), letting Plus-POSIX's typedefs win.
//
// For everything newlib already exposes that does NOT conflict — time_t,
// clock_t, clockid_t, struct timespec, errno values — disable the
// Plus-POSIX duplicates so its own internals defer to newlib.

#ifndef PLATFORM_FREERTOS_S32Z2_FREERTOS_POSIX_PORTABLE_H_
#define PLATFORM_FREERTOS_S32Z2_FREERTOS_POSIX_PORTABLE_H_

#include <time.h>
#include <errno.h>

#define posixconfigENABLE_CLOCK_T       0
#define posixconfigENABLE_CLOCKID_T     0
#define posixconfigENABLE_MODE_T        0
#define posixconfigENABLE_PID_T         0
#define posixconfigENABLE_SSIZE_T       0
#define posixconfigENABLE_TIME_T        0
#define posixconfigENABLE_TIMER_T       0
#define posixconfigENABLE_USECONDS_T    0
#define posixconfigENABLE_OFF_T         0
#define posixconfigENABLE_TIMESPEC      0
#define posixconfigENABLE_ITIMERSPEC    0

#endif  // PLATFORM_FREERTOS_S32Z2_FREERTOS_POSIX_PORTABLE_H_

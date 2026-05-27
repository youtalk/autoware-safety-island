// Copyright (c) 2026, Arm Limited and contributors.
// SPDX-License-Identifier: Apache-2.0
//
// Port-specific FreeRTOS+POSIX configuration for S32Z2 (Cortex-R52).
// Included from FreeRTOS_POSIX.h before FreeRTOS_POSIX_portable_default.h,
// so anything left undefined here picks up the upstream default.
//
// On bare-metal arm-none-eabi we run with newlib, which already exposes
// time_t / clock_t / clockid_t / struct timespec / errno / etc. via
// <sys/types.h>, <time.h>, <errno.h>. If Plus-POSIX also defines those
// (its int64_t time_t collides with newlib's long time_t, and a second
// struct timespec is a redefinition) the firmware does not compile.
// Pre-include the relevant newlib headers, then disable the Plus-POSIX
// duplicates so Plus-POSIX's own internals defer to the newlib versions.

#ifndef PLATFORM_FREERTOS_S32Z2_FREERTOS_POSIX_PORTABLE_H_
#define PLATFORM_FREERTOS_S32Z2_FREERTOS_POSIX_PORTABLE_H_

#include <sys/types.h>
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

// Copyright (c) 2026, Arm Limited and contributors.
// SPDX-License-Identifier: Apache-2.0
//
// S32Z2 board bring-up: RTD init, UART9 console, newlib _write retarget.
// PIT tick setup is added by Task 5.

#include <stdint.h>
#include <unistd.h>
#include <errno.h>

#include "platform/freertos/s32z2/board_init.h"

// RTD-provided initialisation entry points.
extern void Mcu_Init(void);
extern void Port_Init(void);
extern void Platform_Init(void);

// RTD-provided UART API. The engineer substitutes the actual symbols observed
// in $S32_RTD_PATH (Task 4 Step 1). Common variants:
//   LinFlexD_Uart_Ip_AsyncSend(uint8_t instance, const uint8_t* buf, uint32_t len);
//   Lin_Lpuart_Uart_Ip_SyncSend(uint8_t instance, ...);
extern void uart9_init_115200_8N1(void);
extern int  uart9_tx_byte(uint8_t b);

int board_init(void) {
    Mcu_Init();
    Platform_Init();
    Port_Init();
    uart9_init_115200_8N1();
    return 0;
}

// Newlib retarget: every printf/puts/fwrite to stdout/stderr ends up here.
int _write(int fd, const char *buf, int len) {
    if (fd != 1 && fd != 2) {
        errno = EBADF;
        return -1;
    }
    for (int i = 0; i < len; ++i) {
        if (uart9_tx_byte((uint8_t)buf[i]) != 0) {
            return i;
        }
    }
    return len;
}

// Newlib stubs to keep the link clean for bare metal.
int _close(int fd) { (void)fd; errno = EBADF; return -1; }
int _lseek(int fd, int off, int whence) { (void)fd; (void)off; (void)whence; errno = EBADF; return -1; }
int _read(int fd, char *buf, int len) { (void)fd; (void)buf; (void)len; errno = EBADF; return -1; }
int _fstat(int fd, void *st) { (void)fd; (void)st; errno = EBADF; return -1; }
int _isatty(int fd) { return (fd == 1 || fd == 2) ? 1 : 0; }
int _getpid(void) { return 1; }
int _kill(int pid, int sig) { (void)pid; (void)sig; errno = EINVAL; return -1; }

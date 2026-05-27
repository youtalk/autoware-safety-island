// Copyright (c) 2026, Arm Limited and contributors.
// SPDX-License-Identifier: Apache-2.0
//
// Minimal newlib syscall stubs for a bare-metal FreeRTOS image.
//
// libgcc and libc reference these for abort/exit/printf paths even when the
// application doesn't deliberately use them. We give them no-op implementations
// because the firmware never returns to a host OS — abort/exit hang, file I/O
// fails, etc.

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

void _exit(int status)
{
    (void)status;
    for (;;) {}
}

int _close(int fd) { (void)fd; errno = EBADF; return -1; }
int _fstat(int fd, struct stat *st) { (void)fd; (void)st; errno = EBADF; return -1; }
int _isatty(int fd) { (void)fd; return 1; }
int _lseek(int fd, off_t off, int whence) { (void)fd; (void)off; (void)whence; errno = EBADF; return -1; }
int _open(const char *path, int flags, int mode) { (void)path; (void)flags; (void)mode; errno = ENOENT; return -1; }
int _read(int fd, void *buf, size_t n) { (void)fd; (void)buf; (void)n; return 0; }
int _kill(int pid, int sig) { (void)pid; (void)sig; errno = EINVAL; return -1; }
int _getpid(void) { return 1; }
int _gettimeofday(struct timeval *tv, void *tz) { (void)tv; (void)tz; errno = ENOSYS; return -1; }
int _write(int fd, const void *buf, size_t n);  // implemented in board_init.c (UART9)

// Tiny static heap for any incidental newlib malloc (FreeRTOS code uses
// pvPortMalloc + heap_4, so this just covers stragglers like libc internals).
#define NEWLIB_HEAP_SIZE (64 * 1024)
static char newlib_heap[NEWLIB_HEAP_SIZE];

void *_sbrk(intptr_t incr)
{
    static char *brk = newlib_heap;
    char *prev = brk;
    if (brk + incr > newlib_heap + NEWLIB_HEAP_SIZE) {
        errno = ENOMEM;
        return (void *)-1;
    }
    brk += incr;
    return prev;
}

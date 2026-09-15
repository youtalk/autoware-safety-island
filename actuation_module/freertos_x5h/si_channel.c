// SPDX-License-Identifier: Apache-2.0
#include "si_channel.h"
#include <stdio.h>
#include <string.h>

static volatile int s_fault;
static volatile unsigned s_rx_count;

static int eq(const char *s, unsigned len, const char *lit)
{
    unsigned n = (unsigned)strlen(lit);
    if (len == n + 1 && s[n] == '\n') len = n;
    return len == n && memcmp(s, lit, n) == 0;
}

si_msg_t si_channel_parse(const void *data, unsigned len)
{
    const char *s = (const char *)data;
    if (!s || len == 0) return SI_MSG_IGNORE;
    if (eq(s, len, "hello")) return SI_MSG_HELLO;
    if (eq(s, len, "fault=1")) return SI_MSG_FAULT_SET;
    if (eq(s, len, "fault=0")) return SI_MSG_FAULT_CLEAR;
    return SI_MSG_IGNORE;
}

int si_channel_format_hb(char *buf, unsigned cap, unsigned seq, unsigned uptime_ms, int fault)
{
    int n = snprintf(buf, cap, "hb seq=%u uptime_ms=%u fault=%d\n", seq, uptime_ms, fault ? 1 : 0);
    if (n < 0 || (unsigned)n >= cap) return -1;
    return n;
}

void si_channel_rx(const void *data, unsigned len)
{
    s_rx_count++;
    switch (si_channel_parse(data, len)) {
    case SI_MSG_FAULT_SET:   s_fault = 1; break;
    case SI_MSG_FAULT_CLEAR: s_fault = 0; break;
    default: break;
    }
}

int si_channel_fault(void) { return s_fault; }
unsigned si_channel_rx_count(void) { return s_rx_count; }

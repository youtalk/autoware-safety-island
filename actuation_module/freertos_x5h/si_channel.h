// SPDX-License-Identifier: Apache-2.0
/* Second RPMsg channel "rpmsg-si": a 1 Hz heartbeat from the CR52 and a
 * fault input from Linux. Pure functions here have no FreeRTOS or OpenAMP
 * dependency so they are host-tested (test/test_si_channel.c). */
#ifndef SI_CHANNEL_H
#define SI_CHANNEL_H

#define SI_CHANNEL_SERVICE "rpmsg-si"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SI_MSG_IGNORE = 0,
    SI_MSG_HELLO = 1,       /* "hello": Linux announces its endpoint address */
    SI_MSG_FAULT_SET = 2,   /* "fault=1" */
    SI_MSG_FAULT_CLEAR = 3  /* "fault=0" */
} si_msg_t;

/* Parse one datagram. Length-bounded: the wire carries no NUL. A trailing
 * newline is accepted. */
si_msg_t si_channel_parse(const void *data, unsigned len);

/* "hb seq=<seq> uptime_ms=<ms> fault=<0|1>\n". Returns the byte count
 * written (without NUL) or -1 when cap is too small. */
int si_channel_format_hb(char *buf, unsigned cap, unsigned seq, unsigned uptime_ms, int fault);

/* Receive path: applies the parsed message to the fault latch. */
void si_channel_rx(const void *data, unsigned len);
int si_channel_fault(void);
unsigned si_channel_rx_count(void);

#ifdef __cplusplus
}
#endif
#endif

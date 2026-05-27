// Copyright (c) 2026, Arm Limited and contributors.
// SPDX-License-Identifier: Apache-2.0
//
// lwIP options for the NXP S32Z2 build. Pulled in by the lwIP sources
// from the NXP RTD via the lwIP build system.

#ifndef LWIPOPTS_H
#define LWIPOPTS_H

#define NO_SYS                          0
#define SYS_LIGHTWEIGHT_PROT            1

#define LWIP_SOCKET                     1
#define LWIP_COMPAT_SOCKETS             1
#define LWIP_NETCONN                    1
#define LWIP_NETIF_API                  1

#define LWIP_TCPIP_CORE_LOCKING         1
#define TCPIP_THREAD_STACKSIZE          8192
#define TCPIP_THREAD_PRIO               (12)   /* configMAX_PRIORITIES - 3 */
#define DEFAULT_THREAD_STACKSIZE        4096

#define MEM_SIZE                        (4 * 1024 * 1024)
#define MEMP_NUM_TCP_PCB                16
#define MEMP_NUM_UDP_PCB                16
#define MEMP_NUM_PBUF                   64
#define PBUF_POOL_SIZE                  64

#define LWIP_DHCP                       1
#define LWIP_IGMP                       1   /* SPDP multicast */
#define LWIP_UDP                        1
#define LWIP_TCP                        1
#define LWIP_RAW                        0
#define LWIP_DNS                        1

#define LWIP_SO_RCVBUF                  1
#define SO_REUSE                        1

#endif  // LWIPOPTS_H

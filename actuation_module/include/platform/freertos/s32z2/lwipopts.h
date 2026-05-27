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

// S32Z2's int_sram_dram region is only 512 KB for all data; the lwIP heap
// has to share it with the FreeRTOS kernel heap (configTOTAL_HEAP_SIZE),
// initialised globals, and CycloneDDS buffers.
#define MEM_SIZE                        (64 * 1024)
#define MEMP_NUM_TCP_PCB                4
#define MEMP_NUM_UDP_PCB                4
#define MEMP_NUM_PBUF                   16
#define PBUF_POOL_SIZE                  16

#define LWIP_DHCP                       1
#define LWIP_IGMP                       1   /* SPDP multicast */
#define LWIP_UDP                        1
#define LWIP_TCP                        1
#define LWIP_RAW                        0
#define LWIP_DNS                        1

#define LWIP_SO_RCVBUF                  1
#define SO_REUSE                        1

// lwip_bringup.c calls netif_set_status_callback after dhcp_start so the
// blocking bring-up returns as soon as DHCP hands us a lease.
#define LWIP_NETIF_STATUS_CALLBACK      1

// NXP's NETC <-> lwIP glue (code/ports/netif/ethif/rtd/generic/eth_port.c)
// extends struct pbuf with a back-pointer to the NETC RX buffer so it can
// hand it back to Eth_43_NETC_ProvideRxBuffer() when the pbuf is freed.
// lwIP exposes this extension point via LWIP_PBUF_CUSTOM_DATA.
#define LWIP_PBUF_CUSTOM_DATA           uint8_t *rx_buf;

#endif  // LWIPOPTS_H

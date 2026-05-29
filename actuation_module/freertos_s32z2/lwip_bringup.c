// Copyright (c) 2026, Arm Limited and contributors.
// SPDX-License-Identifier: Apache-2.0
//
// Brings up lwIP on the NXP S32Z2 NETC Ethernet controller and waits for a
// DHCP lease before returning. Called from configure_network() in
// include/platform/freertos/s32z2/freertos_network.h.

#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "lwip/tcpip.h"
#include "lwip/netif.h"
#include "lwip/dhcp.h"
#include "lwip/ip4_addr.h"

#include "platform/freertos/s32z2/lwip_init.h"

// NETC controller bring-up. The NXP RTD lwIP port (eth_port.c::
// ethif_low_level_init) only calls Eth_ProvideRxBuffer + Eth_SetControllerMode
// (ACTIVE); it ASSUMES the application has already run Eth_43_NETC_Init to set
// up the Station Interface, RX/TX BD rings, and MAC — exactly what NXP's
// device.c::device_init() does before bringing up lwIP. Our board_init() never
// did this, so the RX ring stayed empty and the board received zero frames.
// We invoke the minimal init subset here (poll mode needs no Platform/MRU IRQ
// plumbing): OsIf for the driver's timeout loops, the integrated switch, then
// the controller. PRECOMPILE_SUPPORT=STD_ON so each takes NULL_PTR.
#include "OsIf.h"
#include "EthSwt_43_NETC.h"
#include "Eth_43_NETC.h"

// Static-IP fallback used when no DHCP lease arrives (e.g. a link with no DHCP
// server, like the bench). Lets the controller + DDS still come up. Overridable
// at build time via -D. Board is .105/24; the gateway points at the bench host
// (.101), which is the DDS peer on the same /24 — same-subnet traffic to it
// needs no router, this just gives a sane default route.
#ifndef LWIP_FALLBACK_IP
#define LWIP_FALLBACK_IP       "192.168.0.105"
#endif
#ifndef LWIP_FALLBACK_NETMASK
#define LWIP_FALLBACK_NETMASK  "255.255.255.0"
#endif
#ifndef LWIP_FALLBACK_GW
#define LWIP_FALLBACK_GW       "192.168.0.101"
#endif

// NXP-provided NETC <-> lwIP glue from
// $LWIP_PATH/code/ports/netif/ethif/rtd/generic/eth_port.c.
extern err_t ethif_ethernetif_init(struct netif *netif);

static struct netif s_netif;
static SemaphoreHandle_t s_dhcp_done;

static void on_dhcp_state_changed(struct netif *netif) {
    if (dhcp_supplied_address(netif)) {
        xSemaphoreGive(s_dhcp_done);
    }
}

static void tcpip_init_done(void *arg) {
    SemaphoreHandle_t *done = (SemaphoreHandle_t *)arg;
    xSemaphoreGive(*done);
}

int lwip_bring_up_blocking(void) {
    SemaphoreHandle_t tcpip_done = xSemaphoreCreateBinary();
    s_dhcp_done = xSemaphoreCreateBinary();
    if (tcpip_done == NULL || s_dhcp_done == NULL) {
        return -1;
    }

    tcpip_init(tcpip_init_done, &tcpip_done);
    if (xSemaphoreTake(tcpip_done, pdMS_TO_TICKS(5000)) != pdTRUE) {
        printf("lwip: tcpip_init timed out\n");
        return -2;
    }

    // Initialise the NETC controller BEFORE netif_add() (netif_add ->
    // ethif_ethernetif_init -> ethif_low_level_init, which only sets the
    // controller ACTIVE and assumes the BD rings/SI/MAC are already configured).
    // Runs in this task's thread context so the driver's OsIf timeout loops have
    // a live tick. Eth_T_EnableIRQs() is deliberately omitted: poll mode services
    // RX from a thread, avoiding the RX-ISR FPU-corruption and GIC/MRU walls.
    printf("lwip: initialising NETC controller...\n");
    OsIf_Init(NULL_PTR);
    EthSwt_43_NETC_Init(NULL_PTR);
    Eth_43_NETC_Init(NULL_PTR);

    ip4_addr_t ipaddr = {0}, netmask = {0}, gw = {0};
    if (netif_add(&s_netif, &ipaddr, &netmask, &gw, NULL,
                  ethif_ethernetif_init, tcpip_input) == NULL) {
        printf("lwip: netif_add failed\n");
        return -3;
    }
    netif_set_default(&s_netif);
    netif_set_status_callback(&s_netif, on_dhcp_state_changed);
    netif_set_up(&s_netif);

    if (dhcp_start(&s_netif) != ERR_OK) {
        printf("lwip: dhcp_start failed\n");
        return -4;
    }
    printf("lwip: DHCP requested, waiting for lease (timeout 30s)...\n");

    if (xSemaphoreTake(s_dhcp_done, pdMS_TO_TICKS(30000)) != pdTRUE) {
        ip4_addr_t sip, snm, sgw;
        ip4addr_aton(LWIP_FALLBACK_IP, &sip);
        ip4addr_aton(LWIP_FALLBACK_NETMASK, &snm);
        ip4addr_aton(LWIP_FALLBACK_GW, &sgw);
        printf("lwip: DHCP lease timed out; using static IP %s\n", LWIP_FALLBACK_IP);
        dhcp_stop(&s_netif);
        netif_set_addr(&s_netif, &sip, &snm, &sgw);
    }

    char ip_str[16];
    ip4addr_ntoa_r(netif_ip4_addr(&s_netif), ip_str, sizeof(ip_str));
    printf("lwip: IP=%s\n", ip_str);
    return 0;
}

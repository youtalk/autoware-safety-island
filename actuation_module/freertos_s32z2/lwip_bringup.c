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

// RTD-provided NETC + lwIP glue. Substitute the actual symbols observed in
// Task 11 Step 1. Typical names:
//   err_t ethernetif_init(struct netif *netif);   // from RTD-supplied ethernetif.c
//   void  Eth_43_NETC_Init(...);                  // RTD MCAL initialiser
extern err_t ethernetif_init(struct netif *netif);

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

    ip4_addr_t ipaddr = {0}, netmask = {0}, gw = {0};
    if (netif_add(&s_netif, &ipaddr, &netmask, &gw, NULL,
                  ethernetif_init, tcpip_input) == NULL) {
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
        printf("lwip: DHCP lease timed out\n");
        return -5;
    }

    char ip_str[16];
    ip4addr_ntoa_r(netif_ip4_addr(&s_netif), ip_str, sizeof(ip_str));
    printf("lwip: IP=%s\n", ip_str);
    return 0;
}

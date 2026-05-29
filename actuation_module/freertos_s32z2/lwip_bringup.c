// Copyright (c) 2026, Arm Limited and contributors.
// SPDX-License-Identifier: Apache-2.0
//
// Brings up lwIP on the NXP S32Z2 NETC Ethernet controller and waits for a
// DHCP lease before returning. Called from configure_network() in
// include/platform/freertos/s32z2/freertos_network.h.

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "lwip/tcpip.h"
#include "lwip/netif.h"
#include "lwip/dhcp.h"
#include "lwip/ip4_addr.h"
#include "lwip/etharp.h"

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
// IP-layer MDIO accessor (raw PHY-address access, no DET/config validation) used
// by the session-4 PHY probe below.
#include "Netc_EthSwt_Ip.h"

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

// --- TEMP RX-datapath diagnostic (session 4) -----------------------------
// The CCS/gdb debugger cannot read the NETC AXI register space (0x74Axxxxx /
// 0x74Bxxxxx) in any context, but the core can (NETC init + TX work). So we
// read the RX-path counters here, in the actuation task that already ran
// Eth_43_NETC_Init, and print them over UART to localize where RX frames are
// dropped: external MAC -> switch ingress -> host SI -> SI RX BD ring.
// Register map verified against RTD 2.0.1 S32Z2_* headers.
#define NETC_REG32(a) (*(volatile uint32_t *)(uintptr_t)(a))
// External RGMII MAC ports: PORT0 (SW@0x74A04000, MAC@0x74A05000) = ETH0,
// PORT1 (SW@0x74A08000, MAC@0x74A09000) = ETH1. PORT2 = internal host-SI
// pseudo-port. The bench cable's port is whichever has TFRMN(tx) > 0.
static void s32z2_dump_extport(const char *name, uint32_t sw, uint32_t mac) {
    printf("  %s SW.POR=0x%08x PSR=0x%08x PRXDCR=%u PTXDCR=%u | "
           "MAC.CMDCFG=0x%08x IF_MODE=0x%08x TFRMN(tx)=%u RFRMN(rx)=%u RERRN=%u RDRPN=%u ROCTN=%u\n",
           name,
           (unsigned)NETC_REG32(sw + 0x100), (unsigned)NETC_REG32(sw + 0x104),
           (unsigned)NETC_REG32(sw + 0x1C0), (unsigned)NETC_REG32(sw + 0x1E0),
           (unsigned)NETC_REG32(mac + 0x008), (unsigned)NETC_REG32(mac + 0x300),
           (unsigned)NETC_REG32(mac + 0x220),
           (unsigned)NETC_REG32(mac + 0x120), (unsigned)NETC_REG32(mac + 0x138),
           (unsigned)NETC_REG32(mac + 0x158), (unsigned)NETC_REG32(mac + 0x108));
}
static void s32z2_netc_rx_diag(const char *tag) {
    printf("=== NETC RX DIAG [%s] ===\n", tag);
    /* ETH0_RX_RGMII_CLK = MC_CGM_1 MUX_7 (SEL_7), src ETH0_EXT_RX_CLK.
       CSS bit16=SWIP (switch-in-progress, stuck=>ext clock absent), DC0 bit31=DE. */
    printf("  CGM   ETH0_RX MUX7_CSC=0x%08x MUX7_CSS=0x%08x MUX7_DC0=0x%08x\n",
           (unsigned)NETC_REG32(0x408304C0), (unsigned)NETC_REG32(0x408304C4),
           (unsigned)NETC_REG32(0x408304C8));
    s32z2_dump_extport("PORT0/ETH0", 0x74A04000, 0x74A05000);
    s32z2_dump_extport("PORT1/ETH1", 0x74A08000, 0x74A09000);
    printf("  SI0   SIRFRM(rxOK)=%u SIROCT=%u\n",
           (unsigned)NETC_REG32(0x74B00308), (unsigned)NETC_REG32(0x74B00300));
    printf("  BDR0  RBMR=0x%08x RBSR=0x%08x RBLENR=0x%08x RBBSR=0x%08x\n",
           (unsigned)NETC_REG32(0x74B08100), (unsigned)NETC_REG32(0x74B08104),
           (unsigned)NETC_REG32(0x74B08120), (unsigned)NETC_REG32(0x74B08108));
    printf("  BDR0  RBBAR0=0x%08x RBPIR(prod)=%u RBCIR(cons)=%u RBDCR(drop)=%u\n",
           (unsigned)NETC_REG32(0x74B08110),
           (unsigned)NETC_REG32(0x74B08118), (unsigned)NETC_REG32(0x74B0810C),
           (unsigned)NETC_REG32(0x74B08180));
    printf("  LWIP  Tcpip_RxIndications[0]=%u TxConfirmations[0]=%u\n",
           (unsigned)NETC_REG32(0x317a2270), (unsigned)NETC_REG32(0x317a2298));
    printf("=== end NETC RX DIAG [%s] ===\n", tag);
}

// --- TEMP MDIO PHY probe (session 4) -------------------------------------
// Resolves "NETC RX clock not enabled" vs "PHY not delivering RGMII RX" by
// reading the ETH0 PHY over the NETC EMDIO (clause-22). EMDIO is already
// configured by EthSwt_43_NETC_Init(). TrcvIdx == raw MDIO PORT_ADDR.
static void s32z2_mdio_probe(void) {
    printf("=== MDIO PHY SCAN (clause-22, SW0 EMDIO) ===\n");
    int found = 0;
    for (uint8_t addr = 0U; addr < 32U; ++addr) {
        uint16_t id1 = 0xFFFFU, id2 = 0xFFFFU;
        if (Netc_EthSwt_Ip_ReadTrcvRegister(0U, addr, 2U, &id1) != E_OK) continue;
        if (Netc_EthSwt_Ip_ReadTrcvRegister(0U, addr, 3U, &id2) != E_OK) continue;
        if ((id1 == 0xFFFFU && id2 == 0xFFFFU) || (id1 == 0U && id2 == 0U)) continue;
        uint16_t bmcr = 0U, bmsr = 0U, stat1000 = 0U;
        (void)Netc_EthSwt_Ip_ReadTrcvRegister(0U, addr, 0U, &bmcr);
        (void)Netc_EthSwt_Ip_ReadTrcvRegister(0U, addr, 1U, &bmsr);  /* BMSR is latch-low: */
        (void)Netc_EthSwt_Ip_ReadTrcvRegister(0U, addr, 1U, &bmsr);  /* read twice for current */
        (void)Netc_EthSwt_Ip_ReadTrcvRegister(0U, addr, 10U, &stat1000);
        printf("  PHY@%u ID1=0x%04x ID2=0x%04x BMCR=0x%04x BMSR=0x%04x 1000T_STAT=0x%04x"
               " | link=%u anDone=%u\n",
               addr, id1, id2, bmcr, bmsr, stat1000,
               (unsigned)((bmsr >> 2) & 1U), (unsigned)((bmsr >> 5) & 1U));
        found++;
    }
    if (found == 0) {
        printf("  NO PHY responded on MDIO 0-31 (EMDIO/MDIO dead or PHY unpowered/in-reset)\n");
    }
    printf("=== end MDIO PHY SCAN ===\n");
}

// --- HYPOTHESIS-TEST FIX (session 4): KSZ9031 RGMII RX skew ----------------
// The ETH0 PHY (KSZ9031 @ MDIO addr 2) links at 1000M but the NETC MAC RXes
// zero frames: at 1000M RGMII the RX clock/data arrive edge-aligned because the
// KSZ9031 pad-skew regs are at reset defaults and neither the PHY (no init) nor
// the NETC MAC adds the required ~2ns RX internal delay. Program the KSZ9031
// RGMII pad-skew regs (MMD device 2) to add RX_CLK delay (RGMII-ID equivalent).
// MMD indirect access via MMDCTRL(0x0D)/MMDDATA(0x0E).
#define KSZ9031_PHY_ADDR 2U
static void ksz9031_mmd_write(uint8_t phy, uint8_t mmd, uint16_t reg, uint16_t val) {
    (void)Netc_EthSwt_Ip_WriteTrcvRegister(0U, phy, 0x0DU, mmd);          /* func=addr */
    (void)Netc_EthSwt_Ip_WriteTrcvRegister(0U, phy, 0x0EU, reg);
    (void)Netc_EthSwt_Ip_WriteTrcvRegister(0U, phy, 0x0DU, 0x4000U | mmd);/* func=data */
    (void)Netc_EthSwt_Ip_WriteTrcvRegister(0U, phy, 0x0EU, val);
}
static void s32z2_ksz9031_set_rgmii_skew(void) {
    const uint8_t phy = KSZ9031_PHY_ADDR;
    printf("=== KSZ9031 RGMII skew programming (PHY@%u) ===\n", phy);
    ksz9031_mmd_write(phy, 2U, 0x0004U, 0x0077U); /* control signal pad skew (RX/TX_CTL) */
    ksz9031_mmd_write(phy, 2U, 0x0005U, 0x7777U); /* RX data pad skew (RXD0-3) */
    ksz9031_mmd_write(phy, 2U, 0x0006U, 0x7777U); /* TX data pad skew (TXD0-3) */
    ksz9031_mmd_write(phy, 2U, 0x0008U, 0x03FFU); /* clock pad skew: GTX+RX_CLK max delay */
    printf("=== KSZ9031 RGMII skew done ===\n");
}
static uint16_t ksz9031_mmd_read(uint8_t phy, uint8_t mmd, uint16_t reg) {
    uint16_t v = 0xFFFFU;
    (void)Netc_EthSwt_Ip_WriteTrcvRegister(0U, phy, 0x0DU, mmd);
    (void)Netc_EthSwt_Ip_WriteTrcvRegister(0U, phy, 0x0EU, reg);
    (void)Netc_EthSwt_Ip_WriteTrcvRegister(0U, phy, 0x0DU, 0x4000U | mmd);
    (void)Netc_EthSwt_Ip_ReadTrcvRegister(0U, phy, 0x0EU, &v);
    return v;
}
static void s32z2_ksz9031_readback(void) {
    const uint8_t phy = KSZ9031_PHY_ADDR;
    printf("  KSZ9031 MMD2 skew readback: ctrl(4)=0x%04x rxd(5)=0x%04x txd(6)=0x%04x clk(8)=0x%04x\n",
           ksz9031_mmd_read(phy, 2U, 0x0004U), ksz9031_mmd_read(phy, 2U, 0x0005U),
           ksz9031_mmd_read(phy, 2U, 0x0006U), ksz9031_mmd_read(phy, 2U, 0x0008U));
}

// --- FIX HYPOTHESIS #2 (session 4): enable the NETC ETH0 RX RGMII clock -----
// ETH0_RX_RGMII_CLK = MC_CGM_1 MUX_7 (src ETH0_EXT_RX_CLK from the PHY's RXC).
// Clock_Ip_Init(config0) selected the source (CSC=0x31000000) but left the
// divider DISABLED (DC_0 DE=0) -> no RX clock to the NETC RX domain -> MAC RX
// frozen (RFRMN=RERRN=ROCTN=0). The PHY is linked by now, so the external RXC
// is present; enable the divider (DE=1, DIV=0 => /1 pass-through of 125 MHz).
#define MC_CGM_1_MUX_7_CSC 0x408304C0U
#define MC_CGM_1_MUX_7_CSS 0x408304C4U
#define MC_CGM_1_MUX_7_DC0 0x408304C8U
static void s32z2_enable_eth0_rx_clock(void) {
    printf("=== enable ETH0_RX clock (MC_CGM_1 MUX_7) ===\n");
    uint32_t csc = NETC_REG32(MC_CGM_1_MUX_7_CSC);
    uint32_t css = NETC_REG32(MC_CGM_1_MUX_7_CSS);
    printf("  before: CSC=0x%08x CSS=0x%08x DC0=0x%08x SELCTL=%u SELSTAT=%u SWIP=%u\n",
           (unsigned)csc, (unsigned)css, (unsigned)NETC_REG32(MC_CGM_1_MUX_7_DC0),
           (unsigned)((csc >> 24) & 0x3FU), (unsigned)((css >> 24) & 0x3FU),
           (unsigned)((css >> 16) & 1U));
    // The mux switch to the external RX clock (SELCTL=49) failed at Clock_Ip_Init
    // time (PHY RXC absent then) -> SELSTAT stuck at 0. PHY is linked now, so the
    // external RXC is present: re-trigger the switch (keep SELCTL, set CLK_SW b2).
    NETC_REG32(MC_CGM_1_MUX_7_CSC) = (csc & 0x3F000000U) | 0x4U;
    for (int t = 0; t < 200000; ++t) {            /* wait SWIP (b16) to clear */
        if ((NETC_REG32(MC_CGM_1_MUX_7_CSS) & 0x10000U) == 0U) break;
    }
    NETC_REG32(MC_CGM_1_MUX_7_DC0) = 0x80000000U; /* DE=1, DIV=0 (/1) */
    for (volatile int i = 0; i < 20000; ++i) { }
    css = NETC_REG32(MC_CGM_1_MUX_7_CSS);
    printf("  after:  CSC=0x%08x CSS=0x%08x DC0=0x%08x SELSTAT=%u SWIP=%u\n",
           (unsigned)NETC_REG32(MC_CGM_1_MUX_7_CSC), (unsigned)css,
           (unsigned)NETC_REG32(MC_CGM_1_MUX_7_DC0),
           (unsigned)((css >> 24) & 0x3FU), (unsigned)((css >> 16) & 1U));
    printf("=== ETH0_RX clock enable done ===\n");
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
    s32z2_netc_rx_diag("post-init");
    s32z2_mdio_probe();
    s32z2_ksz9031_set_rgmii_skew();
    s32z2_ksz9031_readback();
    s32z2_enable_eth0_rx_clock();

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
    // Announce ourselves so peers learn our IP->MAC without having to solicit
    // (and to test the RX-driven reply path indirectly). TEMP diagnostic.
    etharp_gratuitous(&s_netif);
    // After the DHCP window (host has been flooding broadcast ARP the whole
    // time), re-read the RX counters: deltas vs "post-init" reveal which NETC
    // layer the inbound frames reach (or fail to reach).
    s32z2_netc_rx_diag("post-dhcp");
    return 0;
}

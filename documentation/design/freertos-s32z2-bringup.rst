FreeRTOS on the ARM Automotive Kit (NXP S32Z2)
===============================================

This document records the design of Phase 5 of the FreeRTOS port: bringing the
actuation firmware to the ARM Automotive Kit (X-S32Z27X-DC, NXP S32Z2,
Cortex-R52). Issue: ``autowarefoundation/autoware-safety-island#1``.

Scope
-----

The kit runs FreeRTOS with feature parity to the existing Zephyr backend on
the same SoC:

- Cortex-R52 RTU0, lock-step mode (reset default).
- FreeRTOS V11.1.0 via NXP's Real-Time Drivers (RTD) Cortex-R52 port.
- lwIP socket layer, BSD-socket-compatible, driving the NETC Ethernet
  controller.
- CycloneDDS cross-compiled statically into the firmware image.
- ``DDS_ONLY`` control-command output mode; CAN is not validated on the kit
  in Phase 5.
- 150 ms periodic controller loop, identical to the Zephyr backend.

Out of scope: eSync OTA, Linux-side Autoware integration, CAN frame
observation, Phase 6 tuning, an on-hardware CI job, migration to the Phase 2
stack (issue #14).

Build tree layout
-----------------

The Phase 5 build is an additional CMake entry point under
``actuation_module/freertos_s32z2/`` that does not modify the existing
``actuation_module/freertos/`` POSIX-simulator build, the Zephyr boards under
``actuation_module/boards/``, or the PAL backend headers under
``actuation_module/include/platform/freertos/*.h``. New variant headers live
under ``actuation_module/include/platform/freertos/s32z2/``.

The single dispatcher edit is ``actuation_module/include/platform/platform_network.h``,
which gains a ``#elif defined(PLATFORM_FREERTOS_S32Z2)`` branch selecting the
new variant network header.

Boot bring-up (B-1)
-------------------

``board_init()`` calls ``Mcu_Init`` / ``Platform_Init`` / ``Port_Init`` from
the RTD, brings UART9 up at 115200 8N1, and registers the PIT as the FreeRTOS
tick source. ``configCPU_CLOCK_HZ`` matches the clock programmed by
``Mcu_Init``. Newlib's ``_write`` is retargeted to UART9 so ``printf`` reaches
the OpenSDA serial port.

A heartbeat task prints ``actuation alive ticks=N`` every 150 ms. The B-1
acceptance gate is the appearance of at least five such lines within ten
seconds of reset.

Networking and DDS (B-2)
------------------------

``configure_network()`` is selected at compile time by the dispatcher branch
in ``platform_network.h`` and dispatches to ``lwip_bring_up_blocking()`` from
``include/platform/freertos/s32z2/freertos_network.h``. ``lwip_bring_up_blocking``
calls ``tcpip_init()``, registers the NETC driver via ``ethernetif_init`` /
``netif_add``, requests DHCP, and blocks on a semaphore until a lease arrives
(30 s timeout).

CycloneDDS is cross-compiled as a static library for Cortex-R52 with security,
SSL, shared memory, and IPv6 disabled (see
``actuation_module/freertos_s32z2/scripts/build-cdds-target.sh``). The host
``idlc`` from the POSIX-simulator phase 1 build is reused for IDL → C
generation of ``autoware_msgs``.

The Edge ECU peer is the development host itself: ``dds_pub`` and ``dds_sub``
(unchanged) compile with host gcc against the host CycloneDDS and run as
``edge_ecu_pub`` / ``edge_ecu_sub``. The acceptance gates are:

- ``Controller Node Started`` and ``Actuation Safety Island is Live`` appear
  on UART9.
- ``edge_ecu_sub`` receives ``STEERING REPORT`` at least twice.
- ``actuation_main`` does not return during the verification window.

These are the same string markers used by the POSIX-simulator smoke
(PRs #10 / #13).

References
----------

- NXP S32Z2 product page: https://www.nxp.com/products/S32Z2
- S32Z2 block diagram: https://www.nxp.com/assets/block-diagram/en/S32Z2.pdf
- X-S32Z27X-DC Zephyr docs (cross-reference for hardware wiring):
  https://docs.zephyrproject.org/latest/boards/nxp/s32z2xxdc2/doc/index.html
- How to download RTD:
  https://www.nxp.com/company/about-nxp/smarter-world-videos/HOW-TO-DWLD-RTD

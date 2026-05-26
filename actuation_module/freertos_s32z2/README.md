# FreeRTOS on NXP S32Z2 — Build and Verification

This directory contains the FreeRTOS build for the ARM Automotive Kit
(X-S32Z27X-DC carrier, NXP S32Z2 SoC, Cortex-R52, RTU0 lock-step). All build
and flash steps run on the development host.

## Prerequisites (manual)

The NXP-licensed pieces are not in this repository.

1. NXP S32 Real-Time Drivers (RTD) for S32Z/E — download from
   https://www.nxp.com/ → My NXP Account → Software Licensing →
   Automotive SW – S32Z/E Standard Software. See
   https://www.nxp.com/company/about-nxp/smarter-world-videos/HOW-TO-DWLD-RTD
   for the navigation walkthrough. Naming pattern:
   `SW32Z2_RTD_<version>_DS_updatesite_<date>.zip`.

2. NXP FreeRTOS module for S32 Platform — same portal; RTD must be installed
   first.

3. NETC + lwIP — typically bundled inside the RTD archive. If absent on your
   release, install via S32 Design Studio "Communication Stack" extension.

4. S32 Design Studio for S32 Platform (3.6.1+):
   https://www.nxp.com/design/design-center/software/automotive-software-and-tools/s32-design-studio-ide/s32-design-studio-for-s32-platform:S32DSS32PLATFORM
   The S32 Flash Tool is bundled with this installer.

5. `arm-none-eabi-gcc` 12.x or newer.

Extract the RTD under e.g. `/opt/nxp/RTD_<version>` and export
`S32_RTD_PATH=/opt/nxp/RTD_<version>` system-wide (e.g. in `/etc/profile.d/nxp.sh`).

## Hardware wiring

- USB-C from host to the carrier's OpenSDA port (UART9 console at 115200 8N1
  on `/dev/ttyACM0`).
- 8 V–36 V DC power on the carrier.
- B-2 only: Ethernet 0 of the carrier on the same LAN segment as the host.

## Build

```bash
export S32_RTD_PATH=/opt/nxp/RTD_<version>
cmake -S actuation_module/freertos_s32z2 -B build-s32z2 \
    -DCMAKE_TOOLCHAIN_FILE=actuation_module/freertos_s32z2/cmake/arm-cortex-r52.cmake
cmake --build build-s32z2 -j
```

Output: `build-s32z2/actuation_freertos_s32z2.elf`.

## Flash

```bash
west flash --runner <runner> --elf-file build-s32z2/actuation_freertos_s32z2.elf
```

If `west flash` cannot drive the probe, fall back to the S32 Flash Tool CLI:

```bash
$S32DS_HOME/S32FlashTool/bin/S32FlashTool ... <flash command>
```

(Refer to the S32 Flash Tool User Guide.)

## Verify B-1

```bash
S32_RTD_PATH=/opt/nxp/RTD_<version> \
WEST_FLASH_RUNNER=<runner> \
  ./actuation_module/freertos_s32z2/scripts/verify-b1.sh
```

Expected last line: `B-1 verification OK (N heartbeats)`.

## Verify B-2

See B-2 PR. (Section added by Task 18.)

## Resolved RTD paths

Recorded once the engineer pins them in `CMakeLists.txt`:

- FreeRTOS Cortex-R52 port: `$S32_RTD_PATH/...`
- FreeRTOS kernel headers: `$S32_RTD_PATH/...`
- Mcu / Port / Platform driver sources: `$S32_RTD_PATH/MCAL/...`
- LinFlexD UART driver: `$S32_RTD_PATH/MCAL/...`
- PIT driver: `$S32_RTD_PATH/MCAL/...`

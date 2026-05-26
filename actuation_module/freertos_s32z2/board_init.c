// Copyright (c) 2026, Arm Limited and contributors.
// SPDX-License-Identifier: Apache-2.0
//
// S32Z2 board bring-up entry. UART9 and PIT setup come in later tasks.

#include "platform/freertos/s32z2/board_init.h"

// RTD-provided initialisation entry points. The exact header path depends on
// the RTD version; adjust if the engineer's archive uses a different layout.
extern void Mcu_Init(void);
extern void Port_Init(void);
extern void Platform_Init(void);

int board_init(void) {
    Mcu_Init();
    Platform_Init();
    Port_Init();
    return 0;
}

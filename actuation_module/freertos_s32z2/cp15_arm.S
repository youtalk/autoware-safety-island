// Copyright (c) 2026, Arm Limited and contributors.
// SPDX-License-Identifier: Apache-2.0
//
// ARM-mode replacements for NXP's RTD Thumb-encoded CP15 / VFP accessors,
// hooked in via `-Wl,--wrap=read_CPACR,...` so callers reach these
// instead of NXP's. NXP ships read_CPACR / write_CPACR / write_FPEXC
// inside core_utility.s with `.thumb2`. Core_FPU_Init in core.c is
// ARM-mode and reaches them via `blx`; the `blx` correctly switches to
// Thumb on entry, but the Thumb `mov pc, lr` epilogue does *not*
// interwork back to ARM on this Cortex-R52 — the T bit stays set, the
// caller's ARM-encoded body is decoded as Thumb-2, and the CPU branches
// off into the .intc_vector data area. Replacing the helpers with
// ARM-mode versions that finish with `bx lr` keeps the LSB-clear LR set
// by the caller's `blx`, so the return is interworked correctly and
// control comes back to the caller in ARM state.

    .syntax unified
    .arch armv8-r
    .arm

    .section .sram_code, "ax"

    .global __wrap_read_CPACR
    .type   __wrap_read_CPACR, %function
__wrap_read_CPACR:
    mrc     p15, 0, r0, c1, c0, 2
    bx      lr
    .size   __wrap_read_CPACR, . - __wrap_read_CPACR

    .global __wrap_write_CPACR
    .type   __wrap_write_CPACR, %function
__wrap_write_CPACR:
    mcr     p15, 0, r0, c1, c0, 2
    isb     sy
    bx      lr
    .size   __wrap_write_CPACR, . - __wrap_write_CPACR

    .global __wrap_write_FPEXC
    .type   __wrap_write_FPEXC, %function
__wrap_write_FPEXC:
    vmsr    fpexc, r0
    isb     sy
    bx      lr
    .size   __wrap_write_FPEXC, . - __wrap_write_FPEXC

    .global __wrap_read_FPEXC
    .type   __wrap_read_FPEXC, %function
__wrap_read_FPEXC:
    vmrs    r0, fpexc
    bx      lr
    .size   __wrap_read_FPEXC, . - __wrap_read_FPEXC

    // write_64bit(lo=r0, hi=r1, dest=r2) — 64-bit atomic store for ECC SRAM.
    // NXP's Thumb-encoded original has the same `mov pc, lr` non-interwork
    // problem as the CPACR helpers; init_data_bss → zero_copy64 → write_64bit
    // is the chain that derailed before this wrap was added.
    .global __wrap_write_64bit
    .type   __wrap_write_64bit, %function
__wrap_write_64bit:
    strd    r0, r1, [r2]
    bx      lr
    .size   __wrap_write_64bit, . - __wrap_write_64bit

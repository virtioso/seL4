/*
 * Copyright 2020, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include <arch/machine/hardware.h>

/*
 * On ARMv8-A the page table walker is cache-coherent, so kernel page
 * table writes are visible to the hardware without explicit cache
 * maintenance. The elfloader flushes all loaded images to PoC before
 * entering the kernel, so no boot-time bulk cache clean is needed here.
 *
 * These functions are called from activate_kernel_vspace() and
 * benchmark code. Making them no-ops avoids the need to walk a
 * VA range for cache maintenance, which is problematic with disjoint
 * memory regions (UEFI memory map with firmware carveout gaps).
 */

void clean_D_PoU(void)
{
    dsb();
    isb();
}

void cleanInvalidate_D_PoC(void)
{
    dsb();
    isb();
}

void cleanInvalidate_L1D(void)
{
    dsb();
    isb();
}

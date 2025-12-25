/*
 * Copyright 2025, Technology Innovation Institute
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Orin AGX SDEI handler for RAS errors.
 *
 * ATF dispatches per-CPU RAS events 300-311 when uncorrectable errors
 * are detected. This handler prints diagnostic info and halts, as
 * continuing with potentially corrupted memory is unsafe.
 */

#include <config.h>

#ifdef CONFIG_ARM_SDEI

#include <types.h>
#include <machine/io.h>
#include <arch/machine.h>

/*
 * Platform-specific SDEI handler for Tegra T234 RAS errors.
 * Overrides the weak default handler in sdei.c.
 *
 * RAS (Reliability, Availability, Serviceability) errors indicate
 * uncorrectable hardware problems. We halt rather than risk executing
 * with corrupted state.
 */
void sdei_handler(word_t event_num, word_t arg, word_t interrupted_pc)
{
    word_t cpu_id = event_num - ARM_SDEI_EVENT_BASE;

    printf("RAS ERROR: SDEI event=%lu cpu=%lu interrupted_pc=0x%lx\n",
           event_num, cpu_id, interrupted_pc);
    printf("Uncorrectable hardware error detected - halting system\n");

    halt();
}

#endif /* CONFIG_ARM_SDEI */

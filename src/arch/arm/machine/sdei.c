/*
 * Copyright 2025, Technology Innovation Institute
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * SDEI (Software Delegated Exception Interface) support for ARM64.
 *
 * This module registers seL4 as a handler for SDEI events dispatched by
 * ARM Trusted Firmware (ATF/TF-A). Event numbers are platform-specific and
 * configured via CMake (ARM_SDEI_EVENT_BASE, ARM_SDEI_EVENT_COUNT).
 *
 * When ATF dispatches an SDEI event, it invokes our registered handler
 * at the highest non-secure exception level.
 */

#include <config.h>

#ifdef CONFIG_ARM_SDEI

#include <types.h>
#include <machine/io.h>
#include <arch/machine/sdei.h>
#include <arch/machine.h>

/*
 * SMC call wrapper for SDEI functions.
 * Uses ARM SMC calling convention: function ID in x0, args in x1-x4.
 * Returns result in x0.
 */
static inline word_t sdei_smc(word_t func_id, word_t arg1, word_t arg2,
                              word_t arg3, word_t arg4)
{
    register word_t r0 asm("x0") = func_id;
    register word_t r1 asm("x1") = arg1;
    register word_t r2 asm("x2") = arg2;
    register word_t r3 asm("x3") = arg3;
    register word_t r4 asm("x4") = arg4;

    asm volatile("smc #0"
                 : "+r"(r0), "+r"(r1), "+r"(r2), "+r"(r3), "+r"(r4)
                 :
                 : "x5", "x6", "x7", "x8", "x9", "x10", "x11",
                   "x12", "x13", "x14", "x15", "x16", "x17", "memory");

    return r0;
}

/*
 * C handler called from sdei_handler_entry assembly.
 * Runs at highest non-secure EL with SDEI critical priority (0x20).
 *
 * Entry:
 *   event_num - SDEI event number (platform-specific)
 *   arg - argument we registered (event index)
 *   interrupted_pc - PC where execution was interrupted
 *
 * Default behavior: log the event and return. Platforms can override
 * this weak symbol to provide custom behavior.
 */
WEAK void sdei_handler(word_t event_num, word_t arg, word_t interrupted_pc)
{
    word_t event_index = event_num - ARM_SDEI_EVENT_BASE;

    printf("SDEI: event=%lu index=%lu interrupted_pc=0x%lx\n",
           event_num, event_index, interrupted_pc);
}

/*
 * Initialize SDEI interface.
 * Called during kernel boot to check SDEI availability.
 */
void sdei_init(void)
{
    word_t version = sdei_smc(SDEI_VERSION, 0, 0, 0, 0);

    if ((sword_t)version < 0) {
        printf("SDEI: not supported by firmware (ret=%ld)\n", (sword_t)version);
        return;
    }

    /* SDEI version format: bits[63:48]=major, bits[47:32]=minor */
    word_t major = (version >> 48) & 0xFFFF;
    word_t minor = (version >> 32) & 0xFFFF;
    printf("SDEI: version %lu.%lu detected (raw=0x%lx)\n", major, minor, version);

    /* Unmask SDEI events on this PE so they can be dispatched */
    word_t ret = sdei_smc(SDEI_PE_UNMASK, 0, 0, 0, 0);
    if ((sword_t)ret < 0) {
        printf("SDEI: PE_UNMASK failed (ret=%ld)\n", (sword_t)ret);
        return;
    }

    printf("SDEI: PE unmasked, ready for events\n");
}

/*
 * Register SDEI handlers for configured events.
 * Event range is configured via CMake (ARM_SDEI_EVENT_BASE, ARM_SDEI_EVENT_COUNT).
 * ATF will dispatch these events to our handler when they occur.
 */
void sdei_register_handlers(void)
{
    word_t handler_addr = (word_t)sdei_handler_entry;
    word_t ret;
    int registered = 0;

    printf("SDEI: registering handlers at 0x%lx (events %d-%d)\n",
           handler_addr, ARM_SDEI_EVENT_BASE,
           ARM_SDEI_EVENT_BASE + ARM_SDEI_EVENT_COUNT - 1);

    /* Register handler for each event in configured range */
    for (int i = 0; i < ARM_SDEI_EVENT_COUNT; i++) {
        word_t event = ARM_SDEI_EVENT_BASE + i;

        /*
         * Register handler:
         *   arg1: event number
         *   arg2: handler entry point (EL2 address)
         *   arg3: argument passed to handler (we pass the index)
         *   arg4: flags (RM_PE for private per-CPU events)
         */
        ret = sdei_smc(SDEI_EVENT_REGISTER, event, handler_addr,
                       (word_t)i, SDEI_REG_FLAG_RM_PE);

        if ((sword_t)ret < 0) {
            if (ret == (word_t)SDEI_NOT_SUPPORTED) {
                /* Event not available - skip silently */
                continue;
            }
            printf("SDEI: register event %lu failed (ret=%ld)\n",
                   event, (sword_t)ret);
            continue;
        }

        /* Enable the event so ATF can dispatch to us */
        ret = sdei_smc(SDEI_EVENT_ENABLE, event, 0, 0, 0);
        if ((sword_t)ret < 0) {
            printf("SDEI: enable event %lu failed (ret=%ld)\n",
                   event, (sword_t)ret);
            /* Try to unregister since enable failed */
            sdei_smc(SDEI_EVENT_UNREGISTER, event, 0, 0, 0);
            continue;
        }

        registered++;
    }

    if (registered > 0) {
        printf("SDEI: registered %d event handlers (events %d-%d)\n",
               registered, ARM_SDEI_EVENT_BASE,
               ARM_SDEI_EVENT_BASE + registered - 1);
    } else {
        printf("SDEI: no events available in range %d-%d\n",
               ARM_SDEI_EVENT_BASE,
               ARM_SDEI_EVENT_BASE + ARM_SDEI_EVENT_COUNT - 1);
    }
}

#endif /* CONFIG_ARM_SDEI */

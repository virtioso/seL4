/*
 * Copyright 2025, Technology Innovation Institute
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * SDEI handler entry point for ARM64.
 *
 * When ATF dispatches an SDEI event, we enter here with:
 *   x0 = event number
 *   x1 = handler argument (we registered event index)
 *   x2 = interrupted PC
 *   x3-x17 = undefined (ATF may have clobbered them, use SDEI_EVENT_CONTEXT to retrieve)
 *   x18-x29 = preserved from interrupted context (per AAPCS64 callee-save convention)
 *   x30 (LR) = return address within ATF SDEI dispatcher
 *   SP = stack from interrupted context
 *
 * CRITICAL: SDEI_EVENT_COMPLETE does NOT restore x18-x29. We must:
 *   1. Save callee-saved registers (x19-x29, x30) on entry
 *   2. Call C handler (which may clobber x19-x29)
 *   3. Restore x19-x29 BEFORE calling SDEI_EVENT_COMPLETE
 *
 * The C handler must NOT call SDEI_EVENT_COMPLETE itself, otherwise
 * x19-x29 will be corrupted when execution resumes.
 */

#include <config.h>

#ifdef CONFIG_ARM_SDEI

#include <machine/assembler.h>

.section .text

/*
 * SDEI handler entry point.
 * This function is registered with ATF via SDEI_EVENT_REGISTER.
 *
 * Entry state:
 *   x0 = event_num (SDEI event number)
 *   x1 = arg (handler argument we registered)
 *   x2 = interrupted_pc (PC where execution was interrupted)
 *
 * Must preserve x19-x29 across call to C handler.
 */
BEGIN_FUNC(sdei_handler_entry)
    /* Save callee-saved registers and return address on stack */
    stp     x29, x30, [sp, #-16]!
    stp     x27, x28, [sp, #-16]!
    stp     x25, x26, [sp, #-16]!
    stp     x23, x24, [sp, #-16]!
    stp     x21, x22, [sp, #-16]!
    stp     x19, x20, [sp, #-16]!

    /* Save arguments in callee-saved registers before C call */
    mov     x19, x0             /* event_num */
    mov     x20, x1             /* arg */
    mov     x21, x2             /* interrupted_pc */

    /* Call C handler: sdei_handler(event_num, arg, interrupted_pc)
     * Arguments are already in x0, x1, x2 */
    bl      sdei_handler

    /*
     * C handler has returned. We must restore callee-saved registers
     * BEFORE calling SDEI_EVENT_COMPLETE, because SDEI_EVENT_COMPLETE
     * resumes at the interrupted context but does NOT restore x18-x29.
     */
    ldp     x19, x20, [sp], #16
    ldp     x21, x22, [sp], #16
    ldp     x23, x24, [sp], #16
    ldp     x25, x26, [sp], #16
    ldp     x27, x28, [sp], #16
    ldp     x29, x30, [sp], #16

    /* SDEI_EVENT_COMPLETE SMC:
     *   x0 = 0xC4000025 (function ID)
     *   x1 = 0 (SDEI_EV_HANDLED - resume at interrupted PC)
     * Now x19-x29 are restored to their interrupted context values.
     */
    ldr     x0, =0xC4000025     /* SDEI_EVENT_COMPLETE */
    mov     x1, #0              /* SDEI_EV_HANDLED */
    smc     #0

    /*
     * SDEI_EVENT_COMPLETE should not return.
     * If we get here, something is very wrong - spin forever.
     */
1:  wfe
    b       1b
END_FUNC(sdei_handler_entry)

#endif /* CONFIG_ARM_SDEI */

/*
 * Copyright 2025, Technology Innovation Institute
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * ARM SDEI (Software Delegated Exception Interface) definitions.
 * See ARM DEN0054A specification.
 */

#pragma once

#include <config.h>

#ifdef CONFIG_ARM_SDEI

#include <types.h>

/* SDEI SMC function IDs (ARM DEN0054A specification) */
#define SDEI_VERSION                    0xC4000020UL
#define SDEI_EVENT_REGISTER             0xC4000021UL
#define SDEI_EVENT_ENABLE               0xC4000022UL
#define SDEI_EVENT_DISABLE              0xC4000023UL
#define SDEI_EVENT_CONTEXT              0xC4000024UL
#define SDEI_EVENT_COMPLETE             0xC4000025UL
#define SDEI_EVENT_COMPLETE_AND_RESUME  0xC4000026UL
#define SDEI_EVENT_UNREGISTER           0xC4000027UL
#define SDEI_EVENT_STATUS               0xC4000028UL
#define SDEI_EVENT_GET_INFO             0xC4000029UL
#define SDEI_EVENT_ROUTING_SET          0xC400002AUL
#define SDEI_PE_MASK                    0xC400002BUL
#define SDEI_PE_UNMASK                  0xC400002CUL
#define SDEI_INTERRUPT_BIND             0xC400002DUL
#define SDEI_INTERRUPT_RELEASE          0xC400002EUL
#define SDEI_PRIVATE_RESET              0xC4000011UL
#define SDEI_SHARED_RESET               0xC4000012UL

/*
 * SDEI event configuration - must be set by platform config.cmake
 *
 * ARM_SDEI_EVENT_BASE:  First event number to register
 * ARM_SDEI_EVENT_COUNT: Number of consecutive events to register
 */

/* SDEI return codes */
#define SDEI_SUCCESS                    0
#define SDEI_NOT_SUPPORTED              (-1)
#define SDEI_INVALID_PARAMETERS         (-2)
#define SDEI_DENIED                     (-3)
#define SDEI_PENDING                    (-5)
#define SDEI_OUT_OF_RESOURCE            (-10)

/*
 * SDEI_EVENT_CONTEXT parameter indices.
 * NOTE: SDEI only saves x0-x17 (18 GPRs). It does NOT provide access to
 * ELR, SPSR, ESR, or FAR. Those must be read from system registers directly
 * or obtained from the dispatch arguments.
 */
#define SDEI_CONTEXT_X0                 0
#define SDEI_CONTEXT_X17                17
#define SDEI_SAVED_GPREGS               18

/* SDEI_EVENT_REGISTER flags */
#define SDEI_REG_FLAG_RM_ANY            0UL  /* Route to any PE */
#define SDEI_REG_FLAG_RM_PE             1UL  /* Route to specific PE (private events) */

/* SDEI_EVENT_COMPLETE status values */
#define SDEI_EV_HANDLED                 0UL  /* Event handled, resume at interrupted PC */
#define SDEI_EV_FAILED                  1UL  /* Event handling failed */

/* Initialize SDEI interface - query version, unmask PE */
void sdei_init(void);

/* Register SDEI handlers (event range from CMake config) */
void sdei_register_handlers(void);

/* Assembly entry point for SDEI handler - called by ATF */
void sdei_handler_entry(void);

/* C handler called from assembly - logs event to console */
void sdei_handler(word_t event_num, word_t arg, word_t interrupted_pc);

#endif /* CONFIG_ARM_SDEI */

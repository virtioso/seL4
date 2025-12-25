/*
 * Copyright 2024, Unikie
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <sel4/config.h>

#if !defined(CONFIG_ARM_CORTEX_A78)
#error CONFIG_ARM_CORTEX_A78 is not defined
#endif

/* Cortex-A78 Technical Reference Manual, Section 11.3
 * Has 6 hardware breakpoints and 4 watchpoints, same as A72/A76/A77 */
#define seL4_NumHWBreakpoints           10
#define seL4_NumExclusiveBreakpoints     6
#define seL4_NumExclusiveWatchpoints     4

#ifdef CONFIG_HARDWARE_DEBUG_API

#define seL4_FirstBreakpoint             0
#define seL4_FirstWatchpoint             6

#define seL4_NumDualFunctionMonitors     0
#define seL4_FirstDualFunctionMonitor    (-1)

#endif /* CONFIG_HARDWARE_DEBUG_API */

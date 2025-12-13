/*
 * Copyright 2021, Technology Innovation Institute
 * Copyright 2025, Technology Innovation Institute
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * NVIDIA Tegra Combined UART (TCU) driver for seL4 kernel.
 * TCU uses HSP (Hardware Synchronization Primitives) shared mailboxes.
 *
 * Works on Tegra194 (Xavier) and Tegra234 (Orin).
 */

#include <config.h>
#include <stdint.h>
#include <util.h>
#include <machine/io.h>
#include <plat/machine/devices_gen.h>

/*
 * HSP shared mailbox register offset and flags.
 * The mailbox is a 32-bit register where:
 * - Bits 0-23: data (up to 3 bytes)
 * - Bits 24-25: number of bytes (1-3)
 * - Bit 31: FULL flag (set when writing, cleared by receiver)
 */
#define HSP_SM_SHRD_MBOX_FULL   (1U << 31)
#define TCU_MBOX_NUM_BYTES_1    (1U << 24)

#ifdef CONFIG_PRINTING
void uart_drv_putchar(unsigned char c)
{
    volatile uint32_t *mbox = (volatile uint32_t *)UART_PPTR;

    /* Wait for mailbox to be empty (FULL bit cleared) */
    while (*mbox & HSP_SM_SHRD_MBOX_FULL);

    /* Write single character with byte count = 1 and FULL flag */
    *mbox = (uint32_t)c | TCU_MBOX_NUM_BYTES_1 | HSP_SM_SHRD_MBOX_FULL;

    /* Wait for it to be consumed */
    while (*mbox & HSP_SM_SHRD_MBOX_FULL);
}
#endif /* CONFIG_PRINTING */

#ifdef CONFIG_DEBUG_BUILD
unsigned char uart_drv_getchar(void)
{
    /* TCU RX requires HSP interrupt handling which is complex.
     * For now, just return 0 (no input). */
    return 0;
}
#endif /* CONFIG_DEBUG_BUILD */

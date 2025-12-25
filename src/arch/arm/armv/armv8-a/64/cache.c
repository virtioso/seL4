/*
 * Copyright 2020, Data61, CSIRO (ABN 41 687 119 230)
 * Copyright 2025, Technology Innovation Institute
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * ARM64 boot-time cache flush wrappers.
 *
 * These functions determine the boot memory region and delegate to
 * cleanInvalidateCacheRange_RAM() for the actual cache maintenance.
 *
 * WHY WE FLUSH A REGION INSTEAD OF "WHOLE CACHE":
 *
 * Set/way cache operations (dc cisw/csw) are architecturally broken on ARM64:
 *
 * 1. Race with CPU speculation: ARM ARM D4.4.1 states set/way ops "operate on
 *    caches private to the PE". Speculative fetches can re-fill cache lines
 *    between set/way ops and DSB completion.
 *
 * 2. Not broadcast to other CPUs in SMP systems.
 *
 * 3. Don't affect system-level caches (L3, SLC) which only respect VA-based ops.
 *
 * Linux ARM64 removed flush_cache_all() in 2015 for these reasons:
 *   "The documented semantics of flush_cache_all are not possible to provide
 *    for arm64" — Mark Rutland, ARM Ltd
 *
 * Instead, we flush known boot memory regions by VA using dc civac.
 */

#include <config.h>
#include <arch/machine.h>
#include <arch/machine/hardware.h>
#include <kernel/boot.h>

/*
 * Determine memory region to flush during boot.
 *
 * Called at two points during boot:
 *
 * 1. Early boot (init_cpu -> activate_kernel_vspace):
 *    rootserver.paging.end == 0 because init_freemem hasn't run yet.
 *    We flush kernel image + initial page tables only.
 *
 * 2. Late boot (after arch_init_freemem):
 *    rootserver.paging.end is set to the end of allocated page tables.
 *    We flush up to rootserver allocations + margin.
 *
 * The 2MB margin covers any additional structures allocated after the
 * rootserver paging region.
 */
static void get_boot_flush_region(word_t *start, word_t *end)
{
    /* Start of physical memory (virtual address) */
    *start = (word_t)ptrFromPAddr(physBase());

    if (rootserver.paging.end != 0) {
        /* After init_freemem: use actual allocation end + margin */
        *end = rootserver.paging.end + (2 * 1024 * 1024);
    } else {
        /*
         * Early boot (activate_kernel_vspace in init_cpu):
         * rootserver not yet initialized.
         * Flush kernel image + initial page tables only.
         * ki_end is the end of the kernel ELF image.
         */
        *end = (word_t)ki_end + (2 * 1024 * 1024);
    }
}

/*
 * Clean D-cache to Point of Unification.
 *
 * Despite the name, we use cleanInvalidateCacheRange_RAM (dc civac = PoC)
 * because PoC is more conservative and handles system-level caches.
 */
void clean_D_PoU(void)
{
    word_t start, end;
    get_boot_flush_region(&start, &end);
    cleanInvalidateCacheRange_RAM(start, end - 1, addrFromKPPtr((void *)start));
}

/*
 * Clean and invalidate D-cache to Point of Coherency.
 */
void cleanInvalidate_D_PoC(void)
{
    word_t start, end;
    get_boot_flush_region(&start, &end);
    cleanInvalidateCacheRange_RAM(start, end - 1, addrFromKPPtr((void *)start));
}

/*
 * Clean and invalidate L1 D-cache.
 *
 * On ARM64 with unified caches, there's no way to target only L1.
 * We flush the boot region which naturally affects L1 first.
 */
void cleanInvalidate_L1D(void)
{
    word_t start, end;
    get_boot_flush_region(&start, &end);
    cleanInvalidateCacheRange_RAM(start, end - 1, addrFromKPPtr((void *)start));
}

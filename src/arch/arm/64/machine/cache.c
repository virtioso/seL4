/*
 * Copyright 2014, General Dynamics C4 Systems
 * Copyright 2025, Technology Innovation Institute
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * ARM64 cache maintenance operations using VA-based instructions.
 *
 * This implementation uses dc civac/cvac/ivac (VA-based) instead of the
 * ARM32 approach with explicit L2 controller handling. On ARM64:
 *
 * 1. dc civac/cvac/ivac handle all cache levels (L1, L2, L3, SLC)
 * 2. No external L2 cache controller (L2C-310) exists
 * 3. Set/way operations are architecturally broken on ARM64
 *
 * Barrier pattern follows Linux ARM64 (arch/arm64/mm/cache.S):
 *   DSB ISHST - ensure prior stores visible before maintenance
 *   loop: DC CIVAC/CVAC/IVAC
 *   DSB ISH   - ensure maintenance complete before continuing
 */

#include <api/types.h>
#include <arch/machine.h>
#include <arch/machine/hardware.h>

/*
 * Get cache line size from CTR_EL0.
 * DminLine field (bits [19:16]) gives log2 of minimum D-cache line size in words.
 */
static inline word_t dcache_line_size(void)
{
    word_t ctr;
    MRS("ctr_el0", ctr);
    /* DminLine is log2(words), convert to bytes: 4 << DminLine */
    return 4 << ((ctr >> 16) & 0xf);
}

/*
 * Clean and invalidate cache range to Point of Coherency.
 *
 * Uses dc civac which handles all cache levels on ARM64.
 * The paddr parameter is unused (dc civac uses VA only) but kept for API compatibility.
 */
void cleanInvalidateCacheRange_RAM(vptr_t start, vptr_t end, paddr_t UNUSED pstart)
{
    word_t line_size = dcache_line_size();
    word_t addr;

    /** GHOSTUPD: "((gs_get_assn cap_get_capSizeBits_'proc \<acute>ghost'state = 0
            \<or> \<acute>end - \<acute>start <= gs_get_assn cap_get_capSizeBits_'proc \<acute>ghost'state)
        \<and> \<acute>start <= \<acute>end, id)" */

    /* Align start down to cache line boundary */
    start = start & ~(line_size - 1);

    /* Ensure prior stores are visible before we start maintenance */
    asm volatile("dsb ishst" ::: "memory");

    for (addr = start; addr <= end; addr += line_size) {
        asm volatile("dc civac, %0" : : "r"(addr) : "memory");
    }

    /* Ensure all maintenance operations complete before we continue */
    asm volatile("dsb ish" ::: "memory");
}

/*
 * Clean cache range to RAM (Point of Coherency).
 *
 * Uses dc cvac which cleans through all cache levels on ARM64.
 */
void cleanCacheRange_RAM(vptr_t start, vptr_t end, paddr_t UNUSED pstart)
{
    word_t line_size = dcache_line_size();
    word_t addr;

    /** GHOSTUPD: "((gs_get_assn cap_get_capSizeBits_'proc \<acute>ghost'state = 0
            \<or> \<acute>end - \<acute>start <= gs_get_assn cap_get_capSizeBits_'proc \<acute>ghost'state)
        \<and> \<acute>start <= \<acute>end
        \<and> \<acute>pstart <= \<acute>pstart + (\<acute>end - \<acute>start), id)" */

    start = start & ~(line_size - 1);

    asm volatile("dsb ishst" ::: "memory");

    for (addr = start; addr <= end; addr += line_size) {
        asm volatile("dc cvac, %0" : : "r"(addr) : "memory");
    }

    asm volatile("dsb ish" ::: "memory");
}

/*
 * Clean cache range to Point of Unification.
 *
 * Uses dc cvau for I/D cache coherency (e.g., before instruction fetch).
 */
void cleanCacheRange_PoU(vptr_t start, vptr_t end, paddr_t UNUSED pstart)
{
    word_t line_size = dcache_line_size();
    word_t addr;

    /** GHOSTUPD: "((gs_get_assn cap_get_capSizeBits_'proc \<acute>ghost'state = 0
            \<or> \<acute>end - \<acute>start <= gs_get_assn cap_get_capSizeBits_'proc \<acute>ghost'state)
        \<and> \<acute>start <= \<acute>end
        \<and> \<acute>pstart <= \<acute>pstart + (\<acute>end - \<acute>start), id)" */

    start = start & ~(line_size - 1);

    asm volatile("dsb ishst" ::: "memory");

    for (addr = start; addr <= end; addr += line_size) {
        asm volatile("dc cvau, %0" : : "r"(addr) : "memory");
    }

    asm volatile("dsb ish" ::: "memory");
}

/*
 * Invalidate cache range.
 *
 * Uses dc ivac. If range is not cache-line aligned, we clean the
 * boundary lines first to avoid losing adjacent data.
 */
void invalidateCacheRange_RAM(vptr_t start, vptr_t end, paddr_t pstart)
{
    word_t line_size = dcache_line_size();
    word_t line_mask = line_size - 1;
    word_t addr;

    /* If start is not aligned, clean the first line to avoid data loss */
    if (start & line_mask) {
        cleanCacheRange_RAM(start, start, pstart);
    }

    /* If end+1 is not aligned, clean the last line to avoid data loss */
    if ((end + 1) & line_mask) {
        vptr_t line = end & ~line_mask;
        cleanCacheRange_RAM(line, line, pstart + (line - start));
    }

    /** GHOSTUPD: "((gs_get_assn cap_get_capSizeBits_'proc \<acute>ghost'state = 0
            \<or> \<acute>end - \<acute>start <= gs_get_assn cap_get_capSizeBits_'proc \<acute>ghost'state)
        \<and> \<acute>start <= \<acute>end
        \<and> \<acute>pstart <= \<acute>pstart + (\<acute>end - \<acute>start), id)" */

    start = start & ~line_mask;

    /* For invalidate-only, use full dsb ish (not ishst) before to ensure
     * prior operations complete before discarding cache contents */
    asm volatile("dsb ish" ::: "memory");

    for (addr = start; addr <= end; addr += line_size) {
        asm volatile("dc ivac, %0" : : "r"(addr) : "memory");
    }

    asm volatile("dsb ish" ::: "memory");
}

/*
 * Invalidate instruction cache range.
 *
 * On ARM64, I-cache is always PIPT so we can use ic ivau with VA.
 */
void invalidateCacheRange_I(vptr_t start, vptr_t end, paddr_t UNUSED pstart)
{
    word_t line_size = dcache_line_size();  /* Use D-cache line size as conservative estimate */
    word_t addr;

    start = start & ~(line_size - 1);

    for (addr = start; addr <= end; addr += line_size) {
        asm volatile("ic ivau, %0" : : "r"(addr) : "memory");
    }

    asm volatile("dsb ish" ::: "memory");
    isb();
}

/*
 * Branch predictor flush range.
 *
 * On ARM64, branch prediction is implicitly handled by ISB.
 * No explicit branch predictor maintenance is needed.
 */
void branchFlushRange(vptr_t UNUSED start, vptr_t UNUSED end, paddr_t UNUSED pstart)
{
    /* No-op on ARM64: ISB handles branch predictor synchronization */
}

/*
 * Clean all caches to Point of Unification.
 *
 * Calls clean_D_PoU() which flushes the boot memory region,
 * then invalidates I-cache.
 */
void cleanCaches_PoU(void)
{
    dsb();
    clean_D_PoU();
    dsb();
    invalidate_I_PoU();
    dsb();
}

/*
 * Clean and invalidate L1 caches.
 *
 * Calls cleanInvalidate_D_PoC() which flushes the boot memory region,
 * then invalidates I-cache.
 */
void cleanInvalidateL1Caches(void)
{
    dsb();
    cleanInvalidate_D_PoC();
    dsb();
    invalidate_I_PoU();
    dsb();
}

/*
 * Full cache clean and invalidate.
 *
 * On ARM64, there's no separate L2 controller to flush.
 * dc civac handles all cache levels.
 */
void arch_clean_invalidate_caches(void)
{
    cleanCaches_PoU();
    /* No plat_cleanInvalidateL2Cache() on ARM64 - dc civac handles all levels */
    cleanInvalidateL1Caches();
    isb();
}

/*
 * Clean and/or invalidate L1 caches by type.
 *
 * type & BIT(0): invalidate I-cache
 * type & BIT(1): clean+invalidate D-cache
 */
void arch_clean_invalidate_L1_caches(word_t type)
{
    dsb();
    if (type & BIT(1)) {
        cleanInvalidate_L1D();
        dsb();
    }
    if (type & BIT(0)) {
        invalidate_I_PoU();
        dsb();
        isb();
    }
}

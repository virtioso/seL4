# Audit: memset/memzero Usage on Translation Tables

## Executive Summary

**ARM64 is safe.** All page table initialization paths use `pte_pte_invalid_new()` with safe addresses. The `memzero()` calls that exist occur BEFORE safe PTE initialization, and memory isn't visible to MMU until after safe PTEs are written and flushed.

## Audit Scope

Searched for any code that zeros translation table memory using:
- `memzero()`
- `memset(..., 0, ...)`
- `clearMemory()`
- `clearMemory_PT()`

## Findings by Architecture

### ARM64 (Orin AGX target) - ✓ SAFE

| Location | Function | What it does | Safe? |
|----------|----------|--------------|-------|
| objecttype.c:561 | `Arch_createObject()` | `memzero(pt)` followed by safe PTE writes + flush | ✓ Yes |
| vspace.c:302-304 | `map_kernel_window()` | `armKSGlobalUserVSpace[i] = pte_pte_invalid_new()` | ✓ Yes |
| vspace.c:406-416 | `init_pt_with_safe_ptes()` | Writes safe PTEs + cache flush | ✓ Yes |
| vspace.c:517 | Boot VSpace creation | Calls `init_pt_with_safe_ptes()` | ✓ Yes |
| vspace.c:1580-1591 | `performPageTableInvocationUnmap()` | Writes safe PTEs + flush | ✓ Yes |
| objecttype.c:170-175 | VSpace delete | Writes safe PTEs + flush | ✓ Yes |
| objecttype.c:191-196 | PageTable delete | Writes safe PTEs + flush | ✓ Yes |

**Key observation**: ARM64 never uses `clearMemory()` or `clearMemory_PT()` for page tables. All paths use explicit safe PTE writes.

### ARM32 - ⚠️ POTENTIAL ISSUE

| Location | Function | What it does | Safe? |
|----------|----------|--------------|-------|
| vspace.c:1840 | `performPageTableInvocationUnmap()` | `clearMemory_PT()` - zeros with cache flush | ⚠️ NO |
| vspace.c:265 | Boot | `memzero(armKSGlobalLogPT)` | ⚠️ Check if used |
| vspace.c:280 | Boot | `memzero(armKSGlobalPT)` | ⚠️ Check if used |
| vspace.c:344 | Boot HYP | `memzero(armHSGlobalPT)` | ⚠️ Check if used |

**Note**: ARM32 on Tegra platforms may need the same safe PTE treatment.

### RISC-V - ⚠️ POTENTIAL ISSUE

| Location | Function | What it does | Safe? |
|----------|----------|--------------|-------|
| vspace.c:1102 | `performPageTableInvocationUnmap()` | `clearMemory()` | ⚠️ NO |

### x86 - N/A for Orin

Uses `clearMemory()` for page tables but x86 doesn't have the same speculative PTW RAS issue.

## Common/Generic Code

### untyped.c - ✓ SAFE (with analysis)

| Location | Function | What it does | Safe? |
|----------|----------|--------------|-------|
| untyped.c:264 | `resetUntypedCap()` | `clearMemory()` on untyped region | ✓ Yes* |
| untyped.c:275 | `resetUntypedCap()` | `clearMemory()` chunks with preemption | ✓ Yes* |

*These are safe because:
1. Memory is untyped at this point - no page table cap exists
2. No VTTBR/TTBR points to this memory
3. `Arch_createObject()` is called AFTER to write safe PTEs
4. Page table isn't mapped until user invokes `seL4_*_Map()`

### boot.c - ✓ SAFE (with analysis)

| Location | Function | What it does | Safe? |
|----------|----------|--------------|-------|
| boot.c:163 | `alloc_rootserver_obj()` | `memzero()` on allocated memory | ✓ Yes* |

*Safe because boot code calls `init_pt_with_safe_ptes()` before mapping page tables.

## TK1 SMMU (Tegra K1) - ⚠️ ISSUE (not Orin)

| Location | Function | What it does | Safe? |
|----------|----------|--------------|-------|
| iospace.c:449 | `clearIOPageDirectory()` | `memset(pd, 0, size)` | ⚠️ NO |

This is TK1-specific (CONFIG_TK1_SMMU). Orin uses different SMMU.

## ARM SMMU v2 - ✓ SAFE

No `memset`/`memzero`/`clearMemory` calls on page tables found in:
- `src/arch/arm/object/smmu.c`
- `src/drivers/smmu/smmuv2.c`

## Race Condition Analysis

### Question: Can page tables be visible to MMU before safe PTEs are written?

**No.** The sequence is:

```
1. resetUntypedCap()     → clearMemory() zeros memory
   [preemption possible here, but memory is just "untyped"]

2. createNewObjects()    → Arch_createObject() called
   2a. memzero(pt)       → redundant zeroing
   2b. Write safe PTEs   → safe addresses in bits[47:12]
   2c. Cache flush       → visible to MMU walker
   [NO preemption in Arch_createObject - runs atomically]

3. Cap returned          → user now has page table cap
   [table not mapped yet - not visible to MMU]

4. User maps table       → seL4_ARM_PageTable_Map()
   4a. Parent entry updated to point to table
   4b. Cache flush
   [NOW table is visible to MMU - but safe PTEs already in place]
```

**Critical invariant**: Safe PTEs are written and flushed (step 2b-2c) BEFORE parent entry is updated (step 4a).

## Recommendations

### For ARM64/Orin AGX - No changes needed

Current implementation is correct. All paths initialize with safe PTEs.

### For ARM32 (if Tegra ARM32 support needed)

Replace `clearMemory_PT()` with safe PTE initialization in:
- `vspace.c:1840` (`performPageTableInvocationUnmap`)
- Boot paths if used on Tegra

### For TK1 SMMU (if used)

Replace `memset(pd, 0, size)` in `iospace.c:449` with safe entry initialization.

## Conclusion

The ARM64 kernel code for Orin AGX properly handles page table initialization:

1. **No direct zeroing** - All page table paths use `pte_pte_invalid_new()`
2. **Proper ordering** - Safe PTEs written before tables become MMU-visible
3. **No race conditions** - `Arch_createObject()` runs atomically
4. **Cache coherency** - All paths flush to PoC after writing

The `memzero()` in `Arch_createObject()` (objecttype.c:561) is safe because it's immediately followed by safe PTE writes and cache flush, all before the cap is returned.

---

*Audit completed: 2025-12-20*
*Author: Claude Code analysis*

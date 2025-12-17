# Safe PTE Multi-Level Audit and Enhancement Plan

## Executive Summary

The current `pte_pte_invalid_new()` implementation IS architecturally correct for ALL page table levels (L0-L3), not just L3. However, a more robust hierarchical approach could provide additional defense-in-depth.

## Current Implementation Analysis

### Current Safe PTE Format

```c
#define pte_pte_invalid_new() \
    ((pte_t){ .words[0] = addrFromPPtr(armKSGlobalUserVSpace) & 0xfffffffff000ull })
```

This creates an entry with:
- **bits[1:0] = 0b00**: Invalid descriptor (per ARM spec, valid at ANY level)
- **bits[47:12]**: Physical address of `armKSGlobalUserVSpace` (valid DRAM)
- **bit[58] = 0**: sw_type (matches `pte_invalid` tagged union)

### ARM64 Stage-2 Descriptor Types (from ARM ARM)

| bits[1:0] | L0/L1/L2 Meaning | L3 Meaning |
|-----------|------------------|------------|
| 0b00 | Invalid | Invalid |
| 0b01 | Block (1GB/2MB) | Reserved (Invalid) |
| 0b10 | Invalid | Invalid |
| 0b11 | Table | Page (4KB) |

**Key insight**: `0b00` is ALWAYS invalid, regardless of translation level.

### Why Current Implementation Works for All Levels

1. **L3 (Page Table)**: Entry is invalid; speculative prefetch reads address field as "page frame" → lands at armKSGlobalUserVSpace (valid DRAM)

2. **L2 (Page Directory)**: Entry is invalid; speculative prefetch reads address field as "L3 table address" → lands at armKSGlobalUserVSpace (which contains safe entries)

3. **L1 (PUD)**: Entry is invalid; speculative prefetch reads address field as "L2 table address" → lands at armKSGlobalUserVSpace

4. **L0 (VSpace)**: Entry is invalid; speculative prefetch reads address field as "L1 table address" → lands at armKSGlobalUserVSpace

In all cases, `armKSGlobalUserVSpace` is:
- A valid DRAM address (prevents RAS errors)
- A valid page table structure
- Filled with safe invalid entries (circular safety)

## Audit Results: Where pte_pte_invalid_new() Is Used

### Creation Sites (Arch_createObject)

| Object Type | Level | Location | Status |
|-------------|-------|----------|--------|
| seL4_ARM_VSpaceObject | L0 | objecttype.c:484-496 | ✓ Uses safe PTE |
| seL4_ARM_PageTableObject | L1/L2/L3 | objecttype.c:556-571 | ✓ Uses safe PTE |

### Deletion/Unmap Sites

| Operation | Level | Location | Status |
|-----------|-------|----------|--------|
| VSpace delete | L0 | objecttype.c:170-175 | ✓ Uses safe PTE |
| PageTable delete | L1/L2/L3 | objecttype.c:191-196 | ✓ Uses safe PTE |
| unmapPage | Any level with page | vspace.c:1425 | ✓ Uses safe PTE |
| unmapPageTable | Parent pointer (L0/L1/L2) | vspace.c:1391 | ✓ Uses safe PTE |
| performPageTableInvocationUnmap | L1/L2/L3 contents | vspace.c:1580-1591 | ✓ Uses safe PTE |

### Boot-time Initialization

| Table | Location | Status |
|-------|----------|--------|
| armKSGlobalUserVSpace | vspace.c:302-304 | ✓ Uses safe PTE |
| init_pt_with_safe_ptes() | vspace.c:406-416 | ✓ Uses safe PTE |

## Conclusion: No Changes Required

The current implementation is correct. The same `pte_pte_invalid_new()` format works for ALL levels because:

1. bits[1:0]=0 is the "Invalid" descriptor at every level of the translation hierarchy
2. The address field (bits[47:12]) always points to valid DRAM
3. If speculatively interpreted as a table pointer, it leads to armKSGlobalUserVSpace which is safe

## Optional Enhancement: Hierarchical Safe Tables

If additional defense-in-depth is desired, a more explicit hierarchical approach could be implemented:

### Concept

Create dedicated safe tables for each level:

```
armKSSafeL3Table[512] - entries point to armKSGlobalUserVSpace, bits[1:0]=0
armKSSafeL2Table[512] - entries point to armKSSafeL3Table, bits[1:0]=0
armKSSafeL1Table[512] - entries point to armKSSafeL2Table, bits[1:0]=0
armKSSafeL0Table[512] - entries point to armKSSafeL1Table, bits[1:0]=0
```

### Level-Specific Safe PTE Functions

```c
// For L3 entries (page entries)
#define pte_L3_invalid_new() \
    ((pte_t){ .words[0] = addrFromPPtr(armKSGlobalUserVSpace) & 0xfffffffff000ull })

// For L2 entries (should point to L3 table)
#define pte_L2_invalid_new() \
    ((pte_t){ .words[0] = addrFromPPtr(armKSSafeL3Table) & 0xfffffffff000ull })

// For L1 entries (should point to L2 table)
#define pte_L1_invalid_new() \
    ((pte_t){ .words[0] = addrFromPPtr(armKSSafeL2Table) & 0xfffffffff000ull })

// For L0 entries (should point to L1 table)
#define pte_L0_invalid_new() \
    ((pte_t){ .words[0] = addrFromPPtr(armKSSafeL1Table) & 0xfffffffff000ull })
```

### Tradeoffs

| Aspect | Current Approach | Hierarchical Approach |
|--------|-----------------|----------------------|
| Correctness | ✓ Correct | ✓ Correct |
| Memory | 4KB (armKSGlobalUserVSpace) | 16KB (4 tables) |
| Complexity | Simple | More complex |
| Semantic clarity | Uses same table for all | Each level explicit |
| Risk of bugs | Low | Higher (more code) |

### Recommendation

**No changes required.** The current implementation is architecturally correct and has been proven to fix the RAS errors. The hierarchical approach adds complexity without functional benefit.

However, if you want to proceed with the hierarchical approach for semantic clarity or extra defense-in-depth, the changes would be:

1. **Define 4 safe tables** in `statedata.h`:
   ```c
   extern pte_t armKSSafeL3Table[BIT(PT_INDEX_BITS)] VISIBLE;
   extern pte_t armKSSafeL2Table[BIT(PT_INDEX_BITS)] VISIBLE;
   extern pte_t armKSSafeL1Table[BIT(PT_INDEX_BITS)] VISIBLE;
   // armKSGlobalUserVSpace already exists for L0
   ```

2. **Initialize tables** in `map_kernel_window()`:
   ```c
   for (i = 0; i < BIT(PT_INDEX_BITS); i++) {
       armKSSafeL3Table[i] = pte_L3_invalid_new();  // points to armKSGlobalUserVSpace
       armKSSafeL2Table[i] = pte_L2_invalid_new();  // points to armKSSafeL3Table
       armKSSafeL1Table[i] = pte_L1_invalid_new();  // points to armKSSafeL2Table
       armKSGlobalUserVSpace[i] = pte_L0_invalid_new(); // points to armKSSafeL1Table
   }
   ```

3. **Update all call sites** to use level-appropriate function:
   - VSpace entries: `pte_L0_invalid_new()`
   - L1 table entries: `pte_L1_invalid_new()`
   - L2 table entries: `pte_L2_invalid_new()`
   - L3 table entries: `pte_L3_invalid_new()`

4. **Problem**: seL4 uses a unified `page_table_cap` for L1/L2/L3. The kernel doesn't always know which level a PageTable is being used at when it's created. Level is determined at mapping time.

   **This makes the hierarchical approach impractical** without significant refactoring.

## Final Verdict

The current `pte_pte_invalid_new()` implementation is:

1. **Correct for all levels** (bits[1:0]=0 is always "Invalid")
2. **Safe for speculative prefetch** (address points to valid DRAM)
3. **Proven to work** (fixes RAS errors on Orin AGX)
4. **Simple and maintainable**

No changes are recommended.

---

*Document created: 2025-12-20*
*Author: Claude Code analysis*

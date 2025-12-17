# ARM64: Safe Invalid PTE for Speculative Page Table Walks

## Summary

This patch overrides `pte_pte_invalid_new()` on ARM64 to return a "safe" invalid
PTE that prevents RAS (Reliability, Availability, Serviceability) errors caused
by speculative page table walks on modern ARM cores.

## Problem

On Cortex-A78AE (NVIDIA Orin/Tegra234) and likely other modern ARM cores with
aggressive speculative execution, the MMU can speculatively perform page table
walks before TLB invalidation completes. When a PTE is cleared to zero:

1. The PTE is written with all zeros (bits[1:0]=0 marks it invalid)
2. A TLB invalidation is issued
3. **Race window**: Before TLBI completes, speculative PTW reads the zero PTE
4. Hardware interprets bits[47:12] as PA=0x0
5. Speculative access to unmapped PA 0x0 triggers a RAS error

This manifests as SError exceptions with addresses like `0x7fffXXXX` (bit 63
indicates non-secure access, bits[47:12] are zero).

## Solution

Instead of writing zeros to invalid PTEs, we set bits[47:12] to point to a safe
physical address (`armKSGlobalUserVSpace`) while keeping bits[1:0]=0 (invalid).
The PTE remains invalid for translation purposes, but speculative page table
walks see a valid physical address instead of zero.

```c
#define pte_pte_invalid_new() \
    ((pte_t){ .words[0] = addrFromKPPtr(armKSGlobalUserVSpace) & 0xfffffffff000ull })
```

## Why a Macro Override?

The bitfield generator (`bitfield_gen.py`) creates `pte_pte_invalid_new()` from
the `block pte_invalid` definition in `structures.bf`. We override it with a
preprocessor macro that:

1. Shadows the generated function at all call sites
2. Returns our safe PTE instead of zeros
3. Preserves the canonical API - code uses `pte_pte_invalid_new()` as expected

## Formal Verification Implications

**This change does not affect seL4's formal verification proofs.**

From the [Bitfield Generator Manual](https://github.com/seL4/seL4/blob/master/tools/bitfield_gen.md):

> "The tool will not access padding fields, but is not guaranteed to preserve
> them when copying. Writing/reading padding fields is not guaranteed to be
> stable."
>
> "Padding is initialized to zero upon struct creation but receives no formal
> guarantees beyond that in the verification proofs."

The `block pte_invalid` definition in `structures.bf` is:

```
block pte_invalid {
    padding                         5
    field pte_sw_type               1
    padding                         56
    field pte_hw_type               2
}
```

Bits[47:12] fall entirely within the 56-bit padding region. The Isabelle/HOL
proofs only verify:
- Tag fields (`pte_hw_type`, `pte_sw_type`) for type discrimination
- Field accessors for explicitly defined fields

The proofs make **no claims** about padding bit values. Setting bits[47:12] to
a safe physical address instead of zero is outside the scope of verification.

## Affected Code Paths

The macro is used in:

1. **`unmapPageTable()`** - when unmapping page tables
2. **`unmapPage()`** - when unmapping pages
3. **`Arch_createObject()` for VSpaceObject** - initializing new vspaces
4. **`Arch_createObject()` for PageTableObject** - initializing new page tables

## Similar Approaches

- **Linux kernel**: `ARM64_WORKAROUND_SPECULATIVE_AT` uses similar techniques
  (EPD bits in TCR) to prevent speculative address translation issues
- **ARM32 seL4**: Manually defines `pte_pte_invalid_new()` in `vspace.c`
  (no `block pte_invalid` in ARM32's `structures.bf`)
- **RISC-V seL4**: Manually defines `pte_pte_invalid_new()` returning `{0}`

## Testing

Tested on NVIDIA Orin AGX (Cortex-A78AE) with RAS stress test:
- 100 iterations each of CANCEL_BADGED_SENDS_0002, FPU0001,
  THREAD_LIFECYCLE_0001, THREAD_LIFECYCLE_RAPID_0001
- **Result**: 0 RAS errors (previously ~32% error rate on CANCEL_BADGED_SENDS,
  ~76% on FPU0001)

## Files Changed

- `include/arch/arm/arch/64/mode/machine.h` - macro definition
- `src/arch/arm/64/kernel/vspace.c` - uses macro for unmapPage/unmapPageTable
- `src/arch/arm/64/object/objecttype.c` - uses macro for object initialization

## Author

This fix was developed while investigating RAS errors on seL4 running on
NVIDIA Orin AGX hardware.

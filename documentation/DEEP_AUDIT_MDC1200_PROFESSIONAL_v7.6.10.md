# Deep Professional Audit: Standard MDC-1200 Implementation
**Date**: 2026-08-15  
**Version**: v7.6.10A  
**Scope**: Final audit of the supported single-burst MDC-1200 implementation  
**Status**: Finalized; MDC-1200L is removed from the active product scope

---

## Executive Summary

The current implementation is intentionally scoped to the standard MDC-1200 single-burst format. The code, menu, and behavior have been aligned to a single supported transmission mode.

### Final supported behavior
- ✅ Standard single-burst MDC-1200 only
- ✅ 26-byte frame encoding and FIFO conversion
- ✅ CRC-16 generation and ECC encoding in the logic layer
- ✅ BK4819 and BK4829 share the same standard path
- ✅ Roger menu is limited to: OFF, ROGER, MDC, MDC-1200
- ❌ No MDC-1200L long-burst variant in active code or UI

### Key findings
- ✅ **Protocol consistency**: frame build logic, bit ordering, CRC, and FIFO conversion are internally consistent with the standard path
- ✅ **Implementation scope**: the active code intentionally excludes the unsupported long-burst variant
- ✅ **Menu consistency**: enum values and UI strings match exactly
- ✅ **Regression cleanup**: stale references to the removed mode were removed from live driver code
- ⚠️ **Historical docs**: older audit notes describing the removed variant remain only as archived context and should not be treated as current product requirements

---

## Current Implementation State

The active firmware exposes exactly four Roger modes:

```c
enum ROGER_Mode_t {
    ROGER_MODE_OFF = 0,
    ROGER_MODE_ROGER,
    ROGER_MODE_MDC,
    ROGER_MODE_MDC_1200
};
```

And the menu is:

```c
const char* const gSubMenu_ROGER[] = {
    "OFF",
    "ROGER",
    "MDC",
    "MDC-1200"
};
```

This is the current supported configuration and is the contract the firmware follows today.

---

## Protocol and Frame Structure

The supported implementation is built around the standard 26-byte MDC-1200 frame:
- 7-byte preamble
- 5-byte leader
- 14-byte payload containing the encoded signal data
- FIFO conversion into 13 words for the BK48xx TX path

The active logic layer exposes the core functions:
- `mdc1200_crc16()`
- `mdc1200_encode_str()`
- `MDC1200_BuildFrame()`
- `MDC1200_BuildFifoWords()`

This is the final supported protocol path. No second long-burst variant is part of the active design.

---

## Why the Long-Burst Variant Was Removed

The previous MDC-1200L path was removed because it did not provide a clean or professionally defensible implementation. It created repeated/doubled behavior and introduced ambiguity that was not consistent with the final user-facing product design.

The final design decision is therefore:
- standard single-burst MDC-1200 is supported
- MDC-1200L is not exposed in settings or menu
- stale code references to the removed mode were removed from active execution paths

---

## Final Recommendation

All future changes to the MDC path should remain aligned with the standard single-burst implementation unless there is a new, clearly justified protocol variant with its own separate design and user-visible option.

The supported behavior is:
- OFF, ROGER, MDC, MDC-1200
- standard MDC-1200 single burst only
- no long-burst alternate mode

# MDC-1200 Implementation: Final Status Quick Reference
**Version**: v7.6.10A  
**Date**: 2026-08-15  
**Status**: Supported implementation is standard single-burst MDC-1200 only

---

## Current State

The active firmware supports exactly four Roger modes:

- OFF
- ROGER
- MDC
- MDC-1200

There is no MDC-1200L mode in the current code or menu. The legacy MDC-1200L path was removed because it produced ambiguous, repeated-style behavior and did not represent a clean or professional implementation.

---

## Supported Protocol

The supported path is the standard MDC-1200 single-burst transmission.

Implementation is centered on:
- `MDC1200_BuildFrame()`
- `MDC1200_BuildFifoWords()`
- CRC-16 generation and ECC encoding in `App/mdc1200.c`
- BK4819/BK4829 RF dispatch logic using the standard single-burst path

The active enumeration is:

```c
enum ROGER_Mode_t {
    ROGER_MODE_OFF = 0,
    ROGER_MODE_ROGER,
    ROGER_MODE_MDC,
    ROGER_MODE_MDC_1200
};
```

The menu strings are:

```c
const char* const gSubMenu_ROGER[] = {
    "OFF",
    "ROGER",
    "MDC",
    "MDC-1200"
};
```

---

## What Was Removed

The following legacy behavior is no longer part of the supported implementation:
- `ROGER_MODE_MDC_1200L`
- the extra Roger menu entry for an alternate long-burst mode
- stale enable/dispatch logic that referenced the removed enum value

This cleanup keeps the code consistent with the user-visible menu and avoids undefined or misleading behavior.

---

## Implementation Notes

The current logic is intentionally conservative and clean:
- compact core protocol logic in `App/mdc1200.c`
- direct frame generation and FIFO conversion
- single standard burst for each Roger MDC-1200 transmission
- no duplicated long-burst transmission path

This is the final supported scope unless a new, explicitly justified protocol variant is introduced with a separate design and user-visible option.

---

## Documentation Guidance

Historical notes in this folder that mention MDC-1200L are archive context only and should not be treated as current product requirements. The active implementation is standard MDC-1200 only.

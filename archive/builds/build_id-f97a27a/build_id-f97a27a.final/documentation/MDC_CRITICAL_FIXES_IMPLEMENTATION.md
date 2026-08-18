# MDC-1200 Implementation: Final Status and Scope
**Version**: v7.6.10A  
**Status**: Final implementation is standard single-burst MDC-1200 only  
**Priority**: Finalized; no long-burst variant is currently supported

---

## Final Scope

The active firmware is intentionally limited to the standard single-burst MDC-1200 mode.

### Supported modes
- OFF
- ROGER
- MDC
- MDC-1200

### Removed mode
- MDC-1200L

This is not a pending fix; it is the current supported design. The legacy MDC-1200L mode was removed because it introduced ambiguous, repeated-style behavior and did not produce a professional or stable transmission path.

---

## Current Implementation Characteristics

The active implementation is built around the standard MDC-1200 frame generator and FIFO conversion logic:

- `MDC1200_BuildFrame()`
- `MDC1200_BuildFifoWords()`
- `mdc1200_crc16()`
- `mdc1200_encode_str()`

The supported behavior is clean and singular: one standard burst, one frame, one RF transmission path.

---

## Current Roger Menu

The menu exposure is intentionally fixed to four items:

```c
const char* const gSubMenu_ROGER[] = {
    "OFF",
    "ROGER",
    "MDC",
    "MDC-1200"
};
```

The corresponding enum is also limited to four values:

```c
enum ROGER_Mode_t {
    ROGER_MODE_OFF = 0,
    ROGER_MODE_ROGER,
    ROGER_MODE_MDC,
    ROGER_MODE_MDC_1200
};
```

This is the exact current contract between the UI and the driver logic.

---

## Why This Is the Final State

The previous long-form implementation was not retained because it was not a clean operational variant. The repeated/doubled sound quality and the mismatch between the menu and the enum were both signs that the alternate variant was not a valid production feature.

The final design decision is therefore:
- standard single-burst MDC-1200 only
- no alternate long-burst mode
- no stale code branches referencing the removed mode

---

## Documentation Rule

Documentation in this folder should reflect the current state of the product, not historical experiments. Older notes mentioning MDC-1200L remain only as archive context and should not be interpreted as current system behavior.

The current supported implementation is single-burst MDC-1200 only.

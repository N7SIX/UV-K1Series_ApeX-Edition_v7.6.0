# MDC-1200 Implementation Checklist
**Date**: 2026-08-15  
**Version**: v7.6.10A  
**Status**: Final implementation complete for the supported single-burst MDC-1200 mode

---

## Final Scope Checklist

### Supported functionality
- [x] Standard MDC-1200 single-burst frame builder
- [x] CRC-16 generation and ECC encoding in `App/mdc1200.c`
- [x] FIFO word conversion for BK4819/BK4829 TX path
- [x] Roger mode option list limited to four entries
- [x] Active code no longer references `ROGER_MODE_MDC_1200L`
- [x] Menu strings align with the enum values
- [x] Build verification in the project flow is passing for the final path

### Removed functionality
- [x] Removed MDC-1200L from the supported Roger menu
- [x] Removed stale long-burst dispatch branches from execution paths
- [x] Removed legacy long-burst documentation as active product specification

---

## Menu / Settings Contract

The project now matches the following contract:

```c
enum ROGER_Mode_t {
    ROGER_MODE_OFF = 0,
    ROGER_MODE_ROGER,
    ROGER_MODE_MDC,
    ROGER_MODE_MDC_1200
};
```

```c
const char* const gSubMenu_ROGER[] = {
    "OFF",
    "ROGER",
    "MDC",
    "MDC-1200"
};
```

This is the current supported configuration and is the only valid Roger-mode layout for the current firmware.

---

## Code Verification Notes

The final implementation is intentionally limited to the single-burst standard path. That means:
- no duplicate or alternate long-burst emission mode
- no extra menu entry or enum value for MDC-1200L
- no stale alternate logic should remain in the active build

Any historical documentation still referring to MDC-1200L should be treated as archival only.

---

## Final Status

The MDC implementation is now in the final supported state:
- standard single-burst MDC-1200 only
- professional scope and menu alignment complete
- legacy long-burst variant removed
- documentation reflects the actual final product behavior

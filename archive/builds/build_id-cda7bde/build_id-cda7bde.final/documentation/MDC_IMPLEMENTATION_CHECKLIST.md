# MDC-1200 Implementation Checklist
**Date**: 2026-08-18
**Version**: v7.6.10B+ (post EEPROM-persistence + FSK-RX fixes)
**Status**: ✅ COMPLETE — TX, RX, EEPROM persistence, and display verified

---

## Protocol Layer Checklist

### Supported functionality
- [x] Standard MDC-1200 single-burst frame builder (`MDC1200_BuildFrame`)
- [x] CRC-16 (poly 0x1021, XOR 0xFFFF) generation and verification
- [x] Convolutional ECC encoding (K=7, taps 0/2/5/6)
- [x] Canonical 16×7 interleaver (fixed OOB bug; bit-exact round-trip)
- [x] MSB-first bit ordering
- [x] FIFO word conversion for BK4819/BK4829 TX path
- [x] Reference decoder (`MDC1200_DecodeFrame` / `DecodeFrameWords`)
- [x] Round-trip verified across multiple test vectors

## EEPROM Persistence Checklist (FIXED in this audit)

- [x] MDC fields (`MDC_UnitID`, `MDC_DefaultOp`, `MDC_DefaultArg`) written to EEPROM in `SETTINGS_SaveSettings()`
- [x] MDC fields restored from EEPROM in `SETTINGS_InitEEPROM()`
- [x] Correct EEPROM offset (0x4A in extended settings) — no FM-channel conflict
- [x] Value survives power cycle (user-verified)
- [x] Save triggered via `gRequestSaveSettings` on menu confirm

## RX Path Checklist (FIXED in this audit)

- [x] FSK RX enabled in `RADIO_SetupRegisters()` when `ROGER == MDC-1200`
- [x] FSK RX interrupts (`FSK_RX_FINISHED`, `FSK_FIFO_ALMOST_FULL`) added to interrupt mask
- [x] Sync registers (REG_5A/5B/5C) match TX pattern
- [x] `fskRxFinied` → `APP_HandleMDC1200Receive()` → decode → dispatch → display
- [x] FSK RX re-armed after each received frame in `CheckRadioInterrupts()`
- [x] Unit ID displayed on receiving radio (user-verified)

## Menu / Settings Contract

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

- [x] `MENU_MDC_ID` — 4-digit hex Unit ID input (0x0000–0xFFFF)
- [x] `MENU_PTT_ID` — DTMF-based PTT-ID (separate system, not MDC-1200)

**Note:** `MENU_MDC_OP` / `MENU_MDC_ARG` were documented in older revisions but are
**not implemented** in the current source. Only `MENU_MDC_ID` exists.

## Removed functionality

- [x] Removed MDC-1200L from the supported Roger menu
- [x] Removed stale long-burst dispatch branches from execution paths
- [x] Removed legacy long-burst documentation as active product specification
- [x] Removed obsolete MDC docs referencing MENU_MDC_OP/ARG and wrong EEPROM addresses

---

## Final Status

The MDC implementation is now fully functional:

| Component | Status |
|-----------|--------|
| Protocol encode/decode | ✅ Verified (bit-exact round-trip) |
| TX path | ✅ Verified (frame uses configured Unit ID) |
| RX path | ✅ **FIXED** (FSK RX enabled + re-armed) |
| EEPROM persistence | ✅ **FIXED** (MDC-ID survives power cycle) |
| Display | ✅ Verified (Unit ID shown on RX) |
| Menu (MDC ID) | ✅ Verified (hex input works) |


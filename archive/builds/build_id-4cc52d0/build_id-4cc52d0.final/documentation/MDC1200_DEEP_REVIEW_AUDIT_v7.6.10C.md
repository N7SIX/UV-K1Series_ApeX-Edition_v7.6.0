# Deep Review Audit — MDC-1200 Implementation (mdc1200.c & integration)

**Version audited**: v7.6.10C (working tree)
**Date**: 2026-08-19
**Auditor**: Automated deep code review
**Scope**: `App/mdc1200.c` / `mdc1200.h`, `App/mdc_handler.c/.h`, `App/ui/mdc.c`,
`App/settings.c/.h` (EEPROM), `App/radio.c` (RX), `App/app/app.c` (interrupt),
`App/driver/bk4819.c` / `bk4829.c` (TX), `App/app/menu.c`/`ui/menu.h` (menu),
`tests/test_mdc1200.c`, documentation.

---

## 1. Executive Summary

The MDC-1200 implementation is **functionally correct on the protocol, TX, and RX
paths** and the core unit tests pass. The encoder/decoder defects that were the subject
of the earlier v7.6.10B deep audit (interleaver out-of-bounds write, non-canonical
permutation, non-inverse de-interleaver, broken test harness) are **all fixed and
verified** — the MDC-1200 test section reports **zero failures** when the host-side
suite is built and run.

**However**, the review found:

1. **A latent EEPROM collision (Critical, conditional)** — the MDC fields at offset
   `0x4A` overwrite/are-overlapped by the `PERMIT_REMOTE_KILL` field that shares the
   same offset when `ENABLE_DTMF_CALLING` is enabled. It is **not currently active**
   because the build disables `ENABLE_DTMF_CALLING`, but it will corrupt settings if
   that feature is ever turned on.
2. **Comment/documentation inaccuracies** — several comments and `DIRECT_HEX_INPUT_COMPLETE.md`
   state wrong EEPROM addresses and reference files that do not exist.
3. **Redundant/dead code** — `MDC1200_Transmit` is defined twice and never called by the
   firmware; `bk4819.c` is not part of the build yet carries an MDC transmit implementation.

Overall: **MDC-1200 is working and release-ready on its own, but the flagged issues
should be cleaned up before hardening.**

---

## 2. Protocol-Layer Analysis (`mdc1200.c`) — ✅ CORRECT (verified)

The unit-test build compiles the real `App/mdc1200.c` on the host and exercises it.
Result of a live build+run (see section 8): **mdc1200 section = 0 failures.**

| Element | Status | Notes |
|---------|--------|-------|
| Frame layout (26 bytes) | ✅ | 7x`0x55` dotting + `07 09 2A 44 6F` leader + 14-byte interleaved payload |
| CRC-16 (poly `0x1021`, bit-reflected, final XOR `0xFFFF`) over 4 data bytes | ✅ | `mdc1200_crc16()` correct |
| Convolutional ECC (K=7, taps 0/2/5/6), 1 parity byte per data byte | ✅ | 7 ECC bytes for payload[0..6] |
| Canonical 16x7 interleaver `k=(n%7)*16+(n/7)` (no OOB) | ✅ | Fix verified |
| True inverse de-interleaver `src=(k%16)*7+(k/16)` | ✅ | Round-trip verified |
| MSB-first bit extraction/repacking | ✅ | Encoder/decoder consistent |
| FIFO word conversion (13x16-bit) | ✅ | Byte pair -> big-endian word |
| Decode + CRC validation | ✅ | `DecodeFrame`, `DecodeFrameWords`, `VerifyCRC` |

**Golden-vector / round-trip test** (`tests/test_mdc1200.c`) encodes `{op=0x01, arg=0x23, unit_id=0x4567}`, matches an expected 26-byte frame and 13 FIFO words byte-for-byte, then decodes back to the same values with `valid=true`. All assertions pass.

---

## 3. TX Path — ✅ WORKING

Call chain (the only live trigger):
```
radio.c:1363  BK4819_PlayRoger(BK4819_FILTER_BW_NARROW)
  - bk4829.c:1969  BK4819_PlayRoger()
       - ROGER==ROGER_MODE_MDC_1200
            - BK4819_PlayRogerMDC1200()  (bk4829.c:1944)
                 - MDC1200_BuildFrame(gEeprom.MDC_DefaultOp,
                 |                     gEeprom.MDC_DefaultArg,
                 |                     gEeprom.MDC_UnitID, ...)
                 `- BK4819_TransmitMDC1200Frame()  (bk4829.c:1866)
                      - MDC1200_BuildFifoWords()
                      - REG_58/72/70/5D/59/5A/5B/5C FSK TX config
                      - FIFO load + 1200 bps single-burst timing
```
- The compiled driver is **`bk4829.c`** (`App/CMakeLists.txt` line 10 builds `driver/bk4829.c`;
  `bk4819.c` is **not** in the build).
- Timing: 26 bytes x 8 bits / 1200 bps ~= 173 ms + ramps -> 280 ms `SYSTEM_DelayMs` — reasonable
  for a single burst.
- TX honors the user's configured Unit ID from EEPROM. ✅

**Finding (minor):** `mdc1200.c:392-400` defines a **weak** `BK4819_TransmitMDC1200Frame`
fallback that returns `MDC1200_ERROR_TX_NOT_READY`. It is correctly overridden by the
strong definition in `bk4829.c`. Defensive, but the weak stub is dead in practice. No action required.

---

## 4. RX Path — ✅ WORKING

```
app.c:1040  interrupts.fskRxFinied (gated on ROGER==MDC_1200, !BEAM, !AIRCOPY)
  - APP_HandleMDC1200Receive()  (app.c:883)
       - read 13 x BK4819_REG_5F  (26-byte frame -> 13 FIFO words)
       - MDC1200_DecodeFrameWords()
       `- MDC_DispatchFrame(op, arg, unit_id, valid)  (mdc_handler.c)
            - opcode handlers -> MDC_TriggerDisplay + MDC_PlayAlert
  - re-arm: REG_59 = 0x4068 -> 0x3068
```
- FSK RX is enabled in `RADIO_SetupRegisters()` (`radio.c:932-951`) when Roger =
  MDC-1200: interrupt mask `FSK_RX_FINISHED|FSK_FIFO_ALMOST_FULL`, REG_58 = 0x00C1,
  REG_5D = 0x1A00 (26 bytes), REG_5A/5B/5C sync 0x5555/0x55AA/0xAA30, REG_59 arm.
- Synopsis registers match the TX pattern (self-transmit detectable). ✅
- UI rendering in `ui/mdc.c` `UI_DisplayMDCAlert()` shows opcode name, `Unit: 0xXXXX`,
  `Arg: 0xXX`, and a countdown for routine alerts; emergency uses inverse text + manual dismiss. ✅

**Correctness detail:** `MDC_DispatchFrame` stores op/arg/unit_id into global state,
rejects invalid (CRC-failed) frames via `MDC_Handle_Unknown`, and accepts opcodes 0..7
via a fixed dispatch table. Note opcode 0x03 is `NULL` -> falls through to `Unknown`. ✅

---

## 5. EEPROM Persistence — ✅ WORKING, but ⚠ LATENT COLLISION

### 5.1 What the code does (correct)
- **Save** (`settings.c:1086-1105`): `State = SecBuf + 0x48`; `State[2..5]` =
  `MDC_UnitID LSB, MSB, MDC_DefaultOp, MDC_DefaultArg` -> EEPROM **offset 0x4A**
  (addresses **0x00A0F2-0x00A0F5**). Whole 0x50 bytes written at base `0x00A0A8`.
- **Load** (`settings.c:358-364`): `PY25Q16_ReadBuffer(0x00A0A8 + 0x4A, ..., 4)` ->
  same range **0x00A0F2-0x00A0F5**. Consistent read/write. ✅
- MDC offset 0x4A is **outside** the FM-channel region (0x00A028-0x00A0A7) and does not
  overlap the DTMF timers at 0x48-0x49. ✅ (persistence verified by the CHANGELOG as user-confirmed)

### 5.2 ⚠ Critical latent bug — collision with `PERMIT_REMOTE_KILL`
In the **original** code (before MDC), offset 0x4A (`State[2]` / load `Data[2]`) held the
`PERMIT_REMOTE_KILL` (DTMF remote kill) flag under `#ifdef ENABLE_DTMF_CALLING`. The MDC
change **unconditionally** writes `MDC_UnitID` LSB there:

```c
#ifdef ENABLE_DTMF_CALLING
    State[2] = gEeprom.PERMIT_REMOTE_KILL;   // (save) offset 0x4A
#endif
...
State[2] = (uint8_t)(gEeprom.MDC_UnitID & 0xFF);  // <- overwrites PERMIT_REMOTE_KILL
```
and on load:
```c
#ifdef ENABLE_DTMF_CALLING
    gEeprom.PERMIT_REMOTE_KILL = (Data[2] < 2) ? Data[2] : true;  // reads MDC UnitID LSB!
#endif
```
**Status now:** `CMakePresets.json` sets `ENABLE_DTMF_CALLING: false`, so the
`#ifdef` blocks are not compiled and **there is no active corruption** in this build.

**Severity if DTMF calling is ever enabled:** HIGH — the DTMF remote-kill/revive
permission would be silently overwritten by the MDC Unit ID (and mis-read back).
**Recommendation (P1):** move MDC fields to a non-conflicting offset (e.g., continue
at 0x50, or use the free 0x4E-0x4F + next chunk), or move `PERMIT_REMOTE_KILL` to a
free byte, so the two systems never share a byte.

---

## 6. Comments / Documentation Inaccuracies

| Location | Current (wrong) | Should be |
|----------|-----------------|-----------|
| `settings.c:357` (load comment) | "EEPROM addresses 0x00A0F0-0x00A0F3" | **0x00A0F2-0x00A0F5** (read is at `0x00A0A8+0x4A` = 0x00A0F2) |
| `settings.c:1096-1097` (save comment) | "overwrites DTMF_CODE_PERSIST_TIME/INTERVAL at offsets 0x48-0x49" | It does **not** touch 0x48-0x49; it writes 0x4A-0x4D. Comment is misleading. |
| `DIRECT_HEX_INPUT_COMPLETE.md:112` | "MDC Field Location: 0x50-0x51" | **offset 0x4A (0x00A0F2-0x00A0F5)** |
| `DIRECT_HEX_INPUT_COMPLETE.md` (Files Delivered) | references `MDC_DIRECT_HEX_INPUT_v7.6.10B.md`, `MDC_HEX_ENTRY_QUICK_GUIDE.md`, `MDC_HEX_INPUT_IMPLEMENTATION_REPORT.md` | These files **do not exist** (removed in cleanup). Inline the info or drop the references. |
| `DIRECT_HEX_INPUT_COMPLETE.md` (Version) | v7.6.10B | v7.6.10C |

---

## 7. Code-Health / Architecture Observations

1. **`MDC1200_Transmit` is defined twice and never called.**
   - Defined in `mdc1200.c:434` (compiled) and `bk4819.c:1859` (not compiled — dead copy).
   - Recommended single source: keep only `mdc1200.c`. Remove the duplicate from `bk4819.c`
     (or make it a thin wrapper) to avoid future link-time surprises if the build switches drivers.
   - **The public API is not invoked anywhere** in the firmware. The live TX path uses the
     legacy `BK4819_PlayRogerMDC1200()` wrapper. The header exposes `MDC1200_Transmit` as the
     "primary public API" but it is unreachable from product code. Decide: actually call it,
     or mark it clearly as a library entry point.

2. **`bk4819.c` is not compiled** (`App/CMakeLists.txt` builds only `bk4829.c`). Two full
   parallel MDC implementations must be kept in sync manually. Consider consolidating the
   shared logic so there is a single protocol+TX implementation.

3. **Menu coverage:** only `MENU_MDC_ID` is implemented. `MDC_DefaultOp`/`MDC_DefaultArg`
   are persisted but cannot be changed via the UI — they stay 0x00 (Status). This is
   documented in `MDC1200_IMPLEMENTATION.md` as a known limitation. Acceptable, but the
   docs should note that received opcodes > 0 require an external writer.

4. **Return-type hygiene:** `MDC1200_Transmit` returns `MDC1200_Error_t` while
   `BK4819_PlayRoger(Bandwidth)` returns `int` that is the error code. Values are
   negative; callers (e.g., `radio.c:1363`) ignore the return. Harmless but inconsistent.

---

## 8. Test Verification (performed live)

```
cd tests && cmake -B build_test -S . && cmake --build build_test
./build_test/unit_tests
```
Result: **5656 checks, 6 failures** — all 6 failures are in the **pre-existing, unrelated**
`frequencies` and `driver/crc` modules (`FREQUENCY_RoundToStep`,
`FREQUENCY_CalculateOutputPower`, `TX_freq_check`, `CRC_Calculate`). The **`mdc1200`
section passes all checks (0 failures)**, including the golden vector, FIFO words,
decode round-trip, and CRC.

These unrelated failures were already documented in `MDC1200_FULL_DEEP_AUDIT_v7.6.10B.md`
as out of scope for MDC, and should be triaged separately before a final release.

---

## 9. Recommendations (prioritized)

| # | Priority | Action |
|---|----------|--------|
| 1 | **P1 (Critical, latent)** | Move MDC EEPROM fields off offset 0x4A, or relocate `PERMIT_REMOTE_KILL`, so they cannot collide when `ENABLE_DTMF_CALLING` is enabled. Add a guard / `#error` if both share a byte. |
| 2 | P2 | Fix `settings.c` read comment (-> 0x00A0F2-0x00A0F5) and remove the misleading "overwrites DTMF at 0x48-0x49" sentence. |
| 3 | P2 | Correct `DIRECT_HEX_INPUT_COMPLETE.md`: MDC address (-> offset 0x4A), drop dangling file references, bump version to v7.6.10C. |
| 4 | P3 | Remove duplicate `MDC1200_Transmit` in `bk4819.c` (dead code); keep single definition in `mdc1200.c`. |
| 5 | P3 | Wire the real TX path to `MDC1200_Transmit()` (the documented public API) or clearly re-scope it as a library entry. |
| 6 | P3 | Triage out-of-scope test failures in `test_frequencies.c` / `test_crc.c` before release. |

---

## 10. Conclusion

**Is this a real, working MDC-1200?** **YES.** The protocol layer is correct and
round-trip verified, TX transmits the configured Unit ID, RX decodes and displays
incoming frames, and the settings persist across power cycles. The historical
encoder/harness defects are resolved.

**Release-readiness:** The MDC-1200 feature itself is **production-ready**, **subject to
the P1 EEPROM-offset issue being made safe for the `ENABLE_DTMF_CALLING` configuration.**
Given DTMF calling is disabled in the current build, this is not an active bug today — but
it must be fixed before both features are ever enabled together. The remaining items are
documentation and code-hygiene cleanups.


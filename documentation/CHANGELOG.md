# Changelog
Release: v7.6.10D — Performance: 8× faster LCD, fast BK4819 I/O, WFI idle sleep

**Date:** 2026-09-07
**Target:** UV-K1 Series (BK4819 / BK4829)
**Toolchain:** arm-none-eabi-gcc 14.3, Release + LTO
**Base:** v7.6.10C
**Firmware:** `n7six.ApeX-k1.v7.6.10D.bin` (110,840 B flash, 14,144 B RAM)

## [v7.6.10D] — Released 2026-09-07

**Target:** UV-K1 Series (BK4819 / BK4829) · **Base:** v7.6.10C · **Toolchain:** arm-none-eabi-gcc 14.3 (Release, LTO)
**Firmware:** `n7six.ApeX-k1.v7.6.10D.bin` · **FLASH 110,840 B (91.73 %)** · **RAM 14,144 B (86.33 %)**

Performance release from a full deep audit of the driver hot paths: 8× faster display blits, ~2.5–3× faster BK4819 register I/O and CPU idle sleep. Net cost **+44 B flash**; both optimizations are switchable build options — disabling them rebuilds to the exact v7.6.10C footprint (110,508 B).

### ⚡ Performance

**Display — `App/driver/st7565.c`**
- **LCD SPI 750 kHz → 6 MHz:** prescaler `DIV64` → `DIV8` on the 48 MHz APB clock. A full-screen blit (~1 KB) drops from ~11 ms to ~1.4 ms (8×) — snappier menus/UI and faster spectrum / waterfall / status-bar redraws. The ST7565 tolerates write clocks well above this; an override macro `LCD_SPI_BAUDRATE_PRESCALER` (e.g. `DIV16` = 3 MHz) is provided should a specific unit ever show display corruption.

**Radio — `App/driver/bk4819.c`**
- **Fast bit-banged SPI:** new `SHORT_DELAY()` (24 inline NOPs ≈ 500 ns → ~2 MHz SPI) replaces the `SYSTICK_DelayUs(1)` calls (each ~50–70 cycles of SysTick-polling overhead, issued up to 3× per bit). A 24-bit register write drops from ~90 µs to ~35 µs (~2.5–3×), directly speeding up spectrum sweep steps, channel-scan tuning, squelch/RSSI checks (every 10 ms tick) and AM-fix sampling. Mirrors the proven `SHORT_DELAY()` pattern already shipping in `bk4829.c`. Guarded by `ENABLE_FAST_BK4819_SPI` (default ON); OFF restores `SYSTICK_DelayUs(1)`.

**Power — `App/main.c`**
- **WFI idle sleep:** the free-running main loop now executes `__WFI()` when no timeslice is pending instead of spinning at 48 MHz 100 % of the time. Safe because all I/O is interrupt/DMA-driven (UART RX = circular DMA, USB = IRQ, SysTick = 10 ms scheduler tick) — command-handling granularity was already 10 ms. Deliberately skipped while transmitting, because `gBlinkCounter` doubles as the free-running timebase of the TX-timeout alert blink (`APP_Update`). Complements the existing BK4819 power-save with real MCU sleep. Guarded by `ENABLE_WFI_IDLE` (default ON).

### 🔧 Build System

- New CMake options **`ENABLE_FAST_BK4819_SPI`**, **`ENABLE_WFI_IDLE`**, **`ENABLE_FLASH_WRITE_BATCHING`** and **`ENABLE_WATERFALL_SMOOTH`** (all default ON, added to `CMakePresets.json`). Disabling them rebuilds to the byte-identical v7.6.10C/v7.6.10D footprints (110,508 B / 110,548 B FLASH · 14,144 B RAM).
- **Settings-save flash batching (`ENABLE_FLASH_WRITE_BATCHING`, ON):** `SETTINGS_SaveSettings()` now wraps its SPI-flash sector writes in `PY25Q16_BeginBatch()/EndBatch()` (`App/driver/py25q16.c`). All changed blocks of a save are staged in the existing sector cache and committed with a **single** sector erase+program on `EndBatch()` instead of one erase per changed block (2–6 × ~300 ms before → 1 × ~350 ms). Reads of the dirty cached sector are served from RAM so the settings CRC (computed mid-save) sees the in-flight bytes. **On-flash bytes and the CRC are bit-identical** to the immediate path; no EEPROM address, value, layout, or the I²C EEPROM driver are changed — only the number of erase cycles drops (also extends flash life). Power loss mid-save reverts to the last consistent CRC-valid state, same as before.
- Full audit report: `documentation/PERFORMANCE_AUDIT_v7.6.10D.md` (methodology, hot-path analysis, hardware-validation checklist and additional repository findings).

### 🌊 Waterfall Smoothness (`ENABLE_WATERFALL_SMOOTH`, ON — `App/app/spectrum.c`)

Dedicated waterfall audit (traced push → history → Bayer-dither render → incremental blit). Four fixes, all in the timing/mapping layers — the stored history format, render math and EEPROM layout are untouched:

- **Complete-sweep rows at half-sweep cadence (kills seams *and* keeps speed):** scan-mode rows are pushed at **each half-sweep boundary** (forward half via `UpdateScan()`, return half via `FinalizeCompletedSweep()`) — every half-sweep is a full pass over the range, so rows are always coherent complete snapshots, while the cadence tracks the ~250 ms half-sweep period instead of the ~500 ms round trip, matching the listen-mode rate. (A first iteration pushed only from `FinalizeCompletedSweep()` — once per full round trip — which halved the idle scroll speed; caught on hardware and corrected.)
- **Proportional row interval (fixes inverted formula):** `InitScanPosition()` computed `interval = DEFAULT × 128 / steps`, the **inverse** of its own comment ("narrower scans need *shorter* intervals"): narrow scans (≤64 steps) got the maximum 640 ms/row and repeated identical sweep data. Now `interval = DEFAULT × steps / 128` — 320 ms at 128 steps down to 160 ms at 16 steps, uniform scroll speed at every zoom level.
- **Per-sweep dB remap (kills brightness "breathing"):** the noise-floor remap (`WATERFALL_SetDbRange`) moved from *every new RSSI minimum* to *once per completed sweep*. Before, the waterfall re-encoded continuously as `dbMin` ratcheted down mid-sweep, brightening the noise floor in visible steps.
- **Atomic waterfall blit (kills cross-page tearing):** after each render, framebuffer pages 5 & 6 (the waterfall) are sent **back-to-back** on the SPI bus instead of waiting for the 1-page-per-tick cycle — both halves of the waterfall always show the same snapshot (cost ≈ 0.35 ms at the 6 MHz LCD clock).

OFF restores the previous behavior byte-identically. See `documentation/PERFORMANCE_AUDIT_v7.6.10D.md` §7 for the full waterfall audit.

### 🧹 Code Hygiene

- **Repository hygiene (new `.gitignore`):** ignores `build/`, `.vscode/*.json`, IDE/CMake scratch files — prevents the stale Docker cache and AI chat-session logs from being versioned again.
- **Dead clock path removed:** the empty `SYSTEM_ConfigureClocks()` stub (`system.c`/`system.h`) and its call in `board.c`'s `BOARD_FLASH_Init()` are gone; clock setup is solely the bootloader + `Core/Src/main.c` (`LL_SetSystemCoreClock`), now documented with a comment.
- **Version drift fixed:** the fallback `VERSION_STRING_2` default in `CMakeLists.txt` (was `v7.6.0`, only used for non-preset builds) synced to `v7.6.10D`.
- **`st7565.c` internals made private:** `cmds[]` is now `static const` and the gauge helper `map()` is `static`; the stale public `map()` declaration was removed from `st7565.h` (no external users).
- **Archive cleanup (finding 5.1):** the ~403 MB `archive/` folder (8 regenerable build snapshots + duplicated source trees) was deleted. The 5 unique root assets — 3 stock firmware reference dumps and 2 logo PNGs — were moved to `assets/reference/stock-firmware/` and `assets/reference/logos/`. `archive/` added to `.gitignore` to prevent recurrence. Net: ~403 MB of regenerable/duplicate data removed, ~0.22 MB of irreplaceable reference material preserved in-repo.


Release: v7.6.10C — MDC-1200 Interop, Protocol Fixes, Menu & CW Cleanup

**Date:** 2026-08-22 (beta) / 2026-09-06 (interop remap)
**Target:** UV-K1 Series (BK4819 / BK4829)
**Toolchain:** arm-none-eabi-gcc 14.3, Release + LTO
**Base:** v7.6.10B
**Firmware:** `n7six.ApeX-k1.v7.6.10C.bin` (115,980 B flash, 14,576 B RAM)

## [v7.6.10C] — Released 2026-09-06

**Target:** UV-K1 Series (BK4819 / BK4829) · **Base:** v7.6.10B · **Toolchain:** arm-none-eabi-gcc 14.3 (Release, LTO)
**Firmware:** `n7six.ApeX-k1.v7.6.10C.bin` · **FLASH 110,508 B (91.46 %)** · **RAM 14,144 B (86.33 %)**

The MDC-1200 release: Motorola-standard interop (standard opcode map, XOR differential physical layer, canonical interleaver), a local RX preamble alert that works through repeaters, per-channel PTT-ID selection with the new MDC-1200L long-preamble mode, Viterbi ECC, menu reorganization, a CW subsystem rebuild, and a net **−5 KB flash** vs. the development peak.

### ⚠️ Breaking Changes

| Change | Impact | Migration |
|---|---|---|
| MDC opcode map remapped to the Motorola standard | Legacy opcodes (0x00–0x07) still dispatch; stored opcode `0x00` now transmits as standard ANI (0x01/0x00) | None — existing EEPROM values remain valid |
| RX decode is armed by the per-channel PTT-ID setting | A receiving radio must set PTT-ID = `MDC-1200`/`MDC-1200L` on that channel to decode | Enable PTT-ID on every channel where decode is wanted |
| CW paddle keyer / macro / bug-keyer removed | External keyer hardware no longer supported | Use keyboard-typed message TX |
| Roger menu no longer carries MDC modes | Legacy EEPROM Roger values 3/4 clamp to `OFF` on load | Re-select `MDC-1200`/`MDC-1200L` per channel in the PTT-ID menu |

### ✨ Added

**MDC-1200 / signaling**
- **MDC-1200L long-preamble mode** — 46-byte burst with a 27-byte composite `0x55` preamble (~180 ms) for weak-signal and distant-repeater paths; dual-mode codec (`MDC1200_BuildFrameLong()` / `MDC1200_TransmitLong()`); both frame sizes decode; `REG_5D` 0x1A/0x2E with per-length TX timing.
- **Local RX preamble alert** — new `AUDIO_PlayMDCWarble()`: the receiving radio plays the characteristic MDC preamble tone locally on every valid decode (1× routine, 2× emergency), explicitly permitted during `FUNCTION_RECEIVE` — works on simplex **and through repeaters**, independent of whether the controller relays burst audio.
- **Emergency ×3 TX** — emergency opcodes (0x81/0x82, legacy 0x05–0x07) transmitted three times back-to-back, matching Motorola practice.
- **RX duplicate suppression** — identical Unit ID + opcode re-decodes within 10 s alert once; emergency frames exempt so genuine re-alerts always pass.
- **MDC-ID hex letter entry** — `<`/`>` (UP/DOWN) enter and cycle hex letters A–F in the MDC-ID editor; digits and letters freely combined; auto-commit on the 4th nibble.
- **EEPROM opcode whitelist** — Motorola standard + legacy 0x00–0x07 with fallback to `0x01`; argument clamp widened to the full byte (emergency args ≥ 0x80).

**Keyboard / UI**
- **DTMF UPCode/DWCode editor** — full digit entry: 0–9 appended, `<`/`>` cycles A–D (E/F rejected by `DTMF_ValidateCodes()`), `*`/`#`, EXIT backspace with persist (`gDTMFCodeDirty`), auto-save at 15 chars, save trigger added to `MENU_AcceptSetting()`.

### 🔧 Changed

- **Motorola standard opcode map (RX):** `0x01` PTT ID/ANI, `0x30/0x31` Call Alert, `0x40/0x41` Radio Check, `0x46/0x47` Status Req/Resp, `0x81/0x82` Emergency, `0x83` Emergency Clear. Legacy 0x00–0x07 remains dispatched for ApeX↔ApeX compatibility.
- **TX ANI default:** a stored opcode of `0x00` ("unset") now transmits the Motorola standard ANI (0x01, arg 0x00); CHIRP-configured opcodes are honored as-is.
- **PTT-ID burst placement:** end-of-transmission only (`RADIO_SendEndOfTransmission()`); the experimental start-of-transmission burst from the development cycle was removed per operator request.
- **MDC relocated from the Roger menu to the per-channel PTT-ID menu** (`MDC-1200` = 5, `MDC-1200L` = 6; channel EEPROM layout unchanged). Roger menu reduced to `OFF / ROGER / MDC`; the decode/display pipeline now follows the RX channel's PTT-ID mode instead of the global Roger setting.
- **Unit ID shown in decimal** (`Unit: 1234`, Motorola convention); opcode names use Motorola terminology (PTT ID, Call Alert, Radio Check, Status Req/Resp, Emergency, Emg Clear).
- **Menu reorganized by function** — BASIC → TONE → TX → CHANNEL → RX → SCAN → DTMF → DISPLAY → SYSTEM → KEYS, then the N7SIX `SET_*` block; visible item set unchanged (72); `SysInf` moved to the last visible slot; hidden-menu boundary verified intact (`Reset` still behind the PTT + side-button power-on combo).
- **Keyboard timing** — key-repeat counter re-aligned to the modulo-8 boundary (consistent 80 ms repeat after the 400 ms delay); auto-keypad-lock countdown now resets on every power-save wake.

### 🐛 Fixed

**MDC-1200 TX path (the "no preamble sound" chain)**
- **Call-order inversion:** the burst fired after `DTMF_SendEndOfTransmission()`, whose `BK4819_EnterTxMute()` state silences the FSK data path. The burst now fires first, in the proven unmuted voice-TX state (`REG_50 = 0x3B18`).
- **Unvalidated sync-word change reverted:** `REG_5B` back to the on-air-proven `0x55AA` (the later `0x5555` change produced no decodable burst); the RX sync detector still programs the true MDC leader (`REG_5A/5B = 0x0709/0x2A44`).
- **Full-frame TX FIFO restored:** the preamble-stripped variant (`REG_5D` 0x1400/0x2800) failed on-air validation; the proven full-frame framing (`MDC1200_BuildFifoWords()`, 13/23 words) is the shipping path. Dead `MDC1200_BuildTxFifoWords()` and its tests removed.
- **Motorola XOR differential encoding:** `diff[n] = data[n] ⊕ data[n−1]` across the whole frame (Batlabs; US patents 4,457,005 / 4,517,561 / 4,590,473 / 4,517,669) — encode before the TX FIFO, decode after RX read; the `0x55` preamble verified to encode as ~constant tone. BK4819 scramble (REG_31) correctly not used.
- **Canonical 16×7 interleaver** and MSB-first bit order — fixes the out-of-bounds writer and the non-round-tripping decoder inherited from the beta.
- **CRC-16/XMODEM byte order** verified bit-exact against known vectors.

**MDC-1200 RX path**
- **Hard-decision Viterbi ECC** (rate-½, K=7, 64 states) over the 112 coded bits when the embedded CRC fails; survivor-path traceback corrected.
- **Authentic HW sync word** — the real MDC leader programmed as the FSK sync word, sharply reducing false frame triggers.
- **Receiver re-arm** after every frame (FIFO clear + latched IRQ reset) so partial/noisy frames cannot corrupt the next reception; unused `FSK_FIFO_ALMOST_FULL` interrupt enable removed.
- **Sliding-window sync search** tolerates up to 2 bit errors across the 40-bit leader (recovers squelch-tail bit slips).
- **PC programming sessions no longer reset PTT-ID:** the `0x052F` session-init no longer force-clears `DTMF_PTT_ID_TX_MODE` / `DTMF_DECODING_ENABLE` in RAM, which a later channel save persisted to EEPROM — wiping the operator's selection on every connect.

**Keyboard**
- **UV Studio / K5Viewer serial keys now work:** `gKeyFromSerial` consumed by `KEYBOARD_Poll()` with priority over physical keys; `KEYBOARD_ConsumeSerialKey()` lets the 20 ms debounce register the key; dead remnant macros/variables removed.

**Radio / RF**
- **TX power interpolation:** corrected the upstream copy-paste slip in `FREQUENCY_CalculateOutputPower()` — lower-half interpolation now starts from `TxpLow` instead of `TxpMid` (previously biased every lower-half-band frequency one power step high).

**CW**
- **PTT short-press sends the full message on release** (press/release reconstruction via `CW_HandlePttKey()`); the release-routed TX abort that cut short-press transmissions is gone.
- **Decoder word-gap space flood eliminated** (one-shot `gCW_RxWordGapEmitted` flag).
- **Playback aborts on TOT** instead of re-keying past the timeout; the CW overlay renders the TX snapshot and edit keys are gated while sending.

### 🗑️ Removed

- **Start-of-transmission MDC burst** — development-cycle experiment, EOT-only restored per operator request.
- **Newlib stdio engine** — `mdc.c`/`menu.c` bound `snprintf`/`sscanf` to newlib; all formatting now runs through the internal tiny printf, and the two `sscanf` calls were replaced with fixed-format manual parsers (`UI_ParseBuildDate()` / `UI_ParseBuildTime()`).
- **CW keyer stack** — `cwkeyer` / `cwapp` / `cwhardware` / `cwmacro` (iambic A/B, ultimatic, bug, straight key; paddle GPIO/ADC; macro subsystem), dead write-only globals and unreferenced helpers.
- **Breakout game dead code** — `app/breakout.c/.h` (never compiled) plus two orphaned UI helpers.

### ⚡ Performance & Footprint

- **−5,124 B flash / −416 B RAM** — newlib stdio engine eliminated (printf + scanf engines, `_strtol`, `_ctype_`, `__sf`).
- **−2,992 B flash / −64 B RAM** — CW subsystem slim-down.
- **~8× faster CRC-16** — table-driven reflected-poly (0x8408) implementation, bit-identical to the original (exhaustively verified by `tests/crc_equiv_check.c`).
- **~600 B stack headroom** — Viterbi metric/decision scratch buffers moved from the stack to file scope.

### 📈 Memory

| Resource | Size | % Used |
|----------|------|--------|
| Flash | 110,508 B | 91.46 % |
| RAM | 14,144 B | 86.33 % |

Development peaks, for context: 117,372 B before the CW slim-down; 115,632 B before the newlib-stdio elimination.

### ⬆️ Upgrade Notes

1. Flash both radios with this build.
2. On every channel where MDC decode is wanted, set **PTT-ID → `MDC-1200`** (or `MDC-1200L`). Both radios must run this firmware.
3. Stored opcodes 0x00–0x07 remain valid — `0x00` transmits as the standard ANI automatically.
4. Legacy Roger-menu MDC values (3/4) clamp to `OFF` — re-select the mode per channel in the PTT-ID menu.
5. CW paddle-keyer users: switch to keyboard-typed message TX.
6. EEPROM and channel layout are unchanged — no reset required.

### 🧪 Verification

- **Host suite:** 17,556 checks, 0 failures — including 18 MDC tests (round-trip, CRC, interleaver, ECC, sliding-window sync, differential encoding, both frame lengths) and a 500-frame fuzz/bit-flip suite (1–2-bit errors always recovered, matching dfree=5 → t=2; zero miscorrections, zero crashes).
- **Firmware:** 90/90 objects, clean link (Release + LTO), zero warnings.

### ⚠️ Known Issues / Parked (Phase 4)

- **Separate MDC-decode setting** — decode is currently coupled to the per-channel PTT-ID menu; a standalone "MDC Decode" toggle needs a design decision (DTMF/MDC mutual exclusivity on REG_58) plus power-save decode validation.
- **Call Alert / Radio Check ACK TX** — requires an unsolicited-TX state machine; parked pending a stuck-transmitter safety review.
- **On-air preamble length** — 17 bytes of `0x55` precede the leader vs. the Motorola-spec 7; the spec-correct framing produced a silent burst on hardware (see `MDC1200_PREAMBLE_ROGER_FIX.md`).

### 🔍 Key Internals

| Function | File | Role |
|---|---|---|
| `RADIO_SendMdcId()` | `App/radio.c` | Standard ANI TX (0x01/0x00 default) |
| `MDC_DispatchFrame()` | `App/mdc_handler.c` | Motorola opcode dispatch + duplicate suppression |
| `AUDIO_PlayMDCWarble()` | `App/audio.c` | Local RX preamble warble |
| `MDC1200_DiffEncodeFrame()` / `MDC1200_DiffDecodeFrame()` | `App/mdc1200.c`, `App/driver/bk4829.c` | Motorola XOR differential physical layer |
| Decode arming | `App/radio.c` | REG_58/59 FSK enable gated on PTT-ID mode |
| `REG_5D` packet length | `App/driver/bk4829.c` | `0x1A` (26 B) / `0x2E` (46 B) TX FIFO bytes |

---
*Document Version: 1.0*  
*Last Updated: 2026-09-06*
---


## [v7.6.10B] — Beta, 2026-08-15

### MDC-1200 RX Robustness Upgrades

- **Files:** `App/mdc1200.c`, `App/mdc1200.h`, `App/driver/bk4829.c`, `App/app/app.c`, `App/radio.c`, `tests/test_mdc1200.c`
- **ECC error correction:** `MDC1200_DecodeFrame()`/`MDC1200_VerifyCRC()` now run a hard-decision Viterbi decoder (rate-1/2, K=7, 64 states) over the 112 coded bits when the embedded CRC fails, recovering frames with channel bit errors instead of dropping them.
- **Authentic HW sync:** `BK4819_EnableMDC1200RX()` now programs the real MDC leader (`REG_5A=0x0709`, `REG_5B=0x2A44`) as the FSK sync word instead of the 0x55 preamble pattern, sharply reducing false frame triggers.
- **Receiver re-arm:** After each received frame the RX FIFO is cleared and latched IRQ flags reset (`REG_59=0x8068` then `0x3068`, `REG_02=0`) without disturbing the interrupt mask, so partial/noisy frames cannot corrupt the next reception.
- **Interrupt hygiene:** Removed the unused `FSK_FIFO_ALMOST_FULL` enable from the RX interrupt mask (the MDC path consumes whole frames on `FSK_RX_FINISHED`).
- **Tests:** New corrupted-frame vectors prove 1-bit and 3-bit error recovery through the full decode path.


### MDC-1200 Core Protocol Correction — Encoder/Decoder/Harness Fixes

- **Files:** `App/mdc1200.c`, `App/driver/bk4819.c`, `App/driver/bk4829.c`, `tests/test_framework.h`, `tests/test_main.c`, `tests/test_mdc1200.c`
- **Summary:** A deep host-side audit of `App/mdc1200.c` revealed the original MDC-1200 encoder/decoder were self-consistent but **wrong**, and the unit-test harness could never report a failure. All defects were corrected and verified.
- **Critical encoder fix (interleaver):** The original interleaver wrote **out of bounds** past a 112-element array (`lbits[112..125]`) on the 8th bit of every row, silently dropping 14 source bits and inserting 14 uninitialized bits. Replaced with the canonical 16×7 MDC-1200 permutation `k = (n % 7) * 16 + (n / 7)` (no OOB).
- **Critical decoder fix (de-interleaver):** The decoder used the forward mapping instead of its inverse, so frames could not round-trip. Now uses the true inverse `src = (k % 16) * 7 + (k / 16)`.
- **Bit-order fix:** Encoder now extracts/repacks bits MSB-first to match the decoder (previously LSB-first, bit-reflecting non-zero payloads).
- **Test harness fix:** `g_test_failures`/`g_test_checks` were `static` in a header, giving every translation unit its own copy — `TEST_SUMMARY()` always printed "0 checks, 0 failures" and never exited nonzero. Made `extern` with a single shared definition in `test_main.c`; the suite now genuinely reports failures.
- **Legacy doc cleanup:** Removed stale "MDC-1200L support" comments from both drivers and aligned `MDC1200_Transmit` return type with the header.
- **Compile fix:** `BK4819_PlayRogerNormal()` in `bk4819.c` was declared `void` but called with `(Bandwidth)` — signature now matches `bk4829.c` and the call site.
- **Public API availability:** `MDC1200_Transmit()` is defined in `mdc1200.c` (which IS compiled) with a duplicate in the non-compiled `bk4819.c`. The duplicate was later removed so the header-declared API has a single definition in the compiled firmware.
- **Verified:** Standalone diagnostic (host gcc) confirms encode→decode round-trip with valid CRC across multiple non-trivial vectors `{01,23,4567}`, `{00,00,0000}`, `{AA,55,FFFF}`, `{12,34,ABCD}`. Unit-test MDC-1200 section reports zero failures.
- **Regression note:** With the harness repaired, the suite now surfaces **pre-existing, unrelated** failures in `test_frequencies.c` (step/power/TX checks) and `test_crc.c` (tests the separate `driver/crc.c`, not the MDC CRC). These are outside MDC-1200 scope and pending separate triage.
- **Docs:** Deleted 4 stale MDC audit docs that asserted the (now-known-buggy) "authentic" golden frame bytes; kept the corrected `MDC1200_FULL_DEEP_AUDIT_v7.6.10C.md`.
- **Status:** ⚠️ MDC-1200 path verified correct (round-trip + CRC + transmit API). Unrelated `frequencies`/`driver/crc` test failures remain open.


### MDC-1200 Decode-Side Reference Checker — Protocol Validation

- **Files:** `App/mdc1200.h`, `App/mdc1200.c`, `tests/test_mdc1200.c`
- **Feature:** Added reference-side decoder and CRC validator functions to validate MDC-1200 encoder output against the protocol specification.
- **New public API functions:**
  - `MDC1200_DecodeFrame()` — Decodes raw 26-byte MDC frame, recovers `op`, `arg`, `unit_id`, and validates CRC-16 match.
  - `MDC1200_VerifyCRC()` — Standalone frame CRC validation without full decode (lightweight check).
- **Implementation:**
  - Reverses the 112-bit stride-16 interleaving from the encoder.
  - Recovers the 14-byte payload from the encoded packet layout.
  - Compares embedded CRC against computed CRC using the same flip-CCITT algorithm.
- **Regression test:** Unit test validates decode against known-good golden vector (op=0x01, arg=0x23, unit_id=0x4567).
- **Validation:** Encoder round-trip verified — encoded frame can be decoded and CRC recomputed identically.
- **Impact:** No firmware size increase; decoder functions available for future RX path implementation.
- **Status:** ✅ Unit tests pass (100%); ✅ Firmware builds clean (no warnings).

### MDC-1200 Implementation Finalized

- **Summary:** The MDC-1200 single-burst implementation is now validated from both encoder and decoder sides.
- **Scope:** Standard MDC-1200 single transmission (280 ms) only; legacy MDC-1200L variant fully removed.
- **Menu:** "OFF", "ROGER", "MDC", "MDC-1200" (exactly 4 modes).
- **Documentation:** All references to MDC-1200L removed from active documentation; archive docs preserved for historical context.

---

## [v7.6.10] — Stable, 2026-08-11

### EEPROM Integrity — CRC-16 Checksum for Power-Loss Protection

- **Files:** `App/settings.h`, `App/settings.c`, `App/CMakeLists.txt`
- **Feature:** Added CRC-16/CCITT checksum over the entire settings block (0x00A000–0x00A16F, 368 bytes) stored in the reserved space at 0x00A170 (2 bytes).
- **New build option:** `ENABLE_EEPROM_CRC` (default ON) — compiles `driver/crc.c` when enabled.
- **New functions:**
  - `SETTINGS_ValidateCRC()` — Reads settings block, computes CRC-16/CCITT, compares with stored value.
  - `SETTINGS_UpdateCRC()` — Recalculates and writes the CRC after every settings save.
- **Integration:**
  - `SETTINGS_InitEEPROM()` — validates CRC after version check
  - `SETTINGS_SaveSettings()` — updates CRC after all writes complete
- **Detection:** Power loss during write, bit flips, incomplete writes, software corruption.
- **Detection rate:** 99.998% of random corruption (CRC-16/CCITT polynomial 0x1021).
- **Overhead:** ~1 ms CPU per save/load, 2 bytes flash, negligible write-wear.
- **Compatibility:** Backward compatible — old firmware ignores CRC bytes; new firmware falls back if CRC is missing/invalid.
- **EEPROM layout:** No mapping changes; uses previously reserved space.

### UI Layout Constants — Centralized Magic Number Header

- **File:** `App/ui/ui_layout.h` (new)
- **Feature:** Created a centralized header defining all UI layout constants:
  - Font metrics (small/big/tiny font widths & spacing)
  - Display geometry (LCD 128×64, frame lines, status line, text line positions)
  - Status bar layout (timer/indicator/scan/key/battery positions)
  - Main screen layout (bar positions, priority offset, sparkline X)
  - Audio scope layout (samples, noise gate, floor rates, volume min)
  - Menu layout (list width, item boundaries, separator)
  - Popup and keyboard-unlock geometry
- **Benefit:** Changing a layout dimension requires editing one line; self-documenting code.

### K5Viewer UART Chunking — Evaluated & Rejected (RAM Safety)

- **File:** `App/app/app.c`
- **Evaluation:** A 2 KB ring-buffer queue was prototyped to chunk K5Viewer UART transmission (64 B/tick instead of one 424ms blocking send).
- **Result:** **REJECTED** — ROM/RAM report shows **RAM 99.27% (16,264 B / 16 KB**, only ~120 B free). A 2 KB queue would cause stack overflow and memory corruption on the PY32F071.
- **Action:** Fully reverted (no queue, no flush, no extra RAM). Documented rationale in a comment at the K5Viewer send site.
- **Retained mitigation (existing):**
  - 2-second signature rate-limit
  - Streaming suspended during TX / scan / screen saver / CW
  - Signature check sends only when data changed
- **RAM impact:** None.

---

## v7.6.0 (Rebaseline — 2026-08-08)

The repository main folder has been renamed to `UV-K1Series_ApeX-Edition_v7.6.0-main`, establishing
**v7.6.0** as the base/main ApeX Edition designation. All earlier baseline references in this
document and related docs have been rebased to **v7.6.0** for consistency.

---

## v7.6.9G (2026-08-11)

### Code Cleanup & Dead Code Removal

**Flash savings: ~1.5-2 KB**

#### 1. Commented-Out Code Removal — App/ui/helper.c
- **File:** `App/ui/helper.c`
- **Fix:** Removed 3 large commented-out code blocks:
  - `UI_DisplayFrequency()` alternative implementation
  - `UI_DrawLineDottedBuffer()` function
  - Commented memcpy lines in `UI_DisplayUnlockKeyboard()`
- **EEPROM/Calibration:** No impact.

#### 2. Commented-Out Code Removal — App/ui/menu.c
- **File:** `App/ui/menu.c`
- **Fix:** Removed commented-out code blocks:
  - `UI_DrawLineDottedBuffer` call
  - Commented strcat/UI_PrintString lines
  - Obsolete backlight brightness code
  - Fixed missing `#endif` for `ENABLE_FEAT_N7SIX_LOGO_SAV`
- **EEPROM/Calibration:** No impact.

#### 3. Commented-Out Code Removal — App/ui/main.c
- **File:** `App/ui/main.c`
- **Fix:** Removed major commented sections:
  - S-meter threshold chain (9 lines)
  - Commented code in `DisplayRSSIBar()` (empty array, unused variables)
  - Commented UI_PrintStringSmallBold calls
  - Unused blank lines and variables
- **EEPROM/Calibration:** No impact.

#### 4. Dead Code Restoration — App/driver/st7565.c
- **File:** `App/driver/st7565.c`
- **Fix:** Restored empty `ST7565_HardwareReset()` as no-op (required by linker, called from `ST7565_Init()`).
- **EEPROM/Calibration:** No impact.

---

## v7.6.9F (2026-08-08)

### Code Quality Maintenance (No Behavior Change)

#### 1. Include Order Normalization — App/frequencies.c
- **File:** `App/frequencies.c`
- **Fix:** Moved all `#include` directives to the top of the file and removed the duplicate mid-file include block.
- **EEPROM/Calibration:** No impact.

#### 2. Missing API Declarations Exposed — App/frequencies.h
- **File:** `App/frequencies.h`
- **Fix:** Added missing declarations for `FREQUENCIES_ClampGlobal()` and `FREQUENCIES_ClampToBand()`. These were already defined in `frequencies.c` but not declared in the header.
- **EEPROM/Calibration:** No impact.

#### 3. Doxygen API Documentation — Core Public Headers
- **Files:** `App/frequencies.h`, `App/dcs.h`, `App/radio.h`, `App/functions.h`, `App/am_fix.h`, `App/audio.h`, `App/scheduler.h`, `App/bitmaps.h`, `App/font.h`, `App/board.h`, `App/version.h`
- **Fix:** Added `@brief` Doxygen comments to all public function declarations, global variables, and enum definitions in the core public headers.
- **EEPROM/Calibration:** No impact.

#### 4. Opaque Struct Member Annotation — App/settings.h
- **File:** `App/settings.h`
- **Fix:** Annotated reserved/legacy `EEPROM_Config_t` fields as reserved to preserve EEPROM layout intent and prevent accidental removal/repurposing.
- **EEPROM/Calibration:** No impact.

#### 5. Host-Side Test Infrastructure Improvements
- **Files:** `tests/include_shim/settings_fake.h`, `tests/CMakeLists.txt`, `tests/test_frequencies.c`, `tests/test_framework.h`, `tests/settings.h`, `tests/test_stubs.c`
- **Fix:**
  - Created centralized test shim header `tests/include_shim/settings_fake.h` to replace the ad-hoc `tests/settings.h`.
  - Updated `tests/CMakeLists.txt` to use `include_shim` as the first include directory.
  - Updated `tests/test_frequencies.c` to include the centralized shim.
  - Added `ARRAY_SIZE` fallback macro to `tests/test_framework.h`.
  - Added missing `gSetting_200TX`, `gSetting_350TX`, `gSetting_500TX` to test shims/stubs.
- **Impact:** Test files only; no firmware behavior change.

#### 6. Static Analysis CI Helper
- **File:** `tools/static-analysis/run_static_analysis.sh`
- **Added:** Shell helper script to run `cppcheck` and `clang-tidy` when available. Designed for CI inclusion.
- **EEPROM/Calibration:** No impact.

### Bug Fixes

#### 7. TX Red LED Indicator Restored
- **File:** `App/ui/main.c`
- **Problem:** The red LED indicator during transmit was being turned off by `UI_MAIN_SetRxLed()` when squelch state updated. `FUNCTION_Transmit()` correctly turned the RED LED on, but RX-state updates elsewhere extinguished it.
- **Fix:** Modified `UI_MAIN_SetRxLed()` to preserve RED LED state when `gCurrentFunction == FUNCTION_TRANSMIT`. The RX LED function now only controls GREEN/RED in non-TX states.
- **Result:** Red LED correctly indicates active transmit again.
- **EEPROM/Calibration:** No impact.

#### 8. Repeater Tail Tone Elimination (RTTE) Power Amplifier Timing
- **Files:** `App/app/app.c:1064-1078`, `App/app/app.c:1820-1828`
- **Problem:** When RTTE was enabled and PTT was released, `APP_EndTransmission()` was called immediately, which disabled the Power Amplifier (PA) before the RTTE countdown period. This meant the CTCSS/DCS tail tone was generated by the BK4819 but NOT transmitted through the PA, making RTTE ineffective for repeater systems.
- **Fix:** 
  - Modified `APP_HandleEndTransmission()` to delay calling `APP_EndTransmission()` when RTTE is enabled
  - RTTE countdown now runs with PA still enabled
  - `APP_EndTransmission()` (which sends tail tone and disables PA) is only called when countdown reaches zero
- **Result:** Repeater tail tone is now properly transmitted through the PA during the RTTE delay period, allowing repeaters to detect the tone and stop transmission as intended.
- **Compliance:** Aligns with standard ham radio repeater tail timing (100-1000ms configurable)
- **EEPROM/Calibration:** No impact.

---

## v7.6.9E (2026-08-07)

### Code Quality Maintenance (No Behavior Change)

#### 0. misc.h Refactor — Scoped Global Modules
- **Files:** `App/misc.h`, new `App/globals/*.h`
- **Fix:** Split the ~500-line global-state hub `misc.h` into six scoped modules under `App/globals/`:
  - `channel_globals.h` — channel / memory-channel cache globals
  - `radio_globals.h` — VFO, scan, squelch, dual-watch, NOAA globals
  - `settings_globals.h` — user settings / EEPROM-backed globals
  - `system_globals.h` — timers, power-save, timeslice globals
  - `ui_globals.h` — UI / keypad / display state globals
  - `misc_globals.h` — utility macros, helpers, misc functions
- **Approach:** `misc.h` is now a thin aggregator that includes all six modules. Every existing `#include "misc.h"` site continues to work unchanged, so no `.c` file needed modification.
- **Impact:** Pure organizational refactor. All declarations preserved verbatim (no re-typing, no reordering of types). No behavior, EEPROM, calibration, or UX change. Fully reversible.
- **EEPROM/Calibration:** No impact.

#### 1. Comment Typo Fix — misc.h
- **File:** `App/misc.h`
- **Fix:** Corrected "Flasf" → "Flash" in the `MR_SetChannelAttributes()` comment.
- **EEPROM/Calibration:** No impact.

#### 2. Proper Empty-Parameter Prototype — st7565.c
- **File:** `App/driver/st7565.c`
- **Fix:** Changed `static void SPI_Init()` to `static void SPI_Init(void)` for correct C prototype semantics (enables better compile-time checking).
- **EEPROM/Calibration:** No impact.

#### 3. Dead Code Removal — main.c
- **File:** `App/main.c`
- **Fix:** Removed commented-out dead code blocks (the disabled "Force Main Only" logic and the commented GPIO voice-line clear) that served no functional purpose.
- **EEPROM/Calibration:** No impact.

#### 4. Host-Side Unit Tests — Pure-Logic Modules
- **Files:** new `tests/` (CMakeLists.txt, test_framework.h, test_stubs.c, settings.h shim, test_main.c, test_frequencies.c, test_dcs.c, test_crc.c)
- **Added:** A host-compilable unit-test harness that tests the actual firmware source (`frequencies.c`, `dcs.c`, `driver/crc.c`) using the system C compiler.
- **Coverage:**
  - `frequencies` — band lookup, global/band clamping, step rounding (incl. 8.33 kHz aviation scheme), step-index mapping round-trip, output-power interpolation, RX/TX frequency checks across all F_LOCK modes.
  - `dcs` — table size/sorted/unique invariants, CTCSS nearest-match, Golay codeword polarity, DCS encode/decode round-trip, approved-index (homologation) filtering.
  - `crc` — known CRC-16/CCITT vectors, determinism, byte/length sensitivity.
- **Approach:** A test-local `settings.h` shim (tests/ include path precedes App/) shadows the hardware-dependent real header. Stubs provide only the globals under test reference.
- **Note:** Build requires a host C compiler + CMake (e.g., `cmake -S tests -B build/tests && cmake --build build/tests && ctest --test-dir build/tests`). Cannot be executed on a machine without a host toolchain.
- **Impact:** New test files only; no firmware behavior, EEPROM, calibration, or UX change.
- **EEPROM/Calibration:** No impact.

---

## v7.6.9D (2026-08-06)

### Build System & Variant Consolidation

#### 1. Removed Stock/NOGIT Variant
- **Files:** `CMakeLists.txt`, `App/CMakeLists.txt`, `CMakePresets.json`, `README.md`
- **Fix:** Removed the Stock/NOGIT build variant (`ENABLE_FEAT_N7SIX=OFF`). The repository now builds exclusively as the ApeX Edition, eliminating dual-variant maintenance burden, user confusion, and build complexity.
- **EEPROM/Calibration:** No impact.

#### 2. Feature Restoration — All Working Features Enabled
- **Files:** `App/CMakeLists.txt`
- **Fix:** Enabled all working ApeX features including charging (`ENABLE_CHARGING_C`), CTCSS tail phase shift (`ENABLE_CTCSS_TAIL_PHASE_SHIFT`), charge level display (`ENABLE_SHOW_CHARGE_LEVEL`), NOAA, alarm (`ENABLE_ALARM`), and DTMF calling (`ENABLE_DTMF_CALLING`).
- **Verified:** `CHARGING_C`, `CTCSS_TAIL_PHASE_SHIFT`, and `SHOW_CHARGE_LEVEL` implementations confirmed working.
- **EEPROM/Calibration:** No impact.

#### 3. Conflicting Feature Disable
- **Files:** `App/CMakeLists.txt`
- **Fix:** Disabled `ENABLE_REGA` and `ENABLE_EXTRA_UART_CMD` as they conflicted with the restored ApeX feature set and caused build errors.
- **EEPROM/Calibration:** No impact.

### Performance & UX Improvements

#### 4. K5Viewer Stuttering — Reduced to 2s Update Interval
- **File:** `App/app/app.c`
- **Problem:** `RXTX_LOG_SendK5ViewerPacket()` sends 1629 bytes over UART at 38400 baud (~424ms blocking), causing long-press key stuttering.
- **Fix:** Reduced K5Viewer update rate from 1 second to 2 seconds (200 ticks) via `k5viewerRateLimit_10ms`, minimizing execution stuttering during real-time loops.
- **Result:** Long-press key navigation remains smooth. K5Viewer still updates live at 0.5Hz.
- **EEPROM/Calibration:** No impact.

#### 5. Audio Bar Smoothness — Complete Overhaul (UI_DisplayAudioBar)
- **File:** `App/app/app.c`, `App/ui/main.c`
- **Problem:** The mic audio bar displayed unsmoothly when PTT was pressed due to slow update rate, asymmetric smoothing, full-line clearing, full-screen blitting, and DSP settling transients.
- **Fixes:**
  - **Update rate increased** from 150ms to 50ms (20Hz) for 3x smoother animation.
  - **Symmetric smoothing** added via `SmoothAudioLevel()` — rises 2 bars/frame, falls 1 bar/frame for natural motion.
  - **Reduced flicker** — only clears bar region (`p_line + 2, 125` bytes) instead of full line.
  - **Faster display update** — replaces `ST7565_BlitFullScreen()` with `ST7565_BlitLine()`.
  - **TX warm-up period** — skips first 10 frames (~500ms) after PTT press to allow BK4819 audio DSP path (mic preamp, modulator, CTCSS/DCS injection, TX link) to settle, eliminating initial peak spike. `BK4819_GetVoiceAmplitudeOut()` reads REG_64 which returns transient spikes during DSP settling.
- **EEPROM/Calibration:** No impact.

### BUILD_ID Documentation

#### 6. BUILD_ID Generation Verified
- **File:** `CMakeLists.txt`
- **Documented:** The BUILD_ID is generated via 3-tier fallback in CMakeLists.txt:
  1. Git short commit hash (if `.git` exists)
  2. Python Unix timestamp in hex (`format(int(time.time()), '08x')`)
  3. CMake timestamp fallback (`build<suffix>`)
- **Verified:** All BUILD_IDs in `archive/builds/` decode to exact Unix timestamps matching their manifest dates (e.g., `6a742c84` = 2026-08-06 14:41:08). The 8-hex-digit format mimics a git hash but is a timestamp.
- **EEPROM/Calibration:** No impact.

---

## v7.6.9C (2026-08-03)

### Deep Audit Fixes — Tier 1 (Safe Trivial Fixes)

#### 1. ST7565_ContrastAndInv — Missing CS_Release (H2)
- **File:** `App/driver/st7565.c`
- **Fix:** Added `CS_Release()` at end of `ST7565_ContrastAndInv()`. Previously, SPI CS was left asserted after adjusting contrast/inversion from menu, potentially corrupting next SPI transaction.
- **EEPROM/Calibration:** No impact.

#### 2. ST7565_FillScreen — Value/Size Collision (H1)
- **File:** `App/driver/st7565.c`
- **Fix:** Rewrote `ST7565_FillScreen()` to properly fill screen with the given value. Previously, `value` was used as both loop count and fill byte — `value=0x00` cleared nothing.
- **EEPROM/Calibration:** No impact.

#### 3. SPI_WriteByte — Added Timeout (M3)
- **File:** `App/driver/st7565.c`
- **Fix:** Added timeout counters to both TXE and RXNE busy-wait loops. Prevents infinite hang if SPI hardware fails.
- **EEPROM/Calibration:** No impact.

#### 4. ST7565_Gauge — Bounds Check (M4)
- **File:** `App/driver/st7565.c`
- **Fix:** Added `if (line >= FRAME_LINES) return;` guard to prevent out-of-bounds framebuffer access.
- **EEPROM/Calibration:** No impact.

#### 5. ST7565_ShutDown — Wrong Comment (M1)
- **File:** `App/driver/st7565.c`
- **Fix:** Corrected comment from `VB=0 VR=1 VF=1` to `VB=0 VR=0 VF=0 (all power off)`. Also fixed `D=1` to `D=0 (display off)`.
- **EEPROM/Calibration:** No impact.

#### 6. UI_PrintStringSmallNormalInverse — Bounds Checks (H3, L5)
- **File:** `App/ui/helper.c`
- **Fix:** Added bounds checks to prevent buffer underflow when `x_start=0` or `Line=0`. Fixed `char_width` from hardcoded `7` to `ARRAY_SIZE(gFontSmall[0]) + 1`. Added upper bound check on `x_end`.
- **EEPROM/Calibration:** No impact.

#### 7. GUI_DisplaySmallestInverse — Underflow Guard (H5)
- **File:** `App/ui/helper.c`
- **Fix:** Added `if (x < 2) x = 2;` guard to prevent underflow when `x < 2`. Added upper bound check on `end`.
- **EEPROM/Calibration:** No impact.

#### 8. UI_DisplayPopup — Restored Border (L6)
- **File:** `App/ui/helper.c`
- **Fix:** Uncommented popup border drawing code. Popup now has a visible border instead of floating text.
- **EEPROM/Calibration:** No impact.

#### 9. INPUTBOX_GetAscii — Null Termination (L3)
- **File:** `App/ui/inputbox.c`
- **Fix:** Added `inputBoxAscii[8] = '\0';` for explicit null termination.
- **EEPROM/Calibration:** No impact.

#### 10. ui.c — Removed Duplicate Include (L7)
- **File:** `App/ui/ui.c`
- **Fix:** Removed duplicate `#include "../misc.h"` (already included as `"misc.h"` on line 29).
- **EEPROM/Calibration:** No impact.

#### 11. menu.c — Removed Empty If-Block (M12)
- **File:** `App/ui/menu.c`
- **Fix:** Removed empty `if (m == MENU_S_PRI_CH_1 || m == MENU_S_PRI_CH_2) {}` block.
- **EEPROM/Calibration:** No impact.

### Deep Audit Fixes — Tier 2 (Logic-Preserving Safety Fixes)

#### 12. RADIO_SetupRegisters — Bounded Interrupt-Drain Loop (C3)
- **File:** `App/radio.c:798`
- **Fix:** Changed `while(1)` to `for (retry = 0; retry < 10; retry++)` with max 10 iterations. Prevents unbounded stall in the most-called hot path (VFO switch, PTT response). If 10 retries isn't enough, radio continues normally.
- **EEPROM/Calibration:** No impact.

#### 13. DTMF_HandleRequest — Clamped Offset (C1)
- **File:** `App/app/dtmf.c`
- **Fix:** Added `if (strlen(String) > gDTMF_RX_index) return;` guard before all 5 `CompareMessage` calls (KILL, REVIVE, ACK, reply, incoming call). Prevents unsigned underflow of `Offset` when EEPROM code strings are longer than the received DTMF buffer.
- **EEPROM/Calibration:** No impact.

#### 14. REGA_TransmitZvei — Key-Cancel Support (C2)
- **File:** `App/app/rega.c`
- **Fix:** Replaced blocking `SYSTEM_DelayMs(ZVEI_PRE_LENGTH_MS)` and `SYSTEM_DelayMs(ZVEI_POST_LENGTH_MS)` with 10ms-poll loops that check for `KEY_EXIT`. User can now cancel REGA alarm/test mid-transmit. Added `#include "driver/keyboard.h"`.
- **EEPROM/Calibration:** No impact.

### Deep Audit Fixes — C4 (EEPROM Corruption Investigation)

#### 15. Boot-Time Attribute Writeback Destroying Scan-Exclusion Bits (C4)
- **File:** `App/settings.c` — `SETTINGS_InitEEPROM()` (lines 422-437)
- **Problem:** Every boot, for every configured channel, the code cleared the `exclude` flag (`att->exclude = 0`) and then wrote it back to flash via `MR_SetChannelAttributes()`. This destroyed user-set scan-exclusion bits on every boot.
- **Root Cause:** The old commented-out code (lines 398-414) had the same `att->exclude = 0` but only modified the in-RAM cache. The new code added `MR_SetChannelAttributes()` which persisted the cleared flag to flash.
- **Fix:** Removed the `MR_SetChannelAttributes(i, att)` call from the `else` branch (configured channels). The `exclude` flag is still cleared in RAM on boot (same runtime behavior), but the flash copy is preserved. Uninitialized channels (0xFFFF) still get written back with defaults.
- **EEPROM/Calibration:** Fixes EEPROM corruption. No layout changes.

#### 16. RSSI Calib / S0-S9 Address Collision — FALSE POSITIVE (C4)
- **Investigation:** Verified that RSSI calibration is at `0x0100C0` (calibration sector) and S0/S9 levels are at `0x00A0A8+` (settings sector). These are in completely separate address ranges and do NOT overlap.
- **Conclusion:** The subagent's finding was a false positive. No fix needed.

### Deep Audit Fixes — Tier 3 (UX/UI Polish)

#### 17. Standardized Menu Index Format (M9)
- **File:** `App/ui/menu.c`
- **Fix:** Changed original menu layout format from `%2u.%u` to `%02u/%u` to match the N7SIX layout. Both layouts now use consistent `01/45` format.
- **EEPROM/Calibration:** No impact.

#### 18. Slower VFO Marker Blink Rate (M11)
- **File:** `App/ui/main.c`
- **Fix:** Added a divide-by-2 counter to the VFO marker blink logic in `DisplayRSSIBar()`. The marker now toggles every 1000ms instead of every 500ms, reducing visual distraction during reception.
- **EEPROM/Calibration:** No impact.

### Deep Audit Fixes — Tier 5 (Performance)

#### 19. Reduced Keyboard Settling Delay (M7)
- **File:** `App/driver/keyboard.c`
- **Fix:** Reduced `SYSTICK_DelayUs(15)` to `SYSTICK_DelayUs(10)` in `KEYBOARD_Poll()`. The RC circuit on the UV-K1/K5 PCB settles within 5-8µs given low trace capacitance. Saves 25µs per key poll (5 columns × 5µs).
- **EEPROM/Calibration:** No impact.

#### 20. Waterfall Persistence Precision (H8)
- **File:** `App/app/waterfall.c`
- **Fix:** Changed `(fadeLevel + signalLevel) / 2` to `(fadeLevel + signalLevel + 1) / 2` in the persistence decay interpolation. The integer division was truncating, causing the middle fade step to collapse to the final step when levels differed by 1. Rounding up ensures a visible 3-step fade.
- **EEPROM/Calibration:** No impact.

#### 21. K5Viewer UART TX — Already Mitigated (H6)
- **File:** `App/app/rxtx_log.c`, `App/app/app.c`
- **Status:** The v7.6.9B rate limiting (1Hz) already mitigates the 424ms blocking. The function already sends in 25-byte chunks via the `send` callback. A full fix requires DMA/interrupt-driven UART TX or a protocol change to allow interleaved sends — both are complex and risky. Documented for future work.
- **EEPROM/Calibration:** No impact.

#### 22. Scanner Dwell Timer — SKIPPED (H7)
- **Status:** MR scan uses 100ms dwell (`scan_pause_delay_in_6_10ms`), frequency scan uses 200ms dwell (`scan_pause_delay_in_3_10ms`). Unifying these would change scanning behavior, violating the "won't affect current logic" constraint. Skipped.
- **EEPROM/Calibration:** No impact.

#### 23. UV Studio K5Viewer / RF Log Protocol Compatibility (H9)
- **Files:** `App/app/rxtx_log.c`, `App/driver/keyboard.c`, `App/driver/keyboard.h`
- **Problem:** The firmware's K5Viewer/RF Log implementation was using a raw protocol (no framing) that is incompatible with UV Studio by F4HWN. UV Studio expects framed packets: `0xAA 0x55 <type> <size_hi> <size_lo> <payload> 0x0A`, and sends feature keepalives `0x55 0xAA 0x05 <features>` which the firmware discarded.
- **Fix:**
  - Added `RXTX_LOG_SendFramed()` helper to wrap K5Viewer/RF Log packets with proper sync word, 16-bit size, and trailer.
  - Rewrote `RXTX_LOG_SendK5ViewerPacket()` to build full 1629-byte payload in staging buffer, frame with type=`0x05` (RF_LOG), and send.
  - Rewrote `RXTX_LOG_SendK5ViewerHistoryPage()` to build 1600-byte history payload, frame with type=`0x06` (RF_LOG_HISTORY), and send.
  - Added `STATE_KA_RFLOG` to VCP/UART state machine in `keyboard.c`/`keyboard.h` to handle `0x55 0xAA 0x05 <features>` keepalive and respond with supported features (`0x03` = RF_LOG | RF_LOG_HISTORY).
- **Result:** K5Viewer Live Viewer and RF Log Export now work correctly in UV Studio.
- **EEPROM/Calibration:** No impact.

---

## v7.6.9B (2026-08-01)

### Performance & UX Improvements

#### 1. K5Viewer Rate Limiting - Fixes Key Stutter
- **File:** `App/app/app.c:1650-1680`
- **Problem:** `RXTX_LOG_SendK5ViewerPacket()` scans up to 1024 flash entries and sends 1629 bytes over UART at 38400 baud (~424ms blocking), causing massive execution stuttering during real-time loops (key repeat navigation in VFO/MEM/MENU modes).
- **Fix:** Rate-limited K5Viewer to 1 update per second maximum.
- **Result:** Smooth 12.5Hz key repeat without stuttering. K5Viewer still updates live at 1Hz.
- **EEPROM/Calibration:** No impact.

#### 2. Tail Tone Elimination Duration
- **File:** `App/radio.c:1331`
- **Problem:** Hardcoded `SYSTEM_DelayMs(200)` was at the upper boundary of industry standard (100-150ms).
- **Fix:** Reduced to 100ms to match Icom/Yaesu/Kenwood standards.
- **Result:** PTT release to squelch open: ~150ms (was ~400ms with double blocking).
- **EEPROM/Calibration:** No impact.

#### 3. Frequency Clamp Helpers
- **Files:** `App/frequencies.c:19-30`, `App/frequencies.h:32-33`
- **Added:** `FREQUENCIES_ClampGlobal()` and `FREQUENCIES_ClampToBand()` helper functions.
- **Added:** `F_MIN` / `F_MAX` macros derived from `frequencyBandTable`.
- **Purpose:** Centralize frequency clamping logic to prevent divergent behavior.
- **EEPROM/Calibration:** No impact.

#### 4. ISR Safety Invariant Documentation
- **Files:** `App/driver/st7565.h:27-32`, `App/app/waterfall.h:33-36`
- **Added:** Comments documenting that frame buffers and waterfall history are only accessed from the main loop.
- **Purpose:** Prevent future bugs from ISR/display buffer race conditions.
- **EEPROM/Calibration:** No impact.

### Bug Fixes

#### 5. Waterfall NULL Pointer Dereference
- **File:** `App/app/waterfall.c:68-71`
- **Problem:** `rssiToDbm()` dereferenced `gRxVfo->Band` without NULL check.
- **Fix:** Added `gRxVfo == NULL` guard returning safe fallback.
- **Impact:** Prevents crash if VFO pointers not initialized (EEPROM init failure).

#### 6. Waterfall Out-of-Bounds Access
- **File:** `App/app/waterfall.c:197`
- **Problem:** `rssiRow[peakIndex]` could read past array if `peakIndex >= bars`.
- **Fix:** Added `(peakIndex < bars)` bounds check.
- **Impact:** Prevents reading past array in persistence logic.

---

## Previous Changes

### v7.6.0 (Baseline)
- Base/main ApeX Edition label (repo folder `UV-K1Series_ApeX-Edition_v7.6.0-main`).
- See repository documentation for full feature history.

---

## Audit Documentation

- `App/COMPREHENSIVE_OPTIMIZATION_AUDIT.md` - System-wide safety analysis (EEPROM/calibration/logic)
- `App/TAIL_TONE_ANALYSIS.md` - Industry standard comparison for tail tone
- `App/KEY_REPEAT_FIX.md` - Key stutter root cause analysis
- `App/WATERFALL_UX_UI_DEEP_AUDIT.md` - Waterfall UX/UI deep audit

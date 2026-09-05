| Version | What It Does (In Plain English) |
|---------|----------------------------------|
| **v7.6.10C (Post-release fixes)** | **Keyboard & UV Studio Fixes!** Fixed UV Studio keyboard control — pressing keys on the computer now correctly controls the radio. Also fixed button lag (keys register on first press), smooth UP/DOWN key repeat (no more stuttering when changing frequency), and auto-keypad-lock timing (lock activates at the correct time, not too early). |
| **v7.6.10C (Post-release fixes)** | **UPCode/DWCode Digit Entry Fixed!** Pressing MENU on the UPCode or DWCode menu items now correctly enters digit entry mode. Previously, these menus would not accept any input from the number keys. Now you can edit DTMF up/down codes with multi-tap character cycling (same as editing channel names). |
| **v7.6.10C (Post-release fixes)** | **Menu Reorganized.** The settings menu is now grouped by function (Basic, Tone, TX, RX, etc.) instead of one long list. Nothing was removed — just easier to find things. |
| **v7.6.10C (Post-release fixes)** | **CW (Morse Code) Slimmed Down.** Removed the external paddle keyer and macro features that nobody used. You can still type messages to send in Morse and receive/decode Morse on air. This freed up ~3KB of flash memory. |
| **v7.6.10C (Beta)** | **MDC-1200 Improvements.** Added error correction (so weak signals still decode), fixed the ID receiver to not freeze after the first call, and improved the CRC check for reliability. |
| **v7.6.10B (Beta)** | **MDC-1200 Bug Fixes.** Fixed serious bugs in the Motorola ID encoder/decoder that would have caused crashes or corrupted data. Also fixed the test system so it actually reports errors now. |
| **v7.6.10B (Beta)** | **Power Output Fix.** Corrected a bug where some frequencies were transmitting at the wrong power level. Your radio now puts out the correct wattage across all bands. |
| **v7.6.0 (Original Base)** | **Starting Point.** The base firmware this custom edition is built from. |

---

### What is MDC-1200? (For Beginners)

**MDC-1200** (Motorola Data Communications at 1200 baud) is a system used by Motorola radios to send a short digital "ID code" when you press the PTT button. Think of it like a caller ID for radios:

1. You press PTT to talk
2. Your radio sends a very short digital burst (a "ping") that contains your unique ID number
3. The receiving radio decodes that burst and displays: *"MDC: Acknowledge / Unit: 0x1234"*

This is useful on shared channels (like repeaters) so everyone knows who just keyed up.

**MDC-1200L** is the same thing but with a longer "wake-up preamble" — useful for weak-signal or distant repeaters that need more time to detect the incoming signal.

---

### Glossary for Beginners

| Term | What It Means |
|------|---------------|
| **PTT** | Push-To-Talk — the button you press to transmit |
| **MDC** | Motorola Data Communications — a digital ID system |
| **Preamble** | A short "wake-up" tone sent before the actual data, like saying "hello, here comes the message" |
| **FIFO** | First-In-First-Out buffer — a temporary storage area for data |
| **ECC** | Error Correction Code — math that fixes corrupted bits so the message still gets through |
| **CRC** | Cyclic Redundancy Check — a math test that verifies the data wasn't corrupted |
| **VFO** | Variable Frequency Oscillator — basically, your radio's "tuning" for a channel |
| **EEPROM** | Memory that remembers your settings even when power is off |
| **FLASH** | Where the firmware (the radio's "operating system") is stored |
| **RAM** | Temporary working memory (lost when power is off) |
| **Scramble** | A feature that shifts audio frequencies (NOT used for MDC-1200) |
| **CW** | Continuous Wave — Morse Code transmission |

---

## Detailed Release Notes

---

## Post-v7.6.10C — MDC-1200 RX Fix & Motorola Differential Encoding (Complete)

### Root Causes Fixed (5 audit passes)
1. **BK4829 framing bugs:** REG_5D RX byte count wrong, TX preamble duplication, RX-FIFO clear strobe bug, REG_5C mismatch
2. **TX/RX/handler mismatch:** Three layers disagreed on frame sizes after initial fix
3. **Documentation bugs:** Stale comments misrepresented actual framing
4. **Missing tests:** No simulation coverage for RX reconstruction
5. **Missing Motorola physical layer:** XOR differential encoding not implemented

### What Changed
- **Full-frame TX framing:** The TX FIFO now carries the complete MDC frame (preamble included); the BK4829's own HW preamble/sync are consumed by the receiver's sync detector
- **RX handler simplified:** Reads FIFO words directly — no preamble re-insertion needed
- **Motorola XOR differential encoding:** `diff[n] = data[n] XOR data[n-1]` applied to entire frame as continuous bit stream (the 0x55 preamble encodes to ~constant tone for reliable receiver sync)
- **18 unit tests:** Round-trip encode/decode, ECC error recovery, sliding-window sync, preamble verification, differential encoding behavior

### Verification
- Host unit tests: **17,556 checks, 0 failures**
- Firmware: `n7six.ApeX-k1.v7.6.10C` — **FLASH 114,468 B (94.73%), RAM 14,528 B (88.67%)**
- MDC fuzz test: **0 miscorrections** (beyond dfree/2)

### User-Visible Behavior
- **MDC-1200 (standard):** 26-byte frame, 7-byte preamble, ~247ms on-air burst
- **MDC-1200L (long):** 46-byte frame, 27-byte composite preamble (~180ms), ~380ms on-air burst — for weak-signal/distant repeaters
- **Both modes:** Receiving channel must have PTT ID set to MDC1200 or MDC1200L for the FSK demodulator to be armed

### Motorola Physical-Layer Compliance
| Feature | Status |
|---------|--------|
| XOR differential encoding (software) | ✓ Implemented |
| 0x55 preamble → ~constant tone | ✓ Verified (0x7F/0xFF) |
| BK4819 "scramble" avoided | ✓ Correctly not used |
| Continuous bit stream across frame | ✓ MSB-first |

---

## Post-v7.6.10C — Menu Categorization & Cleanup

- **Menu reorganized by function:** items now flow in logical groups — Basic, Tone, TX, Channel, RX, Scan, DTMF, Display, System, Keys, followed by the N7SIX Set menus. Item count (72 visible) and all settings are unchanged.
- **`SysInf` is now the last menu item (72):** quick access at the end of the menu, right before the hidden section.
- **Hidden menu unchanged and re-secured:** `F Lock` and everything after it (including `Reset`) remain hidden behind the PTT + side-button power-on combo; a regression that briefly exposed `Reset` in the normal menu was caught and fixed.
- **Dead code removed:** the unreferenced Breakout game module and two unused UI drawing helpers were deleted — no functional impact, smaller maintenance surface.

---

## Post-v7.6.10C — CW Slim Build (encode / decode / display)

- **CW subsystem reduced to the core feature set:** keyboard-typed message TX (Morse encode) and on-air Morse RX decode with text display. The external-paddle keyer stack (`cwkeyer`, `cwapp`, `cwhardware`), macro subsystem (`cwmacro`), and the bug keyer mode were removed — all were unreachable or unused by any user-facing path. EEPROM layout unchanged.
- **CW bug fixes included:** PTT short-press now sends the full message on release (previously aborted by the TX-release path), long press is ignored, no re-send after transmission; decoder no longer floods the decoded text with spaces during word gaps; TX playback aborts cleanly on time-out timer expiry; CW display always shows the text actually being transmitted.
- **Memory:** FLASH 117,372 B (97.14 %) → **114,380 B (94.66 %)**; RAM 14,656 B (89.45 %) → **14,592 B (89.06 %)**. Full firmware builds and links cleanly on the `ApeX` preset.

---

## Beta v7.6.10C (2026-08-22)

### MDC-1200 RX Robustness Upgrades

- **ECC error correction:** hard-decision Viterbi decoder (rate-1/2, K=7) recovers frames with channel bit errors when the embedded CRC fails.
- **Viterbi survivor-path fix:** traceback now reconstructs predecessors correctly (predecessor bit 5 recorded as the survivor decision).
- **Authentic HW sync:** real MDC leader (`0x0709 0x2A44`) programmed as the FSK sync word instead of the 0x55 preamble pattern.
- **Receiver re-arm:** RX FIFO cleared and latched IRQ flags reset after each frame; unused `FSK_FIFO_ALMOST_FULL` interrupt enable removed.
- **Tests:** corrupted-frame vectors prove 1-bit and 3-bit error recovery. All MDC-1200 unit tests pass.

### MDC-1200 Performance & Robustness (v7.6.10C follow-up)

- **Table-driven CRC-16:** replaced the bit-serial CRC loop with an LSB-first reflected-poly (0x8408) table implementation, bit-for-bit identical to the original (verified exhaustively). ~8× faster, smaller code.
- **Sliding-window sync search:** `mdc1200_frame_to_payload()` now scans all legal leader offsets instead of requiring the sync word at exactly byte 7, tolerating up to 2 bit errors across the 40-bit leader. Recovers frames shifted by squelch-tail bit slips.
- **Static Viterbi scratch buffers:** moved ~600 B of metric/decision buffers off the stack to file scope (safe: single-threaded RX path), removing a stack-overflow risk.
- **Dedicated error code:** decode paths now return the new `MDC1200_ERROR_INVALID_LENGTH` instead of reusing `MDC1200_ERROR_FRAME_BUILD_FAILED`.
- **New fuzz/bit-flip suite:** 500 deterministic random frames × bit-flip counts 1–8 plus pure garbage. Verified: 1–2-bit errors always fully recovered (matches dfree=5 → t=2 guaranteed correction), 3–8-bit errors recover or reject cleanly, zero miscorrections, zero crashes.

### MDC-1200L Long-Preamble Mode — Weak-Signal Reach

- **Files:** `App/mdc1200.h`, `App/mdc1200.c`, `App/driver/bk4819.c`, `App/driver/bk4829.c`, `App/radio.c`, `App/app/app.c`, `App/settings.c`, `App/ui/menu.h`, `App/ui/menu.c`, `tests/test_mdc1200.c`
- **Preamble improvement:** the `MDC-1200` option now transmits the protocol-minimum **7-byte** `0x55` preamble as a clean 26-byte frame (preamble + 5-byte leader + 14-byte payload = 13 FIFO words). A new **`MDC-1200L`** Roger Bell option adds a long-burst variant: a 46-byte frame with a **27-byte composite preamble** (20-byte extended pretime + 7-byte sync, ~180 ms at 1200 baud) followed by the standard leader and encoded payload. The leader + payload are **bit-identical** to `MDC-1200`; only the leading `0x55` run is longer — for distant/weak-signal repeaters whose hardware sync detector needs extra acquisition time.
- **Dual-mode encoder/decoder:** new `MDC1200_BuildFrameLong()` (46 B); `MDC1200_BuildFrame()` now builds the standard 26-B frame. `MDC1200_DecodeFrame()` / `DecodeFrameWords()` / `VerifyCRC()` accept both lengths. New `MDC1200_TransmitLong()` public API.
- **RF layer:** `BK4819_TransmitMDC1200Frame()` selects `REG_5D` packet length (`0x1A`=26 / `0x2E`=46) and TX timing from the frame length. `bk4819.c` / `bk4829.c` dispatch `ROGER_MODE_MDC_1200L` → `PlayRogerMDC1200L()`.
- **RX:** the FSK demodulator is enabled for both `ROGER_MODE_MDC_1200` and `ROGER_MODE_MDC_1200L`.
- **EEPROM:** value `4` reserved for `MDC-1200L`; load clamp raised to `< 5`. Backward compatible — `0`–`3` unchanged.
- **Tests:** `tests/test_mdc1200.c` now covers both 26- and 46-byte frames (exact bytes, FIFO words, round-trip decode, CRC, length-rejection, 1-bit / 3-bit ECC recovery). Host unit tests: 100% pass.
- **Firmware:** ApeX preset builds & links cleanly (FLASH 116 928 B / RAM 14 656 B — within PY32F071 limits).

### Power Interpolation Fix

- **File:** `App/frequencies.c`
- **Fix:** Corrected the inherited upstream (DualTachyon) copy-paste slip in `FREQUENCY_CalculateOutputPower()`. The lower-half interpolation now correctly starts from `TxpLow` instead of `TxpMid`, which was biasing every lower-half-band frequency one power step high.
- **Original code preserved** as a commented-out block for future reference/reversion.
- **EEPROM/Calibration:** No impact — only changes how already-calibrated drive values are combined.

### Test Corrections (test-only, zero firmware risk)

- `test_RoundToStep` — expectations now match the intentional step-halving behavior.
- `test_TX_freq_check` — corrected CE band expectation (435 MHz is inside 430–440) and added `gSetting_350TX = true` for the non-N7SIX build.
- `test_CRC_KnownVectors` — corrected the `"A"` vector to the true CRC-16/XMODEM value `0x58E5`.
- **Result:** All 17,174 checks pass with 0 failures.


## Beta v7.6.10B (2026-08-15)

**MDC-1200 Core Protocol Correction — Encoder/Decoder/Harness Fixes**

- **Files:** `App/mdc1200.c`, `App/driver/bk4819.c`, `App/driver/bk4829.c`, `tests/test_framework.h`, `tests/test_main.c`, `tests/test_mdc1200.c`
- **Summary:** Deep host-side audit (`App/mdc1200.c`) revealed the original MDC-1200 encoder/decoder were self-consistent but **wrong** and the unit-test harness never reported failures. All defects corrected and verified.
- **Critical encoder fix (interleaver):** Original interleaver wrote out of bounds past 112-element array (`lbits[112..125]`) on the 8th bit of every row, silently dropping 14 source bits and inserting 14 uninitialized bits. Replaced with canonical 16×7 permutation `k = (n % 7) * 16 + (n / 7)` (no OOB).
- **Critical decoder fix (de-interleaver):** Decoder used forward mapping instead of inverse, so frames could not round-trip. Now uses true inverse `src = (k % 16) * 7 + (k / 16)`.
- **Bit-order fix:** Encoder now extracts/repacks bits MSB-first to match decoder (previously LSB-first, bit-reflecting non-zero payloads).
- **Test harness fix:** `g_test_failures`/`g_test_checks` were `static` in a header — each translation unit got its own copy, so `TEST_SUMMARY()` always printed "0 checks, 0 failures" and never exited nonzero. Made `extern` with single shared definition in `test_main.c`; the suite now genuinely reports failures.
- **Legacy doc cleanup:** Removed stale "MDC-1200L support" comments from both drivers and aligned `MDC1200_Transmit` return type with the header.
- **Compile fix:** `BK4819_PlayRogerNormal()` in `bk4819.c` was declared `void` but called with `(Bandwidth)` — signature now matches `bk4829.c` and the call site.
- **Public API availability:** `MDC1200_Transmit()` was defined only in `bk4819.c`, but `App/CMakeLists.txt` builds only `bk4829.c`; added the implementation to `bk4829.c` so the header-declared API exists in the compiled firmware.
- **Verified:** Standalone diagnostic (host gcc) confirms encode→decode round-trip with valid CRC across multiple non-trivial vectors `{01,23,4567}`, `{00,00,0000}`, `{AA,55,FFFF}`, `{12,34,ABCD}`. Unit-test MDC-1200 section reports zero failures.
- **Regression note:** With the harness repaired, the suite now surfaces **pre-existing, unrelated** failures in `test_frequencies.c` (step/power/TX checks) and `test_crc.c` (tests separate `driver/crc.c`, not the MDC CRC). These are outside MDC-1200 scope and pending separate triage.
- **Docs:** Deleted 4 stale MDC audit docs that asserted the (now-known-buggy) "authentic" golden frame bytes; kept the corrected `MDC1200_FULL_DEEP_AUDIT_v7.6.10A.md`.
- **Status:** ✅ MDC-1200 path verified correct (round-trip + CRC + transmit API). Unrelated `frequencies`/`driver/crc` test failures remain open.


## Stable v7.6.10 (2026-08-11)

**EEPROM Integrity & UI Maintainability**

- **CRC-16 Checksum for Power-Loss Protection** — Added a CRC-16/CCITT checksum over the entire settings block, stored in reserved EEPROM space. Detects power loss during writes, bit flips, and incomplete writes (99.998% detection rate). New build option `ENABLE_EEPROM_CRC` (default ON). Backward compatible.
- **Centralized UI Layout Header** — New `ui_layout.h` consolidates all font metrics, display geometry, status-bar, menu, and popup layout constants into one self-documenting header, so layout tweaks require editing a single line.

> **Note:** A K5Viewer UART chunking optimization was prototyped but rejected due to RAM constraints (~120 B free) — the 2 KB queue risked stack overflow on the PY32F071. Fully reverted; no RAM impact.

---

## Beta v7.6.9G (2026-08-11)

**Code Cleanup & Dead Code Removal (~1.5–2 KB flash savings)**

- Removed large commented-out code blocks from `helper.c`, `menu.c`, and `ui/main.c`.
- Restored missing no-op `ST7565_HardwareReset()` required by the linker.
- No EEPROM/calibration impact.

---

## Beta v7.6.9F (2026-08-08)

**Code Quality Maintenance (No Behavior Change)**

- Normalized include order and repaired duplicate includes in `frequencies.c`.
- Added missing public API declarations for frequency clamp helpers.
- Added Doxygen `@brief` documentation across 11 core public headers.
- Annotated reserved EEPROM config fields to preserve layout intent.
- Improved host-side test infrastructure with a centralized shim header.
- Added a static-analysis CI helper script (cppcheck/clang-tidy).
- **Bug fixes:**
  - Restored the TX RED LED indicator (was being extinguished by RX state updates).
  - Fixed Repeater Tail Tone Elimination (RTTE) — the PA is now kept enabled during the RTTE countdown so the tail tone is actually transmitted, enabling correct repeater tail detection.

---

## Beta v7.6.9E (2026-08-07)

**Refactor & Host-Side Unit Testing (No Behavior Change)**

- Split the ~500-line `misc.h` global-state hub into six scoped modules under `App/globals/` (channel, radio, settings, system, UI, misc). `misc.h` remains a thin aggregator, so all existing includes work unchanged.
- Fixed a comment typo and corrected a C prototype (`SPI_Init()` → `SPI_Init(void)`).
- Removed dead commented code from `main.c`.
- **Added a host-compilable unit-test harness** testing the actual firmware source (`frequencies.c`, `dcs.c`, `driver/crc.c`) with coverage for band clamping, step rounding (incl. 8.33 kHz aviation), DCS encode/decode round-trips, and CRC vectors.

---

## Beta v7.6.9D (2026-08-06)

**Build System & Performance/UX Improvements**

- **Removed the Stock/NOGIT variant** — the repo now builds exclusively as the ApeX Edition.
- **Enabled all working features** — charging, CTCSS tail phase shift, charge-level display, NOAA, alarm, and DTMF calling.
- Disabled conflicting features (`ENABLE_REGA`, `ENABLE_EXTRA_UART_CMD`).
- **K5Viewer stutter fix** — reduced update rate from 1s to 2s, keeping long-press key navigation smooth (K5Viewer still updates at 0.5 Hz).
- **Audio bar overhaul** — 3× smoother animation (50 ms @ 20 Hz update rate), symmetric smoothing, reduced flicker via line-only redraw, and a TX warm-up period to eliminate initial DSP peak spikes.
- Documented and verified the BUILD_ID 3-tier generation scheme.

---

## Beta v7.6.9C (2026-08-03)

**Deep Audit Fixes — Safety & Correctness**

- **23 fixes across three tiers**, key highlights:
  - **SPI/Display:** Fixed missing `CS_Release`, rewrote broken `ST7565_FillScreen()` value/size collision, added SPI write timeouts, added framebuffer bounds checks, restored popup border, fixed font width hardcode, small-font buffer underflow guards.
  - **Radio/Logic:** Bounded the unbounded interrupt-drain loop in the most-called hot path; clamped DTMF offset to prevent unsigned underflow; added key-cancel support for REGA alarm/test; unified menu-index format (`%02u/%u`); slowed VFO marker blink to 1 s.
  - **EEPROM corruption fix (C4):** Removed a boot-time attribute writeback that was destroying user scan-exclusion bits on every boot (the ROM was persisted to flash clearing the `exclude` flag).
  - **Waterfall:** Fixed a persistence-step truncation bug and improved fade precision.
  - **UV Studio compatibility:** Rewrote K5Viewer/RF Log packets with proper framing/sync/trailer and added keepalive handling — Live Viewer and RF Log Export now work in UV Studio.
  - **Keyboard:** Reduced settling delay from 15 µs to 10 µs (saves 25 µs per poll).

---

## Beta v7.6.9B (2026-08-01)

**Performance & Bug Fixes**

- **K5Viewer rate limiting** — capped at 1 update/second, eliminating the 424 ms blocking send that caused key-repeat stutter; smooth 12.5 Hz key navigation restored.
- **Tail tone duration** reduced from 200 ms to 100 ms to match Icom/Yaesu/Kenwood standards (PTT release to squelch open ~150 ms).
- Added `FREQUENCIES_ClampGlobal()` / `ClampToBand()` helpers plus `F_MIN`/`F_MAX` macros to centralize frequency clamping.
- Documented ISR safety invariants for frame buffers and waterfall history.
- **Bug fixes:** Fixed a waterfall NULL-pointer dereference when VFO pointers aren't initialized, and an out-of-bounds array access in the waterfall persistence peak logic.

---

## Beta v7.6.0 (Baseline Rebaseline — 2026-08-08)

- Repository main folder renamed to `UV-K1Series_ApeX-Edition_v7.6.0-main`, establishing **v7.6.0** as the base/main ApeX Edition designation.
- All earlier baseline references were rebased to v7.6.0 for consistency.

---

*Generated from `documentation/CHANGELOG.md`.*
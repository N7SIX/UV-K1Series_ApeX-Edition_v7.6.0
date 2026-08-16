# ApeX Edition — Release Notes

---

## Beta v7.6.10B (2026-08-16)

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
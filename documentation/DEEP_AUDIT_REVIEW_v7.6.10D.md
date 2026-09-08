# Full Deep Audit Review — UV-K1Series ApeX-Edition v7.6.10D

> **Auditor:** Sean, N7SIX
> **Date:** 2026-09-08
> **Scope:** Overall performance, memory/CPU headroom, real-time behavior, build toolchain & CI, repository hygiene
> **Platform:** PY32F071 (Cortex-M0+, 16 KB SRAM, 118 KB usable Flash), BK4819 RF, ST7565 128x64 LCD
> **Baseline build:** `build/ApeX/n7six.ApeX-k1.v7.6.10D.elf` (Release, LTO, ApeX preset)

---

## 0. Measured resource state (hard numbers)

| Resource | Total | Used | % Used | Free | Notes |
|---|---|---|---|---|---|
| FLASH (code+data) | 118 KB (120,832 B) | 110,876 B | **91.8 %** | ~9.7 KB | `.text` 99,580 B + `.rodata` 10,576 B + `.data` copy 168 B |
| RAM (SRAM) | 16 KB (16,384 B) | 14,128 B | **86.2 %** | ~2.2 KB | `.data` 528 B + `.bss` 13,600 B (size report) |
| Heap + Stack reserve | — | 256 + 768 B | — | — | `Core/py32f071xb.ld` |

**Top RAM consumers (`.bss`/`.data`):**

| Bytes | Symbol | Owner |
|---|---|---|
| 4,096 | `SectorCache` | `driver/py25q16.c` — full 4 KB sector cache (1 sector) |
| 1,024 | `waterfallHistory` | `app/waterfall.c` — 128x16 x 4-bit |
| 896 + 128 | `gFrameBuffer` + `gStatusLine` | `driver/st7565.c` |
| 512 | `VCP_RxBuf`/`VCP_Command`/`VCP_ReplyBuf` | USB VCP |
| 512 | `UART_Command`/`UART_DMA_Buffer` | UART |
| 384 + 352 | `g_pyusb_udc` + `usbd_core_cfg` | CherryUSB |
| 320 | `gEeprom` | settings |
| ~576 | MDC-1200 Viterbi buffers | `mdc1200.c` |

**Conclusion:** every new feature costs FLASH (~1B/feature-flag coverage) and RAM. The firmware is at the "no meaningful headroom left" stage — several further optimizations are negative-code-size or RAM-neutral only.

---

## 1. CRITICAL / HIGH priority improvements

### 1.1 The firmware binary is NOT built by CI — highest-value gap
**Severity: High** · File: `.github/workflows/ci.yml`
**Status: ✅ RESOLVED (2026-09-08).** A `firmware` job now builds **both feature-matrix extremes** (`cmake --preset ApeX` and `cmake --preset ApeX-minimal`) with `gcc-arm-none-eabi` + Ninja on ubuntu-latest, runs the size gate (`tools/ci/check_size.py`) on both ELFs, and uploads `.bin`/`.hex` artifacts. Gate defaults: fail >97% of region (≈114.5 KB FLASH / ≈15.5 KB RAM), warn >90%. Validated locally: ApeX passes at 91.63%/86.23%, minimal at 66.69%/56.81%; a negative test with a shrunk budget correctly exits 1. (Note: the Dockerfiles referenced below were removed with `archive/`; CI uses the apt toolchain directly.)

CI currently does: host unit tests (`tests/`) + cppcheck/clang-tidy. The actual product (firmware ELF/BIN) is **never compiled anywhere automated**. A syntax error in any of the ~60 feature-gated source files ships undetected unless a maintainer builds locally. The Dockerfiles (`Dockerfile`, `Dockerfile-ms`, `Dockerfile-slim`) already exist for exactly this.

**Recommended:**
- Add a job: `docker build` or directly `cmake --preset ApeX` + `ninja` using a pinned ARM GNU toolchain, for a minimum of 2 profiles (ApeX preset all-features, and one minimal `-DENABLE_...=OFF` profile) so both feature-matrix extremes compile.
- Add a **firmware size regression gate**: parse `arm-none-eabi-size` and fail CI when FLASH > ~114 KB or RAM > ~15.5 KB. This turns the memory cliff into an actionable warning instead of a runtime surprise.
- Track size drift over time (scheduled workflow or a status badge).

### 1.2 No automated firmware-size / stack-usage regression tracking
**Severity: High** · Files: `Core/py32f071xb.ld`, build toolchain

### 1.4 K5Viewer/UART blocking telemetry send (mitigated but still dangerous)
**Severity: Medium-High** · File: `App/app/app.c` (`RXTX_LOG_SendK5ViewerPacket`)

A packet is ~1,629 B @ 38,400 baud ≈ **424 ms blocking** UART send. It is currently rate-limited to 2 s and skipped during TX/scan/CW — good. Remaining risks:
- The 424 ms stall **also suppresses the 10 ms SysTick processing** while it runs (scheduler still fires, queueing work). If the send lands mid-RX, the radio can miss a tone/DTMF tail.
- Any future increase in packet size or baud change must re-validate the skip conditions.
- Recommended: chunk the send across ticks (e.g., 64 B per 10 ms tick) — no user-visible change, removes the stall entirely.

---

## 2. Memory / footprint optimization opportunities

### 2.1 Reclaim ~2 KB of RAM from hot spots (Medium effort, high engineering care)
| Area | Current | Opportunity | Risk |
|---|---|---|---|
| `SectorCache` 4 KB | full-sector cache for py25q16 | If EEPROM traffic is page-granular with write-back, a **half-sector (2 KB)** cache + write-through policy saves 2 KB RAM; measure first — the cache exists to make RMW sequences safe | Medium |
| `waterfallHistory` 1,024 B | 128x16 @ 4-bit | Row interval default 320 ms; if RAM is the scarce resource: 3-bit/px (768 B) or halve height to 8 rows (512 B) | Low-Medium |
| `gFrameBuffer`+`gStatusLine` 1,024 B | 8x128 pages | CW uses pages 3–6; spectrum uses 5–6. Page 0 == `gStatusLine` phys. Merging/overlay saves up to 128–256 B | Medium (display driver refactor risk) |

### 2.2 FLASH reduction candidates (~2–3 KB achievable)
- Biggest code objects: `APP_RunSpectrum` (5.8 KB), `UI_DisplayMain` (6.4 KB), `MENU_ProcessKeys` (2.8 KB), `SETTINGS_*` (5+ KB combined). Already `-Os`+LTO; remaining wins come from **feature gating**.
- `bitmaps.c`/`font.c` rodata is 10.6 KB total; font compression and sparse glyph packing remain the largest safe wins (~0.5–1.5 KB).
- Verify `-fno-common` / `-Wl,--sort-common` / `-Wl,--sort-section=name` for small alignment/padding savings in `.text`+`.rodata`.

---

## 3. CPU hot-path review (measured by inspection)

| Path | Verdict |
|---|---|
| LCD blit (SPI @ 6 MHz, DIV8) | ✅ Full-screen ~1.4 ms — good. Remaining tax: `SPI_WriteByte()` polls TXE+RXNE per byte (2 register polls × ~1,160 B/full-screen). A DMA push for blits would drop main-loop CPU to near zero during paints; framebuffer access is main-loop-only (invariant documented), so DMA is **safe to add** — worth doing for spectrum/waterfall |
| BK4819 bit-bang SPI w/ NOP delay | ✅ ~2.5–3× faster than SysTick version. See 4.1 for the interrupt-hazard caveat |
| Spectrum sweep `GetRssi()` (2 reads + glitch guard) | ✅ Sensible; second-read discard is correct for AGC settling |
| Waterfall `WATERFALL_Render()` | ⚠️ 16×128 = 2,048 px with per-row `%` and per-px level compare + Bayer lookup, executed on every render tick/pass. Precompute the two page row-masks (Bayer row pattern is 4-row periodic) or restrict render to the changed pages only |
| `iSqrt` / integer dB mapping | ✅ No floating point anywhere in app hot paths (verified `__aeabi_*double*` absent from ELF) — critical on Cortex-M0+ (no FPU) |
| printf float support | ✅ Disabled (`PRINTF_DISABLE_SUPPORT_FLOAT`) — no soft-float linkage |
| Main loop spin when not WFI | ✅/⚠️ During TX the loop deliberately runs hot (fast keys/audio bar). Not a defect, but TX drain is worth a test-bench measurement |
With FLASH at 91.8 % and RAM at 86.2 % (+ stack/heap reserve), there is no guard rail:
- The linker only fails once the binary literally does not fit; a "fits but no headroom" state passes silently.
- Stack is `_Min_Stack_Size = 0x300` (768 B). `UI_DisplayMain` (6.4 KB code), `APP_TimeSlice10ms.part.0` (6.4 KB), `APP_RunSpectrum` (5.8 KB) carry multi-level calls with `char buf[16..64]` frames. 768 B is plausible-but-unproven.

**Recommended:** compile with `-fstack-usage` (GCC) and/or add a stack watermark (fill stack w/ 0xAA at boot, report high-water mark via K5Viewer/debug UART). Then gate CI on the measured high-water mark.

### 1.3 Synchronous flash writes block the whole main loop (~300 ms/sector)
**Severity: High** · Files: `App/driver/py25q16.c` (SectorErase/PageProgram), `App/settings.c`, `App/app/app.c`
**Status: ✅ RESOLVED (2026-09-08) — feature `ENABLE_DEFERRED_FLASH_WRITES` (default ON).**
Design (write-back cache, not a queue — **zero static RAM added**): `PY25Q16_WriteBuffer()` no longer erases+programs synchronously; when a write requires an erase, the new sector image stays in the existing 4 KB `SectorCache` (dirty-flagged). `PY25Q16_FlushPendingWrite()` (called from `APP_TimeSlice10ms()` after `SETTINGS_SaveVfoIndicesFlush()` and UI rendering) performs erase + full-sector program + 64 B-chunk read-back verify with one retry. Coherency guarantees: (1) reads overlapping the dirty sector are served from the cache; (2) a write to a *different* sector evicts by flushing first; (3) all three reboot paths (`app.c` reduced-service, `menu.c` reset command, `app/uart.c` 0x05DD) flush before `NVIC_SystemReset()`; (4) `EEPROM_LastWriteVerified()` now combines the per-chunk and flush verifies. Multiple saves to the same sector between flushes coalesce into **one** erase cycle (less wear + fewer stalls). Measured cost: +168 B FLASH (110,548 → 110,716), RAM unchanged (14,144 B). Host unit tests 1/1 pass; both profiles build clean. Power-loss semantics: a dirty sector survives until flushed (≤ one 10 ms tick after the save, or immediately on any reboot path); if the battery is pulled in that window the save is stale — identical exposure to the pre-existing `RXTX_LOG` deferral, and `ENABLE_EEPROM_CRC` still guards integrity at boot.

`PY25Q16_SectorErase()` polls WIP — a sector erase is ~300 ms of **blocking** in the foreground. Call sites that run on the main loop thread:
- `SETTINGS_SaveSettings()` / `SETTINGS_SaveChannel()` / `SETTINGS_SaveVfoIndices()` (menu exit, key release, channel change)
- `RXTX_LOG_*` — already partially deferred (good, see below)
- `toggle_chan_scanlist()` / `SETTINGS_UpdateChannel(...)` → synchronous flash write on a keypress

While one of these runs, radio scanning, squelch monitoring, UI, and the 10 ms timeslice all stall. The commit history shows flash-write batching was tried and reverted (`revert flash write batching`); re-introduce it correctly:
- **Deferred-write queue** (the `rxtx_log.c` pattern): dirty-page flags + a single `"flush one sector per 10 ms tick"` worker, so a save is acknowledged immediately and the erase happens in the background.
- Keep the CRC/checksum + power-loss semantics (`ENABLE_EEPROM_CRC`) intact; write ordering matters more than speed.
- Never call `PY25Q16_SectorErase` inside key-handler code paths (`toggle_chan_scanlist`).
---

## 4. Real-time / concurrency risks

### 4.1 Bit-banged SPI is interruptible mid-frame
**Severity: Medium** · File: `App/driver/bk4819.c`

The BK4819 bit-bang has no IRQ guard. With SysTick every 10 ms, a 60–90 µs register op has roughly a 0.6–0.9 % chance per call of an ISR landing mid-frame and stretching bit timing. Most ops are non-critical (repeated writes are idempotent), but an RSSI/glitch read corrupted by a stray interrupt pollutes scan/AGC/AM-fix decisions.

**Recommended:** wrap **short critical bursts** (`BK4819_ReadRegister` for RSSI/AGC, squelch threshold writes) with `__disable_irq()`/`__enable_irq()` (`primitives.h`), or retry once on glitch anomaly. Do **not** disable IRQs for the whole init sequence (~40 registers — that would add ~10 ms interrupt latency).

### 4.2 Commented-out `SCHEDULER_Disable()` around serial command handling
**Severity: Low-Medium** · File: `App/app/app.c` (`APP_Update`, `APP_TimeSlice10ms`)

`UART_HandleCommand()` / VCP command execution can perform flash reads/writes and re-enters UI code. The scheduler-disable around them was commented out — meaning the 10 ms tick can now fire *inside* a serial command handler. No bug has been reported, and ISR work is shallow, but this deserves a documented decision (either restore the guard around only the long commands, or add a reentrancy flag).

### 4.3 `gFrameBuffer` invariant — correctly documented, needs enforcement
**Severity: Low** · `st7565.h:27-32`, `waterfall.h:33-36`

Good "IF you touch these from ISR/DMA, add synchronization" comments. If DMA blitting is ever added (see 3), this invariant changes and must be re-audited (double-buffer or transfer-from-shadow-copy).

---

## 5. Build toolchain observations

| Item | Verdict |
|---|---|
| Release `-Os` + `-flto=auto` + `-Wl,--gc-sections` + `nano.specs` | ✅ Good baseline; binary is efficiently packed |
| `-ffunction-sections -fdata-sections` + `-Wall` | ✅ Present. `-Wall` only — consider `-Wshadow -Wconversion -Wextra` in app code only |
| No `-Werror` in CI | ⚠️ Add for the firmware job so warnings become visible in the log |
| Compiler pins | ⚠️ Docker images pin versions, host builds don't (committed `build/` used 13.3.1; local toolchain is 14.3). Build both and diff sizes if ABI-sensitive |
| `CMAKE_BUILD_TYPE` inference | ⚠️ `CMakeLists.txt:8-10` sets Release default; consider explicit `-DCMAKE_BUILD_TYPE` in all docs/scripts |

---

## 6. Repository hygiene / de-bloating (developer-experience performance)

| # | Severity | Finding |
|---|---|---|
| 6.1 | **High** | Working copy still holds `archive/` **52 MB / 1,552 files** (gitignored, regenerable snapshots). Delete locally — it degrades search/indexing/code-nav in every IDE |
| 6.2 | **Medium** | `App/external/CMSIS_5/` **16.4 MB, 641 files** — full upstream CMSIS 5 repo, **never compiled** (verified: `compile_commands.json` has zero references). Remove and vendor only what is needed |
| 6.3 | **Medium** | `Drivers/CMSIS/DSP_Lib/` **9.1 MB, 309 files** — not compiled (only `Include/` + `Device/` headers are in the include path). Remove |
| 6.4 | **Medium** | Dead UV-K5/DP32G030 driver implementations — verified against `build.ninja` (never compiled) AND `arm-none-eabi-nm <elf>` (their exported symbols are absent from the shipped binary). ✅ **FULLY RESOLVED (2026-09-08):** Deleted `driver/adc.c/.h`, `driver/flash.c/.h`, `driver/spi.c/.h`, `driver/aes.c/.h`, `sram-overlay.c/.h` (10 files), removed all dead `ENABLE_OVERLAY` blocks (`board.c` include + `BOARD_FLASH_Init` + `board.h` decl; `app.c`, `menu.c`, `app/uart.c` includes + reboot-to-bootloader blocks collapsed to `NVIC_SystemReset()`; `#if !defined(ENABLE_OVERLAY)` include guards un-guarded keeping the active `py32f0xx.h` includes), and merged `eeprom_compat.c` into the canonical `eeprom.c` (I2C-EEPROM implementation was the dead one; flash-backed implementation kept). **Verified by same-toolchain before/after build of the ApeX preset:** identical FLASH (110,548 B) / RAM (14,144 B) and a **bit-for-bit identical ELF (0-byte diff)** — the deleted code provably contributed nothing. Host unit tests pass (17,556 checks, 0 failures). `grep` confirms zero residual references to the deleted files/symbols/`ENABLE_OVERLAY` in App/Core/tests. `flash.h` also deleted — after guard removal it had no remaining includers (its only live-file user was inside the dead `BOARD_FLASH_Init` block). |
| 6.5 | **Low** | Stray `tools/_` (874 B — partial Python patch script fragment). Delete |
| 6.6 | **Low** | `tools/misc/*.bpr/.bpf/.res` — Borland Delphi project artifacts. Delete |
| 6.7 | **Low** | `.git` object store ~159 MB with the working tree only 105 MB; large historical binaries (unbrick PNGs, old build snapshots). Optional: `git gc --aggressive` if clone time matters |
| 6.8 | **Info** | Local `main` is **11 commits behind origin/main** (all CI workflow fixes). Fast-forward to get the CI green state |
| 6.9 | **Info** | `tools/unbrick_k5_v1/media/` PNGs 4.4 MB each — legit tool assets, consider `git-lfs` if repo growth continues |
---

## 7. Boot-time observations

| Step | Delay | Notes |
|---|---|---|
| `ST7565_Init` | ~162 ms of fixed delays (120+1+1+40+Cmds) | Could overlap with RF/EEPROM init (probe both busses independently); reduce power-up handshake waits to datasheet minimums |
| Boot-mode detection | 20 ms × samples | Fine |
| PWRON-password lock loop | up to 500 ms | Only with feature enabled |
| `SETTINGS_InitEEPROM` + radio regs | flash reads + ~40 register writes | Serialized at boot; could interleave with logo display (logo already shows during `APP_Update`) |

Cold-boot to main screen is dominated by LCD init + EEPROM + BK4819 setup. Nothing alarming, but the LCD's 162 ms of delays is the cheapest win if boot time matters.

---

## 8. Testing coverage

| Module | Coverage | Gap |
|---|---|---|
| CRC | Host test (`test_crc.c`) | — |
| Frequencies / DCS / MDC1200 | Host tests (`test_frequencies.c`, `test_dcs.c`, `test_mdc1200.c`, fuzz) | Good given constraints |
| Scheduler / app / settings / drivers | None | **Highest-value new tests are host-side** for: `waterfall.c` (pixel math vs golden rows), `settings.c` save/CRC round-trip (shim flash), `scheduler.c` counter logic — all pure-logic and shimmable like the existing pattern |
| Full-firmware integration | None (no hardware in CI) | Manual checklist already documented in `PERFORMANCE_AUDIT_v7.6.10D.md` §4 — keep it up to date |

---

## 9. Prioritized action list (by ROI)

1. ✅ **DONE (2026-09-08):** CI firmware build + size gate (1.1, 1.2) — `firmware` job + `tools/ci/check_size.py`.
2. ✅ **DONE (2026-09-08):** Deferred-write flash architecture (1.3) — `ENABLE_DEFERRED_FLASH_WRITES` write-back cache; +168 B FLASH, 0 B RAM. Remaining: on-hardware validation incl. power-cut during the flush window.
3. **Chunked K5Viewer sends** (1.4) — removes a 424 ms stall class; ~half a day.
4. ✅ **DONE (2026-09-08):** Delete dead code / CMSIS-5 + DSP_Lib copies (6.2–6.6) — dead drivers (`adc/flash/spi/aes/sram-overlay/eeprom-compat`) removed; bit-for-bit identical firmware proven. CMSIS vendor-copy deletion still open.
5. **IRQ guard around critical BK4819 reads** (4.1) — low effort, removes a real (if rare) glitch source; verify with a stress scan.
6. **Stack watermark + -fstack-usage** (1.2b) — proves or disproves the 768 B stack assumption; ~half a day.
7. **DMA LCD blit** (3) — frees main-loop CPU during paints; medium effort, high care with the framebuffer invariant.
8. **Waterfall render micro-opt** (3) — precompute row masks; small but free when already touching the file.
9. **RAM reclamation** (2.1) — only if a new feature requires headroom; start with the SectorCache half-size experiment.
10. **Boot-time LCD delay trim** (7) — optional, only if cold-boot UX matters.

---

## 10. What is already in good shape (do not regress)

- Airtight main-loop / WFI sleep design; battery idle-drain benefit is real.
- LCD SPI 8× faster; BK4819 bit-bang 2.5–3× faster; both feature-flagged with documented fallbacks.
- No FPU in hot paths; printf float disabled — the two classic Cortex-M0+ perf killers are avoided.
- Framebuffer invariants documented.
- Memory maps and prior audit reports maintained in `documentation/` — easy to trace history.
- Host-side unit tests compile the real firmware sources (frequencies/dcs/crc/mdc1200).
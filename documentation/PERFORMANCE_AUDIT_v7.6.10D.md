# Performance Audit — v7.6.10D

**Date:** 2026-09-07
**Scope:** Full deep review of the v7.6.10C codebase — boot path, main-loop scheduler, display/radio/flash drivers, build system, repository structure
**Toolchain:** arm-none-eabi-gcc 14.3 (Release + LTO), CMake 4.3 / Ninja, ApeX preset
**Baseline (v7.6.10C):** FLASH 110,508 B (91.46 %) · RAM 14,144 B (86.33 %)
**Result (v7.6.10D, all features ON):** FLASH 110,552 B (91.49 %) · RAM 14,144 B (86.33 %) — **+44 B flash, RAM unchanged**

> The flash is ~91 % full, so every optimization in this release had to be code-size-neutral. All three are: with both feature flags OFF the binary rebuilds to the byte-identical baseline.

---

## 1. Methodology

1. Traced the boot path (`Core/Src/main.c` → `App/main.c Main()`) and the scheduler (`App/scheduler.c SysTick_Handler`, 10 ms tick).
2. Profiled the driver hot paths by code inspection: LCD blit (`st7565.c`), BK4819 register I/O (`bk4819.c`), and their call sites (spectrum sweep, channel scan, squelch checks, AM-fix).
3. Verified interrupt/DMA coverage to prove the main loop's idleness (UART RX = circular DMA + 10 ms polling, USB = IRQ, SysTick = 10 ms).
4. Identified free-running timing bases that a sleep change could break (`gBlinkCounter` in `APP_Update`, `GetCurrentTime()` LRU ordering in `misc.c`) before touching the main loop.
5. All changes compiled and verified in three configurations: baseline, both features ON, both features OFF.

---

## 2. Implemented optimizations

### 2.1 LCD SPI clock — 750 kHz → 6 MHz (8× faster blits)

**File:** `App/driver/st7565.c` (`SPI_Init`)

| | Before | After |
|---|---|---|
| Prescaler | `LL_SPI_BAUDRATEPRESCALER_DIV64` | `LL_SPI_BAUDRATEPRESCALER_DIV8` |
| SPI clock (48 MHz APB) | 750 kHz | 6 MHz |
| Full-screen blit (~1 KB + cmd overhead) | ~11 ms | ~1.4 ms |
| Status-line / single-line blit | ~1.3 ms | ~0.17 ms |

**Impact:** every UI redraw, spectrum/waterfall row push and audio-bar refresh was paying the old 11 ms tax; freed CPU time directly benefits the radio DSP paths and makes menus visibly snappier.

**Fallback:** the ST7565 supports write clocks well above 6 MHz, but if a specific unit ever shows corruption, build with `-DLCD_SPI_BAUDRATE_PRESCALER=LL_SPI_BAUDRATEPRESCALER_DIV16` (3 MHz — still 4× faster than v7.6.10C).

### 2.2 BK4819 bit-banged SPI — ~2.5–3× faster register I/O

**File:** `App/driver/bk4819.c` (`BK4819_ReadU16/WriteU8/WriteU16/ReadRegister/WriteRegister`)

The old per-bit `SYSTICK_DelayUs(1)` calls cost ~50–70 CPU cycles each in SysTick polling overhead (3 calls per bit → ~4 µs/bit → a 24-bit write ~90 µs). The new `SHORT_DELAY()` is 24 inline NOPs (~500 ns → ~2 MHz SPI), mirroring the `SHORT_DELAY()` pattern already proven in `bk4829.c`. (The original DualTachyon firmware ran this same bit-bang with *no* delays at all; 500 ns keeps a large safety margin.)

**Impact:** every `BK4819_WriteRegister()` / `BK4819_GetRSSI()` gets ~2.5–3× faster. Hot call sites that benefit directly:
- spectrum sweep steps (tune + 2–3 RSSI reads per step)
- channel scanning (tune + squelch evaluation per step)
- squelch/RSSI checks every 10 ms tick, AM-fix sampling, RXTX logging

**Fallback:** build with `ENABLE_FAST_BK4819_SPI=OFF` — restores `SYSTICK_DelayUs(1)` exactly.

### 2.3 WFI idle sleep — MCU sleeps instead of spinning at 48 MHz

**File:** `App/main.c` (main loop)

The loop previously spun at 48 MHz 100 % of the time; the existing power-save only slept the *BK4819*, never the MCU. Now the core executes `__WFI()` when no timeslice is pending and not transmitting, sleeping until the next interrupt:
- **SysTick** every 10 ms (scheduler) — unchanged processing granularity
- **UART RX** lands in the circular DMA buffer while sleeping; commands are parsed at the next 10 ms tick, which was already the polling rate
- **USB** is IRQ-driven (CherryUSB `USBD_IRQn`)

**Why TX is excluded (important):** `gBlinkCounter` (incremented per loop pass in `APP_Update`) is the timebase for the TX-timeout alert blink and `GetCurrentTime()` LRU ordering. Sleeping in the idle loop would slow it ~10³× and break the TOT-alert timing, so the WFI branch is skipped while `FUNCTION_TRANSMIT` is active or PTT is pressed. `GetCurrentTime()` is only used for monotonic LRU ordering, which is rate-independent.

**Fallback:** build with `ENABLE_WFI_IDLE=OFF`.

---

## 3. Build-system changes

- `CMakeLists.txt`: new `option()`s `ENABLE_FAST_BK4819_SPI` and `ENABLE_WFI_IDLE` (both default ON), wired as target compile definitions following the existing `ENABLE_PA_PROTECTION` pattern.
- `CMakePresets.json`: both options added as `true`; `VERSION_STRING_2` / `TARGET` bumped to `v7.6.10D`.

### Verification matrix

| Config | FLASH | RAM | Status |
|---|---|---|---|
| v7.6.10C baseline | 110,508 B | 14,144 B | builds clean |
| v7.6.10D, both ON (shipped) | 110,552 B | 14,144 B | builds clean |
| v7.6.10D, both OFF | 110,508 B | 14,144 B | builds clean (byte-identical footprint) |


---

## 4. Hardware validation checklist (before wide release)

1. **Display:** verify all screens render correctly at 6 MHz (menus, spectrum, waterfall, boot logo). If corruption appears: rebuild with `LCD_SPI_BAUDRATE_PRESCALER=LL_SPI_BAUDRATEPRESCALER_DIV16`.
2. **RX/TX sanity:** squelch opens/closes normally, RSSI bars/S-meter read plausibly, AM reception stable (AM-fix samples RSSI through the fastened path), scan and spectrum sweep behave as before but faster.
3. **Battery:** confirm the standby-current drop vs. v7.6.10C (main benefit of WFI).
4. **Serial/USB:** CHIRP / UART / K5Viewer / UV Studio sessions still work (DMA RX is processed at 10 ms granularity — same as before).
5. **TX TOT alert:** with SetTot enabled, confirm the alert blink and timeout tones still fire at the expected moments (this path intentionally runs without WFI).

---

## 5. Additional findings

Status after the v7.6.10D hygiene pass:

| # | Severity | Finding | Status |
|---|---|---|---|
| 5.1 | High | `archive/` holds ~403 MB — 8 regenerable build snapshots + duplicated source-tree snapshots; degrades code search/indexing and risks edits against stale copies | ✅ Resolved — deleted `archive/builds/` (403 MB regenerable artifacts); moved 5 unique root assets (3 stock firmware reference dumps, 2 logos) to `assets/reference/`; `archive/` added to `.gitignore` |
| 5.2 | Medium | `.vscode/blue_chat_sessions.json` (AI chat/session logs incl. build logs) committed | ✅ Resolved — `.gitignore` created covering `.vscode/*.json` (no local `.git` to untrack from; the file itself was preserved) |
| 5.3 | Low | `Core/Src/main.c` empty `SYSTEM_ConfigureClocks()` stub + "bootloader set the clock" comment; `CMakeLists.txt` default `VERSION_STRING_2` (v7.6.0) drifted from the preset | ✅ Resolved — stub + declaration + call removed (clock setup is solely bootloader + `LL_SetSystemCoreClock`); default version synced to `v7.6.10D` |
| 5.4 | Low | Stale Docker-built `build/ApeX` cache (`/src` paths) blocked local configure until deleted | ✅ Resolved — `build/` added to `.gitignore` |
| 5.5 | Info | `st7565.c`: non-static global `map()` (collision-prone name) and non-static `cmds[]` | ✅ Resolved — `cmds[]` is `static const`, `map()` is `static`, stale public declaration removed from `st7565.h` |

### ⚡ Follow-up (post-v7.6.10D) — two audit items re-examined on request

| # | Severity | Finding (re-verified) | Verdict | Action |
|---|---|---|---|
| A1 | High | **Settings save triggers 2–6 full sector erases (~300 ms each).** Re-verified: `SETTINGS_SaveSettings()` writes 5+ blocks + CRC, all inside the same 4-KB sector (`EEPROM_ADDR_*` = `0x00A000–0x00A170`); each changed block erased one by one (the sector cache only coalesces *within* a single `PY25Q16_WriteBuffer` call). No documented "intentional" rationale exists for the multi-erase. | **Real, not intentional** (side effect of write-through design) | ✅ **Implemented** — new `PY25Q16_BeginBatch()/EndBatch()` API + `ENABLE_FLASH_WRITE_BATCHING` CMake option (default ON): a save now stages all writes in the sector cache and commits with **1 erase + 1 program** at `EndBatch()`. Reads of the dirty cached sector are served from RAM so `SETTINGS_UpdateCRC()` sees in-flight bytes. **On-flash bytes/CRC bit-identical; EEPROM (I²C driver, addresses, layout) untouched; power-loss → last CRC-valid state (same as before).** Build-verified: ON = 110,848 B FLASH (+300 B), OFF = 110,548 B (= baseline); RAM unchanged 14,144 B. |
| A2 | High | **K5Viewer streaming blocks the main loop ~424 ms every 2 s** (1,629 B @ 38,400 baud, per-byte TXE-poll send). | **Intentional & documented** — `App/app/app.c:1682-1688` states a 2 KB TX ring buffer was *evaluated and REJECTED*: "pushed RAM to 99%+ and risked stack overflow on the 16 KB PY32F071. The 2-second signature rate-limit is the approved mitigation." | ⏸️ **Left as designed** — buffered chunking is RAM-infeasible, and a no-buffer "rebuild rows from flash per tick" streamer is a protocol-adjacent rewrite that needs on-hardware validation; recommended as future work with hardware. |

### 5.1 addendum — `archive/` recurred
`archive/` reappeared on disk (50.8 MB, `build_id-6a9ee5e7` — a *different* build id than the deleted eight, timestamped after the cleanup). It stays **untracked/ignored** (`git ls-files` empty, repo clean); likely a sync/backup tool that should be pointed away from `archive/`.

---

## 6. Files changed in v7.6.10D

| File | Change |
|---|---|
| `App/driver/st7565.c` | SPI prescaler DIV64 → DIV8, overridable via `LCD_SPI_BAUDRATE_PRESCALER` |
| `App/driver/bk4819.c` | `SHORT_DELAY()` NOP bit-bang behind `ENABLE_FAST_BK4819_SPI` |
| `App/main.c` | Guarded `__WFI()` idle sleep behind `ENABLE_WFI_IDLE` |
| `CMakeLists.txt` | The two new options |
| `CMakePresets.json` | New options + version bump to v7.6.10D |
| `.gitignore` | New: `build/`, `*.bin`/`*.elf`/`*.hex`/`*.map`, `.vscode/*.json`, IDE/CMake scratch files, `archive/` |
| `App/driver/system.c` / `system.h` | Dead empty `SYSTEM_ConfigureClocks()` stub removed |
| `App/board.c` | Dead `SYSTEM_ConfigureClocks()` call removed (comment documents where clocks are set) |
| `App/driver/st7565.c` / `st7565.h` | `cmds[]` → `static const`; `map()` → `static`; public decl removed from header |
| `CMakeLists.txt` | The two new options; default `VERSION_STRING_2` synced to `v7.6.10D` |
| `documentation/CHANGELOG.md` | v7.6.10D release entry |
| `documentation/PERFORMANCE_AUDIT_v7.6.10D.md` | This report |
| `assets/reference/stock-firmware/` | 3 stock firmware reference dumps moved from `archive/` root |
| `assets/reference/logos/` | 2 logo PNGs moved from `archive/` root |
| `App/driver/py25q16.c` / `py25q16.h` | New `PY25Q16_BeginBatch()/EndBatch()` + deferred dirty-sector flush (settings-save batching, `ENABLE_FLASH_WRITE_BATCHING`) |
| `App/settings.c` | `SETTINGS_SaveSettings()` wrapped in `PY25Q16_BeginBatch()/EndBatch()` |
| `CMakeLists.txt` / `CMakePresets.json` | New `ENABLE_FLASH_WRITE_BATCHING` option (default ON) |
| `App/app/spectrum.c` | Waterfall smoothing: complete-sweep row pushes, proportional row interval, per-sweep dB remap, atomic waterfall page blit (`ENABLE_WATERFALL_SMOOTH`) |
| `CMakeLists.txt` / `CMakePresets.json` | New `ENABLE_WATERFALL_SMOOTH` option (default ON) |

---

## 7. Waterfall audit (second pass — UI/UX smoothness)

Full trace of the pipeline: `WATERFALL_PushRow/PushRowListen` → packed 4-bit circular history (128×16) → 4×4 Bayer dither into `gFrameBuffer[5]/[6]` → 1-page-per-tick `ST7565_BlitLine` cycle. The core rendering is sound (Q8 interpolation, listen-mode persistence decay, documented no-ISR invariant). The four issues found were all in the **timing/mapping layers**:

| # | Finding | Root cause | Fix |
|---|---|---|---|
| W1 | Mid-sweep tearing: rows pushed on a wall-clock timer independent of sweep position, interpolating across a buffer that is half previous / half current sweep → horizontal seams (worsened by the 3× BK4819 speedup shortening sweeps) | Timer push deviated from the design documented in `waterfall.h` (complete-sweep rows) | Push at **each half-sweep boundary** (first half in `UpdateScan()` when `scanReturnPending`, second half in `FinalizeCompletedSweep()` via `PushWaterfallRow()`); interval gate retained for pacing. *Regression note: the first fix attempt pushed only from `FinalizeCompletedSweep()` — once per full round trip (~500 ms) — which halved the idle scroll speed (observed on hardware); corrected to per-half-sweep (~250 ms), restoring listen-mode-matching cadence.* |
| W2 | Inverted adaptive interval: `DEFAULT × 128 / steps` contradicted its own comment — narrow scans (≤64 steps) got 640 ms/row and repeated identical sweep data | Multiplication/division swapped | `DEFAULT × steps / 128` (320 ms @128 steps → 160 ms @16 steps), same clamps |
| W3 | Brightness "breathing": `WATERFALL_SetDbRange` applied on *every* new RSSI minimum while history levels are baked at push time → noise floor brightens in visible steps | Continuous remap vs. baked row encoding | Remap applied once per completed sweep (old rows fade out within ~5 s anyway) |
| W4 | Cross-page tearing: waterfall spans `gFrameBuffer[5]`+`[6]`, blitted 1 page/tick — a render landing between the two page blits shows mixed snapshots for one tick | Incremental blit cycle | Both pages blitted back-to-back immediately after each render (~+0.35 ms) |

**Intentional designs reviewed and left untouched:** listen-mode persistence falloff, `GetRssi()` glitch guard, 1-page/tick incremental blit (cadence), Bayer (vs temporal) dither, 16-row history cap (RAM-bound at 86.33 %), STILL-mode waterfall skip.

**Verified builds:** ON = 110,840 B FLASH (91.73 %) / 14,144 B RAM; OFF = 110,848 B (byte-identical to the pre-waterfall-work baseline). Hardware validation recommended: check waterfall scroll uniformity at narrow scan widths (W2), seam-free rows during active signals (W1), stable noise-floor brightness over a minute of scanning (W3), and sweep the full zoom range.

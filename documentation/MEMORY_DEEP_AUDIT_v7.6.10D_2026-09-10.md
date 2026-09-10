# Deep RAM & FLASH Memory Audit — v7.6.10D (2026-09-10)

> **Scope:** Reduce RAM/FLASH without disabling any feature, without UI/UX changes, and without performance regression.
> **Method:** ELF/map/nm analysis of the v7.6.10D baseline, fresh local rebuilds (GCC 14.3.1, Release + LTO, ApeX preset), and controlled optimization experiments. CI reference build: GCC 13.3.1 (Docker).
> **Constraint:** no feature flags turned off; no display/UI behavior changed; no hot-path slowdowns.

---

## 1. Baseline (hard numbers, verified)

| Profile | Region | Before | % | After this audit | % | Delta |
|---|---|---|---|---|---|---|
| ApeX (all features) | FLASH | 112,556 B / 118 KB | 93.15% | 112,556 B | 93.15% | — |
| ApeX (all features) | RAM | 14,144 B / 16 KB | 86.33% | **13,888 B** | **84.77%** | **−256 B** |
| ApeX-minimal | FLASH | — | — | 82,404 B | 68.20% | — |
| ApeX-minimal | RAM | — | — | **9,048 B** | **55.22%** | **−256 B** |

Section detail (ApeX, final): `.isr_vector` 192, `.text` 101,136, `.rodata` 10,700, `.init/fini_array` 8, `.data` 168, `.noncacheable` 352, `.bss` 12,572, heap+stack reserve 768.

---

## 2. Applied change (verified on both profiles)

### 2.1 Remove the unused 256-byte heap reservation — **RAM −256 B**

**File:** `Core/py32f071xb.ld`

```ld
- _Min_Heap_Size = 0x100;   /* required amount of heap */
+ _Min_Heap_Size = 0x0;     /* no heap: nothing calls malloc/calloc/realloc */
```

**Justification (verified):** a repo-wide symbol search (`malloc|calloc|realloc`) finds **zero** call sites — including CherryUSB and newlib glue. `_sbrk_r` is linked (38 B) but never called. The reservation existed only as linker-script insurance.

**Why it is safe:**
- The reserved area was never a real allocation — it was a *link-time fit check* between `_ebss` and the stack limit. With heap = 0 the same check now guarantees the 768 B stack cannot collide with `.bss`.
- Effectively the 256 B becomes extra stack growth headroom, which directly addresses the stack-depth concern raised in `DEEP_AUDIT_REVIEW_v7.6.10D.md` §1.2 (deep call chains in `main`/`APP_TimeSlice10ms`).
- Both feature-matrix extremes (ApeX, ApeX-minimal) build and link clean.
- The on-device "SRAM %" readout (`App/ui/welcome.c`, `build_usage()`) reads `_Min_Heap_Size` as an absolute linker symbol, so it automatically shows the corrected 84.77% — no display code change needed.

**Performance impact:** none (data-only linker arithmetic).

---

## 3. Experiments run and their measured results

| Experiment | FLASH | RAM | Verdict |
|---|---|---|---|
| `-Oz` instead of `-Os` (with LTO) | ±0 B (byte-identical sections) | ±0 B | **Rejected** — LTO converges `-Oz`/`-Os` for this codebase; `-Oz` would only add inlining risk |
| `-fwhole-program` + LTO | ±0 B | ±0 B | **Rejected** — LTO already internalizes everything |
| `-fmerge-all-constants` | −24 B | 0 | **Rejected** — noise; not worth a toolchain-file divergence from CI |
| Heap reservation 256 → 0 | 0 | **−256 B** | **Applied** (see §2.1) |

**Conclusion:** with `-Os -flto` already active, compiler-level levers are exhausted. Remaining gains require structural changes (§5).

---

## 4. Full RAM breakdown (`.data` + `.noncacheable` + `.bss` = 13,092 B static)

| Bytes | Symbol(s) | Owner | Assessment |
|---|---|---|---|
| 4,096 | `SectorCache` | `py25q16.c` | **Untouchable**: holds the complete new sector image between a deferred save and its flush (`ENABLE_DEFERRED_FLASH_WRITES`). A 2 KB cache cannot guarantee write-back coherency (see §5.3) |
| 1,024 | `waterfallHistory` | `waterfall.c` | Already 4-bit packed (128×16 px / 2). Shrinking degrades the waterfall UI |
| 896 + 128 | `gFrameBuffer` + `gStatusLine` | `st7565.c` | Display core; merging pages is a driver refactor with rendering risk for ≤256 B |
| 384 + 352 | `g_pyusb_udc` + `usbd_core_cfg` | CherryUSB | USB stack state (`.noncacheable` by design) |
| 448 | `mdc1200_vit_decisions` | `mdc1200.c` | Viterbi decoder working memory — required |
| 320 | `gEeprom` | `settings.c` | Live settings shadow |
| 256×4 | `VCP_RxBuf`, `VCP_Command`, `UART_Command`, `UART_DMA_Buffer` | `uart.c` (driver+app) | Protocol-bound: 256 B max K5 command frame (header + 128 B EEPROM payload + footer) and the DMA ring |
| 256 | `previousHash` | `screenshot.c` | Already optimized from a 1 KB frame copy to a 16-bit chunk fingerprint |
| 256 | `rssiHistory` | `spectrum.c` | Live sweep trace |
| 152 | `VCP_ReplyBuf` | `app/uart.c` | MAX_REPLY_SIZE(144) + header/footer |
| 128×7 | `dutyCycle` (backlight DMA), `gCW_TraceHistory`, `gCW_TracePeak`, `gScanProgressMemoryMap`, `gScanProgressMemoryExcludeOrdinalMap`, `peakHoldY`, `read_buffer` | various | Feature working sets, all ≤128 B |
| ~300 | `gCW_Message`/`gCW_DecodeText`/`gCW_TxSnapshot` (81 B each), `gListName`, `gFM_Channels`, misc | various | Fine-grained feature state |
| ~140 | alignment padding | linker | Sum of named symbols = 12,952 vs 13,092 section total — negligible |

### 4.1 RAM verdict
Static allocation is dominated by exactly two intentional design reserves (`SectorCache` 4 KB, display buffers ~1 KB) plus protocol-sized I/O buffers. **The 256 B heap removal was the only zero-risk reduction available.** Everything else trades feature fidelity, UI quality, or data integrity for ≤256 B.

---

## 5. FLASH breakdown and evaluated (rejected) reductions

### 5.1 Top `.text` consumers (local final ELF)
| Bytes | Symbol | Note |
|---|---|---|
| 7,680 | `main` | LTO inlined the full app init — not worth de-inlining churn |
| 6,552 | `APP_RunSpectrum` | spectrum/waterfall engine |
| 6,484 | `APP_TimeSlice10ms.part.0` | scheduler core |
| 6,276 | `UI_DisplayMain` | main screen rendering |
| 5,036 | `UI_DisplayMenu` | menu rendering |
| 3,360 / 3,264 | `MENU_ProcessKeys` / `MAIN_ProcessKeys` | key handling |
| 2,492 | `ProcessKey` | key handling |
| 1,976 / 1,964 | `MENU_AcceptSetting` / `SETTINGS_InitEEPROM` | settings |
| 1,104 + 450 | `_vsnprintf` + `_ntoa_long` | custom lean printf (float support compiled out; 179 sprintf call sites make replacement riskier than the ~1.5 KB upside) |

### 5.2 Top `.rodata` consumers (10,700 B total)
| Bytes | Symbol(s) |
|---|---|
| 2,732 | Fonts: `gFontBig` 1,316, `gFontSmall` 564, `gFontSmallBold` 564, `gFont3x5` 288, `gFontBigDigits` 220 |
| 632 | `MenuList` |
| 432 | `CW_CHAR_MAP` |
| 274 | `BITMAP_QR_GitHub_Compressed` + `BITMAP_QR_GitHub_Wiki_Compressed` (already compressed) |
| ~600 | `DCS_Options` 208, `gSubMenu_SIDEFUNCTIONS` 176, `cdc_descriptor` 158 |
| ~5,500 | UI strings + protocol tables (CTCSS, step table, band table, BK1080 register specs, etc.) |

### 5.3 Evaluated and rejected FLASH/RAM ideas
- **Duplicate string scan:** zero duplicated string literals ≥10 chars in the binary — LTO already merges them.
- **Font/QR optimization:** QR bitmaps are already compressed; fonts total only 2.7 KB and drive the entire UI.
- **Half-sector `SectorCache` (would free 2 KB RAM):** the deferred-write flush programs the *complete* new sector image held in the cache. With a half cache, that image would not survive between save and flush → data-loss window. Rejected.
- **`UART_Command` + `VCP_Command` union (would free 256 B RAM):** the two parse contexts (UART vs VCP port handlers in `app/uart.c`) are independent and ISR-tolerant by design; sharing one struct introduces cross-port re-entrancy risk for 256 B. Viable only as a future refactor with careful locking.
- **Scan-progress maps as bitmaps (~206 B RAM):** byte-per-channel → bitmap would add packing/unpacking code of roughly the same FLASH size and complicate `SCAN_PROGRESS` logic. Rejected.

---

## 6. Findings that affect future memory work

1. **FLASH is the binding constraint, not RAM.** ApeX is at 93.15% FLASH (~8 KB free) vs 84.77% RAM (~2.5 KB free above `.bss` beyond the 768 B stack). Every new feature must now be weighed in FLASH first.
2. **The ApeX-minimal profile has ~35 KB FLASH / ~7 KB RAM headroom** — any feature cut in a future pinch should be moved behind flags rather than deleted.
3. **LTO defeats classic micro-optimizations:** `-Oz` and `-fwhole-program` are provably no-ops here; don't spend time on them again.
4. **The 97% CI size gate (`tools/ci/check_size.py`) is the correct tripwire** — ApeX now sits ~4 points under the fail line and ~3 points under the warn line (90%): a single medium feature (~3.5 KB) crosses the warn threshold.
5. The pre-existing working-tree modifications (app.c, menu.c, welcome.c, ui_globals.h, docs) were **not touched** by this audit; the only build file changed is `Core/py32f071xb.ld`.

---

## 7. Verification performed

- `cmake` Release+LTO rebuild of the **ApeX** preset (GCC 14.3.1): links clean, RAM 13,888 B (84.77%), FLASH 112,556 B (93.15%).
- Rebuild of **ApeX-minimal**: links clean, RAM 9,048 B (55.22%), FLASH 82,404 B (68.20%).
- `arm-none-eabi-size -A` / `nm --size-sort -S` cross-checked section totals against the linker's memory-usage summary.
- On-device memory readout path (`build_usage()`) confirmed consistent with the new heap value (reads linker symbols, not constants).



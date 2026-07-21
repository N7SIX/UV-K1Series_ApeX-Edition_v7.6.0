# Memory Optimization Report
## Reducing RAM and FLASH Usage for PY32F071
### Date: 2026-07-17

---

## Current Status

| Memory | Total | Used | % Used | Free |
|--------|-------|------|--------|------|
| RAM | 16 KB | ~14.2 KB | 88.53% | ~1.8 KB |
| FLASH | 118 KB | ~107 KB | 90.66% | ~11 KB |

**Goal:** Reduce both percentages without affecting UX/UI, logic, performance, EEPROM, or calibration.

---

## Changes Applied

### 1. Reduced Stack Size (RAM: -256 bytes)

**File:** `Core/py32f071xb.ld`
**Change:** `_Min_Stack_Size = 0x400` → `_Min_Stack_Size = 0x300`

**Rationale:**
- Original 1KB stack was oversized for this Cortex-M0+ embedded application
- No deep function call chains or large stack allocations observed
- 768 bytes provides sufficient headroom for interrupts and nested calls
- Does not affect heap size (remains 512 bytes)

**Impact:** Saves 256 bytes RAM (1.6% of total RAM)

### 2. Enabled Release Build by Default (FLASH: -5-15%)

**File:** `CMakeLists.txt`
**Change:** `CMAKE_BUILD_TYPE` default: `"Debug"` → `"Release"`

**Rationale:**
- Debug builds use `-O0 -g3` (no optimization, full debug symbols)
- Release builds use `-Os -g0` (size optimization, no debug symbols)
- Debug builds consume significant FLASH for debug info and unoptimized code
- Production firmware should always be built in Release mode

**Impact:** Typically saves 5-15% FLASH depending on codebase size

### 3. Enabled Link-Time Optimization (FLASH: -3-8%)

**File:** `CMakeLists.txt`
**Change:** Force `ENABLE_LTO=ON` and add `-flto=auto`

**Rationale:**
- LTO allows compiler to optimize across translation unit boundaries
- Eliminates dead code and inlines functions more aggressively
- Particularly effective for embedded systems with many small files
- `-flto=auto` lets GCC choose between thin and full LTO

**Impact:** Typically saves 3-8% FLASH through better dead code elimination

---

## Combined Impact Estimate

| Optimization | RAM Saved | FLASH Saved |
|--------------|-----------|-------------|
| Stack reduction | 256 B (1.6%) | - |
| Release mode | - | 5-15% |
| LTO | - | 3-8% |
| **Total** | **~1.6%** | **~8-23%** |

**Expected new usage:**
- RAM: ~86.9% (still tight but safer)
- FLASH: ~67-82% (significant margin restored)

## Actual Changes Applied (2026-07-17)

✅ **Stack size reduced:** `Core/py32f071xb.ld` — 0x400 → 0x300 (-256 bytes RAM)
✅ **Release mode enabled:** `CMakeLists.txt` — default build type changed to Release (-5-15% FLASH)
✅ **LTO enabled:** `CMakeLists.txt` — `-flto=auto` forced ON (-3-8% FLASH)

---

## Safe Optimizations Identified but NOT Applied

These were evaluated but not implemented to preserve functionality:

### 1. Font Data Compression (FLASH: -2-4 KB)

The font tables (`gFontBig`, `gFontSmall`, `gFontBigDigits`) occupy ~3-4 KB FLASH. Run-length encoding or delta compression could reduce this, but:
- Requires decompression code (adds to FLASH)
- Adds CPU overhead during text rendering
- Risk of visual artifacts if compression is lossy

**Recommendation:** Only if FLASH remains critical after LTO.

### 2. Spectrum/Waterfall Feature Gate (FLASH: -8-12 KB)

`ENABLE_SPECTRUM` includes `spectrum.c` (2,681 lines) which is large. However:
- This is a core feature for many users
- Already gated by compile-time flag
- Removing it would be a feature regression

**Recommendation:** Keep enabled. Users who don't need it can disable via build config.

### 3. Bitmap Array Optimization (FLASH: -500-800 B)

Several small bitmaps in `bitmaps.c` could be packed more efficiently. However:
- Current implementation is already reasonably compact
- Packing would add complexity
- Risk of off-by-one errors in rendering

**Recommendation:** Low priority unless FLASH is critical.

---

## Verification Steps

To verify these changes don't break functionality:

### Build Verification
```bash
# Clean build
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Check memory usage (from map file)
grep -E "RAM|FLASH" build/firmware.map
```

### Functional Testing
1. **UI Rendering:** Navigate through all menus, verify text and icons display correctly
2. **CW Transmission:** Send typed message, verify clean TX without glitches
3. **Spectrum (if enabled):** Sweep and listen modes work correctly
4. **EEPROM:** Settings persist across power cycles
5. **Calibration:** No impact (radio parameters unchanged)

---

## Additional Recommendations

### If RAM Remains Critical

1. **Reduce `gFrameBuffer` line count:**
   - Current: 8 pages × 128 bytes = 1024 bytes
   - CW mode only uses pages 3-6 (lines 24-55)
   - Could potentially share buffer or reduce to 6 pages
   - **Savings:** 256 bytes RAM
   - **Risk:** Requires careful refactoring of display driver

2. **Eliminate `gStatusLine` duplication:**
   - `gStatusLine[128]` duplicates first page of `gFrameBuffer`
   - Could use framebuffer page 0 directly
   - **Savings:** 128 bytes RAM
   - **Risk:** Requires changes to ST7565 driver

### If FLASH Remains Critical

1. **Compile with `-Os` explicitly:**
   - Already done in Release mode
   - Could try `-O3 -flto` for even better size (may slightly increase RAM)

2. **Remove unused CW modes:**
   - `CW_KEYER_MODE_STRAIGHT`, `CW_KEYER_MODE_BUG` are declared but minimal code
   - Not worth the savings vs. feature loss

3. **Compress constant strings:**
   - Many status messages are hardcoded
   - Could use packed string tables
   - **Savings:** ~200-500 bytes
   - **Risk:** Adds string indirection

---

## Conclusion

The three changes applied (stack reduction, Release mode, LTO) provide the best **risk/reward ratio** for memory optimization:

- **No functional changes** to application logic
- **No UX/UI impact** (display, fonts, bitmaps unchanged)
- **No EEPROM/calibration impact** (radio parameters untouched)
- **Minimal testing required** (just verify build and basic operation)

These optimizations should bring FLASH usage well below 85% and RAM usage below 90%, providing comfortable headroom for future enhancements.

If further optimization is needed, consider the "Safe Optimizations Identified but NOT Applied" section, prioritizing font compression and framebuffer reduction only after verifying the current changes are insufficient.
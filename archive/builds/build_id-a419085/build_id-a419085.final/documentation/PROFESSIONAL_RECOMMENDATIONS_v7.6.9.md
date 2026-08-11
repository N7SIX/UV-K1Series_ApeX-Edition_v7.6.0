# Professional Recommendations — UV-K1Series ApeX-Edition v7.6.0/7.6.9D

> **Date:** 2026-08-03 (updated 2026-08-06)  
> **Reference:** `DEEP_AUDIT_REPORT_v7.6.9.md`  
> **Goal:** Actionable, prioritized recommendations that improve UX/UI cleanliness and radio performance **without** affecting current logic, EEPROM layout, or calibration data.

---

## Post-Recommendation Updates (v7.6.9F, 2026-08-08)

The following recommendations have been **implemented** since this document was written:

### Build System
- **Stock/NOGIT variant removed** — repository now builds exclusively as ApeX Edition
- **All working ApeX features restored** — charging, CTCSS tail phase shift, charge level display, NOAA, alarm, DTMF calling
- **Conflicting features disabled** — `ENABLE_REGA`, `ENABLE_EXTRA_UART_CMD`

### Performance
- **K5Viewer stuttering** — update interval reduced to 2s to minimize ~424ms UART blocking
- **Audio bar smoothness** — complete overhaul of `UI_DisplayAudioBar()`:
  - Update rate 150ms → 50ms
  - Symmetric smoothing added
  - Partial line clear reduces flicker
  - Line-only blit replaces full-screen blit
  - TX warm-up period eliminates initial peak spike from BK4819 DSP settling

### BUILD_ID
- **Verified** BUILD_ID is a Unix timestamp in hex (not a git hash) when no `.git` directory exists

### Code Quality Maintenance
- **`misc.h` refactor** — split into scoped modules under `App/globals/` for better navigation and maintainability
- **Include order cleanup** — `App/frequencies.c` normalized; duplicate mid-file includes removed
- **Missing API declarations** — exposed `FREQUENCIES_ClampGlobal()` and `FREQUENCIES_ClampToBand()` in `App/frequencies.h`
- **Doxygen documentation** — added `@brief` comments to core public headers: `frequencies.h`, `dcs.h`, `radio.h`, `functions.h`, `am_fix.h`, `audio.h`, `scheduler.h`, `bitmaps.h`, `font.h`, `board.h`, `version.h`
- **EEPROM struct annotation** — marked reserved/legacy fields in `EEPROM_Config_t` to preserve layout intent
- **Host-side unit tests** — added `tests/` with coverage for `frequencies.c`, `dcs.c`, and `crc.c`
- **Static-analysis CI helper** — added `tools/static-analysis/run_static_analysis.sh` and CMake target `static-analysis`
- **GitHub Actions CI** — added `.github/workflows/ci.yml` to run build, tests, and static analysis on push/PR
- **Bug fix** — restored TX red LED indicator by correcting `UI_MAIN_SetRxLed()` LED ownership during transmit

---

## Tier 1 — Safe One-Line / Trivial Fixes (Do These First)

These are zero-risk changes that don't touch radio logic, EEPROM, or calibration. They fix real bugs or clean up the codebase with no behavioral side effects.

### 1. Add `CS_Release()` to `ST7565_ContrastAndInv()` (H2)
**File:** `App/driver/st7565.c:306`  
**Risk:** None. The TODO is already there. Without this, the SPI bus is left in a claimed state after adjusting contrast/inversion from the menu, which can corrupt the next SPI transaction to either the LCD or the PY25Q16 flash.

```c
    // TODO: Release CS??
    CS_Release();   // <-- ADD THIS
}
```

### 2. Fix `ST7565_FillScreen()` (H1)
**File:** `App/driver/st7565.c:204-212`  
**Risk:** None. The function is currently broken — `value=0x00` clears nothing (loop runs 0 times). This means the boot-time screen clear in `ST7565_Init()` is a no-op.

The cleanest fix is to add a separate `FillLine` helper or pass `LCD_WIDTH` as the size:
```c
void ST7565_FillScreen(uint8_t value)
{
    CS_Assert();
    for (unsigned i = 0; i < 8; i++) {
        ST7565_SelectColumnAndLine(0 + 4, i);
        A0_Set();
        for (unsigned j = 0; j < LCD_WIDTH; j++)
            SPI_WriteByte(value);
    }
    CS_Release();
}
```

### 3. Add Bounds Checks to Inverse Render Functions (H3, H5)
**Files:** `App/ui/helper.c:137, 142` and `App/ui/helper.c:335`  
**Risk:** None. These are latent buffer underflow bugs. Adding guards doesn't change behavior for current call sites (which pass safe values) but prevents future crashes.

```c
// helper.c — UI_PrintStringSmallNormalInverse:
if (x_start > 0)
    gFrameBuffer[Line][x_start - 1] ^= 0x7F;
// ...
if (Line > 0) {
    for (uint8_t x = x_start; x < x_end; x++)
        gFrameBuffer[Line - 1][x] ^= 0x80;
}

// helper.c — GUI_DisplaySmallestInverse:
if (x < 2) x = 2;
```

### 4. Add Null Termination to `INPUTBOX_GetAscii()` (L3)
**File:** `App/ui/inputbox.c:43`  
**Risk:** None. Add one line:
```c
    inputBoxAscii[8] = '\0';
    return inputBoxAscii;
```

### 5. Fix Wrong Comment in `ST7565_ShutDown()` (M1)
**File:** `App/driver/st7565.c:370`  
**Risk:** None. Change `// VB=0 VR=1 VF=1` to `// VB=0 VR=0 VF=0 (all power off)`.

### 6. Remove Dead Code (M2, M5, M12, L7)
**Risk:** None. Remove:
- Empty `ST7565_HardwareReset()` (or make it a documented no-op)
- All commented-out code blocks in `main.c`, `helper.c`, `menu.c` (listed in M5)
- Empty `if` block in `menu.c:1632-1635`
- Duplicate `#include "../misc.h"` in `ui.c:44`

### 7. Uncomment Popup Border (L6)
**File:** `App/ui/helper.c:402-422`  
**Risk:** Minimal UX improvement. The popup currently shows floating text with no border. Uncommenting the border code gives visual context.

---

## Tier 2 — Safe Logic-Preserving Fixes (Do These Next)

These fix real bugs and improve performance/UX. They touch radio or app logic but **preserve the existing behavior** — they just add safety bounds, timeouts, or minor UX polish.

### 8. Bound the Interrupt-Drain Loop (C3)
**File:** `App/radio.c:798-806`  
**Risk:** Low. This is the most impactful single fix for radio responsiveness. Change `while(1)` to a bounded `for` loop with 10 retries max. The existing logic drains pending BK4819 interrupts — bounding it just prevents infinite stalls. If 10 retries isn't enough, the radio continues anyway (same as if the interrupt never cleared).

```c
for (uint8_t retry = 0; retry < 10; retry++)
{
    const uint16_t Status = BK4819_ReadRegister(BK4819_REG_0C);
    if ((Status & 1u) == 0)
        break;
    BK4819_WriteRegister(BK4819_REG_02, 0);
    SYSTEM_DelayMs(1);
}
```

### 9. Clamp DTMF Offset (C1)
**File:** `App/app/dtmf.c` — `DTMF_HandleRequest()`  
**Risk:** Low. Add a length check before computing `Offset`. If the EEPROM-stored code string is longer than the received DTMF buffer, there's no possible match anyway — returning early is correct behavior.

```c
const size_t string_len = strlen(String);
if (string_len > gDTMF_RX_index)
    return;  // impossible match
const int Offset = gDTMF_RX_index - string_len;
```

### 10. Add Key-Cancel to REGA Blocking Delay (C2)
**File:** `App/app/rega.c:108-187`  
**Risk:** Low. Full state-machine conversion is ideal but larger scope. The minimal safe fix is to replace `SYSTEM_DelayMs(1000)` with a poll loop that checks for EXIT key:

```c
for (uint16_t i = 0; i < 100; i++) {
    SYSTEM_DelayMs(10);
    if (KEYBOARD_GetKey() == KEY_EXIT) {
        REGA_Stop();  // or whatever the stop path is
        return;
    }
}
```

### 11. Add SPI Timeout (M3)
**File:** `App/driver/st7565.c:98-109`  
**Risk:** Low. Add a timeout counter to both `while` loops in `SPI_WriteByte()`. If the SPI hardware fails, the radio recovers instead of hanging forever.

### 12. Add Bounds Check to `ST7565_Gauge()` (M4)
**File:** `App/driver/st7565.c:314`  
**Risk:** None. Add `if (line >= FRAME_LINES) return;` at the top.

### 13. Fix `UI_DisplayFrequency` Centering (H4)
**File:** `App/ui/helper.c:180-213`  
**Risk:** Low. The `center` parameter doesn't work without leading spaces. Implement pre-computed centering (as in the commented-out version at lines 216-253). This improves frequency display alignment.

### 14. Fix `char_width` in `UI_PrintStringSmallNormalInverse` (L5)
**File:** `App/ui/helper.c:128`  
**Risk:** Low. Change hardcoded `7` to `ARRAY_SIZE(gFontSmall[0])` to match the actual font width. This fixes the inverse highlight extending 1px too far.

---

## Tier 3 — UX/UI Polish (Do When Convenient)

These improve the user experience without touching radio logic or EEPROM.

### 15. Standardize Menu Index Format (M9)
**File:** `App/ui/menu.c:774, 820`  
Pick one format (`%02u/%u` or `%2u.%u`) and use it in both layouts. The N7SIX format `01/45` is cleaner.

### 16. Slow Down VFO Marker Blink (M11)
**File:** `App/ui/main.c:954-970`  
The VFO marker blinks every 500ms during RX. Consider toggling every 1000ms (every other 500ms call) to reduce visual distraction:
```c
static uint8_t blinkDivide = 0;
if (++blinkDivide >= 2) {
    blinkDivide = 0;
    clean = !clean;
}
```

### 17. Replace Magic Numbers with Named Constants (L4)
**Files:** `App/ui/main.c`, `menu.c`, `status.c`  
Create a `ui_layout.h` with constants like:
```c
#define FREQ_X          32
#define FREQ_SMALL_X    113
#define COMPANDER_X     120
#define VFO_LABEL_X     107
#define MENU_SEPARATOR  48
#define STATUS_INDICATOR_X 69
```

### 18. Refactor Status Bar Position Tracking (M10)
**File:** `App/ui/status.c`  
Replace the fragile `x`/`x1` tracking with a running cursor and explicit spacing constants. Compute battery position from the right edge dynamically.

---

## Tier 4 — EEPROM/Calibration Investigation (Requires Careful Analysis)

### 19. Investigate EEPROM Corruption Bugs (C4)
**Files:** `App/settings.c`, `App/init.c`  

These are the most serious findings but require careful investigation before fixing. The audit identified:

1. **RSSI calib / S0-S9 address collision** — Verify the exact EEPROM addresses used by `gEEPROM_RSSI_CALIB` and `S0_LEVEL`/`S9_LEVEL`. If they overlap, the fix is to adjust the load/save addresses — but this must NOT change the EEPROM layout that CHIRP and existing radios depend on. The safe fix is to add a bounds check in `SETTINGS_InitEEPROM()` that sanitizes loaded values.

2. **Boot-time attribute writeback** — In `init.c`, check if the boot sequence writes channel attributes unconditionally. If so, add a "dirty flag" check so attributes are only written back if they changed.

3. **Power-loss window** — Add a CRC/checksum to the settings block. On load, verify the CRC; if mismatch, load safe defaults. This doesn't change the EEPROM layout — it adds integrity verification.

**Recommendation:** Before touching any EEPROM code, read `documentation/EEPROM_ARCHITECTURE.md` and map out the exact address space. Any fix here must be validated against the CHIRP driver (`CHIRP/n7six.ApeX.chirp.v7.6.9.py`) to ensure compatibility.

---

## Tier 5 — Performance Improvements (Medium Effort)

### 20. Chunk K5Viewer UART TX (H6)
**File:** `App/app/app.c:1650-1680`  
The v7.6.9B rate limiting (1Hz) helps, but the 424ms blocking TX still causes a momentary stutter. Split the 1629-byte packet into ~128-byte chunks sent across multiple 10ms timeslices. This requires a small TX state machine but preserves the K5Viewer protocol.

### 21. Reduce Keyboard Settling Delay (M7)
**File:** `App/driver/keyboard.c:195`  
Profile the actual RC settling time on the PCB. If 5-10µs suffices (likely, given the low capacitance), reduce from 15µs. This saves 25-50µs per key poll — small but free.

### 22. Unify Scanner Dwell Timer (H7)
**Files:** `App/app/scanner.c`, `App/app/chFrScanner.c`  
Ensure MR scan and frequency range scan use the same base dwell time, and that sub-audible detection time is additive (not replacing) the base dwell. This prevents missed signals on fast channels.

### 23. Improve Waterfall Persistence Precision (H8)
**File:** `App/app/waterfall.c`  
Replace the integer division decay with a fixed-point representation (e.g., `value * 128 / 100` instead of `value * decay / 100`) to prevent precision loss at low signal levels.

---

## What NOT to Change

- **`gEeprom` struct layout** — CHIRP and existing radios depend on the exact byte offsets
- **EEPROM addresses** — Any address change breaks compatibility with saved settings
- **Calibration values** — `gEEPROM_RSSI_CALIB`, `gBatteryCalibration`, etc. must not be touched
- **BK4819 register init sequence** — The init values in `BK4819_Init()` are hardware-tuned
- **Frequency band table** — The TX restriction logic depends on exact band boundaries
- **Cooperative scheduler architecture** — Don't introduce an RTOS; the current model works

---

## Recommended Implementation Order

| Priority | Items | Effort | Impact |
|----------|-------|--------|--------|
| **Immediate** | 1, 2, 3, 4, 5, 6, 7 (Tier 1) | ~2 hours | Fixes display bugs, cleans codebase |
| **Next** | 8, 9, 10, 11, 12, 13, 14 (Tier 2) | ~4 hours | Fixes radio responsiveness, DTMF safety |
| **Then** | 15, 16, 17, 18 (Tier 3) | ~3 hours | UX polish, maintainability |
| **Investigate** | 19 (Tier 4) | ~1 day | EEPROM integrity (needs careful analysis) |
| **When convenient** | 20, 21, 22, 23 (Tier 5) | ~1 day | Performance tuning |

**Total estimated effort:** ~2-3 days for all tiers.

---

## Bottom Line

The firmware is well-architected with clean modular separation. The 4 critical issues are real bugs that should be fixed, but only **C4 (EEPROM corruption)** requires careful investigation — the other 3 (C1-C3) have safe, logic-preserving fixes. The bulk of the audit findings are Tier 1 cleanup (dead code, missing bounds checks, wrong comments) that can be done safely in a single session without any risk to radio logic, EEPROM, or calibration.

---

## Implemented Fixes (v7.6.9C)

The following items from the audit recommendations have been implemented in v7.6.9C:

| Item | Description | Status |
|------|-------------|--------|
| H9 | UV Studio K5Viewer / RF Log protocol framing and feature keepalive | ✅ Implemented |
| H1 | `ST7565_FillScreen()` value/size collision | ✅ Implemented |
| H2 | `ST7565_ContrastAndInv()` missing `CS_Release()` | ✅ Implemented |
| H3 | `UI_PrintStringSmallNormalInverse()` bounds checks | ✅ Implemented |
| H5 | `GUI_DisplaySmallestInverse()` underflow guard | ✅ Implemented |
| L6 | `UI_DisplayPopup()` border restoration | ✅ Implemented |
| L3 | `INPUTBOX_GetAscii()` null termination | ✅ Implemented |
| L7 | `ui.c` duplicate include removal | ✅ Implemented |
| C3 | `RADIO_SetupRegisters()` bounded interrupt-drain loop | ✅ Implemented |
| C1 | `DTMF_HandleRequest()` clamped offset | ✅ Implemented |
| C2 | `REGA_TransmitZvei()` key-cancel support | ✅ Implemented |
| C4 | Boot-time attribute writeback destroying scan-exclusion bits | ✅ Implemented |
| M9 | Standardized menu index format | ✅ Implemented |
| M11 | Slower VFO marker blink rate | ✅ Implemented |
| M7 | Reduced keyboard settling delay | ✅ Implemented |
| H8 | Waterfall persistence precision | ✅ Implemented |

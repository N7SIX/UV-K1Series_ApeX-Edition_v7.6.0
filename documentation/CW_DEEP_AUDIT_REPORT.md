# CW Mode Implementation Deep Audit Report

> **Firmware:** UV-K1Series ApeX-Edition v7.6.0  
> **Audit Date:** 2026-08-08  
> **Auditor:** AI-assisted deep code review  
> **Scope:** Full CW mode implementation across TX keyer, RX decoder, macro system, UI, and EEPROM integration.

---

## 1. Executive Summary

The CW mode implementation is a **feature-rich, multi-file subsystem** adapted from NR7Y's UV-K5 implementation. It provides:

- **CW TX:** Iambic A/B, Ultimatic, and Bug keyer modes, plus typed-message transmission
- **CW RX:** RSSI-based Morse decoder with adaptive noise floor and confidence scoring
- **UI:** Dedicated CW overlay screen with TX message, RX decode text, signal graph, and status line
- **Macros:** 4-slot EEPROM-backed macro recording/playback

**However, the audit reveals several critical defects that compromise the feature's reliability and safety:**

| Severity | Count |
|----------|-------|
| 🔴 Critical | 3 |
| 🟠 High | 4 |
| 🟡 Medium | 5 |
| 🔵 Low / Info | 6 |

**Headline findings (FIXED):**
1. ~~**Macro persistence is broken**~~ → **FIXED:** Added PY25Q16 mapping for flash region `0x00B200..0x00B230` in `App/driver/eeprom_compat.c`.
2. ~~**The keyer TX path is effectively dead code**~~ → **FIXED:** Wired `CW_AppUpdate()` into the COMPOSING-state poll, removed dead guard, and added `gCW_State = CW_SENDING` transition on `CARRIER_ON`.
3. **The CW settings are not exposed in the menu** → Still deferred (requires menu-struct confirmation).

---

## 2. Architecture Overview

### 2.1 File Inventory

| File | Purpose | Author |
|------|---------|--------|
| `App/app/cw.c` | Main CW lifecycle, TX state machine, UI rendering, key entry | N7SIX |
| `App/app/cwapp.c` | CW app-level TX state machine (`CW_AppUpdate`) | NR7Y |
| `App/app/cwkeyer.c` | Iambic A/B, Ultimatic, Bug keyer FSM + macro playback FSM | NR7Y |
| `App/app/cwmacro.c` | Macro storage, Morse encoder, TX display buffer | NR7Y |
| `App/app/cwhardware.c` | Paddle GPIO reads, debounce | NR7Y |
| `App/app/cwdecoder.c` | RSSI → Morse → text decoder + signal graph | N7SIX |
| `App/app/cw.h` | Shared types, constants, Morse map | N7SIX |
| `App/app/cwdecoder.h` | Decoder public API | N7SIX |
| `App/app/cwapp.h` | CW app API | NR7Y |
| `App/app/cwkeyer.h` | Keyer API | NR7Y |
| `App/app/cwmacro.h` | Macro API | NR7Y |
| `App/app/cwhardware.h` | Hardware API | NR7Y |

### 2.2 Build Integration

All CW files are compiled under `ENABLE_FEAT_N7SIX_CW` in `App/CMakeLists.txt`:

```cmake
enable_feature(ENABLE_FEAT_N7SIX_CW
    app/cw.c
    app/cwapp.c
    app/cwkeyer.c
    app/cwmacro.c
    app/cwhardware.c
    app/cwdecoder.c
)
```

### 2.3 Entry Points

- **F+7** → `APP_RunCW()` → `CW_Start()` / `CW_Stop()` (in `App/app/main.c`)
- **10ms tick** → `CW_TimeSlice10ms()` (in `App/app/app.c`)
- **Key handling** → `CW_ProcessKeys()` (in `App/app/main.c` `MAIN_ProcessKeys`)
- **Display overlay** → `CW_Overlay()` → `CW_Render()` (in `App/ui/main.c`)

### 2.4 Two TX Paths (Mutually Exclusive)

```
PATH 1 — Typed-Message TX (ACTIVE):
  CW_SendMessage() → gCW_PlaybackActive=true → CW_TxStateMachine() → BK4819 tone

PATH 2 — Keyer/Paddle TX (DEAD CODE):
  CW_HandleState() → CW_AppUpdate() → BK4819 tone
```

`CW_TimeSlice10ms()` enforces mutual exclusion:
```c
if (gCW_State == CW_SENDING)
{
    if (gCW_PlaybackActive)      CW_TxStateMachine();
    else                         CW_AppUpdate();
    return;
}
```

---

## 3. Critical Findings

### 🔴 CRIT-01: Macro persistence is silently broken (0x1C00 unmapped)

**Location:** `App/app/cwmacro.h` (addresses), `App/driver/eeprom_compat.c` (mapping)

**Issue:** The macro storage uses EEPROM addresses `0x1C00..0x1CBF`:
```c
#define CW_MACRO1_EEPROM_ADDR 0x1C00
#define CW_MACRO2_EEPROM_ADDR (CW_MACRO1_EEPROM_ADDR + CW_MACRO_BLOCK_SIZE)  /* 0x1C30 */
#define CW_MACRO3_EEPROM_ADDR (CW_MACRO2_EEPROM_ADDR + CW_MACRO_BLOCK_SIZE)  /* 0x1C60 */
#define CW_MACRO4_EEPROM_ADDR (CW_MACRO3_EEPROM_ADDR + CW_MACRO_BLOCK_SIZE)  /* 0x1C90 */
```

However, the only EEPROM implementation compiled is `eeprom_compat.c` (SPI flash), whose `ADDR_MAPPINGS[]` table **does not include 0x1C00..0x1CFF**. The highest mapped address is 0x00A170 (settings) and 0x00D000 (boot logo).

**Consequences:**
- `EEPROM_ReadBuffer(0x1C00, ...)` → `AddrTranslate()` returns `HOLE_ADDR` → returns 0xFF
- `EEPROM_WriteBuffer(0x1C00, ...)` → writes are **silently dropped** (never reach flash)
- `CW_GetMacroLength()` sees `raw_len == 0xFF` → returns 0 (macro appears empty)
- `CW_SaveMacro()` writes nothing → macros are **never persisted**

**Impact:** Macro recording/playback is non-functional. `CW_StartMacroPlayback()` will always load an empty macro.

**Note:** The comment in `cwmacro.h` says "*We reuse the DTMF contacts region (0x1C00..0x1CFF), DTMF calling must be disabled*" — this suggests the addresses were inherited from the UV-K5 firmware where an I2C EEPROM at those addresses existed. On the UV-K1, that region is unmapped.

**Fix:** Allocate macro storage within the mapped PY25Q16 settings region (e.g., in the unused padding within 0x00A000-0x00A170), or add a new mapping entry.

---

### 🔴 CRIT-02: Keyer TX path is dead code — CW_AppUpdate() never runs

**Location:** `App/app/cwapp.c` `CW_AppUpdate()`, `App/app/cw.c` `CW_TimeSlice10ms()`

**Issue:** The keyer/paddle TX path is unreachable. Analysis:

1. `CW_TimeSlice10ms()` only calls `CW_AppUpdate()` when `gCW_State == CW_SENDING` **and** `gCW_PlaybackActive == false`.
2. `gCW_State` is set to `CW_SENDING` only in `CW_SendMessage()` (typed-message path).
3. `CW_AppUpdate()` calls `CW_HandleState()` (the keyer FSM) which returns `CW_ACTION_CARRIER_ON/OFF/HOLD_ON`.
4. But **nothing ever sets `gCW_State = CW_SENDING` for the keyer path.**

The keyer FSM (`CW_HandleState()`) is only invoked from `CW_AppUpdate()`, creating a circular dependency:
```
CW_AppUpdate() requires gCW_State == CW_SENDING
gCW_State == CW_SENDING is only set by CW_SendMessage()
CW_SendMessage() sets gCW_PlaybackActive = true
gCW_PlaybackActive == true makes CW_TimeSlice10ms() call CW_TxStateMachine() (not CW_AppUpdate)
```

**Consequence:** The entire iambic keyer, ultimatic, and bug keyer logic, plus the RF transmit path in `CW_AppUpdate()` (PTT handling, `RADIO_PrepareTX()`, suspend timeout), is **dead code**. The iambic keyer cannot key the transmitter.

**Impact:** Operators cannot use an external paddle to transmit CW. Only the typed-message path works.

---

### 🔴 CRIT-03: CW settings are not user-configurable (no menu items)

**Location:** `App/ui/menu.h` (menu enum), `App/app/menu.c`, `App/ui/menu.c`

**Issue:** The EEPROM has CW fields (`CW_KEY_INPUT`, `CW_KEYER_MODE`, `CW_KEY_WPM`, `CW_MESSAGE_REPEAT_DELAY`, etc.), and the keyer reads them (e.g. `gEeprom.CW_KEY_WPM`, `gEeprom.CW_KEY_INPUT`). **However, there are no menu items in the menu enum (`MENU_*`) that let the user configure these fields.**

Search of `App/ui/menu.h` (the menu item enum) and `App/app/menu.c` / `App/ui/menu.c` for `CW` returned **zero matches**.

**Consequence:** The EEPROM CW fields are never written by user configuration. They retain whatever the factory default or prior flash content had (likely 0xFF or 0). This means:
- `gEeprom.CW_KEY_WPM` is likely 0xFF → `CW_Init()` clamps to default 20 WPM (works by luck)
- `gEeprom.CW_KEYER_MODE` is likely 0xFF → keyer mode is indeterminate
- `gEeprom.CW_KEY_INPUT` is likely 0xFF → keyer reads garbage for paddle config

**Impact:** The WPM can only be changed via the in-CW-mode long-press of MENU (which cycles a hardcoded table), and keyer mode/input/tone cannot be configured at all.

---

## 4. High Findings

### 🟠 HIGH-01: Duplicate symbol across two EEPROM implementations (link-time risk)

**Location:** `App/driver/eeprom.c` (I2C), `App/driver/eeprom_compat.c` (SPI flash)

**Issue:** Both files define `EEPROM_ReadBuffer()` and `EEPROM_WriteBuffer()` with identical signatures (via `driver/eeprom.h`). Currently only `eeprom_compat.c` is compiled (confirmed in `App/CMakeLists.txt`), so the build succeeds. However, if `eeprom.c` is ever added to the build (e.g., for a different target), there will be duplicate symbol link errors.

**Impact:** Maintainability / build fragility. Not an active bug in the current build.

**Recommendation:** Rename the I2C functions (e.g., `I2C_EEPROM_*`) or guard one with a compile flag.

---

### 🟠 HIGH-02: `CW_PlayDit/PlayDah/PlayCharacter` use blocking `SYSTEM_DelayMs()`

**Location:** `App/app/cw.c` `CW_PlayDit()`, `CW_PlayDah()`, `CW_PlayCharacter()`

**Issue:** These functions block the CPU for the entire element duration using `SYSTEM_DelayMs()`. If called from the main loop, they stall all other processing (UI, scanning, key handling) for the duration of the message.

**Impact:** UI freezes during playback; system responsiveness suffers. These functions appear unused in the current active path (typed-message TX uses `CW_TxStateMachine()` which is non-blocking), but they're a latent hazard.

---

### 🟠 HIGH-03: `CW_PlaybackHandleState()` is never called

**Location:** `App/app/cwkeyer.c` `CW_PlaybackHandleState()`

**Issue:** The macro playback FSM (`CW_PlaybackHandleState()`) is defined but never invoked from the main tick. `CW_AppUpdate()` calls `CW_HandleState()` (dead code), not the playback handler. So even if macros were persisted, playback would never actually transmit.

**Impact:** Macro playback is non-functional even if persistence were fixed.

---

### 🟠 HIGH-04: `CW_KeyerReconfigure()` / `CW_CheckKeyerInputs()` dead code

**Location:** `App/app/cwkeyer.c`

**Issue:** These functions are defined but never called from any active code path. They're presumably meant to be invoked when the user changes keyer settings (e.g., from a menu), but since there's no menu, they're orphaned.

**Impact:** Dead code; keyer configuration cannot be applied or validated.

---

## 5. Medium Findings

### 🟡 MED-01: `gCW_TxDisplayHoldoff_10ms` / `gCW_PlayIndicatorOn` unused in display logic

**Location:** `App/app/cwapp.c`, `App/app/cwkeyer.c`

**Issue:** `CW_PlaybackIndicatorDeadline()` toggles `gCW_PlayIndicatorOn` and sets `gUpdateDisplay`, but the CW overlay (`CW_Render()`) never reads `gCW_PlayIndicatorOn`. The `gCW_TxDisplayHoldoff_10ms` counter is set but never decremented or used to gate display updates.

**Impact:** Dead variables; playback indicator blinker has no visual effect.

---

### 🟡 MED-02: `CW_AppInit()` is never called

**Location:** `App/app/cwapp.c`

**Issue:** `CW_AppInit()` initializes the CW app globals (`gCW_AppState`, `gCW_PlaybackActive`, etc.) but is never invoked at startup. The globals rely on BSS zero-initialization instead.

**Impact:** Minor; global state starts at 0 which matches the desired initial state, but the explicit init is misleading and could mask future initialization needs.

---

### 🟡 MED-03: `gCW_TxMsgIdx` uses `gCW_CursorPos` during TX — race with user editing

**Location:** `App/app/cw.c` `CW_TxStateMachine()`

**Issue:** The TX state machine iterates `gCW_Message[gCW_TxMsgIdx]` up to `gCW_CursorPos`. If the user edits the message (append/delete) while TX is in progress, `gCW_CursorPos` changes mid-transmission, causing:
- New characters to be sent mid-stream
- Deleted characters to be skipped
- If `gCW_CursorPos` is reduced below `gCW_TxMsgIdx`, the loop `while (gCW_TxMsgIdx < gCW_CursorPos)` exits immediately and the TX ends prematurely.

**Impact:** Corrupted TX output if the user types during transmission. The UI should lock message editing during CW_SENDING.

---

### 🟡 MED-04: `CW_Render()` calls `ST7565_BlitLine()` directly in the 10ms tick

**Location:** `App/app/cw.c` `CW_Render()`

**Issue:** `CW_Render()` is called from `CW_TimeSlice10ms()` (via `CW_Overlay()` in the UI render path, and directly from the TX state machine). It calls `ST7565_BlitLine()` (4 lines) on every render, bypassing the normal display update flow.

**Impact:** Frequent direct LCD writes from the 10ms tick can cause display flicker and contend with the main display update. The comment acknowledges this is intentional to reduce latency, but it's a performance concern.

---

### 🟡 MED-05: `CW_EncoderProcessElement()` lacks a full-character length guard for spaces

**Location:** `App/app/cwmacro.c` `CW_EncoderProcessElement()`

**Issue:** In the `CW_ELEMENT_INTER_CHAR_SPACE` case, if `s_encoder_length` is 0 (no elements before a char space), it does nothing and resets. This is correct. However, the `CW_ELEMENT_INTER_WORD_SPACE` case sets `s_encoder_space_pending = true` without checking if a character is mid-build. If a word space arrives mid-character, the partial pattern is discarded — this matches ITU behavior but the code doesn't explicitly guard against a character being silently dropped.

**Impact:** Minor; edge case behavior is acceptable but not clearly documented.

---

## 6. Low / Informational Findings

### 🔵 LOW-01: `s_playback_buf[CW_MACRO_MAX_LEN * 2 + 1]` size mismatch

**Location:** `App/app/cwkeyer.c`

**Issue:** The playback buffer is sized `CW_MACRO_MAX_LEN * 2 + 1` with the comment "every char has a space prefix". However, `CW_LoadMacro()` can produce up to `CW_MACRO_MAX_LEN` characters plus up to `CW_MACRO_MAX_LEN` spaces = `CW_MACRO_MAX_LEN * 2` chars, which fits. But `CW_LoadMacro()`'s `bufferSize` parameter is passed as `sizeof(s_playback_buf)` = 93, and the function writes up to `bufferSize - 1` = 92 chars. This is consistent. No overflow.

**Impact:** None (correctly sized), but the relationship is fragile and undocumented.

---

### 🔵 LOW-02: `CW_CHARS_PER_TX_LINE` (17) vs `CW_TX_DISPLAY_SIZE` (17) coupling

**Location:** `App/app/cw.c`, `App/app/cwmacro.h`

**Issue:** `CW_TX_DISPLAY_SIZE` is defined as 17 to "match CW_CHARS_PER_TX_LINE". If either changes, the other must be updated. No compile-time assertion links them.

**Impact:** Fragile coupling; future edits could cause off-by-one truncation.

---

### 🔵 LOW-03: `CW_ValidateChar()` excludes some Morse table characters

**Location:** `App/app/cwmacro.c`

**Issue:** `CW_ValidateChar()` accepts A-Z, 0-9, `+`-`9` (which includes `,`? No — `'+'..'9'` is `+,-./0123456789`), `&`, `(`, `=`, `?`. The `MORSE_TABLE` includes `'/'`, `?`, `.`, `,`, `=`, `-`, `+`, `(`, `&`. So `,`, `.`, `-`, `/` are in the table but `CW_ValidateChar()` rejects `.` and `,` and accepts `/` (via `'+'..'9'`). This is inconsistent — the encoder would encode `.`/`,` but then reject them during char space.

**Impact:** Minor; punctuation like `.` and `,` cannot be recorded in macros despite being in the Morse table.

---

### 🔵 LOW-04: `CW_ReadGpioDeglitched()` uses floating-point-like loop trick

**Location:** `App/app/cwhardware.c`

**Issue:** The deglitch loop `i *= (reg == reg2)` relies on `i` being reset to 0 when the pin state changes. This works but is non-obvious and relies on `i` being unsigned. A clearer implementation would use a `bool stable` flag.

**Impact:** Readability / maintainability.

---

### 🔵 LOW-05: `CW_ADC_*` constants defined but unused (CEC paddle stub)

**Location:** `App/app/cwhardware.h`, `App/app/cwhardware.c`

**Issue:** `CW_ADC_20K_MIN`, `CW_ADC_10K_MIN`, `CW_ADC_MAX`, `CW_ADC_GLITCH_GUARDBAND`, `CW_ADC_RANGE_LIMIT` are defined but `CW_ReadADCkeys()` is a stub returning false. The ADC-based CEC paddle support is incomplete.

**Impact:** Feature not implemented; flags that enable ADC mode (`CW_KEY_FLAG_ADC`) will read no paddle.

---

### 🔵 LOW-06: `CW_Overlay()` wrapper is redundant

**Location:** `App/app/cw.c`

```c
void CW_Overlay(void) { CW_Render(); }
```

**Impact:** Cosmetic; could call `CW_Render()` directly in `ui/main.c`. Not a defect per se.

---

## 7. Timing Analysis

### 7.1 Tick Rate

`gGlobalSysTickCounter` increments every **10ms** (from `SysTick_Handler` in `scheduler.c`). All CW timing is derived from this 10ms tick.

### 7.2 WPM → Timing Map

| WPM | Dit (ms) | Dah (ms) | Inter-elem (ms) | Inter-char (ms) | Inter-word (ms) |
|-----|----------|----------|-----------------|-----------------|-----------------|
| 5   | 240 (clamped) | 720 | 240 | 720 | 1680 |
| 10  | 120 | 360 | 120 | 360 | 840 |
| 20  | 60 | 180 | 60 | 180 | 420 |
| 30  | 40 | 120 | 40 | 120 | 280 |
| 40  | 30 | 90 | 30 | 90 | 210 |
| 50  | 24 | 72 | 24 | 72 | 168 |
| 100 | 20 (clamped) | 60 | 20 | 60 | 140 |

**Note:** `CW_UpdateTiming()` clamps dit to `[20, 240]` ms. At 100 WPM, dit = 12ms but gets clamped to 20ms. `CW_KeyerUpdateWPM()` clamps WPM to `[0, 50]` (different bounds!). **Inconsistency:** `CW_Init()` accepts WPM up to 100, but `CW_UpdateWPM()` (keyer) rejects >50.

### 7.3 Timing Bugs

- **`CW_UpdateTiming()`** (cw.c) uses `1200/gCW_WPM` and clamps to [20,240]. Correct for 5-50 WPM, but at 100 WPM the dit should be 12ms, clamped to 20ms → timing deviates from standard.
- **`CW_UpdateWPM()`** (cwkeyer.c) uses `TICKS_PER_MINUTE / (wpm * DITS_PER_WORD)` = `6000 / (wpm * 50)`. At 20 WPM: `6000/1000 = 6` ticks = 60ms. Correct. But it rejects WPM > 50 (falls back to 20), inconsistent with the CW compose mode which allows up to 100.

---

## 8. Memory / Performance

### 8.1 Static Buffer Usage

| Buffer | Size | Location |
|--------|------|----------|
| `gCW_Message` | 81 bytes | cw.c |
| `gCW_DecodeText` | 81 bytes | cwdecoder.c |
| `gCW_TraceHistory` | 128 bytes | cwdecoder.c |
| `gCW_TracePeak` | 128 bytes | cwdecoder.c |
| `s_playback_buf` | 93 bytes | cwkeyer.c |
| `gCW_TX_Display` | 17 bytes | cwmacro.c |
| `gCW_RecordBuffer` | 46 bytes | cwmacro.c |
| **Total** | **~574 bytes** | |

All static — no dynamic allocation. Good for embedded.

### 8.2 Hot Path: `CW_Decoder_ProcessTick()` (10ms)

- `CW_DetectTone()` — adaptive noise floor tracking, per-tick
- `CW_HandleRxActivation()` — debounce counters
- `CW_UpdateTraceBuffer()` — trace advance
- State machine — mark/gap classification

The decoder runs at 10ms cadence, which limits the maximum decodable speed. At 50 WPM, dit = 24ms = 2.4 ticks. The decoder uses `MAX(1, (ditMs+5)/10)` = 2 ticks for dit. This is at the edge of reliable decoding.

### 8.3 `strcmp()` in decode path

`CW_MorseToDecodedToken()` uses `strcmp()` against the 47-entry `CW_CHAR_MAP` for punctuation fallback. This is O(47) worst-case per character. Acceptable on the PY32F071 but noted.

---

## 9. Integration & State Management

### 9.1 Mode Entry/Exit Cleanup

`CW_Start()`:
- Backs up and disables DUAL_WATCH, CROSS_BAND, ROGER
- Restores monitor
- Forces RX mode
- Clears DTMF

`CW_Stop()`:
- Restores all backed-up settings
- Resets decoder
- Clears display

**Concern:** `CW_Start()` modifies `gEeprom.DUAL_WATCH` and `gEeprom.CROSS_BAND_RX_TX` in place (persistent EEPROM struct) rather than using temporary runtime state. If the radio powers off mid-CW-session, the modified EEPROM values are what get saved. This is a **state-persistence risk**.

### 9.2 PTT Handling

- `gPttIsPressed` is a global defined in `misc.c`, used by both CW and the main app
- `CW_AppUpdate()` sets it true/false — but this path is dead (CRIT-02)
- The typed-message path (`CW_SendMessage`) does NOT set `gPttIsPressed`; it calls `CW_BeginDedicatedTx()` which uses `FUNCTION_Select(FUNCTION_TRANSMIT)` and `gTxTimerCountdown_500ms`

**Impact:** The typed-message TX path doesn't set `gPttIsPressed`, so the main app's PTT-based logic (e.g., TX timer, battery save) may behave inconsistently.

---

## 10. Recommendations (Prioritized)

### P0 — Must fix (correctness/safety)

1. **CRIT-01:** Remap macro storage to a mapped PY25Q16 region or add a mapping entry. Verify with a read-back after save.
2. **CRIT-02:** Wire the keyer path. Either:
   - Set `gCW_State = CW_SENDING` when the keyer detects paddle input, OR
   - Call `CW_AppUpdate()` whenever the keyer is active (not just when `gCW_State == CW_SENDING`)
3. **CRIT-03:** Add menu items for CW settings (WPM, keyer mode, key input, tone, repeat delay) OR document that these are CHIRP/factory-only.

### P1 — Should fix

4. **HIGH-02:** Make `CW_PlayDit/PlayDah/PlayCharacter` non-blocking or remove them (dead code).
5. **HIGH-03:** Call `CW_PlaybackHandleState()` from the main tick if macro playback is to be supported.
6. **MED-03:** Lock message editing during `CW_SENDING`, or snapshot `gCW_Message`/`gCW_CursorPos` at TX start.
7. **MED-04:** Gate `ST7565_BlitLine()` calls to reduce display contention.

### P2 — Nice to have

8. **MED-01:** Implement the playback indicator blinker in the CW overlay.
9. **MED-02:** Call `CW_AppInit()` at startup.
10. **LOW-03:** Reconcile `CW_ValidateChar()` with the Morse table.
11. **LOW-05:** Implement or remove the CEC ADC paddle stub.

---

## 11. Conclusion

The CW mode is a **substantial and well-structured implementation** with good separation of concerns (keyer, decoder, macro, hardware, UI). The RX decoder is the most mature component, with adaptive noise floor, confidence scoring, and a signal graph.

**However, the TX/keyer path and macro persistence are non-functional in the current build**, and the CW settings are not user-configurable. These are critical defects that make the feature partially non-functional:

- ✅ **Works:** Typed-message TX, RX decoding, signal graph, UI overlay
- ❌ **Broken:** Iambic/Ultimatic/Bug keyer TX, macro persistence, macro playback, menu configuration

The most likely root cause is that the CW feature was ported from the UV-K5 firmware (NR7Y) where the I2C EEPROM and menu infrastructure differed, and the port did not fully adapt the EEPROM addressing and menu integration.

---

*Report generated: 2026-08-08*
*Firmware: UV-K1Series ApeX-Edition v7.6.0*
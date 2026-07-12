# CW Implementation Audit Report
## File: `App/app/cw.c` + `App/app/cw.h`
### Date: 2026-07-10

---

## 1. Architecture Overview

The CW system implements a **two-mode operation**:
- **Typed-message mode**: User composes text via phone-style multi-tap input, then sends via PTT
- **NR7Y keyer mode**: Hardware paddle/keyer FSM for real-time Morse generation (partial implementation)

The system also includes a **CW RX decoder** that runs in the compose state, passively decoding incoming Morse signals.

---

## 2. Strengths

### 2.1 Clean Modular Structure
- `CW_Init()`, `CW_Start()`, `CW_Stop()`, `CW_Toggle()` provide clean lifecycle management
- `CW_TimeSlice10ms()` drives all periodic processing from a single entry point
- `CW_Render()` separates display logic from business logic
- `CW_ProcessKeys()` handles all user input in one place

### 2.2 Robust Tone Detection
- Hysteresis-based threshold (`CW_OFFSET_HYSTERESIS = 5`) prevents chatter
- Dual debounce counters for tone-on/tone-off filtering
- Adaptive peak tracking with slow decay prevents drift
- Squelch state respected via `g_SquelchLost` check

### 2.3 Fuzzy Timing Classification
- Uses adaptive dit timing estimation with EMA smoothing
- Lower/upper hysteresis bands for dit/dah classification
- Gray-zone handling with ratio-based fallback

### 2.4 Good RX State Management
- `CW_RX_ACTIVATE_TICKS` (50ms) debounce prevents noise-triggered activation
- Immediate deactivation on signal loss clears stale decoded text
- Startup delay (500ms) prevents noise transients from triggering false decodes

### 2.5 Professional Timing Diagram Display
- Clean ON/OFF pulse train matching FLDIGI/CW Skimmer convention
- No extraneous baseline line on the signal graph

---

## 3. Issues Found

### 3.1 CRITICAL: Header Declares Externs Not Defined in cw.c

The following externs are declared in `cw.h` but **not defined** in `cw.c`:

| Extern | Status |
|--------|--------|
| `gCW_AppState` | **MISSING** |
| `gCW_RxThreshold` | **MISSING** |
| `gCW_PlaybackActive` | **MISSING** (static in cw.c: `static bool gCW_PlaybackActive`) |
| `gCW_PlaybackRepeat` | **MISSING** |
| `gCW_PlaybackMacroIndex` | **MISSING** |
| `gCW_MessageRepeatCountdown_500ms` | **MISSING** |
| `gCW_PlayIndicatorOn` | **MISSING** |
| `gCW_SuspendCounter_1ms` | **MISSING** |
| `gCW_TxDisplayHoldoff_10ms` | **MISSING** |
| `gCW_Recording` | **MISSING** |
| `gPttIsPressed` | **MISSING** |
| `gCW_KeyerManagesPtt` | **MISSING** |
| `gCW_KeyerUsingSD1` | **MISSING** |

**Impact:** Linker errors if any code references these. `gCW_PlaybackActive` is defined as `static` in cw.c which is a **different symbol** than the extern - the extern will remain unresolved.

### 3.2 CRITICAL: NR7Y Keyer/Playback API is Declared but Not Implemented

The following functions declared in `cw.h` have **no implementation** in `cw.c`:

| Function | Status |
|----------|--------|
| `CW_AppInit()` | **NOT IMPLEMENTED** |
| `CW_AppUpdate()` | Referenced in cw.c but not defined in this file |
| `CW_EndTxNow()` | **NOT IMPLEMENTED** |
| `CW_UpdateWPM()` | **NOT IMPLEMENTED** |
| `CW_KeyerResetRuntime()` | **NOT IMPLEMENTED** |
| `CW_KeyerReconfigure()` | **NOT IMPLEMENTED** |
| `CW_CheckKeyerInputs()` | **NOT IMPLEMENTED** |
| `CW_StartMacroPlayback()` | **NOT IMPLEMENTED** |
| `CW_StopPlayback()` | **NOT IMPLEMENTED** |
| `CW_PlaybackHandleState()` | **NOT IMPLEMENTED** |
| `CW_PlaybackIndicatorDeadline()` | **NOT IMPLEMENTED** |
| `CW_EncoderProcessElement()` | **NOT IMPLEMENTED** |
| `CW_AddToTxDisplay()` | **NOT IMPLEMENTED** |
| `CW_ClearTxDisplay()` | **NOT IMPLEMENTED** |
| `CW_GetTxDisplayTail()` | **NOT IMPLEMENTED** |
| `CW_StartRecording()` | **NOT IMPLEMENTED** |
| `CW_StopRecording()` | **NOT IMPLEMENTED** |
| `CW_LoadMacro()` | **NOT IMPLEMENTED** |
| `CW_SaveMacro()` | **NOT IMPLEMENTED** |
| `CW_GetMacroLength()` | **NOT IMPLEMENTED** |
| `CW_ValidateChar()` | **NOT IMPLEMENTED** |
| `CW_FormatMacroDisplay()` | **NOT IMPLEMENTED** |
| `CW_GetMorseForChar()` | **NOT IMPLEMENTED** |

**Impact:** If any code calls these, the linker will fail. These represent ~70% of the declared public API.

### 3.3 MEDIUM: Unused Variable `gCW_RxSignalHistPos`

Originally used for circular buffer indexing, now replaced by `memmove`-based linear shift. The variable:
```c
static uint8_t gCW_RxSignalHistPos = 0;
```
- Still declared static
- Still initialized in `CW_ResetRxDecoder()`
- **Never read** anywhere in the code

### 3.4 MEDIUM: Unused Scrolling Graph Logic

The variables `gCW_RxGraph` and `CW_AppendGraphElement()` maintain a scrolling `.`/`-`/` ` text buffer, but this buffer is **never rendered** to the display. The display uses the graphical `gCW_RxSignalHistory` instead.

### 3.5 MEDIUM: Potential Integer Division Issue

In `CW_Render()`, the `sprintf` for the status line uses:
```c
sprintf(status, "CW %uwpm %uRX %3d %c",
        gCW_WPM,
        (unsigned)gCW_RxDitTicks,   // uint16_t promoted to unsigned
        (int)gCW_RxLastRssi,         // int16_t promoted to int
        gCW_UpperCase ? 'U' : 'L');
```
The `%u` format expects `unsigned int`, but `gCW_RxDitTicks` is `uint16_t`. On this platform (ARM Cortex-M0+), `uint16_t` will be promoted to `int`, then cast to `unsigned int` by the varargs. This is technically correct but may trigger compiler warnings with `-Wformat`.

### 3.6 MEDIUM: Duplicate Morse Lookup Functions

Both `CW_CharToMorse()` and `CW_GetMorseForChar()` (declared in header) convert char to Morse. Only `CW_CharToMorse()` is implemented. The other is not - creating an API consistency issue.

### 3.7 LOW: Ambiguous Fallthrough Warning

In `CW_TxStateMachine()`:
```c
case CW_TX_PREAMBLE:
    gCW_TxState = CW_TX_ELEMENT;
    __attribute__((fallthrough));
```
The `__attribute__((fallthrough))` is a GCC extension. If the code is ever compiled with a different compiler (e.g., ARMCC, Clang without GCC compatibility), this will generate a warning or error.

### 3.8 LOW: Potential Buffer Overflow in `CW_AppendDecodedText`

```c
if (gCW_DecodeCursor >= CW_MSG_MAX_LEN)
{
    memmove(gCW_DecodeText, gCW_DecodeText + 1, CW_MSG_MAX_LEN - 1);
    gCW_DecodeCursor = CW_MSG_MAX_LEN - 1;
}
gCW_DecodeText[gCW_DecodeCursor++] = text[i];
gCW_DecodeText[gCW_DecodeCursor] = '\0';
```
When `gCW_DecodeCursor == CW_MSG_MAX_LEN`, it's reset to `CW_MSG_MAX_LEN - 1`, then incremented to `CW_MSG_MAX_LEN`, then index `CW_MSG_MAX_LEN` is written. The buffer is `[CW_MSG_MAX_LEN + 1]` so this is safe, but the logic is unorthodox - the `(` after the decrement means it's truly at max after decrement+increment.

Actually wait - `gCW_DecodeCursor = CW_MSG_MAX_LEN - 1` sets it to 79. Then `gCW_DecodeCursor++` makes it 80. Then `gCW_DecodeText[80] = text[i]` writes to index 80 which is within the `[81]` buffer. **This is safe but confusing.**

### 3.9 LOW: CW_Render Called Redundantly

In `CW_Render()`:
```c
memset(gFrameBuffer[CW_LINE_DECODE], 0, LCD_WIDTH);
CW_DrawSignalGraph();
```
But `CW_DrawSignalGraph()` already does `memset(gFrameBuffer[CW_LINE_DECODE], 0, LCD_WIDTH)` as its first operation. The clear is redundant.

---

## 4. Code Complexity Analysis

### 4.1 `CW_TimeSlice10ms()` - Too Many Responsibilities

This single function handles:
1. Startup delay management
2. TX state machine (typed message playback)
3. NR7Y paddle FSM
4. Monitor mode enforcement
5. Tone detection
6. RX activation/deactivation with debounce
7. Signal history recording
8. Multi-tap key timeout
9. Morse timing estimation
10. Morse element classification
11. Character finalization

**Recommendation:** Split into sub-functions:
- `CW_HandleRxActivation()` 
- `CW_UpdateSignalHistory()`
- `CW_ProcessMorseTiming()`

### 4.2 `CW_Render()` - Mixed Concerns

The render function mixes:
- TX text display
- RX text display
- Signal graph rendering
- Status line rendering

This makes it ~60 lines long with nested conditionals. Could benefit from splitting into `CW_RenderTxDisplay()`, `CW_RenderRxDisplay()`, `CW_RenderStatusLine()`.

---

## 5. Display Layout Summary

```
Row 3 (gFrameBuffer[3]): TX message line 1  OR  "RX: <decoded text line 1>"
Row 4 (gFrameBuffer[4]): TX message line 2  OR  "RX: <decoded text line 2>"
Row 5 (gFrameBuffer[5]): Timing diagram pulse train (CW signal graph)
Row 6 (gFrameBuffer[6]): "CW 20wpm 5RX -103 U" (status line)
```

---

## 6. Recommendations

### 6.1 Immediate (High Priority)
1. **Fix header/source mismatch**: Either implement all declared functions/variables in cw.c, or remove their declarations from cw.h
2. **Fix `gCW_PlaybackActive` linkage**: Change from `static` to non-static in cw.c to match the extern declaration, or change header to match

### 6.2 Medium Term
1. Remove unused `gCW_RxSignalHistPos`
2. Remove unused scrolling graph variables/lines (`gCW_RxGraph`, `CW_AppendGraphElement()`)
3. Split `CW_TimeSlice10ms()` into focused sub-functions
4. Remove redundant `memset` in `CW_Render()` before `CW_DrawSignalGraph()`
5. Add `const` to lookup tables like `CW_CHAR_MAP` and `CW_KEY_CHARS`

### 6.3 Low Priority
1. Review `__attribute__((fallthrough))` for portability
2. Add comments explaining the buffer rollover logic in `CW_AppendDecodedText()`
3. Consider moving keyer FSM functions to a separate file (`cw_keyer.c`) to keep cw.c focused on the typed-message mode and RX decoder
4. Document the relationship between the two CW subsystems (typed-message vs NR7Y keyer)

---

## 7. Summary

| Metric | Count |
|--------|-------|
| Total lines (cw.c) | ~1390 |
| Total lines (cw.h) | 182 |
| Declared functions (cw.h) | 37 |
| Implemented functions (cw.c) | ~18 |
| Missing function implementations | 19 |
| Declared externs (cw.h) | 16 |
| Defined externs (cw.c) | 1 |
| Missing extern definitions | 15 |
| Unused variables | 2 |

The implementation has **solid core functionality** for typed-message CW TX and passive RX decoding, but has **significant API mismatch** issues. The NR7Y keyer subsystem is declared in the header but almost entirely unimplemented in this file.
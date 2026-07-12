# Deep Study Audit: CW Implementation (cw.c) — Post-Fix Summary

## Executive Summary
The CW implementation is split across multiple files with two parallel TX paths. `cw.c` contains a self-contained non-blocking FSM for typed-message playback, while `cwkeyer.c`/`cwapp.c` provide the NR7Y-derived paddle keyer and RF state machine. The architecture is generally sound, but there were **known design inconsistencies, potential race conditions, and several areas needing refactoring**.

---

## File Inventory

| File | Lines | Role | Origin |
|------|-------|------|--------|
| `App/app/cw.c` | 1262 | ApeX CW mode: UI, typed TX, RX decoder, key processing | N7SIX |
| `App/app/cw.h` | 182 | Shared header for both TX subsystems | N7SIX |
| `App/app/cwkeyer.c` | 732 | Keyer/paddle FSM, macro playback | NR7Y |
| `App/app/cwkeyer.h` | 66 | Keyer interface | NR7Y |

---

## Architecture Overview

```
CW_TimeSlice10ms (10ms tick from app.c)
    ├── if (gCW_ActiveState && gCW_PlaybackActive)
    │       CW_TxStateMachine()          ← cw.c typed playback
    ├── else if (gCW_ActiveState)
    │       CW_AppUpdate()               ← cwapp.c → invokes keyer or playback
    │
    ├── CW_IsRxTonePresent()            ← RX decoder
    ├── CW_UpdateRxDitEstimate()
    └─ CW_FinalizeRxCharacter()
```

---

## Section 1: cw.c Detailed Analysis

### 1.1 Data Structures & State

**Global State Variables (lines 96-133)**
```c
CW_State_t gCW_State;
char       gCW_Message[81];
uint8_t    gCW_CursorPos;
uint8_t    gCW_WPM;
uint16_t   gCW_ToneFreq;
// Plus ~20 static variables for TX FSM, RX decoder, display, key processing
```

**Findings:**
- `gCW_State` is both **extern** (in cw.h, line 84) and **defined** in cw.c. This is correct.
- `gCW_Message` size is 81 bytes. `CW_MSG_MAX_LEN` is 80. OK, but the cursor can wrap and mulmove truncates correctly.
- Static variables for RX decoder heavily overlap with cwapp.c's `gCW_AppState` and `gCW_RxLastRssi` (lines 262). These serve different purposes but double-buffer state.

### 1.2 Morse Lookup (lines 52-68)

```c
static const CW_CharMap_t CW_CHAR_MAP[] = {
    {'A', ".-"}, ... {'Z', "--.."},
    {'0', "-----"}, ... {'9', "----."},
    {'.', ".-.-.-"}, {',', "--..--"}, {'?', "..--.."}, ...
};
```

**Findings:**
- Full Morse table included. `CW_ALNUM_ONLY` (line 43) is defined as `1`, so punctuation is **compile-time disabled** in `cw.c` lookups.
- `CW_MorseToChar()` returns `'?'` for disallowed chars under `CW_ALNUM_ONLY`. Silent failure.
- `CW_CharToMorse()` returns `NULL` for disallowed chars.

### 1.3 Typed-Message TX FSM (lines 532-688)

This is a **non-blocking state machine** driven at 10ms intervals.

**States:**
```
CW_TX_IDLE → CW_TX_PREAMBLE → CW_TX_ELEMENT → CW_TX_ELEM_GAP
    ↕ CW_TX_CHAR_GAP → CW_TX_ELEMENT
    ↕ CW_TX_WORD_GAP → CW_TX_ELEMENT
    → CW_TX_TAIL → CW_TX_IDLE
```

**Key Variables:**
- `gCW_TxTimer` (uint8_t) - counts down 10ms ticks. Max 255 → 2.55s max duration. **Acceptable since dit at 5WPM is 240ms and element gaps are similar.** But dah+gap+interchar gap at 5WPM = 240 + 240 + 240 = 720ms. OK. At 20WPM dah=180ms, total=540ms. Well within 255.
- `gCW_TxMorse` - pointer into static table; safe.
- `gCW_TxPrevWasSpace` - suppresses word gaps at start/trailing spaces. **Good.**

**Potential Issue - String Terminator:**
Line 628: `if (gCW_TxMorse == NULL || gCW_TxMorseIdx >= strlen(gCW_TxMorse))`
- `strlen()` is called in the 10ms ISR path. Inefficient but not catastrophic given tiny strings.
- The return to `CW_TX_ELEMENT` without resetting `gCW_TxMorse` relies on it still pointing to the char's pattern. But line 615 sets `gCW_TxMorse` explicitly. OK.

**Potential Issue - Preamble:**
Line 566: `gCW_TxTimer = 5;` (50ms preamble) then state `CW_TX_PREAMBLE`. Falls through to ELEMENT. Preamble is just a delay before first element, not a CW "dit" sequence. **Unconventional but harmless.**

### 1.4 RX Decoder (lines 116-807)

**Adaptive Dit Estimation:**
```c
gCW_RxDitTicks = (3 * gCW_RxDitTicks + observedMarkTicks + 1) / 4;
```
- IIR filter with alpha=0.25 (3/4 old + 1/4 new). Good for slow adaptation.
- Clamped to [1, 40] ticks (line 339-342). **40 ticks at 10ms tick = 400ms max dit estimate.**
- Initial estimate: `(gCW_DitMs + 5) / 10` (line 256). At 20WPM = 60ms dit = 6 ticks. Good.

**Signal Detection:**
```c
int16_t peakRssi = -120;  // updated dynamically
openLevel = peakRssi - 1;
closeLevel = peakRssi - 5;
```
- Uses `g_SquelchLost` to **force immediate false** (line 277). This prevents decoding background noise during monitor mode. **Good design.**

**RSSI Noise Tolerance:**
- `CW_OFFSET_HYSTERESIS = 5` → ~0.5dB at typical resolutions.
- `CW_RX_DEBOUNCE_TICKS = 1` → virtually no debouncing. This is **minimal** but since we run at 10ms, it's one sample.

**Invalidation Guards:**
- `if (observedMarkTicks < 3) return;` - ignores noise bursts <30ms.
- `if (observedMarkTicks > (gCW_RxDitTicks * 4)) return;` - ignores spurs >4x dit. Good.

**Finalization Logic:**
- ~~Line 800: in silence, finalize on `finalizeInSilenceGapTicks`. For multi-element codes, uses `charGapTicks`; for 1-char codes uses `singleElementGapTicks` (5*dit). This is actually incorrect for single-char codes — a final char in a word will also use 7*dit if word gap passes.~~ **FIXED:** `singleElementGapTicks` removed; now uses `charGapTicks` (3*dit) for all code lengths, matching standard spacing.

### 1.5 Key Processing (lines 1014-1243)

**Multi-tap Keypad (lines 1095-1146):**
- Keys 1-9 cycle through characters in `CW_KEY_CHARS`.
- Uses `gCW_KeyTick` timeout of 80 ticks = 800ms.
- `gCW_PrevKey` and `gCW_PrevLetter` track multi-tap position.

**Notable Bugs/Issues:**
1. **KEY_0 handling (lines 1150-1168):**
   ```c
   if (gCW_PrevKey == KEY_0 && gCW_CursorPos > 0 && gCW_Message[gCW_CursorPos - 1] == ' ')
   {
       // Toggle space <-> '0'
       gCW_CursorPos--;
       gCW_Message[gCW_CursorPos] = '\0';
       CW_AppendChar('0');
       gCW_PrevKey = KEY_0;
   }
   ```
   - Multiple KEY_0 presses toggle between space and '0'. **Nice.**
   - BUT: `gCW_PrevLetter = 0;` is NOT reset in the else branch. The `gCW_PrevLetter` variable is used only for multi-tap on keys 1-9, so this is **harmless**.

2. **KEY_STAR (lines 1193-1197):**
   - Toggles case. **Wait — there's already KEY_SIDE2 doing the same (lines 1186-1191).** Duplicate function bound to two keys. **Fine, but redundant.**

3. **KEY_F (lines 1212-1221):**
   - `if (!bKeyHeld)` clears message. But bKeyHeld is only true for long-press detection at the front; if KEY_F is both pressed and held, the function is called once for press (bKeyPressed=true, bKeyHeld depends on timing). The logic is correct.

### 1.6 CW_ToneOn/Off (lines 345-353)

```c
static void CW_ToneOn(void)  { BK4819_TransmitTone(true, gCW_ToneFreq); }
static void CW_ToneOff(void) { BK4819_EnterTxMute(); }
```

**FIXED:** Added explicit documentation comment warning that `CW_PlayDit/Dah/Character()` use `BK4819_PlayToneRaw()` which blocks, and must NOT be called from the 10ms tick or `CW_TxStateMachine()`. The typed-message TX path correctly uses `CW_ToneOn/Off()` with `gCW_TxTimer` for non-blocking timing. **No further action needed.**

---

## Section 2: cwapp.c Analysis (NR7Y TX State Machine)

### 2.1 TX State Machine (lines 61-145)

**State Machine:**
```
ACARIO_NONE → CARRIER_ON → AUDIO on, RADIO_PrepareTX
CARRIER_OFF → EnterTxMute → SUSPENDED
CARRIER_HOLD_ON → keep PTT
SUSPENDED + 200ms → CW_EndTxNow()
```

**Findings:**
- ~~`CW_EndTxNow()` (lines 39-56) does hard cleanup: `EnterTxMute`, `ExitSubAu`, `RADIO_SetupRegisters(false)`. This is more aggressive than the typed playback's `CW_EndDedicatedTx()` (cw.c lines 418-432).~~
- **FIXED INCONSISTENCY:** `CW_EndDedicatedTx()` now also calls `RADIO_SetVfoState(VFO_STATE_NORMAL)` and `RADIO_SelectVfos()` to match `cwapp.c` behavior, while preserving the anti-PA-pop comment about avoiding redundant `RADIO_SetupRegisters(false)`.
- **SUSPEND counter:** `gGlobalSysTickCounter` (1ms) — checks `elapsed >= 200` (200ms). This is the "carrier-off timeout" for paddle CW.

### 2.2 CW_Action_t Bridge

The keyer and playback engines in cwkeyer.c return `CW_Action_t`. This is clean separation. But:

**CONCERN:** Line 607-608 in cwapp.c:
```c
else if (elapsed_gap < s_ext_gap_count) {
    // fast qSKCC-style (augmented) iambic opening
    ...
}
```
`s_ext_gap_count = 3 * dit_ticks / 2` (line 164). This is 1.5*dit. **This is a non-standard "augmented" inter-character gap.** Standard is 3*dit. This may cause premature character finalization on short gap codes.

---

## Section 3: cwkeyer.c Analysis

### 3.1 Keyer FSM States

```
IDLE → ACTIVE_ELEMENT → INTER_ELEMENT_GAP → INTER_CHAR_GAP → INTER_WORD_GAP → IDLE
```

**Standard iambic behavior** for A/B/Ultimatic/Bug/Straight modes.

### 3.2 Iambic Logic (lines 564-655)

**Mode A:** queue pending alternate only on **rise** edges.
```c
if (s_active_is_dit && in.dah_rise) { s_pending_alternate = true; }
```
FIXED: looks at `dah_rise` (0->1 transition).

**Mode B:** queue pending on **state** during active element (dah queued on dit), then on **rise** in gap.
```c
if ((!s_active_is_dit) && (elapsed_elem < s_dit_count)) {
    if (in.dit_rise) { s_pending_alternate = true; }
}
```
This matches typical iambic-B (weighted) behavior.

**Potential Bug:** Lines 607-616 (inter-element gap):
```c
if (!s_pending_alternate) {
    // re-read keys to catch late presses
    CW_ReadKeys(&in);
    if ... // same logic as active element
}
```
But `in` is already read only on the rising/latched check at the gap start. Calling `ReadKeys` here **without storing the result back** for the main `have_next` logic is misleading — it reads `in` again on line 628. This is **redundant but not harmful.** However, the fact that `p_CSW_Input` might have changed between reads introduces a **two-sampler inconsistency**.

### 3.3 Bug Keyer (lines 421-516)

Implements semi-automatic bug (dit timed, dah manual hold).
- `s_elem_deadline_extra_ms = gEnableSpeaker ? 0 : 20` — adds 20ms for piezo latency compensation. **Good.**

### 3.4 Macro Playback (lines 196-327)

Uses bit-packed pattern. `s_play_char_pattern` stores elements as LSB-first (0=dit, 1=dah).
- Playback FSM is correct.
- Interrupts on paddle input (line 237): `if (in.dit || in.dah) { CW_StopPlayback(); }` — aborts on paddle press. **Good.**

---

## Section 4: Cross-Cutting Issues

### 4.1 TX Path Division

There are **two separate transmit implementations**:

| Feature | cw.c | cwapp.c + cwkeyer.c |
|---------|------|----------------------|
| Typed message playback | ✅ CW_TxStateMachine | ❌ |
| Paddle/keyer | ❌ | ✅ Keyer FSM |
| Macro playback | ❌ | ✅ CW_PlaybackHandleState |
| RF control | Calls BK4819 directly | Returns CW_Action_t |
| End-TX cleanup | CW_EndDedicatedTx | CW_EndTxNow |

**This is confusing but functionally separated.** cw.c's path is invoked when `gCW_PlaybackActive` is set by typed-message code. cwapp.c's path is invoked only when a keyer/macro is active.

**CRITICAL:** Both paths modify `gCW_State`. If `gCW_PlaybackActive` is ever true while keyer is also running, they conflict. The guard in `CW_TimeSlice10ms` (line 712):
```c
if (gCW_ActiveState && gCW_PlaybackActive)
    CW_TxStateMachine();
else if (gCW_ActiveState)
    CW_AppUpdate();
```
This is serialized, so no race. **Good.**

### 4.2 RX Mode Overrides (lines 110-114, 814-844)

`CW_Start()` forces:
- DUAL_WATCH=OFF
- CROSS_BAND=OFF
- gMonitor=true
- Function_MONITOR

And backs up/restores in `CW_Stop()`. **Good.** But line 844 clears `gCW_DecodeText` AFTER `CW_Init()` already cleared message. Redundant.

### 4.3 Timing Precision (lines 140-149)

```c
gCW_DitMs = 1200 / gCW_WPM;
```
- Integer division. At WPM=20 → 60ms. At WPM=21 → 57ms. Acceptable.
- **No floating point available** on PY32F071 Cortex-M0.

---

## Section 5: Specific Bugs & Issues Found

### BUG-1: Missing `__attribute__((fallthrough))` location
Line 586 in cw.c has `__attribute__((fallthrough));`. This is GCC-specific, but placed *after* the case block, not at the end. Should be before the fallthrough. **Cosmetic.**

### BUG-2: Infinite loop potential in multi-tap (lines 1107-1113)
```c
for (uint8_t n = 0; n < count; n++) {
    next = (next + 1) % count;
    if (CW_IsAllowedInputChar(...)) { found = true; break; }
}
```
If NONE of the characters in a key are allowed (e.g., under `CW_ALNUM_ONLY`, key 4 has 'G','H','I','4' — all OK so fine. But if user pressed invalid chars buffer first), the loop exhausts and `found` stays false. Then line 1122 beeps and does NOT update `gCW_PrevKey`. This means **next press resets multi-tap**. **Cosmetic — edge case.**

### BUG-3: Trailing space suppression (line 602-613)
```c
const bool trailing = (gCW_TxMsgIdx + 1) >= gCW_CursorPos;
if (!gCW_TxSentAny || gCW_TxPrevWasSpace || trailing) {
    gCW_TxMsgIdx++;
    continue;
}
```
If message starts with a space, `gCW_TxSentAny` is false, so space is skipped. This is correct behavior.

---

## Section 6: Fixes Applied

| # | Issue | Fix | Status |
|---|-------|-----|--------|
| 1 | RX `singleElementGapTicks` used 5*dit for single-element codes, deviating from standard 3*dit finalization | Removed `singleElementGapTicks`; `finalizeInSilenceGapTicks` now always uses `charGapTicks` (3*dit) | ✅ Fixed |
| 2 | `CW_EndDedicatedTx()` lacked VFO state reset present in `CW_EndTxNow()` | Added `RADIO_SetVfoState(VFO_STATE_NORMAL)` and `RADIO_SelectVfos()` to unify cleanup | ✅ Fixed |
| 3 | Blocking `BK4819_PlayToneRaw()` usage undocumented | Added explicit warning comment that `CW_PlayDit/Dah/Character()` are blocking and must not be called from 10ms tick or TX FSM | ✅ Documented |

### Remaining Recommendations (Medium/Low Priority)

| Priority | Item | Location |
|----------|------|----------|
| Medium | Consider documenting augmented iambic gap (`s_ext_gap_count = 1.5*dit`) in cwkeyer.c if it's intentional for qSKCC-style operation | cwkeyer.c:164 |
| Medium | Remove redundant `gCW_DecodeText` second clear in `CW_Start()` after `CW_Init()` | cw.c:844 |
| Low | Replace `strlen()` in `CW_TxStateMachine()` line 628 with precomputed length or `sizeof` pattern | cw.c:628 |
| Low | Move static `updateDelay` out of `CW_AppendDecodedText()` to file scope for clarity | cw.c:188 |

All critical bugs have been corrected and the implementation is now more robust and maintainable.

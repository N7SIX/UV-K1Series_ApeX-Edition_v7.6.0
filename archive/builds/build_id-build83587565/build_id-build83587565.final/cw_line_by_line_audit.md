# Line-by-Line Audit: App/app/cw.c

## Overview
- **Total Lines:** 1266
- **Purpose:** ApeX CW mode implementation with typed-message TX, RX decoder, and UI
- **File Type:** Embedded C (ARM Cortex-M0 / PY32F071)
- **Context:** Runs under FreeRTOS-style main loop with 10ms tick

---

## Section 1: Header and Includes (Lines 1-36)

| Line | Content | Assessment |
|------|---------|------------|
| 1-15 | Apache 2.0 license header | ✅ Standard |
| 17 | `#include "app/cw.h"` | ✅ Required |
| 18 | `#include "app/app.h"` | ✅ Required for gCW_PlaybackActive |
| 20 | `#include <string.h>` | ✅ For strlen, memmove, memcpy, memset |
| 22-24 | `#ifdef ENABLE_AM_FIX` | ✅ Conditional AM fix support |
| 25-36 | Various driver and UI includes | ✅ All used in this file |

**Notes:** Include order is mixed (project vs external). Not critical.

---

## Section 2: Data Structures (Lines 38-50)

| Line | Content | Assessment |
|------|---------|------------|
| 38-41 | `typedef struct { char ch; const char *morse; } CW_CharMap_t;` | ✅ Clean |
| 43 | `#define CW_ALNUM_ONLY 1` | ⚠️ Compile-time switch hardcoded to 1. If 0, punctuation and prosigns enabled |
| 45-50 | `#if !CW_ALNUM_ONLY` CW_ProsignMap_t | ⚠️ Dead code if CW_ALNUM_ONLY=1. Could wrap in explicit `#if 0` for clarity |

---

## Section 3: Morse Lookup Tables (Lines 52-94)

| Line | Content | Assessment |
|------|---------|------------|
| 52-68 | `CW_CHAR_MAP[]` - Full Morse table | ✅ Complete A-Z, 0-9, punctuation |
| 63 | `{'\'', ".----."}` | ✅ Escaped single quote |
| 67 | `{'\0'}` terminators in rows | ✅ Correct string termination |
| 70-80 | `#if !CW_ALNUM_ONLY` CW_PROSIGN_MAP | ⚠️ Dead code when CW_ALNUM_ONLY=1 |
| 82-92 | `CW_KEY_CHARS[9][5]` | ✅ Keypad layout: 1-9, 0-space |
| 89 | Row 7: `{'P', 'Q', 'R', 'S', '7'}` (no `\0`) | ✅ 5 chars |
| 94 | `CW_KEY_CHAR_COUNT[9] = {4, 4, 4, 4, 4, 4, 5, 4, 5}` | ✅ Counts per key |

**Observations:** Table is complete. Hardcoding `CW_ALNUM_ONLY=1` means punctuation is excluded at compile time, reducing flash usage.

---

## Section 4: Global State Variables (Lines 96-137)

| Line | Content | Assessment |
|------|---------|------------|
| 96 | `CW_State_t gCW_State = CW_IDLE;` | ✅ extern in cw.h line 84 |
| 97 | `char gCW_Message[81]` | ✅ CW_MSG_MAX_LEN + 1 |
| 98 | `uint8_t gCW_CursorPos = 0;` | ✅ |
| 99 | `uint8_t gCW_WPM = CW_DEFAULT_WPM;` | ✅ Default 20 |
| 100 | `uint16_t gCW_ToneFreq = CW_TONE_FREQ;` | ✅ Default 800 Hz |
| 102-108 | Static UI/input tracking vars | ✅ All properly scoped |
| 110-114 | Static RX mode backup vars | ✅ Save/restore in CW_Start/Stop |
| 116 | `#define CW_RX_MORSE_MAX_LEN 8` | ✅ Max 8 elements (e.g., SOS is 9 chars? Actually SOS=...---... = 9. **Wait:** SOS is 9 chars, but max len is 8. This truncates SOS.) |
| 117-127 | RX decoder state vars | ✅ Complete set |
| 129-133 | Timing constants | ✅ Derived from WPM |
| 135-137 | `CW_OFFSET_HYSTERESIS`, `CW_RX_DEBOUNCE_TICKS`, `CW_MULTI_TAP_TIMEOUT_TICKS` | ✅ Reasonable defaults |

**Issue Found (Line 116):** `CW_RX_MORSE_MAX_LEN 8` is too short for SOS (9 elements). `CW_GetMorseForChar` in cwapp.c would provide correct patterns, but the RX decoder buffer truncates. However, SOS is a prosign and under `CW_ALNUM_ONLY=1` it wouldn't be decoded anyway. **Minor/acceptable.**

---

## Section 5: Timing Update (Lines 139-149)

| Line | Content | Assessment |
|------|---------|------------|
| 141 | `gCW_DitMs = 1200 / gCW_WPM;` | ✅ Standard formula |
| 142 | `if (gCW_DitMs < 20) gCW_DitMs = 20;` | ✅ Floor at 20ms (60 WPM max) |
| 143 | `if (gCW_DitMs > 120) gCW_DitMs = 120;` | ✅ Ceiling at 120ms (10 WPM min) |
| 145-148 | Derived timings | ✅ Dit*3, Dit*3, Dit*7 |

**Correctness:** Integer division means WPM 21 → 57ms dit (close enough). No float available on M0.

---

## Section 6: Input Character Helpers (Lines 151-169)

| Line | Content | Assessment |
|------|---------|------------|
| 151-161 | `CW_IsAllowedInputChar()` | ✅ Under CW_ALNUM_ONLY, only A-Z, 0-9, space. Otherwise all allowed |
| 163-169 | `CW_GetInputChar()` | ✅ Converts to lowercase if !gCW_UpperCase |

---

## Section 7: RX Decoded Text Buffer (Lines 171-195)

| Line | Content | Assessment |
|------|---------|------------|
| 173-174 | NULL/empty guard | ✅ |
| 176-186 | Append loop with wrap | ✅ Uses memmove to shift left when full |
| 188 | `static uint8_t updateDelay = 0;` | ⚠️ Static inside function. Persists across calls. Works but unusual pattern |
| 189-194 | Display update throttling | ✅ Every 3rd call = 30ms at 10ms tick |

**Style Note (Line 188):** Moving `updateDelay` to file scope would be clearer.

---

## Section 8: Morse Decode Token (Lines 197-230)

| Line | Content | Assessment |
|------|---------|------------|
| 199 | `static char oneChar[2];` | ⚠️ Static return buffer. NOT thread-safe, but fine in single-threaded embedded. Caller must copy immediately |
| 201-202 | NULL/empty → " " | ✅ Returns space string |
| 204-212 | Prosign returns under `#if !CW_ALNUM_ONLY` | ⚠️ Dead code when enabled |
| 214-227 | Loop CW_CHAR_MAP | ✅ Standard lookup |
| 218-221 | `#if CW_ALNUM_ONLY` filter | ✅ Returns "?" for disallowed chars |
| 229 | Final return "?" | ✅ Unknown morse |

**Thread Safety:** `oneChar` is static. If caller stores pointer and calls again, it clobbers. Documented behavior? Not explicitly. Consider returning pointer to static const or requiring caller buffer.

---

## Section 9: RX Character Finalization (Lines 232-260)

| Line | Content | Assessment |
|------|---------|------------|
| 232-242 | `CW_FinalizeRxCharacter()` | ✅ Null-terminates morse, appends decoded text, triggers display |
| 244-260 | `CW_ResetRxDecoder(bool)` | ✅ Resets all decoder state. Initializes `gCW_RxDitTicks` from current `gCW_DitMs`/10 |

**Correctness (Line 256):** `(gCW_DitMs + 5) / 10` rounds to nearest tick. At 60ms → 6 ticks. Good.

---

## Section 10: Tone Detection (Lines 264-324)

| Line | Content | Assessment |
|------|---------|------------|
| 266-271 | RSSI calculation | ✅ Includes AM fix and band correction |
| 277 | `if (!g_SquelchLost)` | ✅ **Critical:** Prevents decoding when squelch closed. Avoids noise interpretation |
| 285 | `static int16_t peakRssi = -120;` | ⚠️ Never explicitly reset on CW_Start. Starts -120, rises to first signal. OK |
| 286-289 | Peak tracking: rise to signal, decay 1dB/tick when 10dB below | ✅ Adaptive threshold |
| 290-291 | `if (peakRssi < -110) peakRssi = -110;` | ✅ Floor prevents decoding impossible signals |
| 293-294 | openLevel = peak-1, closeLevel = peak-5 | ✅ 1dB/5dB hysteresis |
| 296-300 | signalStrong evaluation | ✅ Correct state-dependent logic |
| 302-319 | Debounce counters (max 0xFF) | ✅ CW_RX_DEBOUNCE_TICKS=1 → 10ms debounce |
| 321 | `gCW_RxLastRssi` store | ✅ For status display |

**Issue (Line 285):** `peakRssi` is `static` inside function, initialized once at program start. If a very strong signal arrives, sets peak high, then leaves, it takes ~1.5s to decay 15dB. This is acceptable for CW stability.

---

## Section 11: Dit Estimation (Lines 326-343)

| Line | Content | Assessment |
|------|---------|------------|
| 328-329 | observedMarkTicks==0 guard | ✅ |
| 331-332 | observedMarkTicks<3 guard | ✅ Ignores <30ms noise |
| 334-335 | >4x current dit guard | ✅ Rejects spurs |
| 337 | IIR: (3*old + new + 1)/4 | ✅ Alpha 0.25, slow adapt |
| 339-342 | Clamp [1, 40] | ✅ 40 ticks = 400ms max dit at 10ms tick |

---

## Section 12: TX Tone Control (Lines 345-353)

| Line | Content | Assessment |
|------|---------|------------|
| 347 | `BK4819_TransmitTone(true, gCW_ToneFreq);` | ⚠️ Used by non-blocking path. May be redundant if called before BK4819_PlayToneRaw |
| 352 | `BK4819_EnterTxMute();` | ✅ Mutes carrier |

---

## Section 13: TX Begin/End (Lines 355-434)

| Line | Content | Assessment |
|------|---------|------------|
| 355-416 | `CW_BeginDedicatedTx()` | ✅ Validates TX conditions (lock, busy, battery, modulation), sets up timer, disables features |
| 401 | `gTxTimerCountdown_500ms = ((gEeprom.TX_TIMEOUT_TIMER + 1) * 5) * 2;` | ✅ Converts 500ms units to 500ms ticks? Actually (N+1)*5*2 = (N+1)*10. If TX_TIMEOUT_TIMER=0 → 10 ticks = 5 seconds. Maybe OK. |
| 402-405 | `#ifdef ENABLE_FEAT_N7SIX` alert fields | ✅ |
| 410-413 | Disable scramble/compander/subau/mute | ✅ Clean TX init |
| 418-434 | `CW_EndDedicatedTx()` | ✅ Fixed: Now calls RADIO_SetVfoState and RADIO_SelectVfos. Preserves anti-PA-pop comment |
| 432-433 | Added: `RADIO_SetVfoState(VFO_STATE_NORMAL); RADIO_SelectVfos();` | ✅ Unifies with cwapp.c |

---

## Section 14: Blocking Tone Functions (Lines 436-475)

| Line | Content | Assessment |
|------|---------|------------|
| 436-440 | Documentation comment | ✅ Added warning about BK4819_PlayToneRaw blocking |
| 441-446 | `CW_PlayDit()` | ⚠️ Calls blocking `BK4819_PlayToneRaw(gCW_ToneFreq, gCW_DitMs)` |
| 448-453 | `CW_PlayDah()` | ⚠️ Calls blocking BK4819_PlayToneRaw |
| 455-475 | `CW_PlayCharacter()` | ⚠️ Calls CW_PlayDit/Dah + blocking SYSTEM_DelayMs |

**Constraint:** These MUST NOT be called from 10ms tick or CW_TxStateMachine. Documented.

---

## Section 15: Morse Encoding/Decoding (Lines 477-519)

| Line | Content | Assessment |
|------|---------|------------|
| 477-498 | `CW_CharToMorse(char c)` | ✅ Case-insensitive lookup. Returns NULL for disallowed under CW_ALNUM_ONLY. Returns "" for space |
| 500-519 | `CW_MorseToChar(const char *morse)` | ✅ Returns '?' for unknown/disallowed |

---

## Section 16: Message Buffer Ops (Lines 521-537)

| Line | Content | Assessment |
|------|---------|------------|
| 521-528 | `CW_AppendChar(char c)` | ✅ Bounds check at CW_MSG_MAX_LEN |
| 530-537 | `CW_DeleteChar(void)` | ✅ Safe cursor decrement |

---

## Section 17: Typed-Message TX FSM (Lines 539-695)

| Line | Content | Assessment |
|------|---------|------------|
| 541-549 | CW_TxState_t enum | ✅ 7 states |
| 551-557 | Static TX state vars | ✅ |
| 559-577 | `CW_SendMessage()` | ✅ Checks empty, calls CW_BeginDedicatedTx, initializes FSM |
| 561 | `if (strlen(gCW_Message) == 0)` | ⚠️ Calls strlen on every send. Minor inefficiency; could track length |
| 573 | `gCW_TxTimer = 5; // 50ms preamble` | OK but unconventional - preamble isn't a CW element |
| 580-695 | `CW_TxStateMachine()` | ✅ Non-blocking, timer-driven |
| 582-585 | Timer decrement | ✅ Returns immediately if timer active |
| 593 | `__attribute__((fallthrough));` | ` after case label. GCC-specific but acceptable |
| 598 | `if (gCW_TxMorse == NULL || gCW_TxMorse[gCW_TxMorseIdx] == '\0')` | ✅ Advance to next char |
| 609 | `const bool trailing = (gCW_TxMsgIdx + 1) >= gCW_CursorPos;` | ✅ Suppress trailing spaces |
| 615-620 | Space handling with word gap | ✅ Correct |
| 622 | `gCW_TxMorse = CW_CharToMorse(c);` | ✅ |
| 625 | Skip NULL/empty morse | ✅ Graceful degradation |
| 635 | `if (gCW_TxMorse == NULL || gCW_TxMorseIdx >= strlen(gCW_TxMorse))` | ⚠️ Calls strlen in 10ms tick. Inefficient but strings are tiny (max 9 chars) |
| 644-654 | Element emission | ✅ Sets timer based on dit/dah |
| 657-671 | ELEM_GAP | ✅ Advances index, transitions to CHAR_GAP or ELEMENT |
| 674-680 | CHAR_GAP and WORD_GAP | ✅ Simply fall through to ELEMENT |
| 682-689 | CW_TX_TAIL | ✅ Cleanup: set IDLE, call CW_EndDedicatedTx, start listening |
| 691-693 | default | ✅ Safety reset |

**FSM Correctness:** All state transitions are valid. Timer values cast to uint8_t (max 255 ticks = 2.55s). At 5WPM, dah=120ms, interchar=180ms → 30 ticks. Safe.

---

## Section 18: Initialization (Lines 697-714)

| Line | Content | Assessment |
|------|---------|------------|
| 697-714 | `CW_Init()` | ✅ Resets all state, updates timing, clears decoder |

---

## Section 19: Main 10ms Tick (Lines 716-811)

| Line | Content | Assessment |
|------|---------|------------|
| 718-723 | TX FSM / keyer dispatch | ✅ Correct priority: playback first, then keyer |
| 725 | `if (!gCW_ActiveState \|\| gCW_State == CW_SENDING) return;` | ✅ Skip RX during TX |
| 728-731 | Force monitor on | ✅ |
| 733 | `const bool signalNow = CW_IsRxTonePresent();` | ✅ Adaptive RSSI decoder |
| 735-747 | Multi-tap timeout | ✅ 80 ticks = 800ms |
| 749-754 | Timing calculations | ✅ FIXED: now uses charGapTicks for all finalization |
| 756-780 | Signal-present handling | ✅ Finalize on word gap or char gap. Track mark ticks |
| 782-798 | Signal-absent handling (mark edge) | ✅ Convert mark to dit/dah, estimate dit length |
| 801-810 | Silence handling | ✅ Finalize on char gap (3*dit). Double-check via wordGapTicks*2 |

**Finalization Logic (Lines 804, 807-810):**
- Line 804: `if (gCW_RxMorseLen > 0 && gCW_RxSpaceTicks >= finalizeInSilenceGapTicks)`
- Line 807-810: `if (gCW_RxSpaceTicks > wordGapTicks * 2 && gCW_RxMorseLen > 0)`
- Both can fire. Line 807 is a safety net for very long silence. Since `charGapTicks=3*dit` and `wordGapTicks*2=14*dit`, the first fires first. OK.

---

## Section 20: CW Start/Stop (Lines 813-905)

| Line | Content | Assessment |
|------|---------|------------|
| 813-862 | `CW_Start()` | ✅ Backs up dual watch/crossband/monitor, forces monitor, clears framebuffers |
| 847 | `CW_Init();` | ✅ |
| 848 | `CW_ResetRxDecoder(true);` | ✅ Clears decoded text |
| 864-897 | `CW_Stop()` | ✅ Restores all settings |
| 869-870 | `gCW_ActiveState = false; gCW_State = CW_IDLE;` | ✅ Cleanup |
| 895-896 | `gWasFKeyPressed = false; gUpdateDisplay = true;` | ✅ Force redraw |
| 899-905 | `CW_Toggle()` | ✅ |

**Redundancy (Line 848):** `CW_ResetRxDecoder(true)` clears text, but `CW_Init()` already did via `CW_ResetRxDecoder(true)` on line 713. Double-clear is harmless.

---

## Section 21: Rendering (Lines 907-1016)

| Line | Content | Assessment |
|------|---------|------------|
| 907-941 | `CW_DrawWrappedTxText()` | ✅ Splits message into two 17-char lines |
| 943-1016 | `CW_Render()` | ✅ Two display modes: COMPOSING and SENDING |
| 952-993 | COMPOSING: shows RX line with morse + decoded tail | ✅ |
| 986 | `sprintf(status, "CW %3uw%2uR%3d", gCW_WPM, (unsigned)gCW_RxDitTicks, (int)gCW_RxLastRssi);` | ✅ Status format |
| 996-1008 | SENDING: shows TX text and "CW TX WU/L" | ✅ |
| 1015 | `gUpdateDisplay = true;` | ✅ Always request redraw |

---

## Section 22: Key Processing (Lines 1018-1248)

| Line | Content | Assessment |
|------|---------|------------|
| 1018-1248 | `CW_ProcessKeys()` | ✅ Handles all keys for CW mode |
| 1023-1034 | `!bKeyPressed` release handling | ✅ Clears long-press flags, handles MENU release |
| 1036-1037 | `if (Key != KEY_F) gWasFKeyPressed = false;` | ✅ |
| 1039-1085 | Long-press handling | ✅ MENU: WPM cycle; SIDE2: clear; EXIT: stop |
| 1046-1060 | WPM table cycling | ✅ 5,10,15,20,25,30,35,40,45,50 wrap-around |
| 1087-1151 | Keys 1-9 multi-tap | ✅ Complex but correct |
| 1099 | `uint8_t idx = (uint8_t)(Key - KEY_1);` | ✅ 0-8 |
| 1103 | `if (count == 0) break;` | ✅ Safety guard (all counts are 4 or 5, so never triggers) |
| 1106-1127 | Multi-tap existing key | ✅ Find next allowed char, update buffer |
| 1131-1146 | New key press | ✅ Find first allowed char for that key |
| 1153-1171 | KEY_0 space/zero toggle | ✅ Clever: double-press toggle |
| 1174-1180 | KEY_UP delete | ✅ |
| 1182-1188 | KEY_DOWN append space | ✅ |
| 1190-1195 | KEY_SIDE2 toggle case | ✅ |
| 1197-1201 | KEY_STAR toggle case | ⚠️ Redundant with SIDE2. Two keys, same function |
| 1203-1204 | KEY_MENU no-op | ✅ (handled as long-press above) |
| 1206-1214 | KEY_SIDE1 clear message | ✅ |
| 1216-1225 | KEY_F clear message | ✅ |
| 1227-1230 | KEY_EXIT stop | ✅ |
| 1232-1243 | KEY_PTT send | ✅ Triggers CW_SendMessage |

**Minor Issue:** KEY_SIDE2 and KEY_STAR both toggle case. This is UI redundancy but consumes two key bindings.

---

## Section 23: Public API (Lines 1250-1266)

| Line | Content | Assessment |
|------|---------|------------|
| 1250-1253 | `CW_IsActive()` | ✅ |
| 1255-1261 | `APP_RunCW()` | ✅ Toggle entry point |
| 1263-1266 | `CW_Overlay()` | ✅ Calls CW_Render |

---

## Summary of Findings

### High Priority (Fixed)
1. ✅ RX finalization: `finalizeInSilenceGapTicks` now uses `charGapTicks` (3*dit) for all cases
2. ✅ TX cleanup: `CW_EndDedicatedTx()` now calls `RADIO_SetVfoState(VFO_STATE_NORMAL)` and `RADIO_SelectVfos()`
3. ✅ Blocking calls: Documentation added to `CW_PlayDit/Dah/Character()`

### Medium Priority
4. `CW_RX_MORSE_MAX_LEN 8` truncates SOS (9 elements). Acceptable since SOS is a prosign and disabled under CW_ALNUM_ONLY.
5. `peakRssi` static in `CW_IsRxTonePresent()` is never reset per CW session. Works but could be file-scope.
6. Redundant double-clear of `gCW_DecodeText` in `CW_Start()` after `CW_Init()`.
7. KEY_SIDE2 and KEY_STAR both toggle case - intentional redundancy or oversight?

### Low Priority / Style
8. `strlen()` called in 10ms FSM path (lines 561, 635). Acceptable for short strings.
9. `static uint8_t updateDelay` inside `CW_AppendDecodedText` - unusual pattern.
10. `static char oneChar[2]` in `CW_MorseToDecodedToken` - static return buffer, not thread-safe (OK for embedded single-thread).
11. `__attribute__((fallthrough))` placement cosmetic.
12. `#if !CW_ALNUM_ONLY` blocks are dead code when enabled.

---

## Code Quality Score: B+

**Strengths:**
- Clear separation of concerns (TX FSM, RX decoder, UI)
- Non-blocking TX design avoids tick path stalls
- Adaptive RX decoder with noise immunity
- Good use of static functions for internal linkage
- Comprehensive key handling

**Weaknesses:**
- Some dead code under compile-time flags
- Minor redundancy in display/buffer clears
- Unusual static-inside-function pattern
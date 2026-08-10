# CW Implementation Documentation

> **Status (2026-08-06):** Unchanged during v7.6.9D. No CW-related code was modified.
> All v7.6.9D changes were in `CMakeLists.txt`, `App/CMakeLists.txt`, `App/app/app.c`,
> `App/ui/main.c`, and `CMakePresets.json`. See `CHANGELOG.md`.

## Table of Contents
1. [Architecture](#architecture)
2. [Implementation Study](#implementation-study)
3. [Code Review](#code-review)
4. [Audit Reports](#audit-reports)
5. [Bug Fixes and Root Cause Analysis](#bug-fixes-and-root-cause-analysis)
6. [Testing and Validation](#testing-and-validation)
7. [Future Improvements](#future-improvements)

---

## Architecture

### Core Files

| File | Purpose |
|------|---------|
| `App/app/cw.c` | Main CW app lifecycle, TX state machine, UI rendering |
| `App/app/cwapp.c` | CW app initialization and entry points |
| `App/app/cwkeyer.c` | CW keyer FSM (Iambic A/B, Bug, Ultimatic modes) |
| `App/app/cwkeyer.h` | Keyer API and vtable typedef |
| `App/app/cwmacro.c` | Macro playback engine |
| `App/app/cwhardware.c` | Hardware abstraction (keys, ADC, PTT) |
| `App/app/cwdecoder.c` | RX decoder (tone detection → Morse → text) |
| `App/app/cwdecoder.h` | Decoder public API |
| `App/app/cw.h` | Shared types, Morse map, constants |

### System Overview

```
TX PATH:
Paddles → CW_HandleState() → CW_EncoderProcessElement() → CW_TxStateMachine() → BK4819 tone

RX PATH:
RSSI → CW_IsRxTonePresent() → CW_HandleRxActivation() → CW_Decoder_ProcessTick() → Display

UI PATH:
CW_Render() → Frame buffer → LCD
```

---

## Implementation Study

### TX Path (Keyer → Output)

#### Keyer Modes
- **Iambic A/B** - Full iambic keying with paddle memory
  - Mode A: Basic iambic, alternate paddle memory
  - Mode B: Enhanced iambic with early recognition
- **Bug** - Semi-automatic (dit auto-repeat, dah manual)
- **Ultimatic** - Priority keying (last pressed wins)

#### Timing System
- WPM-controlled: `dit = 1200/WPM ms`
- Dah = 3× dit, inter-elem = 1× dit
- Inter-char = 3× dit, inter-word = 7× dit
- All timing derived from single WPM parameter

#### TX Pipeline
1. `CW_HandleState()` - Main keyer FSM (10ms tick)
   - Reads paddle inputs
   - Manages element timing
   - Handles iambic/ultimatic logic
2. `CW_EncoderProcessElement()` - Records dit/dah for decoder sync
3. `CW_TxStateMachine()` - Drives TX timing and tone generation
   - Manages element gaps
   - Generates spaces
4. `BK4819_TransmitTone()` - Hardware output

### RX Path (Input → Display)

#### Signal Processing Pipeline
```
RSSI Input
  ↓
CW_IsRxTonePresent()
  - Adaptive threshold based on noise floor
  - Open at 75% of signal span above noise
  - Close at fixed -80 dBm
  - Debounce: 1 tick (10ms)
  ↓
CW_HandleRxActivation()
  - Activation debounce: 5 ticks (50ms)
  - Deactivation debounce: 3 ticks (30ms)
  ↓
CW_UpdateTraceBuffer()
  - Records signal strength for UI graph
  - Advances trace clock based on signal presence
  ↓
CW_Decoder_ProcessTick()
  - Main decoder FSM
  - Classifies marks as dit/dah
  - Finalizes characters
  - Builds decoded text
```

#### Decoder State Machine

```
IDLE → MARK (tone detected, was silent)
    ↓
GAP (tone lost)
    ↓
  ├─→ Finalize on char gap (3× dit ticks)
    ├─→ Add word space on word gap (7× dit ticks)
    └─→ Finalize on next signal rise (if pending)
```

#### Character Classification
- **Mark timing:** `markTicks >= 2× ditTicks` → dah, else dit
- **Minimum mark:** `MAX(2, ditTicks/2)` to reject noise
- **Max Morse length:** 10 elements (prevents overflow)

#### Lookup Methods

1. **Array scan (primary)**
   - Linear search through `CW_CHAR_MAP[]`
   - 47 entries (A-Z, 0-9, punctuation)
   - O(n) where n=47, negligible on embedded

2. **Tree lookup (considered but not implemented)**
   - 64-byte flattened binary tree
   - O(log n) ~10 iterations
   - Would replace array scan for A-Z, 0-9

#### Confidence System
- **Purpose:** Reject characters decoded from weak/noisy signals
- **Calculation:**
  - Sample signal quality during each character
  - Average SNR across all elements
  - Map 8-20 dB SNR to 0-100% confidence
- **Adaptive Threshold:**
  - Short chars (1 element): 20% minimum
  - Long chars (10 elements): 25% minimum
  - Prevents dropping valid short characters
- **Rejection:** Characters below threshold are dropped

#### Timing Improvements
- **Mark timer fix:** Start at 0 instead of 1
  - Eliminates +1 tick systematic bias
  - Critical at high WPM (50 WPM: 50% error → 0%)
- **Noise floor cap:** -97 dBm maximum
  - Prevents drift toward AGC hold level (-90 dBm)
  - Without cap: noise floor rises -120 → -90 over seconds
  - Reduces false confidence drops

### UI Rendering

#### Display Layout (128×64 LCD)
- **LINE_TX1** - First line of decoded text or transmission message
- **LINE_TX2** - Second line (overflow)
- **LINE_DECODE** - Signal strength graph (127px horizontal)
- **LINE_STATUS** - WPM, mode indicator, confidence %

#### Signal Graph
- 128-pixel wide waterfall display
- Shows last 127 samples of signal strength
- Active signal: full height
- Noise: bottom row
- Gaps: blank

#### Render Triggers
- Character decoded
- Word space detected
- Morse pattern changes during RX
- Manual key input (typing message)
- TX state changes

### Known Issues and Fixes

#### Issue 1: Character Merging (Fixed)
**Symptom:** Characters merged when gaps were short
**Root cause:** GAP handler finalized only if `gCW_RxSpaceTicks >= charGapTicks`
**Fix:** RISE handler now only adds word spaces, doesn't finalize. Finalization occurs:
  - On signal FALL if gap threshold met
  - In GAP handler after char gap threshold
  - On next RISE only for word spaces

#### Issue 2: Confidence Dropping (Fixed)
**Symptom:** Characters randomly dropped after a few seconds
**Root cause:** Noise floor drifted from -120 to -90 dBm
**Fix:** Cap noise floor at -97 dBm when tone not present

#### Issue 3: Timer Bias (Fixed)
**Symptom:** Inaccurate mark timing, especially at high WPM
**Root cause:** `gCW_RxMarkTicks = 1` on mark start
**Fix:** Initialize to 0 for accurate measurement

---

## Code Review

### Strengths
1. **No dynamic allocation** - All buffers static, safe for embedded
2. **Deterministic timing** - No `rand()`, no variable iterations
3. **Minimal stack** - Small local variables, no deep calls
4. **Defensive programming** - Bounds checks, null checks, overflow guards
5. **Hardware abstraction** - Clean separation from BK4819 driver

### Design Patterns
- **State machine** - Clear IDLE/MARK/GAP states
- **Static private functions** - Good encapsulation in C
- **Lookup tables** - Efficient Morse code mapping
- **Debounce/delay** - Robust signal detection

### Potential Concerns
1. **strcmp() in hot path** - Called on every character decode
   - Mitigation: Only 47 entries, cache-friendly
   - Future: Tree lookup for O(log n)
2. **CW_Render() in ISR-like context** - Called from decoder
   - Currently safe (just sets frame buffer)
   - Monitor if display updates become heavy
3. **Static variable initialization** - Relies on BSS zeroing
   - Standard for embedded C, acceptable

### Code Quality Metrics
- **Cyclomatic complexity:** Low (simple state machine)
- **Coupling:** Loose (hardware abstracted)
- **Cohesion:** High (each function has single responsibility)
- **Testability:** Moderate (state machine, but hardware-dependent)

---

## Audit Reports

### 2026-07-17 Comprehensive Audit

**Scope:** Full CW TX/RX pipeline review
**Status:** Complete

#### Findings

| ID | Severity | Component | Issue | Status |
|----|----------|-----------|-------|--------|
| AUD-001 | High | Decoder | Character merging on short gaps | Fixed |
| AUD-002 | High | Decoder | Noise floor drift | Fixed |
| AUD-003 | Medium | Decoder | Mark timer bias | Fixed |
| AUD-004 | Low | Keyer | Unused variable warning | Fixed |
| AUD-005 | Info | Decoder | Confidence threshold too strict for short chars | Fixed |
| AUD-006 | Info | All | Missing comprehensive documentation | Fixed |
| AUD-007 | Info | TX | Word gap rounding uses +5 before /10 | Documented |
| AUD-008 | Info | Keyer | Element deadline extension when speaker off | Documented |

#### Detailed Findings

**AUD-001: Character Merging**
- **Location:** `cwdecoder.c` RISE handler
- **Impact:** Characters merged, incorrect display
- **Fix:** Restored original finalization logic
- **Risk:** High (data corruption in decoded text)

**AUD-002: Noise Floor Drift**
- **Location:** `cwdecoder.c` `CW_IsRxTonePresent()`
- **Impact:** Characters dropped after 5-10 seconds
- **Fix:** Added -97 dBm cap to noise floor calculation
- **Risk:** High (systematic decoding failure)

**AUD-003: Timer Bias**
- **Location:** `cwdecoder.c` RISE handler
- **Impact:** Timing inaccuracy, especially at high WPM
- **Fix:** Changed `gCW_RxMarkTicks = 1` to `= 0`
- **Risk:** Medium (reduced decode accuracy)

**AUD-004: Unused Variable**
- **Location:** `cwkeyer.c` `CW_HandleState()`
- **Impact:** Compiler warning
- **Fix:** Removed unused `cur_count` in fallback path
- **Risk:** Low (cosmetic)

**AUD-005: Confidence Threshold**
- **Location:** `cwdecoder.c` `CW_FinalizeRxCharacter()`
- **Impact:** Short characters (E, T) dropped on weak signals
- **Fix:** Adaptive threshold 20-25% based on Morse length
- **Risk:** Medium (reduced decode sensitivity)

**AUD-006: Documentation**
- **Location:** Repository-wide
- **Impact:** No single reference for CW implementation
- **Fix:** Created this document
- **Risk:** Info (maintainability)

**AUD-007: Word Gap Rounding**
- **Location:** `cw.c` `CW_TxStateMachine()`
- **Impact:** Minor timing deviation (~0.5 ms longer word gaps)
- **Fix:** Documented as intentional rounding optimization
- **Risk:** Info (non-compliant only in strict contest/regulatory use)

**AUD-008: Element Deadline Extension**
- **Location:** `cwkeyer.c` `CW_HandleState()` and `CW_HandleBugState()`
- **Impact:** Adds 20 ms to elements when speaker disabled
- **Fix:** Made configurable via `CW_ELEM_DEADLINE_EXTRA_MS` macro and documented rationale
- **Risk:** Info (intentional operational aid)

### Verification

**Test Case 1: CQ Decoding**
- Input: "CQ CQ CQ" sent at 20 WPM
- Expected: "CQ CQ CQ" displayed
- Actual: "CQ CQ CQ" ✓
- Status: PASS

**Test Case 2: Character Merging**
- Input: Fast sequence "PARIS" at 30 WPM
- Expected: "PARIS" displayed
- Actual: "PARIS" ✓
- Status: PASS

**Test Case 3: Weak Signal**
- Input: "TEST" at low power, noisy conditions
- Expected: Some characters possibly dropped
- Actual: Confidence filter active, weak chars dropped ✓
- Status: PASS

**Test Case 4: Word Spacing**
- Input: "SOS HELP" with clear word gaps
- Expected: "SOS HELP" with space between words
- Actual: Correct spacing ✓
- Status: PASS

---

## Bug Fixes and Root Cause Analysis

### Bug #1: CQ Decoded as TT

**Date:** 2026-07-18
**Reporter:** User
**Severity:** Critical

#### Symptoms
- Input: Morse "CQ" sent and received
- Expected: Display "CQ"
- Actual: Display "TT"

#### Root Cause Analysis

**Investigation:**
1. Compared backup (old) vs current code
2. Both showed same behavior → bug not in lookup method
3. Traced decoder state machine timing

**Root Cause:**
My earlier "always finalize on RISE" change caused premature finalization:
- C = `-.-.` (4 elements)
- Element 1 `-` falls → Morse = `-`
- Inter-element gap (1 dit) → signal rises
- **BUG:** RISE handler finalized `-` as T
- Element 2 `.` starts fresh → becomes E
- Result: "TE" instead of "C"

**Why it affected both versions:**
- Backup file was created AFTER the buggy changes
- Both had the same "always-finalize-on-RISE" logic

#### Fix
Reverted RISE handler to original behavior:
```c
// BEFORE (buggy):
if (gCW_RxMorseLen > 0)
    CW_FinalizeRxCharacter();  // Wrong: finalizes between elements

// AFTER (correct):
// Only add word space, don't finalize
if (gCW_RxSpaceTicks >= wordGapTicks)
    CW_AppendDecodedText(" ");
```

**Finalization now occurs:**
1. On signal FALL (if gap threshold met)
2. In GAP handler (after char gap threshold)
3. NOT on signal RISE

#### Verification
- Rebuilt firmware
- Tested "CQ CQ CQ" → displays correctly
- No character merging observed

### Bug #3: MORSE_TREE Lookup Table Had 8 Incorrect Number Mappings

**Date:** 2026-07-19
**Reporter:** User (field testing)
**Severity:** Critical

#### Symptoms
- Morse code numbers decoded as wrong characters
- Example: `...--` (3) decoded as `2`, `.----` (1) decoded as `*` (unknown)
- Some punctuation characters (`+`, `=`, `/`) also mapped incorrectly
- Letters A-Z decoded correctly, but numbers were unreliable

#### Root Cause Analysis

**Investigation:**
The `MORSE_TREE[64]` flattened binary tree in `cwdecoder.c` had incorrect entries at indices 34-63 (level 6, 5-element patterns). The tree navigation logic (dot = 2*index, dash = 2*index+1) was correct, but the character values placed at those indices were wrong.

**Root Cause:**
The original tree was hand-constructed without verifying the exact index positions for 5-element Morse patterns. The binary tree layout requires careful tracing of each path from root to leaf. The following indices were incorrect:

| Index | Morse Pattern | Old Value | Correct Value | Fix |
|-------|--------------|-----------|---------------|-----|
| 34 | `...-.` | `3` | `*` (unused) | Removed incorrect mapping |
| 35 | `...--` | `2` | `3` | Corrected |
| 39 | `..---` | `1` | `2` | Corrected |
| 42 | `.-.-.` | `*` (unused) | `+` | Added missing punctuation |
| 45 | `.---.` | `*` (unused) | `*` (unused) | Was `1` → corrected to `*` |
| 47 | `.----` | `*` (unused) | `1` | Added missing digit |
| 48 | `-....` | `6` | `6` | Was correct |
| 49 | `-...-` | `=` | `=` | Was correct |
| 50 | `-..-.` | `/` | `/` | Was correct |
| 55 | `-.---` | `*` (unused) | `*` (unused) | Was correct |
| 56 | `--...` | `*` (unused) | `7` | Added missing digit |
| 60 | `---..` | `8` | `8` | Was correct |
| 62 | `----.` | `9` | `9` | Was correct |
| 63 | `-----` | `0` | `0` | Was correct |

**Key incorrect mappings that caused wrong character display:**
- `...--` (should be `3`) was mapped to `2` → sending "3" displayed "2"
- `..---` (should be `2`) was mapped to `1` → sending "2" displayed "1"
- `.----` (should be `1`) was mapped to `*` (unused) → sending "1" displayed "?"
- `.-.-.` (should be `+`) was mapped to `*` → sending "+" displayed "?"

#### Fix
Corrected the `MORSE_TREE[64]` array entries at indices 32-63 to match the standard ITU Morse code tree layout:

```c
static const char MORSE_TREE[64] = {
    '*',  //  0: unused
    '*',  //  1: root
    'E', 'T',  //  2-3
    'I', 'A', 'N', 'M',  //  4-7
    'S', 'U', 'R', 'W', 'D', 'K', 'G', 'O',  //  8-15
    'H', 'V', 'F', '*', 'L', '*', 'P', 'J', 'B', 'X', 'C', 'Y', 'Z', 'Q', '*', '*',  // 16-31
    '5', '4', '*', '3', '*', '*', '*', '2', '*', '*', '+', '*', '*', '*', '*', '1',  // 32-47
    '6', '=', '/', '*', '*', '*', '*', '*', '7', '*', '*', '*', '8', '*', '9', '0'   // 48-63
};
```

#### Verification
- Traced every Morse pattern from root to leaf through the corrected tree
- All 26 letters (A-Z) verified correct
- All 10 digits (0-9) verified correct
- Punctuation `+`, `=`, `/` verified correct (these are in the tree; other punctuation falls back to array scan)
- Verified that `CW_DecodeMorseTree()` correctly returns `?` for `*` (unused) entries, triggering fallback to `CW_CHAR_MAP[]` array scan

### Bug #2: Characters Dropped After Extended RX

**Date:** 2026-07-17
**Reporter:** User
**Severity:** High

#### Symptoms
- Decoder works initially
- After 5-10 seconds, characters start dropping
- No change in signal strength

#### Root Cause Analysis

**Investigation:**
1. Monitored confidence values over time
2. Observed confidence dropping from 80% to 0%
3. Traced noise floor calculation

**Root Cause:**
Noise floor drifted upward over time:
- Started at -120 dBm (correct)
- Rose to -90 dBm over ~10 seconds
- Caused by AGC hold level during gaps
- GAP handler updated noise floor with signal samples
- Reduced SNR, eventually below confidence threshold

#### Fix
Added cap to noise floor:
```c
if (gCW_RxNoiseFloor > -97)
    gCW_RxNoiseFloor = -97;
```

**Rationale:**
- AGC hold: ~-90 dBm during short gaps
- Noise floor should never exceed -97 dBm
- Prevents tracking the signal during gaps

#### Verification
- Monitored noise floor over 60 seconds
- Stays at -120 to -115 dBm in clean conditions
- Characters decoded consistently

---

## Testing and Validation

### Manual Test Procedure

1. **Setup**
   - Enable CW mode: `MENU → CW Mode`
   - Set WPM: `MENU → CW WPM → 20`
   - Ensure monitor mode active

2. **Test Sequence 1: Basic Decoding**
   - Send: "CQ CQ CQ DE N7SIX N7SIX"
   - Verify: Exact text displayed
   - Check: Signal graph shows clear tone pattern
   - Pass criteria: 100% character accuracy

3. **Test Sequence 2: Speed Test**
   - Send: "PARIS" at 10, 20, 30, 40, 50 WPM
   - Verify: All decoded correctly
   - Pass criteria: No errors at ≤30 WPM, acceptable at 40-50

4. **Test Sequence 3: Weak Signal**
   - Send: "TEST TEST TEST" from 1km away
   - Verify: Some chars may drop (expected)
   - Check: Confidence % displays correctly
   - Pass criteria: No complete message loss

5. **Test Sequence 4: Word Spacing**
   - Send: "CQ DE N7SIX" with clear word gaps
   - Verify: Spaces between words
   - Pass criteria: Word spacing detected correctly

### Expected Behavior

| Condition | Expected Result |
|-----------|-----------------|
| Clean signal, 20 WPM | 100% decode accuracy |
| Clean signal, 50 WPM | ~95% accuracy, timing tight |
| Weak signal (low SNR) | Some chars dropped, confidence low |
| Very weak signal | Frequent drops, message fragments |
| Fast keying (<5 WPM) | Works, gaps large |
| Variable speed | Adapts, some boundary issues |

### Automated Testing

No automated tests currently. Future work:
- Host-based decoder unit tests
- Signal replay from sample files
- Regression tests for known patterns

---

## Future Improvements

### Short Term
1. **Tree lookup implementation**
   - Replace array scan with binary tree
   - Saves ~200 bytes flash
   - Faster lookup (O(log n) vs O(n))

2. **Fuzzy matching**
   - Handle near-miss patterns (e.g., `.-..` vs `...-.`)
   - Context-aware correction
   - Would improve weak-signal decoding

3. **Confidence logging**
   - Track per-character confidence
   - Historical accuracy statistics
   - Helpful for tuning thresholds

### Long Term
1. **Adaptive WPM**
   - Auto-detect sending speed
   - Adjust timing dynamically
   - Handle variable-speed operators

2. **Error correction**
   - Dictionary-based word lookup
   - Probabilistic decoding
   - Context-aware suggestions

3. **Multi-path decoding**
   - Run multiple hypotheses in parallel
   - Select highest-probability path
   - Improved accuracy on noisy signals

---

## Appendix

### Morse Code Reference

Standard ITU-R M.1677-1 character set implemented:
- A-Z (uppercase)
- 0-9 (digits)
- Punctuation: . , ? ' ! / ( ) & : ; = + - _ " $ @

### Timing Constants

| Symbol | Value | Notes |
|--------|-------|-------|
| DIT | 1 unit | Base time unit |
| DAH | 3 units | 3× dit |
| INTRA-ELEMENT | 1 unit | Between dit/dah within char |
| INTER-ELEMENT | 3 units | Between characters |
| INTER-WORD | 7 units | Between words |
| WORD SPACE | 7 units | Same as inter-word |

### Confidence Thresholds

| SNR (dB) | Confidence | Action |
|----------|-----------|--------|
| <8 | 0% | Reject |
| 8-12 | 0-33% | Short chars: accept, Long: reject |
| 12-16 | 33-66% | Accept most chars |
| 16-20 | 66-100% | Accept all |
| >20 | 100% | Accept all |

### Intentional Timing Deviations

| Location | Deviation | Reason | Configurability |
|----------|-----------|--------|-----------------|
| `cw.c` `CW_TxStateMachine()` | Word gap rounded up by up to ~0.5 ms | Keeps timing aligned to 10 ms tick boundaries | Hardcoded rounding; change `/10` if strict compliance needed |
| `cwkeyer.c` `CW_HandleState()` / `CW_HandleBugState()` | Element deadline extended by 20 ms when speaker off | Operational comfort for silent keying | Configurable via `CW_ELEM_DEADLINE_EXTRA_MS` macro |

### KEY_PTT Safety

- Serial injection of `KEY_PTT` is blocked in `KEYBOARD_InjectKey()` because PTT release cannot be guaranteed over serial.
- `KEY_PTT` is handled generically by `GENERIC_Key_PTT()` outside CW, and by `CW_ProcessKeys()` / `ptt_action()` inside CW.
- Long press of `KEY_PTT` in CW compose mode is ignored to prevent accidental sends; only short press on release sends typed text.

---

*Document generated: 2026-07-22*
*Firmware: UV-K1 ApeX Edition v7.6.0*
*CW Module: N7SIX Custom Mod*
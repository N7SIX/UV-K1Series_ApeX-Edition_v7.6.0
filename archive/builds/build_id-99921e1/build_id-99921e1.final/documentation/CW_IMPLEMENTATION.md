# CW Implementation Documentation

> **Status (v7.6.10C):** The CW subsystem has been slimmed down to the core
> **encode / decode / display** feature set. The external-paddle keyer stack
> (`cwkeyer.c`, `cwapp.c`, `cwhardware.c`) and the macro subsystem (`cwmacro.c`)
> were removed along with the semi-automatic bug keyer mode. Keyboard-typed
> message TX and on-air Morse RX decode with text display remain fully functional.
> FLASH footprint: 114,380 B (94.66 %), RAM: 14,592 B (89.06 %).

## Table of Contents
1. [Architecture](#architecture)
2. [TX Path (Encode)](#tx-path-encode)
3. [RX Path (Decode)](#rx-path-decode)
4. [Display](#display)
5. [Bug Fixes Applied](#bug-fixes-applied)
6. [Removed Features](#removed-features)
7. [Settings and EEPROM](#settings-and-eeprom)
8. [Memory Footprint](#memory-footprint)

---

## Architecture

### Core Files

| File | Purpose |
|------|---------|
| `App/app/cw.c` | CW app lifecycle, keyboard entry, TX state machine, tone output, UI rendering |
| `App/app/cw.h` | Shared types, Morse map, constants, public API |
| `App/app/cwdecoder.c` | RX decoder (RSSI/tone detection → Morse → text) |
| `App/app/cwdecoder.h` | Decoder public API |

Build integration: `ENABLE_FEAT_N7SIX_CW` in `App/CMakeLists.txt` compiles only
`app/cw.c` and `app/cwdecoder.c`.

### System Overview

```
TX PATH:
Keyboard → CW_ProcessKeys() → gCW_Message[] → CW_SendMessage()
        → gCW_TxSnapshot[] → CW_TxStateMachine() → CW_CharToMorse()
        → CW_ToneOn()/CW_ToneOff() → BK4819 tone

RX PATH:
RSSI → CW_Decoder_ProcessTick() (adaptive gate, element/char/word FSM)
     → decoded text buffer → Display

UI PATH:
CW_Overlay() / CW_Render() → Frame buffer → LCD
```

Entry point: `APP_RunCW()` (called from `main.c` via `F+7`).
Integration points outside the CW module are minimal and stable:
`app.c` (`CW_IsActive()`, `CW_TimeSlice10ms()`), `main.c`
(`CW_ProcessKeys()`, `CW_IsActive()`, `CW_Overlay()`), `screenshot.c` (`CW_IsActive()`).

---

## TX Path (Encode)

### Message Composition
- Typed on the keypad in CW compose mode (multi-tap entry, upper/lower case toggle).
- `gCW_Message[]` holds up to `CW_MSG_MAX_LEN` (80) characters with a cursor.
- Digit keys and `UP`/`DOWN` edit the message; `EXIT` leaves CW mode.

### Sending
- **Short PTT press** (release within 400 ms, `CW_PTT_LONG_PRESS_10MS`) sends the
  message **on release**. Long press is ignored to prevent accidental sends.
- On send, a snapshot (`gCW_TxSnapshot`) is taken so live edits during TX cannot
  desynchronize the on-air text from the display.
- `CW_TxStateMachine()` walks the snapshot character by character:
  `CW_TX_IDLE → PREAMBLE → ELEMENT/ELEM_GAP → …`, keying the PA for the full
  message via `CW_BeginDedicatedTx()` and returning to RX via `CW_EndDedicatedTx()`.
- Edit keys are ignored while transmitting; `EXIT` and PTT still pass through.
- **Timeout safety:** if TOT expires (`gTxTimeoutReached`/`gFlagEndTransmission`),
  the playback aborts immediately and runs the clean teardown.

### Timing
- WPM-controlled: `dit = 1200/WPM ms`; dah = 3× dit, inter-element = 1× dit,
  inter-char = 3× dit, inter-word = 7× dit.
- WPM is validated from EEPROM `CW_KEY_WPM` (5–100, otherwise default 20) and
  adjustable on the CW screen (cycles a WPM table).

---

## RX Path (Decode)

### Signal Processing Pipeline
```
RSSI Input (10 ms tick, from CW_TimeSlice10ms)
  ↓
CW_Decoder_ProcessTick()
  - Adaptive threshold based on noise floor
  - Open at 75% of signal span above noise
  - Close at fixed -80 dBm
  - Debounce: 1 tick (10ms)
  ↓
Activation FSM
  - Activation debounce: 5 ticks (50ms)
  - Deactivation debounce: 3 ticks (30ms)
  ↓
Trace buffer
  - Records signal strength for the UI signal graph
  ↓
Decoder FSM
  - Classifies marks as dit/dah (WPM auto-adapt)
  - Finalizes characters on char gap (3× dit)
  - Emits one word space per word gap (7× dit)
  - Builds decoded text buffer
```

### Decoder Notes
- Exactly **one** space is emitted per inter-word gap (one-shot flag), so the
  decoded line stays clean during long pauses.
- Character confidence is displayed in the status row (`%uWPM RX C%u`).
- The RX decoder runs in CW compose mode (monitor forced on); it is suspended
  while the radio is transmitting a typed message.

---

## Display

LCD lines 24–55 (pages 3–6), via `CW_Overlay()` from `UI_DisplayMain`:

| Line | Content |
|------|---------|
| 3–4 | TX text (wrapped, 17 chars/line) — snapshot while sending, live buffer otherwise |
| 5 | RX Morse symbols + decoded text |
| 6 | Status: WPM, RX/TX mode, char confidence + signal gauge |

---

## Bug Fixes Applied

| Fix | Root cause | Resolution |
|-----|-----------|------------|
| PTT short press aborted TX / long press sent / re-send after TX | Old handler fired on press and on release; PTT has no held event (handled out-of-band in `app.c`) and the release was routed to the TX-abort path | New PTT tracker: send on **release** only when the hold began in compose mode and lasted < 400 ms; long press ignored; no re-fire after transmission |
| Decoder word-gap space flood | `gCW_RxSpaceTicks` was pinned at the word-gap threshold, so `>=` re-triggered every 10 ms tick; plus a redundant space on tone rise | One-shot `gCW_RxWordGapEmitted` flag: exactly one space per word gap; reset on tone rise and decoder reset |
| UI freeze during TX tone | `BK4819_TransmitTone()` blocks ~50 ms and was re-invoked on every 10 ms tick by the keyer path | Tone armed once per element; keyer path removed entirely |
| TOT bypass / re-key after TOT | Keyer path never consumed the TOT countdown and re-keyed after `APP_EndTransmission()` | Playback checks `gTxTimeoutReached \|\| gFlagEndTransmission` and aborts cleanly; keyer path removed |
| Display could disagree with on-air text | Overlay rendered the live-editable message during send | Overlay renders `gCW_TxSnapshot` during TX; edit keys gated while sending |
| Dead code/state | Write-only globals (`gCW_TxDisplayHoldoff_10ms`, `gCW_KeyerManagesPtt`, `gCW_KeyerUsingSD1`, `gCW_FlashlightSending`), unreachable macro subsystem, unreferenced `CW_GetTxDisplayTail`/`CW_PlayDit`/`CW_PlayDah` | All removed |

---

## Removed Features

Removed in v7.6.10C to reclaim flash and RAM (all were unreachable or unused
by any user-facing path):

| Removed | Reason |
|---------|--------|
| `cwmacro.c/.h` (macro record/save/load/playback, duplicate Morse table) | Entry points had zero callers; playback FSM was never driven |
| `cwkeyer.c/.h` (iambic A/B, ultimatic, bug, straight key FSMs; paddle edge detection) | External-paddle keying; not reachable via any UI |
| `cwapp.c/.h` (1 ms app TX glue, suspend/resume) | Only served the keyer path |
| `cwhardware.c/.h` (paddle GPIO reads, deglitch, ADC stubs, port config) | Only served the keyer path |
| Keyer types/enums in `cw.h` (`CW_Action_t`, `CW_ElementType_t`, `CW_Input`, keyer-mode/key-flag defines) | Header-only residue of the above |

EEPROM layout is **unchanged** — `CW_KEY_INPUT`/`CW_KEYER_MODE` bytes are simply
ignored now, so existing radio configurations remain compatible.

Note: the historical `CW_DEEP_AUDIT_REPORT.md` was retired; every finding it
tracked is either fixed and documented above, or removed with the deleted
feature it described.

---

## Settings and EEPROM

| Field | State |
|-------|-------|
| `CW_KEY_WPM` | Used (validated 5–100, default 20) |
| `CW_KEY_INPUT`, `CW_KEYER_MODE`, `CW_MESSAGE_REPEAT_DELAY` | Retained in EEPROM layout for compatibility; not read by the slim implementation |
| `CW_ADC_CABLE_20K`, `CW_ADC_CABLE_10K`, `CW_ADC_MAX`, `CW_ADC_RANGE_LIMIT`, `CW_ADC_GLITCH_GUARDBAND` | Retained in EEPROM layout; CEC/ADC paddle support removed |

---

## Memory Footprint

| Metric | Before (full CW stack) | After (slim CW) |
|--------|------------------------|------------------|
| FLASH | 117,372 B (97.14 %) | **114,380 B (94.66 %)** |
| RAM | 14,656 B (89.45 %) | **14,592 B (89.06 %)** |

Verified with `arm-none-eabi-gcc` 14.3.1, `ApeX` preset (Ninja), full-firmware
link: EXIT=0, no errors/warnings.

### KEY_PTT Safety

- Serial injection of `KEY_PTT` is blocked in `KEYBOARD_InjectKey()` because PTT
  release cannot be guaranteed over serial.
- `KEY_PTT` is handled generically by `GENERIC_Key_PTT()` outside CW, and by
  `CW_ProcessKeys()` / `CW_HandlePttKey()` inside CW.
- Long press of `KEY_PTT` in CW compose mode is ignored to prevent accidental
  sends; only short press on release sends typed text.

---

*Document updated: 2026-09-01*
*Firmware: UV-K1 ApeX Edition v7.6.10C*
*CW Module: N7SIX Custom Mod (slim encode/decode/display build)*
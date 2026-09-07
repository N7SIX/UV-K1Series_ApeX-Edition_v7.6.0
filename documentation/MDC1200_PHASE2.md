# MDC-1200 Phase 2 — Opcode Handler Framework

**Date**: 2026-08-18 (design) / 2026-09-06 (interop remap)
**Status**: ✅ COMPLETE — Implemented and integrated
**Version**: post-v7.6.10C (Motorola interop remap)

---

## Executive Summary

Phase 2 adds the opcode dispatch and user-visible reaction layer on top of the
Phase 1 RX path. When a valid MDC-1200 frame is received, the frame is decoded
(`MDC1200_DecodeFrameWords()`), then dispatched to an opcode-specific handler
(`MDC_DispatchFrame()`), which updates global state, triggers a center-line
display alert, and plays a **local MDC preamble warble** through the speaker.

The dispatch table was remapped from the original local 0x00–0x07 convention to
the **Motorola MDC-1200 standard opcode map**, making received bursts from
commercial Motorola radios recognizable and alertable. Legacy local opcodes
remain dispatched so pre-interop ApeX bursts still alert.

**Files:**
- `App/mdc_handler.h` — Public API, Motorola opcode enum, state structs
- `App/mdc_handler.c` — Dispatch switch, Motorola handlers, display/audio utilities
- `App/ui/mdc.c` — Center-line alert rendering (`UI_DisplayMDCAlert()`, `UI_HandleMDCDismiss()`)
- `App/app/app.c` — RX hook (`APP_HandleMDC1200Receive()`), 500ms tick, key-dismiss hook
- `App/ui/main.c` — Renders `CENTER_LINE_MDC_ALERT` via `UI_DisplayMDCAlert()`
- `App/audio.c` / `App/audio.h` — `AUDIO_PlayMDCWarble()` local preamble sound

---

## Opcode Definitions (Motorola Standard)

| Opcode | Name | Handler |
|--------|------|---------|
| 0x01 | PTT ID / ANI | `MDC_Handle_PttID` |
| 0x30 | Call Alert | `MDC_Handle_CallAlert` |
| 0x31 | Call Alert (ack) | `MDC_Handle_CallAlert` |
| 0x40 | Radio Check | `MDC_Handle_RadioCheck` |
| 0x41 | Radio Check (ack) | `MDC_Handle_RadioCheck` |
| 0x46 | Status Request | `MDC_Handle_StatusMessage` |
| 0x47 | Status Response | `MDC_Handle_StatusMessage` |
| 0x81 | Emergency | `MDC_Handle_Emergency` |
| 0x82 | Emergency (ack) | `MDC_Handle_Emergency` |
| 0x83 | Emergency Clear | `MDC_Handle_EmergencyClear` |

Legacy local opcodes (retained for backward compatibility with pre-interop ApeX
bursts):

| Opcode | Name | Handler |
|--------|------|---------|
| 0x00 | Legacy Status | `MDC_Handle_Legacy` |
| 0x02 | Legacy Request | `MDC_Handle_Legacy` |
| 0x04 | Legacy Command | `MDC_Handle_Legacy` |
| 0x05–0x07 | Legacy Emergency | `MDC_Handle_Emergency` |

Frames are dispatched through a `switch` in `mdc_handler.c`
(`MDC_DispatchFrame()`).

---

## Dispatch Flow

```
APP_HandleMDC1200Receive()            [App/app/app.c, 10ms slice]
  └─ MDC1200_DiffDecodeFrame()        [App/mdc1200.c — XOR differential decode]
  └─ MDC1200_DecodeFrameWords()       [App/mdc1200.c — de-interleave + CRC]
  └─ MDC_DispatchFrame(op, arg, unit_id, valid)   [App/mdc_handler.c]
       ├─ Duplicate suppression check (identical unit+op within 10 s → silent;
       │    emergencies exempt so genuine re-alerts always pass)
       ├─ Update g_MDC_LastRxFrame (unit_id, opcode, argument, timestamp, is_new)
       ├─ if (!valid) → MDC_Handle_Unknown()
       └─ else → switch(opcode) → Motorola handler
            ├─ MDC_TriggerDisplay(is_emergency, timeout_ms)
            │    ├─ Save previous center_line mode
            │    ├─ Set dismiss_time (0 = manual dismiss for emergency)
            │    └─ center_line = CENTER_LINE_MDC_ALERT; gUpdateDisplay = true
            └─ MDC_PlayAlert(type) → AUDIO_PlayMDCWarble(repeats)  [LOCAL speaker]
```

Invalid frames (CRC failure) are routed to `MDC_Handle_Unknown()` and shown
briefly (2 s) with no warble to avoid alert fatigue.

---

## Handler Reactions

| Handler | Display | Timeout | Audio |
|---------|---------|---------|-------|
| PTT ID (0x01) | Routine alert | 3 s auto-close | Single warble |
| Call Alert (0x30/0x31) | Routine alert | 3 s auto-close | Single warble |
| Radio Check (0x40/0x41) | Routine alert | 3 s auto-close | Single warble |
| Status Req/Resp (0x46/0x47) | Routine alert | 3 s auto-close | Single warble |
| Emergency (0x81/0x82) | Inverted alert | Manual dismiss | Double warble |
| Emergency Clear (0x83) | Routine alert | 3 s auto-close | Single warble |
| Legacy (0x00/0x02/0x04) | Routine alert | 3 s auto-close | Single warble |
| Unknown / invalid | Routine alert | 2 s auto-close | None |

---

## Display Integration (Phase 3 hooks)

- `UI_DisplayMDCAlert()` (`App/ui/mdc.c`) renders on frame-buffer lines 3–6:
  - Line 3: opcode name (inverted text for emergency)
  - Line 4: `Unit: 1234` (decimal, Motorola convention)
  - Line 5: `Arg: 0xXX` (routine) or "Press key to dismiss" (emergency)
  - Line 6: auto-close countdown for routine alerts
- `UI_DisplayMain()` calls it when `center_line == CENTER_LINE_MDC_ALERT`
  and preserves the MDC alert across display refreshes.
- Any key press during an emergency alert calls `UI_HandleMDCDismiss()`
  (hooked in `ProcessKey()`, `App/app/app.c`), restoring the previous
  center-line mode.
- `MDC_UITimeSlice500ms()` auto-closes routine alerts when the dismiss time
  expires; `MDC_TimeSlice500ms()` expires the status message and clears the
  `is_new` flag. Both are called from `APP_TimeSlice500ms()`.

---

## Global State

```c
MDC_RxFrame_t g_MDC_LastRxFrame = {
    .unit_id = 0xFFFF,      /* sentinel: no frame received yet */
    .opcode = 0xFF,
    .argument = 0xFF,
    .timestamp_ms = 0,
    .is_valid = false,
    .is_new = false
};

MDC_DisplayState_t g_MDC_DisplayState = {
    .previous_mode = 0,     /* CENTER_LINE_NONE */
    .dismiss_time = 0,
    .is_emergency = false
};

/* Duplicate suppression: identical Unit ID + opcode within 10 s are silenced.
 * Emergency frames are exempt so genuine re-alerts always pass. */
static uint32_t g_MDC_LastAlertTime = 0;
static uint16_t g_MDC_LastAlertUnit = 0xFFFF;
static uint8_t  g_MDC_LastAlertOpcode = 0xFF;
```

Note: received frames are **not** written back to EEPROM — the local MDC
configuration (`MDC_UnitID`, `MDC_DefaultOp`, `MDC_DefaultArg`) is preserved.

---

## Timing

| Metric | Value |
|--------|-------|
| RX-to-handler latency | 0–10 ms (one scheduler tick) |
| Handler-to-UI latency | 1 display frame |
| Routine alert timeout | 3 s (2 s for unknown) |
| Emergency alert | Manual dismiss only |
| Duplicate suppression window | 10 s |

---

## Local Preamble Warble (RX-side audio)

`AUDIO_PlayMDCWarble()` (`App/audio.c`) reproduces the characteristic MDC-1200
preamble sound (rapid 1200/1800 Hz alternation) through the **local speaker**.
It is explicitly permitted during `FUNCTION_RECEIVE`/`FUNCTION_MONITOR` — unlike
`AUDIO_PlayBeep()` which refuses to play during RX (the reason MDC RX alerts
never sounded before this fix).

This means the receiving radio plays the preamble sound **whenever it decodes a
valid MDC ID**, independent of whether the repeater relays the on-air burst
audio. Works on simplex and through repeaters.

---

## Status

- ✅ Opcode dispatch switch with Motorola standard map
- ✅ Legacy local opcodes (0x00–0x07) retained for backward compatibility
- ✅ All Motorola handlers + unknown/invalid fallback
- ✅ Center-line alert rendering with emergency inversion
- ✅ Local MDC preamble warble per handler type
- ✅ Decimal Unit ID display (Motorola convention)
- ✅ Auto-timeout for routine alerts, manual dismiss for emergency
- ✅ RX duplicate suppression (10 s window, emergencies exempt)
- ✅ Hard-decision Viterbi ECC correction on RX decode (recovers bit-error frames)
- ❌ Alias table / contacts lookup — not implemented (planned)
- ❌ Call Alert / Radio Check ACK transmission — not implemented (Phase 4, parked)

---

*Document Version: 2.0*
*Last Updated: 2026-09-06*
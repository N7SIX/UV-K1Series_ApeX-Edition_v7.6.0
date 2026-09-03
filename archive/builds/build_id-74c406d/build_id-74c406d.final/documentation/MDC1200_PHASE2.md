# MDC-1200 Phase 2 — Opcode Handler Framework

**Date**: 2026-08-18
**Status**: ✅ COMPLETE — Implemented and integrated
**Version**: v7.6.10C

---

## Executive Summary

Phase 2 adds the opcode dispatch and user-visible reaction layer on top of the
Phase 1 RX path. When a valid MDC-1200 frame is received, the frame is decoded
(`MDC1200_DecodeFrameWords()`), then dispatched to an opcode-specific handler
(`MDC_DispatchFrame()`), which updates global state, triggers a center-line
display alert, and plays an audio alert.

**Files:**
- `App/mdc_handler.h` — Public API, opcode enum, state structs
- `App/mdc_handler.c` — Dispatch table, built-in handlers, display/audio utilities
- `App/ui/mdc.c` — Center-line alert rendering (`UI_DisplayMDCAlert()`, `UI_HandleMDCDismiss()`)
- `App/app/app.c` — RX hook (`APP_HandleMDC1200Receive()`), 500ms tick, key-dismiss hook
- `App/ui/main.c` — Renders `CENTER_LINE_MDC_ALERT` via `UI_DisplayMDCAlert()`

---

## Opcode Definitions (Motorola Standard)

| Opcode | Name | Handler |
|--------|------|---------|
| 0x00 | Status | `MDC_Handle_Status` |
| 0x01 | Acknowledge | `MDC_Handle_Acknowledge` |
| 0x02 | Request | `MDC_Handle_Request` |
| 0x03 | Reserved | *(NULL — falls through to Unknown)* |
| 0x04 | Command | `MDC_Handle_Command` |
| 0x05 | Emergency | `MDC_Handle_Emergency` |
| 0x06 | Emergency + Opcode | `MDC_Handle_Emergency_WithOp` |
| 0x07 | Emergency + Acknowledge | `MDC_Handle_Emergency_WithAck` |

Frames are dispatched through a static table in `mdc_handler.c` (`g_MDC_Handlers[]`).

---

## Dispatch Flow

```
APP_HandleMDC1200Receive()            [App/app/app.c, 10ms slice]
  └─ MDC1200_DecodeFrameWords()       [App/mdc1200.c — de-interleave + CRC]
  └─ MDC_DispatchFrame(op, arg, unit_id, valid)   [App/mdc_handler.c]
       ├─ Update g_MDC_LastRxFrame (unit_id, opcode, argument, timestamp, is_new)
       ├─ if (!valid) → MDC_Handle_Unknown()
       └─ else → g_MDC_Handlers[opcode](unit_id, arg)
            ├─ MDC_TriggerDisplay(is_emergency, timeout_ms)
            │    ├─ Save previous center_line mode
            │    ├─ Set dismiss_time (0 = manual dismiss for emergency)
            │    └─ center_line = CENTER_LINE_MDC_ALERT; gUpdateDisplay = true
            └─ MDC_PlayAlert(type) → AUDIO_PlayBeep(...)
```

Invalid frames (CRC failure) are routed to `MDC_Handle_Unknown()` and shown
briefly (2 s) with no beep to avoid alert fatigue.

---

## Handler Reactions

| Handler | Display | Timeout | Audio |
|---------|---------|---------|-------|
| Status (0x00) | Routine alert | 3 s auto-close | Soft beep |
| Acknowledge (0x01) | Routine alert | 3 s auto-close | Confirmation beep |
| Request (0x02) | Routine alert | 3 s auto-close | Soft beep |
| Command (0x04) | Routine alert | 3 s auto-close | Soft beep |
| Emergency (0x05) | Inverted alert | Manual dismiss | Triple 880 Hz beep |
| Emergency+Op (0x06) | Inverted alert | Manual dismiss | Triple 880 Hz beep |
| Emergency+Ack (0x07) | Inverted alert | Manual dismiss | Triple 880 Hz beep |
| Unknown / invalid | Routine alert | 2 s auto-close | None |

---

## Display Integration (Phase 3 hooks)

- `UI_DisplayMDCAlert()` (`App/ui/mdc.c`) renders on frame-buffer lines 3–6:
  - Line 3: opcode name (inverted text for emergency)
  - Line 4: `Unit: 0xXXXX`
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

---

## Status

- ✅ Opcode dispatch table with runtime re-registration
- ✅ All 7 standard handlers + unknown/invalid fallback
- ✅ Center-line alert rendering with emergency inversion
- ✅ Audio alerts per handler type
- ✅ Auto-timeout for routine alerts, manual dismiss for emergency
- ✅ Hard-decision Viterbi ECC correction on RX decode (recovers bit-error frames)
- ❌ Alias table / contacts lookup — not implemented (planned)
- ❌ PTT-ID auto-transmit on RX — not implemented (planned)

---

*Document Version: 1.0*
*Last Updated: 2026-08-22*
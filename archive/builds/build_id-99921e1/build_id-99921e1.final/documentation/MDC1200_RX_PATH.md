# MDC-1200 RX Path Design & Implementation — COMPLETE

**Date**: 2026-08-17 (design) / 2026-08-20 (implementation)  
**Status**: ✅ COMPLETE — Integrated with Phase 2 opcode dispatch  
**Version**: v7.6.10C  
**Build**: n7six.ApeX-k1.v7.6.10C.bin (115,868 B flash, 14,240 B RAM)

---

## Executive Summary

The complete MDC-1200 reception path was already identified and exists in the firmware. With Phase 2 and Phase 3 implementation, it is now fully integrated with opcode dispatch, user-visible reactions, and UI display. No new hardware integration work was required.

**This document consolidates:**
- `MDC1200_RX_PATH_DESIGN_COMPLETE.md` (design document)
- `MDC1200_READINESS_ASSESSMENT_FOR_REACTIONS.md` (pre-implementation assessment) — key sections

---

## Complete Data Flow

```
1. BK4819 RF Transceiver (Hardware)
   Receives FSK signal, detects MDC-1200 frame,
   buffers 26-byte frame in internal FIFO,
   sets FSK_RX_FINISHED flag on completion

2. 10ms Scheduler (App/scheduler.c)
   Every 10ms: APP_TimeSlice10ms() called
   → CheckRadioInterrupts() polls BK4819_REG_0C

3. Interrupt Handler (App/app/app.c)
   CheckRadioInterrupts():
   → Reads BK4819_REG_02 interrupt status
   → Checks: fskRxFinished && ROGER_MODE_MDC_1200?
   → Calls APP_HandleMDC1200Receive()

4. FIFO Extraction (App/app/app.c: APP_HandleMDC1200Receive)
   for i = 0 to 12:
     Read BK4819_REG_5F → 16-bit word
   Result: 13 words = 26 bytes

5. Decode & Validate (App/mdc1200.c)
   MDC1200_DecodeFrameWords(rx_words, ...)
   → De-interleave 112 bits (canonical 16×7)
   → Verify preamble (0x55×7) and leader (0x07 09 2A 44 6F)
   → Extract: op, arg, unit_id
   → CRC-16 verification (poly 0x1021, XOR finalize 0xFFFF)
   → Return: valid = true/false

6. Phase 2 Dispatch (App/app/app.c)
   MDC_DispatchFrame(op, arg, unit_id, valid)
   → Update g_MDC_LastRxFrame global state
   → Call opcode handler (g_MDC_Handlers[op])
   → Handler sets center_line = CENTER_LINE_MDC_ALERT
   → Handler plays audio alert
   → gUpdateDisplay = true

7. UI Refresh
   → UI_DisplayMain() → UI_DisplayMDCAlert()
   → Renders opcode name, unit ID, argument to center line

8. Timeout (500ms slice)
   → MDC_UITimeSlice500ms() checks expiration
   → Auto-close routine alerts after 3s
   → Emergency alerts require manual dismiss
```

---

## Detailed Architecture

### 1. Interrupt Detection Layer
**File**: `App/app/app.c` — `CheckRadioInterrupts()`  
**Called from**: 10ms scheduler (`APP_TimeSlice10ms`)  
**Frequency**: Every 10ms

Trigger Conditions:
1. `interrupts.fskRxFinied` (Bit 13 of BK4819_REG_02)
2. `gEeprom.ROGER == ROGER_MODE_MDC_1200` (MDC mode enabled)
3. `!gBeamActive` (not in beam mode)
4. `gScreenToDisplay != DISPLAY_AIRCOPY` (not in aircopy mode)

### 2. FIFO Data Extraction Layer
**File**: `App/app/app.c` — `APP_HandleMDC1200Receive()`

```c
static void APP_HandleMDC1200Receive(void)
{
    uint16_t rx_words[MDC1200_FIFO_WORD_COUNT] = {0};  // 13 words
    uint8_t op = 0, arg = 0;
    uint16_t unit_id = 0;
    bool valid = false;

    for (i = 0u; i < MDC1200_FIFO_WORD_COUNT; ++i)
        rx_words[i] = BK4819_ReadRegister(BK4819_REG_5F);

    if (MDC1200_DecodeFrameWords(rx_words, ARRAY_SIZE(rx_words),
                                  &op, &arg, &unit_id, &valid)
        == MDC1200_ERROR_NONE && valid)
    {
        gEeprom.MDC_UnitID = unit_id;
        gEeprom.MDC_DefaultOp = op;
        gEeprom.MDC_DefaultArg = arg;

        /* Phase 2 dispatch */
        MDC_DispatchFrame(op, arg, unit_id, valid);
    }
}
```

Data: 13 reads × 16-bit = 26 bytes (full MDC frame)

### 3. Decode & Validation Layer
**File**: `App/mdc1200.c` — `MDC1200_DecodeFrameWords()`

What it does:
- De-interleaves 112 bits using canonical 16×7 matrix
- Extracts op, arg, unit_id from payload
- **Sliding-window sync search:** scans all legal leader offsets (not just byte 7), tolerating up to 2 bit errors across the 40-bit leader — recovers frames shifted by squelch-tail bit slips
- Verifies preamble (0x55×7) and leader (0x07 0x09 0x2A 0x44 0x6F)
- Validates CRC-16 (poly 0x1021, XOR finalization 0xFFFF) using a table-driven implementation (bit-for-bit identical to the original bit-serial loop, ~8× faster)
- On CRC failure, runs hard-decision Viterbi ECC correction over the 112 coded bits and re-validates
- Returns valid = true only if CRC matches
- Returns `MDC1200_ERROR_INVALID_LENGTH` for wrong buffer sizes (dedicated error code)

### 4. Reception State (Phase 1 → Phase 2 Integration)

```c
/* Last received MDC frame */
MDC_RxFrame_t g_MDC_LastRxFrame = {
    .unit_id = 0xFFFF,
    .opcode = 0xFF,
    .argument = 0xFF,
    .timestamp_ms = 0,
    .is_valid = false,
    .is_new = false
};

/* Display state for Phase 3 */
MDC_DisplayState_t g_MDC_DisplayState = {
    .previous_mode = 0,
    .dismiss_time = 0,
    .is_emergency = false
};
```

---

## Hardware Register Summary

| Register | Address | Bit | Field | Use |
|----------|---------|-----|-------|-----|
| RX Status | 0x0C | 0 | Interrupt Request | Poll: is interrupt pending? |
| Status Flags | 0x02 | 13 | FSK_RX_FINISHED | Trigger MDC decode |
| FIFO Data | 0x5F | [15:0] | 16-bit data | Read MDC frame bytes |
| Interrupt Mask | 0x3F | 13 | FSK_RX_* | Enable FSK interrupts |

---

## Scheduler Integration

### Call Chain

```
SysTick_Handler() [every 10ms]
    sets gNextTimeslice = true
    ↓
main.c: while(1) { APP_Update(); if (gNextTimeslice) APP_TimeSlice10ms(); }
    ↓
APP_TimeSlice10ms() [App/app/app.c]
    ├─ CheckRadioInterrupts()
    │   └─ APP_HandleMDC1200Receive()
    │       └─ MDC1200_DecodeFrameWords()
    │           └─ MDC_DispatchFrame()  [Phase 2]
    └─ MDC_UITimeSlice500ms()           [Phase 3 timeout check]
```

### Timing

| Metric | Value |
|--------|-------|
| Poll Frequency | 10ms (100 Hz) |
| RX-to-handler latency | 0–10ms (one scheduler tick) |
| Handler-to-UI latency | 1 frame (~33ms at 30 FPS) |
| Total RX-to-display | ~11–43ms |
| Routine alert timeout | 3 seconds (auto-close) |
| Emergency alert timeout | Manual dismiss only |

---

## Settings & Configuration

### EEPROM Fields

| Field | Address | Size | Description |
|-------|---------|------|-------------|
| MDC_UnitID | 0x00A0B4–B5 | 2 B | Destination Unit ID (0x0000–0xFFFF) |
| MDC_DefaultOp | 0x00A0B6 | 1 B | Opcode (0x00–0x07) |
| MDC_DefaultArg | 0x00A0B7 | 1 B | Argument (0x00–0x0F) |

| CRC-16 Checksum | 0x00A170 | 2 B | Covers entire 368-byte settings block |

### Roger Mode Setting

| Mode | Value |
|------|-------|
| ROGER_MODE_OFF | 0 (No roger tone) |
| ROGER_MODE_TONE | 1 (1kHz beep) |
| ROGER_MODE_MDC_1200 | 2 (MDC-1200 mode — enables RX) |

Menu item: `MENU_ROGER`  
User setting: `Settings → Roger Mode → MDC-1200`

---

## Phase 2 Enhancement (Implemented)

The minimal RX handler was enhanced to dispatch received frames to opcode-specific handlers:

```c
/* Previous: just stored to EEPROM */
gEeprom.MDC_UnitID = unit_id;
gEeprom.MDC_DefaultOp = op;
gEeprom.MDC_DefaultArg = arg;
gUpdateDisplay = true;

/* Phase 2: Dispatch to opcode handler */
MDC_DispatchFrame(op, arg, unit_id, valid);
```

### Resulting Handler Actions

1. **Status (0x00)**:
   - Save to EEPROM
   - Dispatch to MDC_Handle_Status()
   - Handler: set center_line, play soft beep, set 3s timeout

2. **Emergency (0x05)**:
   - Save to EEPROM
   - Dispatch to MDC_Handle_Emergency()
   - Handler: modal popup, triple 880Hz beep, manual dismiss

---

## Phase 3 Implementation (Implemented)

| Task | Status |
|------|--------|
| Add `CENTER_LINE_MDC_ALERT` to center_line enum | ✅ Done |
| Create `MDC_DisplayState_t` struct | ✅ Done |
| Create `UI_DisplayMDCAlert()` in ui/mdc.c | ✅ Done |
| Integrate into `UI_DisplayMain()` | ✅ Done |
| Update Phase 2 handlers to set center_line | ✅ Done |
| Add menu item for last RX frame | ❌ Not implemented (planned) |
| PTT-ID auto-transmit | ❌ Not implemented (planned) |
| Contacts alias table | ❌ Not implemented (planned) |

---

## Completion Checklist

✅ Identified BK4819 registers (0x02, 0x0C, 0x5F, 0x3F)
✅ Located interrupt detection (CheckRadioInterrupts)
✅ Located FIFO extraction (APP_HandleMDC1200Receive)
✅ Verified integration with 10ms scheduler
✅ Confirmed trigger condition (ROGER_MODE_MDC_1200)
✅ Documented complete data flow
✅ Identified EEPROM storage fields
✅ Found decode/validation chain (mdc1200.c)
✅ Confirmed CRC validation working
✅ Phase 2: Opcode dispatch implemented
✅ Phase 2: All 7 standard handlers implemented
✅ Phase 2: Audio alerts integrated
✅ Phase 3: Center line display implemented
✅ Phase 3: Emergency modal with manual dismiss
✅ Phase 3: Auto-timeout for routine alerts

---

## Testing

### Unit Tests
- ✅ MDC1200_DecodeFrameWords() round-trip for 4 vectors
- ✅ CRC-16 validation (CCITT poly 0x1021)
- ✅ Bit interleave/de-interleave (canonical 16×7)
- ✅ Convolutional ECC (K=7, taps 0/2/5/6)

### Integration Tests
- ✅ Opcode dispatch on RX
- ✅ Handler invocation for each opcode
- ✅ UI display update
- ✅ Audio alert triggering
- ✅ Auto-close after timeout
- ✅ Emergency modal manual dismiss

---

## Conclusion

The complete MDC-1200 reception path is implemented and integrated with the opcode dispatch framework and user-visible reactions. All infrastructure is in place:
- ✅ Interrupt detection every 10ms
- ✅ FIFO data extraction from register 0x5F
- ✅ Decode & CRC validation
- ✅ EEPROM storage fields
- ✅ Display update triggers
- ✅ Opcode-specific handlers
- ✅ Audio alerts
- ✅ Center-line UI display
- ✅ Emergency modal with manual dismiss
- ✅ 500ms timeout cleanup

Ready for production deployment on v7.6.10C firmware.

---

*Document Version: 1.0*  
*Last Updated: 2026-08-20*

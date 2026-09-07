/* Copyright 2026 Sean, N7SIX
 * https://github.com/N7SIX
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * MDC-1200 Opcode Handler Implementation (Phase 2)
 * Provides dispatch and user-visible reactions for received MDC frames.
 */

#include "mdc_handler.h"
#include "audio.h"
#include "settings.h"
#include "scheduler.h"
#include "globals/ui_globals.h"
#include "ui/main.h"

/* ============================================================================
 * Global State
 * ============================================================================ */

/**
 * Last received MDC frame (user-visible state).
 * Updated whenever a valid MDC frame is received.
 */
MDC_RxFrame_t g_MDC_LastRxFrame = {
    .unit_id = 0xFFFF,
    .opcode = 0xFF,
    .argument = 0xFF,
    .timestamp_ms = 0,
    .is_valid = false,
    .is_new = false
};

/**
 * Phase 3: Display state for center_line mode.
 * Manages temporary alert display with timeout.
 */
MDC_DisplayState_t g_MDC_DisplayState = {
    .previous_mode = 0,
    .dismiss_time = 0,
    .is_emergency = false
};

/**
 * Handler function dispatch.
 *
 * Dispatch is a switch on the Motorola MDC-1200 standard opcode map (see
 * mdc_handler.h) rather than an array over 0x00-0x07: genuine Motorola
 * traffic uses 0x01 (PTT ID/ANI) and 0x81/0x82/0x83 (emergency), which the
 * old 8-slot table could not represent.
 */
#if MDC1200_ENABLE_INTEROP
/* Duplicate suppression state packed into a single struct for cache locality */
static struct {
    bool     valid;
    uint16_t unit;
    uint8_t  op;
    uint32_t ms;
} s_LastRx = { .valid = false, .unit = 0, .op = 0, .ms = 0 };

#define MDC_DUP_SUPPRESS_MS 10000u      /* ignore identical repeat within 10 s */
#endif /* MDC1200_ENABLE_INTEROP */

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

static void MDC_Handle_Routine(uint16_t unit_id, uint8_t arg, uint8_t alert_type);

/* ============================================================================
 * Opcode String Lookup (Motorola terminology)
 * ============================================================================ */

const char *MDC_GetOpcodeString(uint8_t opcode)
{
    /* Compressed string table: all opcode names packed into one contiguous
     * string with NUL terminators, indexed by offset. Saves ~40 B FLASH
     * versus individual string literals. */
    static const char s_Strings[] =
        "PTT ID\0"
        "Call Alert\0"
        "Radio Check\0"
        "Status Req\0"
        "Status Resp\0"
        "Emergency\0"
        "Emg Clear\0"
        "MDC ID\0"
        "Unknown\0";

    /* Offsets into s_Strings — must match the order above */
    enum {
        OFF_PTT_ID      = 0,
        OFF_CALL_ALERT  = 7,
        OFF_RADIO_CHECK = 18,
        OFF_STATUS_REQ  = 30,
        OFF_STATUS_RESP = 41,
        OFF_EMERGENCY   = 53,
        OFF_EMG_CLEAR   = 63,
        OFF_MDC_ID      = 73,
        OFF_UNKNOWN     = 80
    };

    switch (opcode) {
        case MDC_OP_PTT_ID:                     return &s_Strings[OFF_PTT_ID];
        case MDC_OP_CALL_ALERT:
        case MDC_OP_CALL_ALERT_ACK:             return &s_Strings[OFF_CALL_ALERT];
        case MDC_OP_RADIO_CHECK:
        case MDC_OP_RADIO_CHECK_ACK:            return &s_Strings[OFF_RADIO_CHECK];
        case MDC_OP_STATUS_REQ:                 return &s_Strings[OFF_STATUS_REQ];
        case MDC_OP_STATUS_RESP:                return &s_Strings[OFF_STATUS_RESP];
        case MDC_OP_EMERGENCY:
        case MDC_OP_EMERGENCY_ALARM:            return &s_Strings[OFF_EMERGENCY];
        case MDC_OP_EMERGENCY_CLEAR:            return &s_Strings[OFF_EMG_CLEAR];
        case 0x00: case 0x02: case 0x04:        return &s_Strings[OFF_MDC_ID];
        case 0x05: case 0x06: case 0x07:        return &s_Strings[OFF_EMERGENCY];
        default:                                return &s_Strings[OFF_UNKNOWN];
    }
}

#if MDC1200_ENABLE_INTEROP
bool MDC_IsEmergencyOpcode(uint8_t opcode)
{
    /* Motorola emergency ANI / emergency+alarm, plus the legacy local
     * emergency opcodes used by pre-interop ApeX builds. */
    return (opcode >= 0x05 && opcode <= 0x07) ||
           opcode == MDC_OP_EMERGENCY ||
           opcode == MDC_OP_EMERGENCY_ALARM;
}
#endif /* MDC1200_ENABLE_INTEROP */

/* ============================================================================
 * Frame Dispatch
 * ============================================================================ */

void MDC_DispatchFrame(uint8_t opcode, uint8_t arg, uint16_t unit_id, bool is_valid)
{
    /* Update global state with last received frame */
    g_MDC_LastRxFrame.unit_id = unit_id;
    g_MDC_LastRxFrame.opcode = opcode;
    g_MDC_LastRxFrame.argument = arg;
    g_MDC_LastRxFrame.timestamp_ms = gGlobalSysTickCounter * 10u;  /* Convert ticks to ms */
    g_MDC_LastRxFrame.is_valid = is_valid;
    g_MDC_LastRxFrame.is_new = true;

    /* Reject invalid frames */
    if (!is_valid) {
        MDC_Handle_Unknown(unit_id, arg);
        return;
    }

#if MDC1200_ENABLE_INTEROP
    /* Duplicate suppression: repeated copies of the same frame (burst
     * retransmissions, repeater double-decodes) must alert only once.
     * Emergency frames are exempt so genuine re-alerts always get through. */
    if (!MDC_IsEmergencyOpcode(opcode) &&
        s_LastRx.valid &&
        s_LastRx.unit == unit_id &&
        s_LastRx.op == opcode &&
        (g_MDC_LastRxFrame.timestamp_ms - s_LastRx.ms) < MDC_DUP_SUPPRESS_MS)
    {
        return;     /* duplicate of a recently decoded frame - stay quiet */
    }
    s_LastRx.valid = true;
    s_LastRx.unit  = unit_id;
    s_LastRx.op    = opcode;
    s_LastRx.ms    = g_MDC_LastRxFrame.timestamp_ms;
#endif /* MDC1200_ENABLE_INTEROP */

    /* Dispatch on the MDC-1200 opcode map.
     * All routine handlers share the same body (TriggerDisplay + PlayAlert),
     * so they collapse to parameterized calls. Emergency handlers are
     * separate (permanent display + emergency warble). */
    switch (opcode) {
#if MDC1200_ENABLE_INTEROP
        /* ---- Motorola MDC-1200 standard opcodes ---- */
        case MDC_OP_PTT_ID:
            MDC_Handle_Routine(unit_id, arg, 0);
            break;
        case MDC_OP_CALL_ALERT:
        case MDC_OP_CALL_ALERT_ACK:
            MDC_Handle_Routine(unit_id, arg, 1);
            break;
        case MDC_OP_RADIO_CHECK:
        case MDC_OP_RADIO_CHECK_ACK:
            MDC_Handle_Routine(unit_id, arg, 1);
            break;
        case MDC_OP_STATUS_REQ:
        case MDC_OP_STATUS_RESP:
            MDC_Handle_Routine(unit_id, arg, 0);
            break;
        case MDC_OP_EMERGENCY:
        case MDC_OP_EMERGENCY_ALARM:
            MDC_Handle_Emergency(unit_id, arg);
            break;
        case MDC_OP_EMERGENCY_CLEAR:
            MDC_Handle_Routine(unit_id, arg, 1);
            break;
#endif /* MDC1200_ENABLE_INTEROP */

        /* ---- Legacy local ApeX opcodes (pre-interop firmware) ---- */
        case 0x00:
        case 0x02:
        case 0x04:
            MDC_Handle_Routine(unit_id, arg, 0);
            break;
        case 0x05:
        case 0x06:
        case 0x07:
            MDC_Handle_Emergency(unit_id, arg);
            break;

        default:
            MDC_Handle_Unknown(unit_id, arg);
            break;
    }
}

/* ============================================================================
 * Phase 3: Display Control (Center Line Mode)
 * ============================================================================ */

static void MDC_TriggerDisplay(bool is_emergency, uint32_t timeout_ms)
{
    /* Save current center_line mode to restore later */
    g_MDC_DisplayState.previous_mode = center_line;
    g_MDC_DisplayState.is_emergency = is_emergency;
    
    if (timeout_ms > 0) {
        /* Auto-close after timeout (routine alerts) */
        g_MDC_DisplayState.dismiss_time = gGlobalSysTickCounter + (timeout_ms / 10u);
    } else {
        /* Permanent display until manual dismiss (emergency) */
        g_MDC_DisplayState.dismiss_time = 0;
    }
    
    /* Switch to MDC alert display */
    center_line = CENTER_LINE_MDC_ALERT;
    gUpdateDisplay = true;
}

/* ============================================================================
 * Display & Audio Utilities
 * ============================================================================ */

void MDC_PlayAlert(int alert_type)
{
    /*
     * MDC-ID-received indication: play the characteristic MDC preamble
     * warble LOCALLY (speaker) instead of a plain key beep.
     *
     * AUDIO_PlayBeep() cannot be used here: it refuses to play while the
     * radio is in FUNCTION_RECEIVE - exactly the state this alert fires in,
     * since the MDC frame is decoded from an incoming transmission. The
     * warble is generated locally from the decoded frame, so it is heard
     * on both simplex and repeater channels regardless of whether the
     * repeater relays the on-air data-burst audio.
     */
    /* Collapse redundant cases: 0/1 → single warble, 2/3 → double warble */
    static const uint8_t warble_table[] = {1, 1, 2, 2};
    if ((uint8_t)alert_type < 4) {
        AUDIO_PlayMDCWarble(warble_table[alert_type]);
    }
}

/* ============================================================================
 * Built-in Handler Implementations
 * ============================================================================ */

static void MDC_Handle_Routine(uint16_t unit_id, uint8_t arg, uint8_t alert_type)
{
    /* All routine handlers share the same body: auto-close display + warble.
     * alert_type selects single (0/1) vs double (2/3) warble via PlayAlert. */
    (void)unit_id;
    (void)arg;
    MDC_TriggerDisplay(false, 3000u);
    MDC_PlayAlert(alert_type);
}

void MDC_Handle_Emergency(uint16_t unit_id, uint8_t arg)
{
    /* Permanent display until manual dismiss + emergency warble.
     * Used for both Motorola emergency opcodes (0x81/0x82) and legacy
     * local emergency opcodes (0x05/0x06/0x07). */
    (void)unit_id;
    (void)arg;
    MDC_TriggerDisplay(true, 0);
    MDC_PlayAlert(3);

    /* Note: Future enhancements could add:
     * - Turn on backlight at max brightness
     * - Switch to emergency/priority channel
     * - Auto-PTT (optional, safety-critical)
     * - LED red blink pattern
     */
}

void MDC_Handle_Unknown(uint16_t unit_id, uint8_t arg)
{
    MDC_TriggerDisplay(false, 2000u);    /* Auto-close after 2 seconds */
    /* No beep for unknown frames to avoid alert fatigue */
}

/* ============================================================================
 * Periodic Update (called from 500ms UI slice)
 * ============================================================================ */

void MDC_TimeSlice500ms(void)
{
    /* Clear "is_new" flag after display update */
    if (g_MDC_LastRxFrame.is_new) {
        g_MDC_LastRxFrame.is_new = false;
    }
}

/* ============================================================================
 * Phase 3: UI Display Functions
 * ============================================================================ */

void MDC_UITimeSlice500ms(void)
{
    /* Phase 3: Check if routine MDC alert should auto-close */
    if (center_line == CENTER_LINE_MDC_ALERT &&
        !g_MDC_DisplayState.is_emergency &&
        g_MDC_DisplayState.dismiss_time > 0 &&
        gGlobalSysTickCounter >= g_MDC_DisplayState.dismiss_time)
    {
        /* Auto-close: restore previous center_line mode */
        center_line = g_MDC_DisplayState.previous_mode;
        gUpdateDisplay = true;
    }
}

/* ============================================================================
 * Phase 3: UI Display Rendering
 * ============================================================================ */



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
#include <stdio.h>
#include <string.h>

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
 * Status message buffer for UI display.
 * Displayed in status bar, auto-clears after timeout.
 */
static struct {
    char message[48];
    uint32_t timeout_ms;
    uint32_t expire_time;
    bool active;
} g_MDC_StatusMessage = {0};

/**
 * Handler function pointers (dispatch table).
 * One function per possible opcode (0x00–0x07, plus one for unknown).
 */
static MDC_OpcodeHandler_t g_MDC_Handlers[8] = {
    MDC_Handle_Status,              /* 0x00: Status */
    MDC_Handle_Acknowledge,         /* 0x01: Acknowledge */
    MDC_Handle_Request,             /* 0x02: Request */
    NULL,                           /* 0x03: Reserved */
    MDC_Handle_Command,             /* 0x04: Command */
    MDC_Handle_Emergency,           /* 0x05: Emergency */
    MDC_Handle_Emergency_WithOp,    /* 0x06: Emergency + Opcode */
    MDC_Handle_Emergency_WithAck    /* 0x07: Emergency + Ack */
};

/* ============================================================================
 * Opcode String Lookup
 * ============================================================================ */

const char *MDC_GetOpcodeString(uint8_t opcode)
{
    static const char *opcode_names[] = {
        "Status",               /* 0x00 */
        "Acknowledge",          /* 0x01 */
        "Request",              /* 0x02 */
        "Reserved",             /* 0x03 */
        "Command",              /* 0x04 */
        "Emergency",            /* 0x05 */
        "Emergency+Op",         /* 0x06 */
        "Emergency+Ack"         /* 0x07 */
    };

    if (opcode < sizeof(opcode_names) / sizeof(opcode_names[0])) {
        return opcode_names[opcode];
    }
    return "Unknown";
}

/* ============================================================================
 * Handler Registration
 * ============================================================================ */

void MDC_RegisterHandler(uint8_t opcode, MDC_OpcodeHandler_t handler)
{
    if (opcode < sizeof(g_MDC_Handlers) / sizeof(g_MDC_Handlers[0])) {
        g_MDC_Handlers[opcode] = handler;
    }
}

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

    /* Dispatch to opcode-specific handler */
    if (opcode < sizeof(g_MDC_Handlers) / sizeof(g_MDC_Handlers[0])) {
        MDC_OpcodeHandler_t handler = g_MDC_Handlers[opcode];
        if (handler != NULL) {
            handler(unit_id, arg);
            return;
        }
    }

    /* Handler not found → unknown opcode */
    MDC_Handle_Unknown(unit_id, arg);
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

void MDC_DisplayStatusUpdate(const char *message, uint32_t timeout_ms)
{
    strncpy(g_MDC_StatusMessage.message, message, sizeof(g_MDC_StatusMessage.message) - 1);
    g_MDC_StatusMessage.message[sizeof(g_MDC_StatusMessage.message) - 1] = '\0';
    g_MDC_StatusMessage.timeout_ms = timeout_ms;
    g_MDC_StatusMessage.expire_time = gGlobalSysTickCounter + (timeout_ms / 10u);
    g_MDC_StatusMessage.active = true;
    gUpdateDisplay = true;
}

void MDC_ShowModal(const char *title, const char *message, uint32_t timeout_ms)
{
    /* 
     * Note: Full modal implementation would go here.
     * For now, display as status message.
     * In Phase 3, integrate with UI framework for true modal.
     */
    char combined[96];
    snprintf(combined, sizeof(combined), "%s: %s", title, message);
    MDC_DisplayStatusUpdate(combined, timeout_ms ? timeout_ms : 3000u);  /* 3s default */
}

void MDC_PlayAlert(int alert_type)
{
    BEEP_Type_t beep_type;

    switch (alert_type) {
        case 0:  /* Soft beep (notification) */
            beep_type = BEEP_1KHZ_60MS_OPTIONAL;
            break;
        case 1:  /* Confirmation beep (ACK) */
            beep_type = BEEP_500HZ_30MS;
            break;
        case 2:  /* Alert tone (warning) */
            beep_type = BEEP_880HZ_60MS_TRIPLE_BEEP;
            break;
        case 3:  /* Emergency alert (loudest) */
            beep_type = BEEP_880HZ_60MS_TRIPLE_BEEP;
            break;
        default:
            return;
    }

    AUDIO_PlayBeep(beep_type);
}

/* ============================================================================
 * Built-in Handler Implementations
 * ============================================================================ */

void MDC_Handle_Status(uint16_t unit_id, uint8_t arg)
{
    MDC_TriggerDisplay(false, 3000u);    /* Auto-close after 3 seconds */
    MDC_PlayAlert(0);                    /* Soft beep notification */
}

void MDC_Handle_Acknowledge(uint16_t unit_id, uint8_t arg)
{
    MDC_TriggerDisplay(false, 3000u);    /* Auto-close after 3 seconds */
    MDC_PlayAlert(1);                    /* Confirmation beep */
}

void MDC_Handle_Request(uint16_t unit_id, uint8_t arg)
{
    MDC_TriggerDisplay(false, 3000u);    /* Auto-close after 3 seconds */
    MDC_PlayAlert(0);                    /* Soft beep */
}

void MDC_Handle_Command(uint16_t unit_id, uint8_t arg)
{
    MDC_TriggerDisplay(false, 3000u);    /* Auto-close after 3 seconds */
    MDC_PlayAlert(0);                    /* Soft beep */
}

void MDC_Handle_Emergency(uint16_t unit_id, uint8_t arg)
{
    MDC_TriggerDisplay(true, 0);         /* Permanent until manual dismiss */
    MDC_PlayAlert(3);                    /* Emergency alert (loudest) */

    /* Note: Future enhancements could add:
     * - Turn on backlight at max brightness
     * - Switch to emergency/priority channel
     * - Auto-PTT (optional, safety-critical)
     * - LED red blink pattern
     */
}

void MDC_Handle_Emergency_WithOp(uint16_t unit_id, uint8_t arg)
{
    MDC_TriggerDisplay(true, 0);         /* Permanent until manual dismiss */
    MDC_PlayAlert(3);                    /* Loudest alert */
}

void MDC_Handle_Emergency_WithAck(uint16_t unit_id, uint8_t arg)
{
    MDC_TriggerDisplay(true, 0);         /* Permanent until manual dismiss */
    MDC_PlayAlert(3);                    /* Loudest alert */
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
    /* Check if status message should expire */
    if (g_MDC_StatusMessage.active) {
        if (gGlobalSysTickCounter >= g_MDC_StatusMessage.expire_time) {
            g_MDC_StatusMessage.active = false;
            gUpdateDisplay = true;
        }
    }

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
 * UI Integration (status bar display)
 * ============================================================================ */

const char *MDC_GetStatusMessage(void)
{
    if (g_MDC_StatusMessage.active) {
        return g_MDC_StatusMessage.message;
    }
    return NULL;
}

/* ============================================================================
 * Phase 3: UI Display Rendering
 * ============================================================================ */



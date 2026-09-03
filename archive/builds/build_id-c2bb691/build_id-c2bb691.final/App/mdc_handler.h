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
 * MDC-1200 Opcode Handler Framework (Phase 2)
 * Provides dispatch and user-visible reactions for received MDC frames.
 */

#ifndef MDC_HANDLER_H
#define MDC_HANDLER_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * MDC-1200 Opcode Definitions (Motorola Standard)
 * ============================================================================ */

typedef enum {
    MDC_OP_STATUS = 0x00,           /*!< Status Report (sender info) */
    MDC_OP_ACK = 0x01,              /*!< Acknowledge Receipt */
    MDC_OP_REQUEST = 0x02,          /*!< Information Request */
    MDC_OP_RESERVED = 0x03,         /*!< Reserved (ignore) */
    MDC_OP_COMMAND = 0x04,          /*!< Command / Instruction */
    MDC_OP_EMERGENCY = 0x05,        /*!< Emergency Signal */
    MDC_OP_EMERGENCY_WITH_OP = 0x06,    /*!< Emergency + Opcode */
    MDC_OP_EMERGENCY_WITH_ACK = 0x07,   /*!< Emergency + Acknowledge */
} MDC_Opcode_t;

/* ============================================================================
 * Global RX Frame State (Last Received MDC Frame)
 * ============================================================================ */

typedef struct {
    uint16_t    unit_id;            /*!< Sender Unit ID */
    uint8_t     opcode;             /*!< Operation (0x00–0x07) */
    uint8_t     argument;           /*!< Argument (0x00–0x0F) */
    uint32_t    timestamp_ms;       /*!< System time when frame was received */
    bool        is_valid;           /*!< CRC verified? */
    bool        is_new;             /*!< New frame since last display update? */
} MDC_RxFrame_t;

extern MDC_RxFrame_t g_MDC_LastRxFrame;

/* ============================================================================
 * Phase 3: Display State (Center Line Mode)
 * ============================================================================ */

typedef struct {
    uint32_t    previous_mode;      /*!< center_line_t saved (restore after timeout) */
    uint32_t    dismiss_time;       /*!< Tick count when to auto-close (0 = permanent) */
    bool        is_emergency;       /*!< Emergency frame? Requires manual dismiss */
} MDC_DisplayState_t;

extern MDC_DisplayState_t g_MDC_DisplayState;

/* ============================================================================
 * Opcode Handler Callback Interface
 * ============================================================================ */

/**
 * MDC opcode handler callback type.
 * 
 * Called when a valid MDC frame with a specific opcode is received.
 * Responsible for:
 *  - Triggering audio alerts (beeps, tones)
 *  - Updating status display (status bar, modal)
 *  - Logging or storing the frame
 *  - Executing any opcode-specific actions
 *
 * @param unit_id   - Sender's Unit ID (0x0000–0xFFFF)
 * @param argument  - Argument field (0x00–0x0F, opcode-dependent meaning)
 */
typedef void (*MDC_OpcodeHandler_t)(uint16_t unit_id, uint8_t argument);

/* ============================================================================
 * Handler Dispatch
 * ============================================================================ */

/**
 * Dispatch a received MDC frame to the appropriate handler.
 *
 * @param opcode   - Operation (0x00–0x07)
 * @param arg      - Argument (0x00–0x0F)
 * @param unit_id  - Sender Unit ID (0x0000–0xFFFF)
 * @param is_valid - CRC verified?
 *
 * Called automatically from APP_HandleMDC1200Receive() after decode/CRC check.
 */
void MDC_DispatchFrame(uint8_t opcode, uint8_t arg, uint16_t unit_id, bool is_valid);

/* ============================================================================
 * Built-in Handler Implementations
 * ============================================================================ */

/**
 * Handler for Status (0x00) frames.
 * 
 * Reaction:
 *  - Store frame in global state
 *  - Update display: "MDC Status: 0x1234"
 *  - Optional: Play soft notification tone
 */
void MDC_Handle_Status(uint16_t unit_id, uint8_t arg);

/**
 * Handler for Acknowledge (0x01) frames.
 *
 * Reaction:
 *  - Store frame in global state
 *  - Update display: "MDC ACK: 0x1234"
 *  - Optional: Play confirmation beep
 */
void MDC_Handle_Acknowledge(uint16_t unit_id, uint8_t arg);

/**
 * Handler for Request (0x02) frames.
 *
 * Reaction:
 *  - Store frame in global state
 *  - Log request type (argument)
 *  - Display: "MDC Request: 0x1234"
 */
void MDC_Handle_Request(uint16_t unit_id, uint8_t arg);

/**
 * Handler for Command (0x04) frames.
 *
 * Reaction:
 *  - Store frame in global state
 *  - Log command
 *  - Display: "MDC Command: 0x1234"
 */
void MDC_Handle_Command(uint16_t unit_id, uint8_t arg);

/**
 * Handler for Emergency (0x05) frames.
 *
 * Reaction:
 *  - Store frame in global state
 *  - LOUD ALERT TONE (100 dB equivalent)
 *  - Modal popup: "!!! EMERGENCY FROM 0x1234 !!!"
 *  - Red LED blink pattern
 *  - Possible: Switch to emergency channel
 */
void MDC_Handle_Emergency(uint16_t unit_id, uint8_t arg);

/**
 * Handler for Emergency + Opcode (0x06) frames.
 *
 * Reaction:
 *  - Handle emergency alert (same as 0x05)
 *  - Execute embedded opcode from argument
 *  - Display: "EMERGENCY OP: 0x1234 (arg=0x0F)"
 */
void MDC_Handle_Emergency_WithOp(uint16_t unit_id, uint8_t arg);

/**
 * Handler for Emergency + Acknowledge (0x07) frames.
 *
 * Reaction:
 *  - Handle emergency alert (same as 0x05)
 *  - Confirm receipt (similar to ACK)
 *  - Display: "EMERGENCY ACK: 0x1234"
 */
void MDC_Handle_Emergency_WithAck(uint16_t unit_id, uint8_t arg);

/**
 * Handler for invalid/unrecognized opcodes.
 *
 * Reaction:
 *  - Store frame in global state
 *  - Display: "MDC Unknown: 0x1234 (op=0x??)"
 */
void MDC_Handle_Unknown(uint16_t unit_id, uint8_t arg);

/* ============================================================================
 * Display & Audio Reaction Utilities
 * ============================================================================ */

/**
 * Play audio alert tone.
 *
 * @param alert_type - 0=soft_beep, 1=confirmation, 2=alert, 3=emergency
 */
void MDC_PlayAlert(int alert_type);

/**
 * Get string description of an MDC opcode.
 *
 * @param opcode - Opcode (0x00–0x07 or other)
 * @return Human-readable string (e.g., "Status", "Emergency")
 */
const char *MDC_GetOpcodeString(uint8_t opcode);

/* ============================================================================
 * Phase 3: UI Display Functions
 * ============================================================================ */

/**
 * Render MDC alert to center line display area.
 * 
 * Called from UI_DisplayMain() when center_line == CENTER_LINE_MDC_ALERT.
 * Displays last received MDC frame information with auto-timeout for routine alerts.
 */
void UI_DisplayMDCAlert(void);

/**
 * Handle dismissal of emergency MDC alert (user presses any key).
 * 
 * Restores previous center_line mode and updates display.
 */
void UI_HandleMDCDismiss(void);

/**
 * Check and process MDC display timeout.
 *
 * Called from UI_TimeSlice500ms() to handle auto-close of routine alerts.
 */
void MDC_UITimeSlice500ms(void);

/**
 * Periodic MDC handler update (called from APP_TimeSlice500ms()).
 *
 * Handles status-message expiry and clears the "is_new" frame flag.
 */
void MDC_TimeSlice500ms(void);

#ifdef __cplusplus
}
#endif

#endif /* MDC_HANDLER_H */

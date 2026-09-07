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
 * MDC-1200 Opcode Definitions
 *
 * Motorola MDC-1200 standard opcodes (per the commercial protocol):
 *   0x01 = PTT ID / ANI (the standard identification burst)
 *   0x30 / 0x31 = Call Alert (+ ack)
 *   0x40 / 0x41 = Radio Check (+ ack)
 *   0x46 / 0x47 = Status Request / Status Response
 *   0x81 / 0x82 = Emergency (0x83 = Emergency Clear/Ack)
 *   0x84+       = Remote Monitor and other system features
 *
 * The 0x00-0x07 range below is the legacy local ApeX map used before the
 * Motorola interop remap; it is still accepted on RX (and selectable via
 * EEPROM/CHIRP) so older bursts keep working between ApeX radios.
 *
 * Feature flag: MDC1200_ENABLE_INTEROP
 *   When enabled (default), the full Motorola interop feature set is built:
 *   - Motorola opcode map dispatch (0x01, 0x30/0x31, 0x40/0x41, etc.)
 *   - Emergency 3x repeated burst on TX
 *   - Decimal Unit ID display (Motorola convention)
 *   - RX duplicate suppression (10 s window)
 *   When disabled, only the legacy 0x00-0x07 map is dispatched and the
 *   simpler TX/RX behavior applies — saving ~300 B FLASH.
 * ============================================================================ */

#ifndef MDC1200_ENABLE_INTEROP
#define MDC1200_ENABLE_INTEROP  1   /*!< Enable Motorola interop features by default */
#endif

#define MDC_OP_PTT_ID           0x01    /*!< Motorola standard PTT ID / ANI */
#define MDC_OP_CALL_ALERT       0x30    /*!< Motorola Call Alert */
#define MDC_OP_CALL_ALERT_ACK   0x31    /*!< Motorola Call Alert ack */
#define MDC_OP_RADIO_CHECK      0x40    /*!< Motorola Radio Check */
#define MDC_OP_RADIO_CHECK_ACK  0x41    /*!< Motorola Radio Check ack */
#define MDC_OP_STATUS_REQ       0x46    /*!< Motorola Status Request */
#define MDC_OP_STATUS_RESP      0x47    /*!< Motorola Status Response */
#define MDC_OP_EMERGENCY        0x81    /*!< Motorola Emergency ANI */
#define MDC_OP_EMERGENCY_ALARM  0x82    /*!< Motorola Emergency + alarm */
#define MDC_OP_EMERGENCY_CLEAR  0x83    /*!< Motorola Emergency clear/ack */

/* Legacy local ApeX opcode map (pre-interop firmware) */
typedef enum {
    MDC_OP_LEGACY_STATUS = 0x00,        /*!< Legacy: Status Report */
    MDC_OP_LEGACY_ACK = 0x01,           /*!< Legacy: Acknowledge Receipt */
    MDC_OP_LEGACY_REQUEST = 0x02,       /*!< Legacy: Information Request */
    MDC_OP_LEGACY_RESERVED = 0x03,      /*!< Legacy: Reserved (ignore) */
    MDC_OP_LEGACY_COMMAND = 0x04,       /*!< Legacy: Command / Instruction */
    MDC_OP_LEGACY_EMERGENCY = 0x05,     /*!< Legacy: Emergency Signal */
    MDC_OP_LEGACY_EMERGENCY_WITH_OP = 0x06,  /*!< Legacy: Emergency + Opcode */
    MDC_OP_LEGACY_EMERGENCY_WITH_ACK = 0x07, /*!< Legacy: Emergency + Acknowledge */
} MDC_Opcode_t;

#if MDC1200_ENABLE_INTEROP
/**
 * Classify an opcode as an emergency (used for loud alert on RX and for the
 * 3x repeated emergency burst on TX, matching Motorola practice).
 *
 * Motorola emergencies: 0x81 / 0x82. Legacy local emergencies: 0x05-0x07.
 */
bool MDC_IsEmergencyOpcode(uint8_t opcode);
#endif /* MDC1200_ENABLE_INTEROP */

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
 * Shared handler for all emergency opcodes.
 *
 * Covers both Motorola emergency ANI (0x81/0x82) and legacy local emergency
 * opcodes (0x05/0x06/0x07). Reaction: permanent display until manual dismiss
 * + emergency (double) warble.
 *
 * @param unit_id - Sender Unit ID
 * @param arg     - Argument field
 */
void MDC_Handle_Emergency(uint16_t unit_id, uint8_t arg);

/**
 * Handler for invalid/unrecognized opcodes.
 *
 * Reaction:
 *  - Store frame in global state
 *  - Display: "MDC Unknown: 0x1234 (op=0x??)"
 *  - Auto-close after 2 s, no warble (avoids alert fatigue)
 *
 * @param unit_id - Sender Unit ID
 * @param arg     - Argument field
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

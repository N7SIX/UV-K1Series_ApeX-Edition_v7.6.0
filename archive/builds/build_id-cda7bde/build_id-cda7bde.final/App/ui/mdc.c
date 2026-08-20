/* ============================================================================
 * MDC-1200 UI Display Module
 * 
 * Phase 3: Display integration for MDC-1200 alert notifications
 * ============================================================================
 */

#include <string.h>
#include <stdio.h>

#include "app/app.h"
#include "driver/st7565.h"
#include "ui/helper.h"
#include "ui/main.h"
#include "../mdc_handler.h"
#include "scheduler.h"
#include "globals/ui_globals.h"

/* ============================================================================
 * UI Display Functions
 * ============================================================================
 */

/**
 * Render MDC alert to center line display area (gFrameBuffer[3-6]).
 * 
 * Display format:
 * - Line 3: Opcode name (Status, Ack, Request, Command, Emergency, Unknown)
 * - Line 4: "Unit: 0xXXXX" 
 * - Line 5: "Arg: 0xXX" or "Press to dismiss" for emergency
 * - Line 6: Auto-timeout countdown for routine alerts (e.g., "Close in 3s")
 * 
 * Emergency alerts use inverse text (white on black) for maximum visibility.
 * Routine alerts use normal text and auto-close after timeout.
 */
void UI_DisplayMDCAlert(void)
{
    char String[64];
    uint16_t unit_id;
    uint8_t opcode;
    uint8_t argument;
    const char *opcode_name;
    bool is_emergency;
    uint32_t remaining_ticks;
    
    /* Get MDC frame data */
    if (!g_MDC_LastRxFrame.is_valid) {
        /* FSK burst received but framing/CRC failed. Show a visible
         * diagnostic instead of rendering nothing, so a decode miss on the
         * RX side is never mistaken for 'burst not received at all'. */
        snprintf(String, sizeof(String), "MDC: RX Error");
        UI_PrintStringSmallNormal(String, 0, 127, 3);
        snprintf(String, sizeof(String), "Frame CRC failed");
        UI_PrintStringSmallNormal(String, 0, 127, 4);
        snprintf(String, sizeof(String), "Check TX unit ID");
        UI_PrintStringSmallNormal(String, 0, 127, 5);
        return;
    }
    
    unit_id = g_MDC_LastRxFrame.unit_id;
    opcode = g_MDC_LastRxFrame.opcode;
    argument = g_MDC_LastRxFrame.argument;
    is_emergency = g_MDC_DisplayState.is_emergency;
    
    /* Get opcode name for display */
    opcode_name = MDC_GetOpcodeString(opcode);
    
    /* ===== Line 3: Opcode Type ===== */
    if (is_emergency) {
        /* Emergency: Large, inverted text for maximum visibility */
        snprintf(String, sizeof(String), "!!! %s !!!", opcode_name);
        UI_PrintStringSmallNormalInverse(String, 0, 127, 3);
    } else {
        /* Routine: Normal text */
        snprintf(String, sizeof(String), "MDC: %s", opcode_name);
        UI_PrintStringSmallNormal(String, 0, 127, 3);
    }
    
    /* ===== Line 4: Unit ID ===== */
    snprintf(String, sizeof(String), "Unit: 0x%04X", unit_id);
    if (is_emergency) {
        UI_PrintStringSmallNormalInverse(String, 0, 127, 4);
    } else {
        UI_PrintStringSmallNormal(String, 0, 127, 4);
    }
    
    /* ===== Line 5: Argument or Dismiss Instruction ===== */
    if (is_emergency) {
        /* Emergency: Prompt user to dismiss */
        snprintf(String, sizeof(String), "Press key to dismiss");
    } else {
        /* Routine: Show argument value */
        snprintf(String, sizeof(String), "Arg: 0x%02X", argument);
    }
    
    if (is_emergency) {
        UI_PrintStringSmallNormalInverse(String, 0, 127, 5);
    } else {
        UI_PrintStringSmallNormal(String, 0, 127, 5);
    }
    
    /* ===== Line 6: Auto-timeout Countdown (Routine Only) ===== */
    if (!is_emergency && g_MDC_DisplayState.dismiss_time > 0) {
        /* Calculate remaining time in tenths of seconds (gGlobalSysTickCounter is in 10ms units) */
        uint32_t current_time = gGlobalSysTickCounter;
        uint32_t dismiss_time = g_MDC_DisplayState.dismiss_time;
        
        if (current_time < dismiss_time) {
            remaining_ticks = dismiss_time - current_time;
            uint16_t remaining_ms = remaining_ticks * 10;  /* Convert 10ms ticks to ms */
            uint8_t remaining_sec = (remaining_ms + 500) / 1000;  /* Round to nearest second */
            
            if (remaining_sec > 0) {
                snprintf(String, sizeof(String), "Close in %u sec", remaining_sec);
                UI_PrintStringSmallNormal(String, 0, 127, 6);
            }
        }
    }
}

/**
 * Handle dismissal of emergency MDC alert (user pressed key during emergency).
 * 
 * Restores previous center_line mode and triggers display update.
 * Called when user presses any key while emergency MDC alert is displayed.
 */
void UI_HandleMDCDismiss(void)
{
    if (center_line == CENTER_LINE_MDC_ALERT &&
        g_MDC_DisplayState.is_emergency)
    {
        /* User dismissed emergency alert: restore previous mode */
        center_line = g_MDC_DisplayState.previous_mode;
        gUpdateDisplay = true;
    }
}

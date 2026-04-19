/**
 * =====================================================================================
 * @file        events_handlers.c
 * @brief       Event handler implementations (Phase 2 Integration)
 * @author      N7SIX (Professional Enhancements, 2025-2026)
 * @author      Sean (Event System Architecture, 2025-2026)
 * @version     v7.6.0 (ApeX Edition)
 * @license     Apache License, Version 2.0
 * * "Event System Phase 2: Handler Integration for TX Channel State Management."
 * =====================================================================================
 *
 * Event handler implementations for APP_EVENT_SAVE_CHANNEL and future events.
 * Each handler is a callback that gets invoked when its subscribed event is raised.
 *
 * HANDLER RESPONSIBILITIES:
 *
 * 1. SETTINGS_OnSaveChannel (settings.c handler)
 *    - Copy TX channel data to gEeprom global struct
 *    - Call SETTINGS_SaveSettings() to persist to EEPROM
 *    - Validate data before saving
 *
 * 2. RADIO_OnSaveChannel (radio.c handler)
 *    - Update BK4819 radio IC with new TX frequency/power/modulation
 *    - Call BK4819_SetFrequency(), BK4819_SetRF(), etc.
 *    - Ensure TX settings are immediately active
 *
 * 3. UI_OnChannelChange (ui.c handler)
 *    - Request display refresh by setting gUpdateDisplay flag
 *    - Render updated channel information to framebuffer
 *    - Call ST7565_BlitFullScreen() if needed
 *
 * THREAD SAFETY:
 *   These handlers block on SPI/EEPROM writes and are NOT safe for ISR
 *   context. Call APP_RaiseEvent() from main task only, not from
 *   interrupts. Synchronous execution ensures radio state consistency.
 *
 * MEMORY MODEL:
 *   - Event data is passed by pointer (not copied)
 *   - Data is only valid during callback execution
 *   - Handlers must copy data if persistence is needed (e.g., to gEeprom)
 *   - Total handler registration overhead: ~56 bytes (7 * 8 bytes/callback)
 */

#include "events_handlers.h"
#include "events.h"
#include "../settings_event.h"

// Forward declarations (actual implementations in respective modules)
extern void SETTINGS_OnSaveChannel(APP_EventType_t event, const void *data);
extern void RADIO_OnSaveChannel(APP_EventType_t event, const void *data);
extern void UI_OnChannelChange(APP_EventType_t event, const void *data);

/**
 * @brief Initialize all event handlers
 *
 * Registers handler callbacks with the event system. Should be called
 * once at application startup, after APP_EventSystem_Init().
 *
 * REGISTRATION:
 * ┌─────────────────────────────────────────┐
 * │ APP_EVENT_SAVE_CHANNEL                  │
 * ├─────────────────────────────────────────┤
 * │ ├→ SETTINGS_OnSaveChannel (saves)       │
 * │ ├→ RADIO_OnSaveChannel (updates IC)    │
 * │ └→ UI_OnChannelChange (redraws)        │
 * └─────────────────────────────────────────┘
 *
 * Execution order: Synchronous, in subscription order (FIFO)
 * All 3 handlers must complete before APP_RaiseEvent() returns.
 */
void APP_InitializeEventHandlers(void)
{
    // Register SAVE_CHANNEL handlers (TX channel persistence & updates)
    // Order: Settings (EEPROM), Radio (IC update), UI (display refresh)
    APP_SubscribeEvent(APP_EVENT_SAVE_CHANNEL, SETTINGS_OnSaveChannel);
    APP_SubscribeEvent(APP_EVENT_SAVE_CHANNEL, RADIO_OnSaveChannel);
    APP_SubscribeEvent(APP_EVENT_SAVE_CHANNEL, UI_OnChannelChange);

    // Register SAVE_VFO handler (VFO/mode/power snapshot persistence)
    #include "../settings_event.h"
    APP_SubscribeEvent(APP_EVENT_SAVE_VFO, SETTINGS_OnSaveVfo);
    // Future event handler registrations can be added here
    // APP_SubscribeEvent(APP_EVENT_FREQUENCY_CHANGE, RADIO_OnFrequencyChange);
    // APP_SubscribeEvent(APP_EVENT_MODE_CHANGE, RADIO_OnModeChange);
    // APP_SubscribeEvent(APP_EVENT_POWER_CHANGE, RADIO_OnPowerChange);
}

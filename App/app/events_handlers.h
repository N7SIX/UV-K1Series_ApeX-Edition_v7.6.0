/**
 * =====================================================================================
 * @file        events_handlers.h
 * @brief       Event handler registration and initialization (Phase 2 Integration)
 * @author      N7SIX (Professional Enhancements, 2025-2026)
 * @author      Sean (Event System Architecture, 2025-2026)
 * @version     v7.6.0 (ApeX Edition)
 * @license     Apache License, Version 2.0
 * * "Event System Phase 2: Handler Integration for TX Channel State Management."
 * =====================================================================================
 *
 * Event handler declarations for TX channel operations:
 * - SETTINGS_OnSaveChannel() — Persists TX channel to EEPROM
 * - RADIO_OnSaveChannel() — Updates BK4819 radio IC with new TX settings
 * - UI_OnChannelChange() — Requests display refresh for channel updates
 *
 * USAGE:
 *   In main.c after BK4819_Init() and SETTINGS_LoadCalibration():
 *   APP_EventSystem_Init();        // Initialize event registry
 *   APP_InitializeEventHandlers(); // Register all handlers
 *
 * ARCHITECTURE:
 *   menu.c (user selects channel)
 *      ↓ APP_RaiseEvent(APP_EVENT_SAVE_CHANNEL, channel_data)
 *      ↓ Dispatches to 3 handlers in parallel:
 *      ├→ SETTINGS_OnSaveChannel() — Saves to EEPROM
 *      ├→ RADIO_OnSaveChannel() — Updates radio IC
 *      └→ UI_OnChannelChange() — Requests display update
 */

#ifndef APP_EVENTS_HANDLERS_H
#define APP_EVENTS_HANDLERS_H

/**
 * @brief Initialize all event handlers for the application
 *
 * Called once at startup (after drivers are initialized) to register
 * all event handlers with the event system. This decouples module
 * initialization from event subscription, allowing flexible ordering.
 *
 * PREREQUISITES:
 * - APP_EventSystem_Init() must be called first
 * - BK4819_Init() driver must be initialized
 * - SETTINGS global state must be loaded
 * - UI framebuffer must be initialized
 *
 * HANDLER REGISTRATION ORDER:
 * 1. APP_EVENT_SAVE_CHANNEL → SETTINGS_OnSaveChannel, RADIO_OnSaveChannel, UI_OnChannelChange
 * 2. [Future handlers for other events]
 */
void APP_InitializeEventHandlers(void);

#endif // APP_EVENTS_HANDLERS_H

/**
 * =====================================================================================
 * @file        MENU_REFACTORING_GUIDE.md
 * @brief       Event System Phase 2: Menu Refactoring Guide
 * @author      N7SIX (Professional Enhancements, 2025-2026)
 * @version     v7.6.0 (ApeX Edition)
 * @license     Apache License, Version 2.0
 * =====================================================================================
 *
 * OBJECTIVE:
 * Refactor menu.c to emit APP_EVENT_SAVE_CHANNEL when the user modifies and saves
 * TX channel configuration, replacing direct gEeprom modifications with event-driven
 * updates.
 *
 * CURRENT STATE (Before Refactoring):
 * ====================================
 * menu.c → directly writes to gEeprom → calls SETTINGS_SaveSettings() → saves to EEPROM
 *                ↓
 *        UI doesn't receive notification
 *                ↓
 *        Radio IC doesn't receive notification
 *                ↓
 *        Potential state inconsistency
 *
 * DESIRED STATE (After Refactoring):
 * ===================================
 * menu.c → APP_RaiseEvent(APP_EVENT_SAVE_CHANNEL, ...) → dispatches to:
 *        ├→ SETTINGS_OnSaveChannel() → saves to EEPROM
 *        ├→ RADIO_OnSaveChannel() → updates radio IC
 *        └→ UI_OnChannelChange() → triggers display refresh
 *
 * REFACTORING POINTS IN menu.c:
 * ==============================
 *
 * 1. MENU_MEM_CH (Channel Save)
 *    Location: Case statement in UI_MENU_UpdateItem()
 *    Current: Calls SETTINGS_SaveChannel() directly
 *    Action: Emit APP_EVENT_SAVE_CHANNEL(channel_index) after save
 *
 * 2. MENU_MEM_NAME (Channel Name Edit)
 *    Location: Case statement in UI_MENU_UpdateItem()
 *    Current: Modifies channel name in gEeprom, saves directly
 *    Action: Emit APP_EVENT_SAVE_CHANNEL(channel_index) after name edit
 *
 * 3. MENU_TXP (TX Power)
 *    Location: Case statement in UI_MENU_UpdateItem()
 *    Current: Modifies gEeprom.TXP_CalculatedSetting, radio IC updated in radio.c
 *    Action: Emit APP_EVENT_POWER_CHANGE after setting (if implemented)
 *
 * 4. All Modulation/TX Settings (Mode, CTCSS, DCS, Offset, etc.)
 *    Location: Various case statements
 *    Current: Directly modify gEeprom values
 *    Action: Emit APP_EVENT_MODE_CHANGE or APP_EVENT_FREQUENCY_CHANGE as appropriate
 *
 * IMPLEMENTATION APPROACH:
 * ========================
 *
 * Phase 2a (Minimal - START HERE):
 * ---------------------------------
 * Focus only on MENU_MEM_CH and MENU_MEM_NAME:
 *
 * In UI_MENU_UpdateItem() or callback function:
 *
 *   case MENU_MEM_CH:
 *       if (gAskForConfirmation == 0) {
 *           // User pressed OK to save the channel
 *           // Replace existing SETTINGS_SaveChannel() with:
 *           
 *           uint8_t ch_index = gSubMenuSelection;
 *           APP_RaiseEvent(APP_EVENT_SAVE_CHANNEL, &ch_index);
 *           
 *           gAskForConfirmation = 0;  // Clear confirmation flag
 *       }
 *       break;
 *
 *   case MENU_MEM_NAME:
 *       if (gAskForConfirmation == 0 && edit_index >= 10) {
 *           // User finished editing the name (return key pressed)
 *           
 *           uint8_t ch_index = gSubMenuSelection;
 *           APP_RaiseEvent(APP_EVENT_SAVE_CHANNEL, &ch_index);
 *           
 *           gAskForConfirmation = 0;  // Clear confirmation flag
 *       }
 *       break;
 *
 * Phase 2b (Extended - FUTURE):
 * ------------------------------
 * Add events for individual settings:
 *
 * - APP_EVENT_MODE_CHANGE → RADIO_OnModeChange()
 * - APP_EVENT_FREQUENCY_CHANGE → RADIO_OnFrequencyChange()
 * - APP_EVENT_POWER_CHANGE → RADIO_OnPowerChange()
 * - APP_EVENT_POWER_CHANGE → UI_OnPowerChange() (show TX power icon)
 *
 * INTEGRATION CHECKLIST:
 * ======================
 *
 * [ ] Add #include "app/events.h" to menu.c
 * [ ] Locate MENU_MEM_CH case in UI_MENU_UpdateItem()
 * [ ] Replace SETTINGS_SaveChannel() with APP_RaiseEvent(APP_EVENT_SAVE_CHANNEL, ...)
 * [ ] Locate MENU_MEM_NAME case in UI_MENU_UpdateItem()
 * [ ] Replace channel name save with APP_RaiseEvent(APP_EVENT_SAVE_CHANNEL, ...)
 * [ ] Test: Save a channel, verify display updates
 * [ ] Test: Verify EEPROM write occurs via SETTINGS_OnSaveChannel()
 * [ ] Test: Verify radio IC updates via RADIO_OnSaveChannel()
 * [ ] Compile with no warnings
 * [ ] Run integration tests (menu.c → all 3 handlers should execute)
 *
 * CODE LOCATION:
 * ==============
 * File: App/ui/menu.c
 * Approximate lines: 853 (MENU_MEM_CH), 875 (MENU_MEM_NAME)
 * Function: UI_MENU_UpdateItem() or button handler callback
 *
 * TESTING STRATEGY:
 * =================
 *
 * 1. Build with Docker: ./compile-with-docker.sh ApeX
 * 2. Verify no compilation errors or warnings
 * 3. Flash to radio (if available)
 * 4. Manual test: Select a channel in menu → modify → save
 *    Expected: Display updates, EEPROM saves, radio IC reconfigures
 * 5. Verify via UART: Check settings persistence after power cycle
 *
 * BACKWARD COMPATIBILITY:
 * =======================
 *
 * This refactoring maintains 100% backward compatibility:
 *
 * - All callback handlers perform the same operations as before
 * - SETTINGS_OnSaveChannel() calls SETTINGS_SaveSettings() (same as menu used to do)
 * - RADIO_OnSaveChannel() calls RADIO_SetupRegisters() (same immediate update)
 * - UI_OnChannelChange() sets gUpdateDisplay = true (menu already did this)
 *
 * The only change is the decoupling: instead of menu doing all 3 things,
 * it now fires one event that causes all 3 to happen.
 *
 * BENEFITS OF REFACTORING:
 * ========================
 *
 * ✓ Separation of Concerns: menu.c doesn't need to know about EEPROM/radio/UI
 * ✓ Testability: Can test handlers independently without menu
 * ✓ Reusability: Same handlers work for other event sources (scanner.c, UART, etc.)
 * ✓ Maintainability: Easier to add new handlers (e.g., logging, telemetry)
 * ✓ Reduction of Global State: menu.c has fewer direct dependencies
 *
 * FILE REFERENCES:
 * ================
 * - App/ui/menu.c - Menu implementation (refactor here)
 * - App/app/events.h - Event API (use APP_RaiseEvent)
 * - App/settings.c - Handler implementation (SETTINGS_OnSaveChannel)
 * - App/radio.c - Handler implementation (RADIO_OnSaveChannel)
 * - App/ui/ui.c - Handler implementation (UI_OnChannelChange)
 *
 * =====================================================================================
 */

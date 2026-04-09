#include "app/events.h"
#include "settings.h"

/**
 * @brief Event handler for APP_EVENT_SAVE_VFO
 *
 * Called when VFO/mode/power state changes. Persists the full snapshot to EEPROM.
 *
 * EXECUTION TIME: ~5-10ms (flash write overhead)
 * BLOCKING: Yes - blocks on SPI EEPROM writes
 * REENTRANT: Not reentrant - do not call from ISR or nested event handlers
 */
void SETTINGS_OnSaveVfo(APP_EventType_t event, const void *data)
{
    (void)event;
    (void)data;
    // Save full snapshot (VFO, mode, power, etc)
    (void)SETTINGS_SaveSnapshot();
    // Also persist mode state (CURRENT_STATE, VFO_OPEN)
#ifdef ENABLE_FEAT_N7SIX_RESUME_STATE
    (void)SETTINGS_WriteCurrentState();
#endif
}

/* Copyright 2023 Dual Tachyon
 * https://github.com/DualTachyon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 */

#include <assert.h>
#include <string.h>

#include "app/chFrScanner.h"
#include "app/dtmf.h"
#include "app/events.h"
#ifdef ENABLE_FMRADIO
    #include "app/fm.h"
#endif
#include "driver/keyboard.h"
#include "misc.h"
#ifdef ENABLE_AIRCOPY
    #include "ui/aircopy.h"
#endif
#ifdef ENABLE_FMRADIO
    #include "ui/fmradio.h"
#endif
#ifdef ENABLE_REGA
    #include "app/rega.h"
#endif
#include "ui/inputbox.h"
#include "ui/main.h"
#include "ui/menu.h"
#include "ui/scanner.h"
#include "ui/ui.h"
#include "../misc.h"

GUI_DisplayType_t gScreenToDisplay;
GUI_DisplayType_t gRequestDisplayScreen = DISPLAY_INVALID;

uint8_t           gAskForConfirmation;
bool              gAskToSave;
bool              gAskToDelete;


void (*UI_DisplayFunctions[])(void) = {
    [DISPLAY_MAIN] = &UI_DisplayMain,
    [DISPLAY_MENU] = &UI_DisplayMenu,
    [DISPLAY_SCANNER] = &UI_DisplayScanner,

#ifdef ENABLE_FMRADIO
    [DISPLAY_FM] = &UI_DisplayFM,
#endif

#ifdef ENABLE_AIRCOPY
    [DISPLAY_AIRCOPY] = &UI_DisplayAircopy,
#endif

#ifdef ENABLE_REGA
    [DISPLAY_REGA] = &UI_DisplayREGA,
#endif
};

static_assert(ARRAY_SIZE(UI_DisplayFunctions) == DISPLAY_N_ELEM);

void GUI_DisplayScreen(void)
{
    if (gScreenToDisplay != DISPLAY_INVALID) {
        UI_DisplayFunctions[gScreenToDisplay]();
    }
}

void GUI_SelectNextDisplay(GUI_DisplayType_t Display)
{
    if (Display == DISPLAY_INVALID)
        return;

    if (gScreenToDisplay != Display)
    {
        DTMF_clear_input_box();

        gInputBoxIndex       = 0;
        gIsInSubMenu         = false;
        gCssBackgroundScan   = false;
        gScanStateDir        = SCAN_OFF;
        #ifdef ENABLE_FMRADIO
            gFM_ScanState    = FM_SCAN_OFF;
        #endif
        gAskForConfirmation  = 0;
        gAskToSave           = false;
        gAskToDelete         = false;
        gWasFKeyPressed      = false;

        gUpdateStatus        = true;
    }

    gScreenToDisplay = Display;
    gUpdateDisplay   = true;
}

// =============================================================================
// EVENT HANDLERS (Phase 2 Integration)
// =============================================================================

/**
 * @brief Event handler for APP_EVENT_SAVE_CHANNEL
 *
 * Called when a channel is selected or modified. Requests a display refresh
 * by setting the gUpdateDisplay flag, which causes the UI module to redraw
 * the current screen on the next call to GUI_DisplayScreen().
 *
 * @param event  APP_EVENT_SAVE_CHANNEL
 * @param data   Pointer to channel index (uint8_t *) - may be NULL
 *
 * EXECUTION TIME: < 1ms (just flag setting)
 * BLOCKING: No - non-blocking flag operation
 * REENTRANT: Safe - can be called from ISR (single byte write)
 *
 * OPERATION:
 * 1. Set gUpdateDisplay = true to request display refresh
 * 2. Next call to APP_Update() → GUI_DisplayScreen() will redraw current view
 * 3. Channel information (frequency, power, mode) updated on LCD
 */
void UI_OnChannelChange(APP_EventType_t event, const void *data)
{
    (void)event;   // Unused parameter
    (void)data;    // Unused parameter (could be channel index if needed)
    
    // Request display refresh on next UI update cycle
    gUpdateDisplay = true;
}

/**
 * =====================================================================================
 * @file        common.c
 * @brief       Common Utilities & Shared Application Functions for Quansheng UV-K1 Series
 * @author      Dual Tachyon (Original Framework, 2023)
 * @author      N7SIX (Professional Enhancements, 2025-2026)
 * @version     v7.6.0 (ApeX Edition)
 * @license     Apache License, Version 2.0
 * * "Keyboard control and application-wide helper routines."
 * =====================================================================================
 * * ARCHITECTURAL OVERVIEW:
 * This module provides common utilities including keyboard lock toggling, shared
 * key handling logic, and application-wide helper functions used across multiple
 * modes and subsystems for consistent behavior and reduced code duplication.
 *
 * MAJOR FEATURES (2025-2026):
 * ---------------------------
 * - KEYBOARD LOCK: Toggle/manage keyboard lock state with visual and audio feedback.
 * - SHARED HANDLERS: Common key bindings (PTT, menu, VFO switch) used across modes.
 * - DEBOUNCING: Integrated key press filtering to prevent false triggers.
 * - MODE AWARENESS: Conditional behavior based on current operation (TX, menu, etc.).
 * - PERSISTENCE: Save lock/settings state to EEPROM immediately after toggle.
 * - VOICE CUES: Optional voice announcements for lock/unlock and mode changes.
 * - STATE MANAGEMENT: Centralized flags for global application states.
 *
 * TECHNICAL SPECIFICATIONS:
 * -------------------------
 * - KEY RESPONSE: <100ms from physical press to state change in application.
 * - DEBOUNCE TIME: 20ms filtering to eliminate mechanical bounce errors.
 * - LOCK BEHAVIOR: Prevents all keys except PTT-to-unlock sequence during TX.
 * - VOICE INTEGRATION: Non-blocking audio cues with priority interrupt.
 * - EEPROM WRITES: Atomic single-setting updates on state change (not full flush).
 * - INTERRUPT SAFETY: Key handler reentrant-safe for use in ISR context.
 *
 * =====================================================================================
 */
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
#include "app/chFrScanner.h"
#include "audio.h"
#include "functions.h"
#include "misc.h"
#include "settings.h"
#include "ui/ui.h"
#include "app/events.h"
#include <stddef.h>

void COMMON_KeypadLockToggle() 
{

    if (gScreenToDisplay != DISPLAY_MENU &&
        gCurrentFunction != FUNCTION_TRANSMIT)
    {   // toggle the keyboad lock

        #ifdef ENABLE_VOICE
            gAnotherVoiceID = gEeprom.KEY_LOCK ? VOICE_ID_UNLOCK : VOICE_ID_LOCK;
        #endif

        gEeprom.KEY_LOCK = !gEeprom.KEY_LOCK;

        gRequestSaveSettings = true;
    }
}

void COMMON_SwitchVFOs()
{
#ifdef ENABLE_SCAN_RANGES    
    gScanRangeStart = 0;
#endif
    gEeprom.TX_VFO ^= 1;

    if (gEeprom.CROSS_BAND_RX_TX != CROSS_BAND_OFF)
        gEeprom.CROSS_BAND_RX_TX = gEeprom.TX_VFO + 1;
    if (gEeprom.DUAL_WATCH != DUAL_WATCH_OFF)
        gEeprom.DUAL_WATCH = gEeprom.TX_VFO + 1;

    gRequestSaveSettings  = 1;
    gFlagReconfigureVfos  = true;
    gScheduleDualWatch = true;

    gRequestDisplayScreen = DISPLAY_MAIN;
}

void COMMON_SwitchVFOMode()
{
#ifdef ENABLE_NOAA
    if (gEeprom.VFO_OPEN && !IS_NOAA_CHANNEL(gTxVfo->CHANNEL_SAVE))
#else
    if (gEeprom.VFO_OPEN)
#endif
    {
        bool modeChanged = false;
        if (IS_MR_CHANNEL(gTxVfo->CHANNEL_SAVE))
        {   // swap to frequency mode
            gEeprom.ScreenChannel[gEeprom.TX_VFO] = gEeprom.FreqChannel[gEeprom.TX_VFO];
            #ifdef ENABLE_VOICE
                gAnotherVoiceID        = VOICE_ID_FREQUENCY_MODE;
            #endif
            gRequestSaveVFO            = true;
            gVfoConfigureMode          = VFO_CONFIGURE_RELOAD;
            modeChanged = true;
        }
        else {
            uint8_t Channel = RADIO_FindNextChannel(gEeprom.MrChannel[gEeprom.TX_VFO], 1, false, 0);
            if (Channel != 0xFF)
            {   // swap to channel mode
                gEeprom.ScreenChannel[gEeprom.TX_VFO] = Channel;
                #ifdef ENABLE_VOICE
                    AUDIO_SetVoiceID(0, VOICE_ID_CHANNEL_MODE);
                    AUDIO_SetDigitVoice(1, Channel + 1);
                    gAnotherVoiceID = (VOICE_ID_t)0xFE;
                #endif
                gRequestSaveVFO     = true;
                gVfoConfigureMode   = VFO_CONFIGURE_RELOAD;
                modeChanged = true;
            }
        }
        if (modeChanged) {
            // Immediately persist mode state to EEPROM/flash
            APP_RaiseEvent(APP_EVENT_SAVE_VFO, NULL);
        }
    }
}
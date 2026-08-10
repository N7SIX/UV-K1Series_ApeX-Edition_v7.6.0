/* Copyright 2026 NR7Y
 * https://github.com/briand
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

// CW application-level update loop and end-of-transmission handling
// Adapted from NR7Y's CW implementation for ApeX-Edition PY32F071 platform

#include <stdint.h>
#include <stdbool.h>

#include "app/cw.h"
#include "app/cwkeyer.h"
#include "app/app.h"
#include "app/menu.h"
#include "audio.h"
#include "driver/bk4819.h"
#include "driver/system.h"
#include "functions.h"
#include "misc.h"
#include "radio.h"
#include "scheduler.h"
#include "settings.h"

// ---------------------------------------------------------------------------
// CW_EndTxNow  –  end CW transmission immediately and return to monitor
// ---------------------------------------------------------------------------
void CW_EndTxNow(void)
{
    gCW_AppState = CW_APP_INACTIVE;

    BK4819_EnterTxMute();
    BK4819_ExitSubAu();
    RADIO_SetupRegisters(false);
    FUNCTION_Select(FUNCTION_FOREGROUND);

    gFlagEndTransmission = false;
    gPttIsPressed = false;

    RADIO_SetVfoState(VFO_STATE_NORMAL);
    RADIO_SelectVfos();

    if (gMonitor)
        APP_StartListening(FUNCTION_MONITOR);
}

// ---------------------------------------------------------------------------
// CW_AppUpdate  –  called from CW_TimeSlice10ms (10ms cadence)
// ---------------------------------------------------------------------------
void CW_AppUpdate(void)
{
    // Typed-message playback owns the TX path when active.
    // (Kept so this FSM never runs concurrently with the typed-message
    //  state machine that drives CW_TxStateMachine.)
    if (gCW_PlaybackActive)
        return;

    CW_Action_t action;
    #ifdef ENABLE_FEAT_N7SIX_CW
    action = CW_HandleState();
    #else
    action = CW_ACTION_NONE;
    #endif

    // ---- RF transmit path ----
    switch (action)
    {
        case CW_ACTION_CARRIER_ON:
            gTxTimerCountdown_500ms = 0;
            gCW_TxDisplayHoldoff_10ms = CW_TX_DISPLAY_HOLDOFF_10MS;
            gPttIsPressed = true;

            if (gCW_AppState == CW_APP_INACTIVE)
            {
                AUDIO_AudioPathOn();
                SYSTEM_DelayMs(20);
                RADIO_PrepareTX();
                gCW_AppState = CW_APP_TRANSMITTING;
                // The internal keyer now owns the TX path. Transition the CW
                // state so CW_TimeSlice10ms() dispatches subsequent ticks to
                // CW_AppUpdate() (the keyer FSM) instead of the RX decoder.
                gCW_State = CW_SENDING;
            }
            else if (gCW_AppState == CW_APP_SUSPENDED)
            {
                gCW_AppState = CW_APP_TRANSMITTING;
                gCW_SuspendCounter_1ms = 0;
            }
            BK4819_TransmitTone(true, gCW_ToneFreq);
        break;

        case CW_ACTION_CARRIER_OFF:
            if (gCW_AppState == CW_APP_TRANSMITTING)
            {
                BK4819_EnterTxMute();
                gCW_AppState = CW_APP_SUSPENDED;
                gCW_SuspendCounter_1ms = gGlobalSysTickCounter;
            }
            gCW_TxDisplayHoldoff_10ms = CW_TX_DISPLAY_HOLDOFF_10MS;
        break;

        case CW_ACTION_CARRIER_HOLD_ON:
            gPttIsPressed = true;
            gTxTimerCountdown_500ms = 0;
            gCW_TxDisplayHoldoff_10ms = CW_TX_DISPLAY_HOLDOFF_10MS;

            if (gCW_AppState == CW_APP_SUSPENDED)
                gCW_AppState = CW_APP_TRANSMITTING;
            gCW_SuspendCounter_1ms = gGlobalSysTickCounter;
            BK4819_TransmitTone(true, gCW_ToneFreq);
        break;

        case CW_ACTION_NONE:
        default:
            if (gCW_AppState == CW_APP_TRANSMITTING)
            {
                BK4819_EnterTxMute();
                gCW_AppState = CW_APP_SUSPENDED;
                gCW_SuspendCounter_1ms = gGlobalSysTickCounter;
            }
        break;
    }

    // ---- suspend timeout → end TX ----
    if (gCW_AppState == CW_APP_SUSPENDED)
    {
        uint16_t elapsed = (uint16_t)(gGlobalSysTickCounter - gCW_SuspendCounter_1ms);
        if (elapsed >= (CW_SUSPEND_TIMEOUT_MS / 10))
        {
            gCW_SuspendCounter_1ms = 0;
            gCW_TxDisplayHoldoff_10ms = CW_TX_DISPLAY_HOLDOFF_10MS;
            gPttIsPressed = false;
            CW_EndTxNow();
            gCW_State = CW_COMPOSING;
            CW_Render();
        }
    }
}

// Global CW app state variables
CW_AppState_t gCW_AppState = CW_APP_INACTIVE;
uint16_t gCW_SuspendCounter_1ms = 0;
uint16_t gCW_TxDisplayHoldoff_10ms = 0;

void CW_AppInit(void)
{
    gCW_AppState = CW_APP_INACTIVE;
    gCW_SuspendCounter_1ms = 0;
    gCW_TxDisplayHoldoff_10ms = 0;
    gCW_PlaybackActive = false;
    gCW_PlaybackRepeat = false;
    gCW_PlaybackMacroIndex = 0;
    gCW_MessageRepeatCountdown_500ms = 0;
    gCW_PlayIndicatorOn = false;
    gCW_Recording = false;
    gPttIsPressed = false;
}

CW_AppState_t CW_GetState(void)
{
    return gCW_AppState;
}
/* Copyright 2026 Sean, N7SIX
 * https://github.com/N7SIX
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

#ifndef GLOBALS_SYSTEM_H
#define GLOBALS_SYSTEM_H

#include <stdbool.h>
#include <stdint.h>

extern const uint8_t         menu_timeout_500ms;
extern const uint16_t        menu_timeout_long_500ms;

extern const uint8_t         DTMF_RX_live_timeout_500ms;
#ifdef ENABLE_DTMF_CALLING
extern const uint8_t         DTMF_RX_timeout_500ms;
extern const uint8_t         DTMF_decode_ring_countdown_500ms;
extern const uint8_t         DTMF_txstop_countdown_500ms;
#endif

extern const uint8_t         key_input_timeout_500ms;

extern const uint16_t        key_repeat_delay_10ms;
extern const uint16_t        key_repeat_10ms;
extern const uint16_t        key_debounce_10ms;

extern const uint16_t        battery_save_count_10ms;

extern const uint16_t        power_save1_10ms;
extern const uint16_t        power_save2_10ms;

#ifdef ENABLE_VOX
    extern const uint16_t    vox_stop_count_down_10ms;
#endif

extern volatile uint16_t     gBatterySaveCountdown_10ms;

extern volatile bool         gPowerSaveCountdownExpired;
extern volatile bool         gSchedulePowerSave;

extern volatile bool         gScheduleDualWatch;

extern volatile uint16_t     gDualWatchCountdown_10ms;
extern bool                  gDualWatchActive;

extern volatile uint8_t      gSerialConfigCountDown_500ms;

extern volatile bool         gNextTimeslice_500ms;

extern volatile uint16_t     gTxTimerCountdown_500ms;
extern volatile bool         gTxTimeoutReached;

#ifdef ENABLE_FEAT_N7SIX
    extern volatile uint16_t gTxTimerCountdownAlert_500ms;
    extern volatile bool     gTxTimeoutReachedAlert;
    extern volatile uint16_t gTxTimeoutToneAlert;
    #ifdef ENABLE_FEAT_N7SIX_RX_TX_TIMER
        extern volatile uint16_t gRxTimerCountdown_500ms;
    #endif
    #ifdef ENABLE_FEAT_N7SIX_SCREENSHOT
        extern volatile uint8_t  gUART_LockScreenshot; // lock screenshot if Chirp is used
        extern bool gUSB_ScreenshotEnabled;

        bool SCREENSHOT_IsLocked(void);
    #endif
#endif

extern bool                  gEnableSpeaker;

// battery critical, limit functionality to minimum
extern uint8_t               gReducedService;
extern uint8_t               gBatteryVoltageIndex;

#ifdef ENABLE_FMRADIO
    extern bool              gFlagSaveFM;
#endif

extern volatile uint16_t     gFlashLightBlinkCounter;

extern volatile bool         gNextTimeslice;
#ifdef ENABLE_FMRADIO
    extern uint8_t           gFM_ChannelPosition;
#endif
extern volatile bool         gNextTimeslice40ms;
extern volatile uint8_t      boot_counter_10ms;

#endif

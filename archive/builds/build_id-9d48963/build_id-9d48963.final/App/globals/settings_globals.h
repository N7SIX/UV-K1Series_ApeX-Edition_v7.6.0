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

#ifndef GLOBALS_SETTINGS_H
#define GLOBALS_SETTINGS_H

#include <stdbool.h>
#include <stdint.h>

#include "radio_globals.h"   // enum BacklightOnRxTx_t

extern const uint8_t         gMicGain_dB2[9];

#ifndef ENABLE_FEAT_N7SIX
extern bool                  gSetting_350TX;
#endif

#ifdef ENABLE_DTMF_CALLING
extern bool                  gSetting_KILLED;
#endif

#ifndef ENABLE_FEAT_N7SIX
extern bool                  gSetting_200TX;
extern bool                  gSetting_500TX;
#endif

extern bool                  gSetting_350EN;
extern uint8_t               gSetting_F_LOCK;
extern bool                  gSetting_ScrambleEnable;

extern enum BacklightOnRxTx_t gSetting_backlight_on_tx_rx;

#ifdef ENABLE_AM_FIX
    extern bool              gSetting_AM_fix;
#endif

#ifdef ENABLE_FEAT_N7SIX_SLEEP 
    extern uint8_t           gSetting_set_off;
    extern bool              gWakeUp;
#endif

#ifdef ENABLE_FEAT_N7SIX_SCAN_FASTER
    extern bool              gSetting_set_scn;
#endif

#ifdef ENABLE_FEAT_N7SIX
    #ifdef ENABLE_FEAT_N7SIX_LOGO_SAV
        enum SET_SAV_t {
            SET_SAV_OFF,
            SET_SAV_LOGO,
            SET_SAV_LOGO_PLUS,
            SET_SAV_MATRIX,
            SET_SAV_LEN
        };
    #endif

    // aligned with gSubMenu_SET_LCK[] and any new entry must keep the range
    // contiguous so that SET_LCK_LEN remains valid as menu bound and EEPROM
    // storage.
    enum SET_LCK_t {
        SET_LCK_KEYS        = 0u,
        SET_LCK_ACTIONS     = 1u,
        SET_LCK_PTT         = 2u,
        SET_LCK_ACTIONS_PTT = SET_LCK_ACTIONS | SET_LCK_PTT,
        SET_LCK_LEN
    };

    extern uint8_t            gSetting_set_pwr;
    extern bool               gSetting_set_ptt;
    extern uint8_t            gSetting_set_tot;
    extern uint8_t            gSetting_set_ctr;
    extern bool               gSetting_set_inv;
    extern uint8_t            gSetting_set_eot;
    extern uint8_t            gSetting_set_lck;
    extern bool               gSetting_set_met;
    extern bool               gSetting_set_gui;
    #ifdef ENABLE_FEAT_N7SIX_AUDIO
        extern uint8_t            gSetting_set_audio_fm;
        extern uint8_t            gSetting_set_audio_am;
    #endif
    #ifdef ENABLE_FEAT_N7SIX_NARROWER
        extern bool               gSetting_set_nfm;
    #endif
    #ifdef ENABLE_FEAT_N7SIX_LOGO_SAV
        extern uint8_t            gSetting_set_sav;
    #endif
    extern bool               gSetting_set_tmr;
    extern bool               gSetting_set_ptt_session;
    #ifdef ENABLE_FEAT_N7SIX_DEBUG
        extern int16_t        gDebug;
    #endif
#endif

#ifdef ENABLE_AUDIO_BAR
    extern bool              gSetting_mic_bar;
#endif
extern bool                  gSetting_live_DTMF_decoder;
extern uint8_t               gSetting_battery_text;

extern const uint32_t        gDefaultAesKey[4];
extern uint32_t              gCustomAesKey[4];
extern bool                  bHasCustomAesKey;
extern uint32_t              gChallenge[4];
extern uint8_t               gTryCount;

extern uint16_t              gEEPROM_RSSI_CALIB[7][4];

extern uint16_t              gEEPROM_1F8A;
extern uint16_t              gEEPROM_1F8C;

#endif
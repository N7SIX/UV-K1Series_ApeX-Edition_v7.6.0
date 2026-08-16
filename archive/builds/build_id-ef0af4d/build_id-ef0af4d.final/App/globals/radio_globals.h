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

#ifndef GLOBALS_RADIO_H
#define GLOBALS_RADIO_H

#include <stdbool.h>
#include <stdint.h>

enum {
    VFO_CONFIGURE_NONE = 0,
    VFO_CONFIGURE,
    VFO_CONFIGURE_RELOAD
};

enum AlarmState_t {
    ALARM_STATE_OFF = 0,
    ALARM_STATE_TXALARM,
    ALARM_STATE_SITE_ALARM,
    ALARM_STATE_TX1750
};
typedef enum AlarmState_t AlarmState_t;

enum ReceptionMode_t {
    RX_MODE_NONE = 0,   // squelch close ?
    RX_MODE_DETECTED,   // signal detected
    RX_MODE_LISTENING   //
};
typedef enum ReceptionMode_t ReceptionMode_t;

enum BacklightOnRxTx_t {
    BACKLIGHT_ON_TR_OFF,
    BACKLIGHT_ON_TR_TX,
    BACKLIGHT_ON_TR_RX,
    BACKLIGHT_ON_TR_TXRX
};

extern const uint8_t         fm_radio_countdown_500ms;
extern const uint16_t        fm_play_countdown_scan_10ms;
extern const uint16_t        fm_play_countdown_noscan_10ms;
extern const uint16_t        fm_restore_countdown_10ms;

extern const uint8_t        vfo_state_resume_countdown_500ms;

extern const uint16_t        dual_watch_count_after_tx_10ms;
extern const uint16_t        dual_watch_count_after_rx_10ms;
extern const uint16_t        dual_watch_count_after_1_10ms;
extern const uint16_t        dual_watch_count_after_2_10ms;
extern const uint16_t        dual_watch_count_toggle_10ms;
extern const uint16_t        dual_watch_count_noaa_10ms;
#ifdef ENABLE_VOX
    extern const uint16_t    dual_watch_count_after_vox_10ms;
#endif

extern const uint16_t        scan_pause_delay_in_1_10ms;
extern const uint16_t        scan_pause_delay_in_2_10ms;
extern const uint16_t        scan_pause_delay_in_3_10ms;
extern const uint16_t        scan_pause_delay_in_4_10ms;
extern const uint16_t        scan_pause_delay_in_5_10ms;
extern const uint16_t        scan_pause_delay_in_6_10ms;
extern const uint16_t        scan_pause_delay_in_7_10ms;

extern const uint8_t         scan_delay_10ms;

extern const uint16_t        NOAA_countdown_10ms;
extern const uint16_t        NOAA_countdown_2_10ms;
extern const uint16_t        NOAA_countdown_3_10ms;

extern bool                  gMonitor;

extern volatile uint16_t     gTailNoteEliminationCountdown_10ms;

#ifdef ENABLE_NOAA
    extern volatile uint16_t gNOAA_Countdown_10ms;
#endif
extern volatile uint8_t      gFoundCTCSS;
extern volatile uint8_t      gFoundCDCSS;
extern volatile bool         gEndOfRxDetectedMaybe;

extern int16_t               gVFO_RSSI[2];
extern uint8_t               gVFO_RSSI_bar_level[2];

// we are searching CTCSS/DCS inside RX ctcss/dcs menu
extern bool         gCssBackgroundScan;

enum
{
    SCAN_REV = -1,
    SCAN_OFF =  0,
    SCAN_FWD = +1
};

extern volatile bool     gScheduleScanListen;
extern volatile uint16_t gScanPauseDelayIn_10ms;

extern AlarmState_t          gAlarmState;
extern bool                  gPttWasReleased;
extern bool                  gPttWasPressed;
extern bool                  gHasVfoBackup;
extern bool                  gFlagReconfigureVfos;
extern uint8_t               gVfoConfigureMode;
extern bool                  gFlagResetVfos;
extern bool                  gRequestSaveVFO;
extern uint16_t              gRequestSaveChannel;
extern bool                  gRequestSaveSettings;
#ifdef ENABLE_FMRADIO
    extern bool              gRequestSaveFM;
#endif
extern uint8_t               gKeypadLocked;
extern bool                  gFlagPrepareTX;

extern volatile bool         g_CDCSS_Lost;
extern volatile uint8_t      gCDCSSCodeType;
extern volatile bool         g_CTCSS_Lost;
extern volatile bool         g_CxCSS_TAIL_Found;
#ifdef ENABLE_VOX
    extern volatile bool     g_VOX_Lost;
    extern bool              gVOX_NoiseDetected;
    extern uint16_t          gVoxResumeCountdown;
    extern uint16_t          gVoxPauseCountdown;
#endif

// true means we are receiving signal
extern volatile bool         g_SquelchLost;

extern bool                  gFlagEndTransmission;
extern ReceptionMode_t       gRxReceptionMode;

 //TRUE when dual watch is momentarly suspended and RX_VFO is locked to either last TX or RX
extern bool                  gRxVfoIsActive;
extern uint8_t               gAlarmToneCounter;
extern uint16_t              gAlarmRunningCounter;
extern uint8_t               gBackup_CROSS_BAND_RX_TX;
extern uint8_t               gScanDelay_10ms;
extern volatile uint8_t      gFSKWriteIndex;
#ifdef ENABLE_NOAA
    extern bool              gIsNoaaMode;
    extern uint8_t           gNoaaChannel;
#endif
extern volatile uint8_t      gFoundCDCSSCountdown_10ms;
extern volatile uint8_t      gFoundCTCSSCountdown_10ms;
#ifdef ENABLE_VOX
    extern volatile uint16_t gVoxStopCountdown_10ms;
#endif
#ifdef ENABLE_NOAA
    extern volatile uint16_t gNOAACountdown_10ms;
    extern volatile bool     gScheduleNOAA;
#endif
extern volatile bool         gFlagTailNoteEliminationComplete;
extern volatile uint8_t      gVFOStateResumeCountdown_500ms;
#ifdef ENABLE_FMRADIO
    extern volatile bool     gScheduleFM;
#endif

#ifdef ENABLE_FEAT_N7SIX
    extern uint8_t            gDW;
    extern uint8_t            gCB;
    extern bool               gSaveRxMode;
    extern uint8_t            crc[15];
    extern uint8_t            lErrorsDuringAirCopy;
    extern uint8_t            gAircopyStep;
    extern uint8_t            gAircopyCurrentMapIndex;
    extern bool               gAirCopyBootMode;
    #ifdef ENABLE_FEAT_N7SIX_RESCUE_OPS
        extern bool               gPowerHigh;
        extern bool               gRemoveOffset;
    #endif
    extern int8_t dBmCorrTable[7];
#endif

#ifdef ENABLE_FEAT_N7SIX
    extern uint16_t gVfoSaveCountdown_10ms;
    extern bool gScheduleVfoSave;
    extern bool gVfoStateChanged;
#endif

#endif
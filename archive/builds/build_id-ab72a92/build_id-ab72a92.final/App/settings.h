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

#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdbool.h>
#include <stdint.h>

#include "frequencies.h"
#include <helper/battery.h>
#include "radio.h"
#include <driver/backlight.h>

// PY25Q16 SPI flash virtual EEPROM address mapping
// (see App/documentation/EEPROM_ARCHITECTURE.md for full map)
#define EEPROM_ADDR_GENERAL_SETTINGS   0x00A000u  // General settings (8 bytes)
#define EEPROM_ADDR_SETTINGS_MIRROR    0x00A010u  // Mirror/backup of general settings (8 bytes)
#define EEPROM_ADDR_FM_SETTINGS        0x00A020u  // FM settings (4 bytes)
#define EEPROM_ADDR_FM_CHANNELS        0x00A028u  // FM memory channels (128 bytes)
#define EEPROM_ADDR_EXTENDED_SETTINGS  0x00A0A8u  // Extended settings (128 bytes)
#define EEPROM_ADDR_BATTERY_CALIB      0x00A0B9u  // Battery calibration (7 bytes)
#define EEPROM_ADDR_LOGO_LINES         0x00A0C8u  // Logo/display lines (32 bytes)
#define EEPROM_ADDR_WELCOME0           0x00A0C8u  // Welcome string 0 (16 bytes, alias)
#define EEPROM_ADDR_WELCOME1           0x00A0D8u  // Welcome string 1 (16 bytes)
#define EEPROM_ADDR_SCAN_LIST          0x00A130u  // Scan list settings (8 bytes)
#define EEPROM_ADDR_F_LOCK             0x00A150u  // Frequency lock settings (8 bytes)
#define EEPROM_ADDR_N7SIX_STATE        0x00A158u  // N7SIX custom state (8 bytes)
#define EEPROM_ADDR_DISPLAY_STATE      0x00A158u  // Display state (alias for N7SIX_STATE)
#define EEPROM_ADDR_VERSION            0x00A160u  // Version/build string (16 bytes)
#define EEPROM_ADDR_SETTINGS_CRC       0x00A170u  // CRC-16 for settings block (2 bytes)
#define EEPROM_SETTINGS_SIZE           0x170u     // Settings block size (368 bytes)

enum POWER_OnDisplayMode_t {
#ifdef ENABLE_FEAT_N7SIX
    POWER_ON_DISPLAY_MODE_ALL,
    POWER_ON_DISPLAY_MODE_SOUND,
#else
    POWER_ON_DISPLAY_MODE_FULL_SCREEN = 0,
#endif
    POWER_ON_DISPLAY_MODE_MESSAGE,
    POWER_ON_DISPLAY_MODE_VOLTAGE,
#ifdef ENABLE_FEAT_N7SIX_LOGO
    POWER_ON_DISPLAY_MODE_LOGO,
#endif
    POWER_ON_DISPLAY_MODE_NONE,
};
typedef enum POWER_OnDisplayMode_t POWER_OnDisplayMode_t;

enum TxLockModes_t {
    F_LOCK_DEF, //all default frequencies + configurable
    F_LOCK_FCC,
#ifdef ENABLE_FEAT_N7SIX_CA
    F_LOCK_CA,
#endif
    F_LOCK_CE,
    F_LOCK_GB,
    F_LOCK_430,
    F_LOCK_438,
#ifdef ENABLE_FEAT_N7SIX_PMR
    F_LOCK_PMR,
#endif
#ifdef ENABLE_FEAT_N7SIX_GMRS_FRS_MURS
    F_LOCK_GMRS_FRS_MURS,
#endif
    F_LOCK_ALL, // disable TX on all frequencies
    F_LOCK_NONE, // enable TX on all frequencies
    F_LOCK_LEN
};

/*
enum {
    SCAN_RESUME_TO = 0,
    SCAN_RESUME_CO,
    SCAN_RESUME_SE
};
*/

enum {
    CROSS_BAND_OFF = 0,
    CROSS_BAND_CHAN_A,
    CROSS_BAND_CHAN_B
};

enum {
    DUAL_WATCH_OFF = 0,
    DUAL_WATCH_CHAN_A,
    DUAL_WATCH_CHAN_B
};

enum {
    TX_OFFSET_FREQUENCY_DIRECTION_OFF = 0,
    TX_OFFSET_FREQUENCY_DIRECTION_ADD,
    TX_OFFSET_FREQUENCY_DIRECTION_SUB
};

enum {
    OUTPUT_POWER_USER = 0,
    OUTPUT_POWER_LOW1,
    OUTPUT_POWER_LOW2,
    OUTPUT_POWER_LOW3,
    OUTPUT_POWER_LOW4,
    OUTPUT_POWER_LOW5,
    OUTPUT_POWER_MID,
    OUTPUT_POWER_HIGH
};

enum ACTION_OPT_t {
    ACTION_OPT_NONE = 0,
    ACTION_OPT_FLASHLIGHT,
    ACTION_OPT_POWER,
    ACTION_OPT_MONITOR,
    ACTION_OPT_SCAN,
    ACTION_OPT_VOX,
    ACTION_OPT_ALARM,
    ACTION_OPT_FM,
    ACTION_OPT_1750,
    ACTION_OPT_KEYLOCK,
    ACTION_OPT_A_B,
    ACTION_OPT_VFO_MR,
    ACTION_OPT_SWITCH_DEMODUL,
    ACTION_OPT_BLMIN_TMP_OFF, //BackLight Minimum Temporay OFF
#ifdef ENABLE_FEAT_N7SIX
    ACTION_OPT_RXMODE,
    ACTION_OPT_MAINONLY,
    ACTION_OPT_PTT,
    ACTION_OPT_WN,
    ACTION_OPT_BACKLIGHT,
    ACTION_OPT_MUTE,
    ACTION_OPT_RXA,
    #ifdef ENABLE_FEAT_N7SIX_RESCUE_OPS
        ACTION_OPT_POWER_HIGH,
        ACTION_OPT_REMOVE_OFFSET,
    #endif
    #ifdef ENABLE_FEAT_N7SIX_RXTX_LOG
        ACTION_OPT_RXTX_LOG,
    #endif
#endif
#ifdef ENABLE_REGA
    ACTION_OPT_REGA_ALARM,
    ACTION_OPT_REGA_TEST,
#endif
#ifdef ENABLE_FEAT_N7SIX_BEAM
    ACTION_OPT_BEAM,
#endif
    ACTION_OPT_LEN
};

#ifdef ENABLE_VOICE
    enum VOICE_Prompt_t
    {
        VOICE_PROMPT_OFF = 0,
        VOICE_PROMPT_CHINESE,
        VOICE_PROMPT_ENGLISH
    };
    typedef enum VOICE_Prompt_t VOICE_Prompt_t;
#endif

enum ALARM_Mode_t {
    ALARM_MODE_SITE = 0,
    ALARM_MODE_TONE
};
typedef enum ALARM_Mode_t ALARM_Mode_t;

enum ROGER_Mode_t {
    ROGER_MODE_OFF = 0,
    ROGER_MODE_ROGER,
    ROGER_MODE_MDC
};
typedef enum ROGER_Mode_t ROGER_Mode_t;

enum CHANNEL_DisplayMode_t {
    MDF_FREQUENCY = 0,
    MDF_CHANNEL,
    MDF_NAME,
    MDF_NAME_FREQ
};
typedef enum CHANNEL_DisplayMode_t CHANNEL_DisplayMode_t;

typedef struct {
    uint16_t               ScreenChannel[2]; // current channels set in the radio (memory or frequency channels)
    uint16_t               FreqChannel[2]; // last frequency channels used
    uint16_t               MrChannel[2]; // last memory channels used
#ifdef ENABLE_NOAA
    uint16_t           NoaaChannel[2];
#endif

    // The actual VFO index (0-upper/1-lower) that is now used for RX, 
    // It is being alternated by dual watch, and flipped by crossband
    uint8_t               RX_VFO;

    // The main VFO index (0-upper/1-lower) selected by the user
    // 
    uint8_t               TX_VFO;

    uint8_t               field7_0xa;  // Reserved/legacy; do not remove to preserve EEPROM layout
    uint8_t               field8_0xb;  // Reserved/legacy; do not remove to preserve EEPROM layout

#ifdef ENABLE_FMRADIO
    uint16_t          FM_SelectedFrequency;
    uint8_t           FM_SelectedChannel;
    bool              FM_IsMrMode;
    uint16_t          FM_FrequencyPlaying;
    uint8_t           FM_Band  : 2;
    //uint8_t         FM_Space : 2;
#endif

    uint8_t               SQUELCH_LEVEL;
    uint8_t               TX_TIMEOUT_TIMER;
    bool                  KEY_LOCK;
#ifdef ENABLE_FEAT_N7SIX
    bool                  KEY_LOCK_PTT;
    bool                  SET_NAV;
#endif
#ifdef ENABLE_FEAT_N7SIX_RESCUE_OPS
    bool                  MENU_LOCK;
    uint8_t               SET_KEY;
#endif
    bool                  VOX_SWITCH;
    uint8_t               VOX_LEVEL;
#ifdef ENABLE_VOICE
    VOICE_Prompt_t    VOICE_PROMPT;
#endif
    bool                  BEEP_CONTROL;
    uint8_t               CHANNEL_DISPLAY_MODE;
    bool                  TAIL_TONE_ELIMINATION;
    bool                  VFO_OPEN;
    uint8_t               DUAL_WATCH;
    uint8_t               CROSS_BAND_RX_TX;
    uint8_t               BATTERY_SAVE;
    uint8_t               BACKLIGHT_TIME;
    uint8_t               SCAN_RESUME_MODE;
    uint8_t               SCAN_LIST_DEFAULT;
    bool                  SCAN_LIST_ENABLED;
    uint16_t              SCANLIST_PRIORITY_CH[6];
//#ifdef ENABLE_FEAT_N7SIX_RESUME_STATE // Fix me !!! What the hell is this?
    uint8_t               CURRENT_STATE;
    uint8_t               CURRENT_LIST;
//#endif                                // Fix me !!! What the hell is this?

    uint8_t               field29_0x26;  // Reserved/legacy
    uint8_t               field30_0x27;  // Reserved/legacy

    uint8_t               field37_0x32;  // Reserved/legacy
    uint8_t               field38_0x33;  // Reserved/legacy

    uint8_t               AUTO_KEYPAD_LOCK;
#if defined(ENABLE_ALARM) || defined(ENABLE_TX1750)
    ALARM_Mode_t      ALARM_MODE;
#endif
    POWER_OnDisplayMode_t POWER_ON_DISPLAY_MODE;
    ROGER_Mode_t          ROGER;
    
    /* MDC-1200 Configuration (v7.6.10A): Parameterized MDC transmission */
    uint16_t              MDC_UnitID;         /*!< Destination Unit ID for MDC frames (default from radio ID) */
    uint8_t               MDC_DefaultOp;      /*!< Default MDC opcode (0x00=Status, etc.) */
    uint8_t               MDC_DefaultArg;     /*!< Default MDC argument (opcode-dependent) */
    
    uint8_t               REPEATER_TAIL_TONE_ELIMINATION;
    uint8_t               KEY_1_SHORT_PRESS_ACTION;
    uint8_t               KEY_1_LONG_PRESS_ACTION;
    uint8_t               KEY_2_SHORT_PRESS_ACTION;
    uint8_t               KEY_2_LONG_PRESS_ACTION;
    uint8_t               MIC_SENSITIVITY;
    uint8_t               MIC_SENSITIVITY_TUNING;
    uint16_t              CHAN_1_CALL;
#ifdef ENABLE_DTMF_CALLING
    char                  ANI_DTMF_ID[8];
    char                  KILL_CODE[8];
    char                  REVIVE_CODE[8];
#endif
    char                  DTMF_UP_CODE[16];

    uint8_t               field57_0x6c;  // Reserved/legacy
    uint8_t               field58_0x6d;  // Reserved/legacy

    char                  DTMF_DOWN_CODE[16];

    uint8_t               field60_0x7e;  // Reserved/legacy
    uint8_t               field61_0x7f;  // Reserved/legacy

#ifdef ENABLE_DTMF_CALLING
    char                  DTMF_SEPARATE_CODE;
    char                  DTMF_GROUP_CALL_CODE;
    uint8_t               DTMF_DECODE_RESPONSE;
    uint8_t               DTMF_auto_reset_time;
#endif  
    uint16_t              DTMF_PRELOAD_TIME;
    uint16_t              DTMF_FIRST_CODE_PERSIST_TIME;
    uint16_t              DTMF_HASH_CODE_PERSIST_TIME;
    uint16_t              DTMF_CODE_PERSIST_TIME;
    uint16_t              DTMF_CODE_INTERVAL_TIME;
    bool                  DTMF_SIDE_TONE;
#ifdef ENABLE_DTMF_CALLING
    bool                  PERMIT_REMOTE_KILL;
#endif
    int16_t               BK4819_XTAL_FREQ_LOW;
#ifdef ENABLE_NOAA
    bool              NOAA_AUTO_SCAN;
#endif
    uint8_t               VOLUME_GAIN;
    #ifdef ENABLE_FEAT_N7SIX
        uint8_t           VOLUME_GAIN_BACKUP;
    #endif
    uint8_t               DAC_GAIN;

    VFO_Info_t            VfoInfo[2];
    uint32_t              POWER_ON_PASSWORD;
    uint16_t              VOX1_THRESHOLD;
    uint16_t              VOX0_THRESHOLD;

    uint8_t               field77_0x95;  // Reserved/legacy
    uint8_t               field78_0x96;  // Reserved/legacy
    uint8_t               field79_0x97;  // Reserved/legacy

    uint8_t               KEY_M_LONG_PRESS_ACTION;
    uint8_t               BACKLIGHT_MIN;
#ifdef ENABLE_BLMIN_TMP_OFF
    BLMIN_STAT_t          BACKLIGHT_MIN_STAT;
#endif
    uint8_t               BACKLIGHT_MAX;
    BATTERY_Type_t        BATTERY_TYPE;
#ifdef ENABLE_RSSI_BAR
    uint8_t               S0_LEVEL;
    uint8_t               S9_LEVEL;
#endif
#ifdef ENABLE_FEAT_N7SIX_CW
    uint8_t               CW_KEY_INPUT;
    uint8_t               CW_KEYER_MODE;
    uint8_t               CW_KEY_WPM;
    uint8_t               field_cw_0x9d;  // Reserved/legacy CW field
    uint8_t               CW_MESSAGE_REPEAT_DELAY;
    uint16_t              CW_ADC_CABLE_20K;
    uint16_t              CW_ADC_CABLE_10K;
    uint16_t              CW_ADC_MAX;
    uint8_t               CW_ADC_RANGE_LIMIT;
    uint8_t               CW_ADC_GLITCH_GUARDBAND;
    uint8_t               field_cw_0xa6;  // Reserved/legacy CW field
    uint8_t               field_cw_0xa7;  // Reserved/legacy CW field
    uint8_t               field_cw_0xa8;  // Reserved/legacy CW field
    uint8_t               field_cw_0xa9;  // Reserved/legacy CW field
    uint8_t               field_cw_0xaa;  // Reserved/legacy CW field
    uint8_t               field_cw_0xab;  // Reserved/legacy CW field
    uint8_t               field_cw_0xac;  // Reserved/legacy CW field
    uint8_t               field_cw_0xad;  // Reserved/legacy CW field
    uint8_t               field_cw_0xae;  // Reserved/legacy CW field
    uint8_t               field_cw_0xaf;  // Reserved/legacy CW field
#endif
} EEPROM_Config_t;

extern EEPROM_Config_t gEeprom;

typedef struct {
    FREQ_Config_t    rx;
    FREQ_Config_t    tx;
    uint32_t         offset;
    uint16_t         stepFrequency;
    STEP_Setting_t   stepSetting;
    ModulationMode_t modulation;
    uint8_t          txOffsetFrequencyDirection;
    uint8_t          outputPower;
    bool             frequencyReverse;
    uint8_t          channelBandwidth;
    uint8_t          busyChannelLock;
    uint8_t          txLock;
#ifdef ENABLE_DTMF_CALLING
    uint8_t          dtmfDecodingEnable;
#endif
    PTT_ID_t         dtmfPttIdTxMode;
} ChannelScanDisplayInfo_t;

void     SETTINGS_InitEEPROM(void);
void     SETTINGS_LoadCalibration(void);
uint32_t SETTINGS_FetchChannelFrequency(const uint16_t channel);
bool     SETTINGS_FetchChannelScanInfo(const uint16_t channel, uint32_t *frequency, ModulationMode_t *modulation);
bool     SETTINGS_FetchChannelScanDisplayInfo(const uint16_t channel, ChannelScanDisplayInfo_t *info);
void     SETTINGS_FetchChannelName(char *s, const uint16_t channel);
void     SETTINGS_FactoryReset(bool bIsAll);
#ifdef ENABLE_FMRADIO
    void SETTINGS_SaveFM(void);
#endif
void SETTINGS_SaveVfoIndices(void);
void SETTINGS_SaveVfoIndicesFlush(void);
void SETTINGS_SaveSettings(void);
void SETTINGS_SaveChannelName(uint16_t channel, const char * name);
void SETTINGS_SaveChannel(uint16_t Channel, uint8_t VFO, const VFO_Info_t *pVFO, uint8_t Mode);
void SETTINGS_SaveBatteryCalibration(const uint16_t * batteryCalibration);
void SETTINGS_UpdateChannel(uint16_t channel, const VFO_Info_t *pVFO, bool keep, bool check, bool save);
void SETTINGS_WriteBuildOptions(void);
#ifdef ENABLE_FEAT_N7SIX_RESUME_STATE
    void SETTINGS_WriteCurrentState(void);
#endif
#ifdef ENABLE_FEAT_N7SIX_VOL
    void SETTINGS_WriteCurrentVol(void);
#endif
#ifdef ENABLE_FEAT_N7SIX
    void SETTINGS_ResetTxLock(void);
#endif
#endif

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

#include "app/cw.h"
#include "app/app.h"

#include <string.h>

#ifdef ENABLE_AM_FIX
    #include "am_fix.h"
#endif
#include "audio.h"
#include "driver/bk4819.h"
#include "driver/keyboard.h"
#include "driver/st7565.h"
#include "driver/system.h"
#include "external/printf/printf.h"
#include "functions.h"
#include "misc.h"
#include "radio.h"
#include "settings.h"
#include "ui/helper.h"
#include "ui/ui.h"

typedef struct {
    char ch;
    const char *morse;
} CW_CharMap_t;

static const CW_CharMap_t CW_CHAR_MAP[] = {
    {'A', ".-"},     {'B', "-..."},   {'C', "-.-."},   {'D', "-.."},
    {'E', "."},      {'F', "..-."},   {'G', "--."},    {'H', "...."},
    {'I', ".."},     {'J', ".---"},   {'K', "-.-"},    {'L', ".-.."},
    {'M', "--"},     {'N', "-."},     {'O', "---"},    {'P', ".--."},
    {'Q', "--.-"},   {'R', ".-."},    {'S', "..."},    {'T', "-"},
    {'U', "..-"},    {'V', "...-"},   {'W', ".--"},    {'X', "-..-"},
    {'Y', "-.--"},   {'Z', "--.."},
    {'0', "-----"},  {'1', ".----"},  {'2', "..---"},  {'3', "...--"},
    {'4', "....-"},  {'5', "....."},  {'6', "-...."},  {'7', "--..."},
    {'8', "---.."},  {'9', "----."},
    {'.', ".-.-.-"}, {',', "--..--"}, {'?', "..--.."}, {'\'', ".----."},
    {'!', "-.-.--"}, {'/', "-..-."},  {'(', "-.--."},  {')', "-.--.-"},
    {'&', ".-..."},  {':', "---..."}, {';', "-.-.-."}, {'=', "-...-"},
    {'+', ".-.-."},  {'-', "-....-"}, {'_', "..--.-"}, {'"', ".-..-."},
    {'$', "...-..-"}, {'@', ".--.-."}
};

static const char CW_KEY_CHARS[9][5] = {
    {'.', ',', '?', '1', '\0'},
    {'A', 'B', 'C', '2', '\0'},
    {'D', 'E', 'F', '3', '\0'},
    {'G', 'H', 'I', '4', '\0'},
    {'J', 'K', 'L', '5', '\0'},
    {'M', 'N', 'O', '6', '\0'},
    {'P', 'Q', 'R', 'S', '7'},
    {'T', 'U', 'V', '8', '\0'},
    {'W', 'X', 'Y', 'Z', '9'}
};

static const uint8_t CW_KEY_CHAR_COUNT[9] = {4, 4, 4, 4, 4, 4, 5, 4, 5};

CW_State_t gCW_State = CW_IDLE;
char       gCW_Message[CW_MSG_MAX_LEN + 1] = {0};
uint8_t    gCW_CursorPos = 0;
uint8_t    gCW_WPM = CW_DEFAULT_WPM;
uint16_t   gCW_ToneFreq = CW_TONE_FREQ;

static bool     gCW_ActiveState = false;
static uint8_t  gCW_PrevKey = 0;
static uint8_t  gCW_PrevLetter = 0;
static uint8_t  gCW_KeyTick = 0;
static bool     gCW_MenuLongHandled = false;
static bool     gCW_Side2LongHandled = false;
static bool     gCW_UpperCase = true;

static bool     gCW_RxModeOverridden = false;
static uint8_t  gCW_BackupDualWatch = DUAL_WATCH_OFF;
static uint8_t  gCW_BackupCrossBand = CROSS_BAND_OFF;
static bool     gCW_BackupMonitor = false;
static bool     gCW_MonitorForced = false;
static uint8_t  gCW_BackupRogerMode = ROGER_MODE_OFF;
static bool     gCW_RogerModeBackedUp = false;

#define CW_RX_MORSE_MAX_LEN 10
#define CW_SIGNAL_THRESHOLD  32  // Signal present when history > this
#define CW_SIGNAL_FULL       64  // Full signal level (tone detected)
#define CW_NOISE_FLOOR       20  // Max noise level when idle
static char     gCW_DecodeText[CW_MSG_MAX_LEN + 1] = {0};
static uint8_t  gCW_DecodeCursor = 0;
static char     gCW_RxMorse[CW_RX_MORSE_MAX_LEN + 1] = {0};
static uint8_t  gCW_RxMorseLen = 0;
static uint16_t gCW_RxMarkTicks = 0;
static uint16_t gCW_RxSpaceTicks = 0;
static bool     gCW_RxSignalPrev = false;
static bool     gCW_RxActive = false;  // True during active reception session
static uint8_t  gCW_RxActiveDebounce = 0;  // Require sustained signal for RX activation
static uint8_t  gCW_RxInactiveDebounce = 0;  // Require sustained loss before RX deactivation
static uint16_t gCW_RxDitTicks = 0;
static uint8_t  gCW_RxSignalHistory[128];  // Signal level history for timing diagram
static uint8_t  gCW_RxTracePeak[128];       // Peak-hold trace for smoother monitoring UI
static uint16_t gCW_RxTraceClock = 0;       // Pace the trace to Morse timing
static uint32_t gCW_RxMarkStart = 0;
static uint32_t gCW_RxSpaceStart = 0;
static uint8_t  gCW_RxSignalDebounce = 0;
static int16_t  gCW_RxNoiseFloor = -120;
static bool     gCW_StartupDelay = false;  // Prevent RX activation during startup

typedef enum {
    CW_RX_STATE_IDLE = 0,
    CW_RX_STATE_MARK,
    CW_RX_STATE_GAP
} CW_RxDecoderState_t;

static CW_RxDecoderState_t gCW_RxDecoderState = CW_RX_STATE_IDLE;
static uint8_t gCW_RxElementConfidence = 0;
static uint8_t gCW_RxCharConfidence = 0;

static uint16_t gCW_DitMs = 60;
static uint16_t gCW_DahMs = 180;
static uint16_t gCW_InterElemMs = 60;
static uint16_t gCW_InterCharMs = 180;
static uint16_t gCW_InterWordMs = 420;

#define CW_DIT_MIN_MS     20  // Fastest dit: ~60 WPM
#define CW_DIT_MAX_MS     240 // Slowest dit: ~5 WPM
#define CW_OFFSET_HYSTERESIS 5
#define CW_RX_DEBOUNCE_TICKS   1
#define CW_RX_ACTIVATE_TICKS   5  // Require 50ms sustained signal before RX mode
#define CW_RX_DEACTIVATE_TICKS 3  // Require 30ms of no signal before deactivating RX
#define CW_RX_SIMPLE_THRESHOLD 8  // dB above noise floor for tone detection
#define CW_MULTI_TAP_TIMEOUT_TICKS 80

static void CW_UpdateTiming(void)
{
    gCW_DitMs = 1200 / gCW_WPM;
    if (gCW_DitMs < CW_DIT_MIN_MS)  gCW_DitMs = CW_DIT_MIN_MS;
    if (gCW_DitMs > CW_DIT_MAX_MS)  gCW_DitMs = CW_DIT_MAX_MS;

    gCW_DahMs = gCW_DitMs * 3;
    gCW_InterElemMs = gCW_DitMs;
    gCW_InterCharMs = gCW_DitMs * 3;
    gCW_InterWordMs = gCW_DitMs * 7;
}

static bool CW_IsAllowedInputChar(char c)
{
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ')
        return true;
    // Allow common punctuation for CW operation
    if (c == '.' || c == ',' || c == '?' || c == '\'' || c == '!' || c == '/' ||
        c == '(' || c == ')' || c == '&' || c == ':' || c == ';' || c == '=' ||
        c == '+' || c == '-' || c == '_' || c == '"' || c == '$' || c == '@')
        return true;
    return false;
}

static char CW_GetInputChar(uint8_t idx, uint8_t letter)
{
    char c = CW_KEY_CHARS[idx][letter];
    if (!gCW_UpperCase && c >= 'A' && c <= 'Z')
        c = (char)(c - 'A' + 'a');
    return c;
}

static void CW_AppendDecodedText(const char *text)
{
    if (text == NULL || text[0] == '\0')
        return;

    for (uint8_t i = 0; text[i] != '\0'; i++)
    {
        if (gCW_DecodeCursor >= CW_MSG_MAX_LEN)
        {
            // Scroll existing text left by 1 to make room
            memmove(gCW_DecodeText, gCW_DecodeText + 1, CW_MSG_MAX_LEN - 1);
            gCW_DecodeCursor = CW_MSG_MAX_LEN - 1;
        }

        gCW_DecodeText[gCW_DecodeCursor++] = text[i];
        gCW_DecodeText[gCW_DecodeCursor] = '\0';
    }
}

/* Returns pointer to static buffer; copy before next call */
static const char *CW_MorseToDecodedToken(const char *morse)
{
    static char oneChar[2];

    if (morse == NULL || morse[0] == '\0')
        return " ";

    for (uint8_t i = 0; i < ARRAY_SIZE(CW_CHAR_MAP); i++)
    {
        if (strcmp(morse, CW_CHAR_MAP[i].morse) == 0)
        {
            oneChar[0] = CW_CHAR_MAP[i].ch;
            oneChar[1] = '\0';
            return oneChar;
        }
    }

    return "?";
}

static void CW_FinalizeRxCharacter(void)
{
    if (gCW_RxMorseLen == 0)
        return;

    gCW_RxMorse[gCW_RxMorseLen] = '\0';

    const char *decodedToken = CW_MorseToDecodedToken(gCW_RxMorse);
    if (decodedToken != NULL && decodedToken[0] != '\0' && decodedToken[0] != '?')
    {
        CW_AppendDecodedText(decodedToken);
    }

    gCW_RxMorseLen = 0;
    gCW_RxElementConfidence = 0;
    gCW_RxCharConfidence = 0;
    gUpdateDisplay = true;

    if (gCW_ActiveState && gCW_State == CW_COMPOSING)
        CW_Render();
}

static void CW_PushTraceSample(uint8_t level)
{
    memmove(gCW_RxSignalHistory, gCW_RxSignalHistory + 1, 127);
    gCW_RxSignalHistory[127] = level;

    memmove(gCW_RxTracePeak, gCW_RxTracePeak + 1, 127);
    if (level > gCW_RxTracePeak[127])
        gCW_RxTracePeak[127] = level;
    else if (gCW_RxTracePeak[127] > 0)
        gCW_RxTracePeak[127]--;
}

// Draw a compact, scrolling vertical-bar trace on CW_LINE_DECODE (5th row).
// This follows the same visual idea as the main-radio TX audio scope: a moving
// set of narrow bars that grow with signal strength and scroll across the line.
static void CW_DrawSignalGraph(void)
{
    memset(gFrameBuffer[CW_LINE_DECODE], 0, LCD_WIDTH);

    const uint8_t columnWidth  = 2u;
    const uint8_t columnGap    = 1u;
    const uint8_t colStride    = columnWidth + columnGap;
    const uint8_t traceColumns = (uint8_t)(LCD_WIDTH / colStride);

    for (uint8_t i = 0; i < traceColumns; i++)
    {
        const uint8_t sampleIndex = (uint8_t)(128u - traceColumns + i);
        const uint8_t liveLevel = gCW_RxSignalHistory[sampleIndex];
        const uint8_t heldLevel = gCW_RxTracePeak[sampleIndex];
        const uint8_t level = (liveLevel > heldLevel) ? liveLevel : heldLevel;
        uint8_t height = 0u;

        if (level >= CW_SIGNAL_FULL)
            height = 7u;
        else if (level > CW_SIGNAL_THRESHOLD)
            height = 5u;
        else if (level > CW_NOISE_FLOOR)
            height = 1u;

        const uint8_t mask = (height > 0u) ? (uint8_t)((0x7Fu << (7u - height)) & 0x7Fu) : 0x40u;
        uint8_t *p_col = &gFrameBuffer[CW_LINE_DECODE][i * colStride];

        p_col[0] = mask;
        if (columnWidth > 1u)
            p_col[1] = mask;
    }
}

static void CW_ResetRxDecoder(bool clearDecodedText)
{
    if (clearDecodedText)
    {
        gCW_DecodeCursor = 0;
        gCW_DecodeText[0] = '\0';
    }
    gCW_RxMorseLen = 0;
    gCW_RxMorse[0] = '\0';
    gCW_RxMarkTicks = 0;
    gCW_RxSpaceTicks = 0;
    gCW_RxSignalPrev = false;
    gCW_RxActive = false;
    gCW_RxActiveDebounce = 0;
    gCW_StartupDelay = false;
    gCW_RxDitTicks = (uint16_t)MAX(1, (int)((gCW_DitMs + 5) / 10));
    memset(gCW_RxSignalHistory, 0, sizeof(gCW_RxSignalHistory));
    memset(gCW_RxTracePeak, 0, sizeof(gCW_RxTracePeak));
    gCW_PeakRssi = -110;
    gCW_RxLastRssi = -120;
    gCW_RxNoiseFloor = -120;
    gCW_RxSignalFloor = -110;
    gCW_RxToneState = false;
    gCW_RxDecoderState = CW_RX_STATE_IDLE;
    gCW_RxElementConfidence = 0;
    gCW_RxCharConfidence = 0;
    gCW_RxToneOnDebounce = 0;
    gCW_RxToneOffDebounce = 0;
    gCW_RxTraceClock = 0;
}


static bool CW_IsRxTonePresent(void)
{
    int16_t rssi_dBm =
        BK4819_GetRSSI_dBm()
#ifdef ENABLE_AM_FIX
        + ((gSetting_AM_fix && gRxVfo->Modulation == MODULATION_AM) ? AM_fix_get_gain_diff() : 0)
#endif
        + dBmCorrTable[gRxVfo->Band];

    // Do not hard-block decoding on squelch state alone. The CW decoder should
    // rely on the RSSI threshold and the current noise floor so real Morse bursts
    // can still be classified while the receiver is in monitor mode or the
    // squelch state is briefly changing.
    if (g_SquelchLost && !gCW_RxToneState)
    {
        gCW_RxToneOnDebounce = 0;
        gCW_RxToneOffDebounce = 0;
    }

    if (rssi_dBm > gCW_PeakRssi)
        gCW_PeakRssi = rssi_dBm;
    else if (rssi_dBm < (gCW_PeakRssi - 10))
        gCW_PeakRssi = (int16_t)(gCW_PeakRssi - 1);
    if (gCW_PeakRssi < -110)
        gCW_PeakRssi = -110;

    if (rssi_dBm > gCW_RxSignalFloor)
        gCW_RxSignalFloor = (int16_t)((gCW_RxSignalFloor * 3 + rssi_dBm + 2) / 4);
    else
        gCW_RxSignalFloor = (int16_t)((gCW_RxSignalFloor * 7 + rssi_dBm + 4) / 8);

    if (rssi_dBm < gCW_RxNoiseFloor)
        gCW_RxNoiseFloor = (int16_t)((gCW_RxNoiseFloor * 7 + rssi_dBm + 4) / 8);
    else
        gCW_RxNoiseFloor = (int16_t)((gCW_RxNoiseFloor * 15 + rssi_dBm + 8) / 16);

    const int16_t signalSpan = (int16_t)MAX(4, (gCW_RxSignalFloor - gCW_RxNoiseFloor) / 2);
    const int16_t openLevel  = (int16_t)(gCW_RxNoiseFloor + signalSpan + 2);
    const int16_t closeLevel = (int16_t)(gCW_RxNoiseFloor + MAX(1, signalSpan / 2));
    const int16_t absOpenLevel = -104;
    const int16_t absCloseLevel = -107;
    const int16_t effectiveOpenLevel = (int16_t)MAX(openLevel, absOpenLevel);
    const int16_t effectiveCloseLevel = (int16_t)MAX(closeLevel, absCloseLevel);

    bool signalStrong;
    if (!gCW_RxToneState)
        signalStrong = (rssi_dBm >= effectiveOpenLevel) && (rssi_dBm >= (gCW_RxNoiseFloor + 3));
    else
        signalStrong = (rssi_dBm >= effectiveCloseLevel);

    if (signalStrong)
    {
        if (gCW_RxToneOnDebounce < 0xFF)
            gCW_RxToneOnDebounce++;
        gCW_RxToneOffDebounce = 0;

        if (!gCW_RxToneState && gCW_RxToneOnDebounce >= CW_RX_DEBOUNCE_TICKS)
            gCW_RxToneState = true;
    }
    else
    {
        if (gCW_RxToneOffDebounce < 0xFF)
            gCW_RxToneOffDebounce++;
        gCW_RxToneOnDebounce = 0;

        if (gCW_RxToneState && gCW_RxToneOffDebounce >= CW_RX_DEBOUNCE_TICKS)
            gCW_RxToneState = false;
    }

    gCW_RxLastRssi = (int16_t)rssi_dBm;

    return gCW_RxToneState;
}

static void CW_UpdateRxDitEstimate(uint16_t observedMarkTicks)
{
    if (observedMarkTicks == 0)
        return;

    if (observedMarkTicks < 3)
        return;

    if (observedMarkTicks > (gCW_RxDitTicks * 4))
        return;

    gCW_RxDitTicks = (uint16_t)((5 * gCW_RxDitTicks + observedMarkTicks + 2) / 6);

    if (gCW_RxDitTicks < 1)
        gCW_RxDitTicks = 1;
    if (gCW_RxDitTicks > 40)
        gCW_RxDitTicks = 40;
}

static bool CW_ClassifyMark(uint16_t markTicks, uint16_t ditTicks, uint16_t dahThreshold)
{
    if (markTicks < MAX(2U, ditTicks / 2U))
        return false;

    if (markTicks >= dahThreshold)
        return true;

    return (markTicks >= (uint16_t)((ditTicks * 3) / 2));
}

static void CW_UpdateDecoderConfidence(bool isDah)
{
    if (isDah)
    {
        if (gCW_RxElementConfidence < 0xFF)
            gCW_RxElementConfidence++;
    }
    else
    {
        if (gCW_RxElementConfidence > 0)
            gCW_RxElementConfidence--;
    }

    if (gCW_RxElementConfidence >= 2)
        gCW_RxCharConfidence = MIN(0xFF, gCW_RxCharConfidence + 1);
    else if (gCW_RxElementConfidence == 0 && gCW_RxCharConfidence > 0)
        gCW_RxCharConfidence--;
}

static void CW_ToneOn(void)
{
    BK4819_TransmitTone(true, gCW_ToneFreq);
}

static void CW_ToneOff(void)
{
    BK4819_EnterTxMute();
}

static bool CW_BeginDedicatedTx(void)
{
    VfoState_t state = VFO_STATE_NORMAL;

    if (gCurrentVfo == NULL)
        RADIO_SelectVfos();

    if (TX_freq_check(gCurrentVfo->pTX->Frequency) != 0 && gCurrentVfo->TX_LOCK)
    {
        state = VFO_STATE_TX_DISABLE;
        gVfoConfigureMode = VFO_CONFIGURE;
    }
    else if (SerialConfigInProgress())
    {
        state = VFO_STATE_TX_DISABLE;
    }
    else if (gCurrentVfo->BUSY_CHANNEL_LOCK && gCurrentFunction == FUNCTION_RECEIVE)
    {
        state = VFO_STATE_BUSY;
    }
    else if (gBatteryDisplayLevel == 0)
    {
        state = VFO_STATE_BAT_LOW;
    }
    else if (gBatteryDisplayLevel > 6)
    {
        state = VFO_STATE_VOLTAGE_HIGH;
    }
#ifdef ENABLE_BYP_RAW_DEMODULATORS
    else if (gCurrentVfo->Modulation == MODULATION_BYP || gCurrentVfo->Modulation == MODULATION_RAW)
    {
        state = VFO_STATE_TX_DISABLE;
    }
#endif

    if (state != VFO_STATE_NORMAL)
    {
        RADIO_SetVfoState(state);
        AUDIO_PlayBeep(BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL);
        return false;
    }

    BK4819_DisableDTMF();

    FUNCTION_Select(FUNCTION_TRANSMIT);

    gTxTimerCountdown_500ms = ((gEeprom.TX_TIMEOUT_TIMER + 1) * 5) * 2;
#ifdef ENABLE_FEAT_N7SIX
    gTxTimerCountdownAlert_500ms = gTxTimerCountdown_500ms;
    gTxTimeoutReachedAlert = false;
#endif
    gTxTimeoutReached = false;
    gFlagEndTransmission = false;
    gRTTECountdown_10ms = 0;

    BK4819_DisableScramble();
    BK4819_SetCompander(0);
    BK4819_ExitSubAu();
    BK4819_SetAF(BK4819_AF_MUTE);

    return gCurrentFunction == FUNCTION_TRANSMIT;
}

static void CW_EndDedicatedTx(void)
{
    BK4819_DisableDTMF();
    BK4819_DisableScramble();
    // BK4819_TurnsOffTones_TurnsOnRX() handles the full mute->RX transition.
    // Do NOT call EnterTxMute() first — it calls ExitTxMute() internally and would
    // re-enable the carrier, causing a tail tone pop.
    BK4819_TurnsOffTones_TurnsOnRX();
    BK4819_ExitSubAu();
    // Ensure DTMF/MDC detection remains disabled during RX to prevent
    // any digital noise from being interpreted as valid tones.
    BK4819_DisableDTMF();
    // Skip RADIO_SetupRegisters(false) here to avoid PA transient on the other radio.
    // BK4819_TurnsOffTones_TurnsOnRX() already sets up the RX registers correctly.
    FUNCTION_Select(FUNCTION_FOREGROUND);
    gFlagEndTransmission = false;
    gRTTECountdown_10ms = 0;
    RADIO_SetVfoState(VFO_STATE_NORMAL);
    RADIO_SelectVfos();
}

// NOTE: CW_PlayDit(), CW_PlayDah(), and CW_PlayCharacter() are BLOCKING calls
// that use BK4819_PlayToneRaw(). They MUST NOT be called from the 10ms tick
// (CW_TimeSlice10ms) or from the non-blocking CW_TxStateMachine() FSM.
// They are intended for short UI/keypress feedback only. The typed-message
// TX path uses CW_ToneOn()/CW_ToneOff() with gCW_TxTimer for non-blocking timing.
void CW_PlayDit(void)
{
    CW_ToneOn();
    BK4819_PlayToneRaw(gCW_ToneFreq, gCW_DitMs);
    CW_ToneOff();
}

void CW_PlayDah(void)
{
    CW_ToneOn();
    BK4819_PlayToneRaw(gCW_ToneFreq, gCW_DahMs);
    CW_ToneOff();
}

void CW_PlayCharacter(const char *morse)
{
    if (morse == NULL || morse[0] == '\0')
    {
        SYSTEM_DelayMs(gCW_InterWordMs - gCW_InterCharMs);
        return;
    }

    for (uint8_t i = 0; morse[i] != '\0'; i++)
    {
        if (morse[i] == '.')
            CW_PlayDit();
        else if (morse[i] == '-')
            CW_PlayDah();

        if (morse[i + 1] != '\0')
            SYSTEM_DelayMs(gCW_InterElemMs);
    }

    SYSTEM_DelayMs(gCW_InterCharMs);
}

const char * CW_CharToMorse(char c)
{
    if (c >= 'a' && c <= 'z')
        c = (char)(c - 'a' + 'A');

    for (uint8_t i = 0; i < ARRAY_SIZE(CW_CHAR_MAP); i++)
    {
        if (CW_CHAR_MAP[i].ch == c)
            return CW_CHAR_MAP[i].morse;
    }

    if (c == ' ')
        return "";

    return NULL;
}

uint8_t CW_MorseToChar(const char *morse)
{
    if (morse == NULL || morse[0] == '\0')
        return ' ';

    for (uint8_t i = 0; i < ARRAY_SIZE(CW_CHAR_MAP); i++)
    {
        if (strcmp(morse, CW_CHAR_MAP[i].morse) == 0)
            return (uint8_t)CW_CHAR_MAP[i].ch;
    }

    return '?';
}

void CW_AppendChar(char c)
{
    if (gCW_CursorPos >= CW_MSG_MAX_LEN)
        return;

    gCW_Message[gCW_CursorPos++] = c;
    gCW_Message[gCW_CursorPos] = '\0';
}

void CW_DeleteChar(void)
{
    if (gCW_CursorPos > 0)
    {
        gCW_CursorPos--;
        gCW_Message[gCW_CursorPos] = '\0';
    }
}

// --- Non-blocking typed-message TX FSM ---
// Driven by CW_TimeSlice10ms() at 10ms cadence.
typedef enum {
    CW_TX_IDLE = 0,
    CW_TX_PREAMBLE,
    CW_TX_ELEMENT,
    CW_TX_ELEM_GAP,
    CW_TX_CHAR_GAP,
    CW_TX_WORD_GAP,
    CW_TX_TAIL
} CW_TxState_t;

static CW_TxState_t  gCW_TxState = CW_TX_IDLE;
static uint8_t       gCW_TxMsgIdx = 0;
static const char   *gCW_TxMorse = NULL;
static uint8_t       gCW_TxMorseIdx = 0;
static uint8_t       gCW_TxTimer = 0;
static bool          gCW_TxPrevWasSpace = false;
static bool          gCW_TxSentAny = false;

void CW_SendMessage(void)
{
    if (strlen(gCW_Message) == 0)
        return;

    if (!CW_BeginDedicatedTx())
        return;

    gCW_State = CW_SENDING;
    gCW_PlaybackActive = true;
    gCW_TxState = CW_TX_PREAMBLE;
    gCW_TxMsgIdx = 0;
    gCW_TxMorse = NULL;
    gCW_TxMorseIdx = 0;
    gCW_TxTimer = 5;  // 50ms preamble
    gCW_TxPrevWasSpace = false;
    gCW_TxSentAny = false;
    CW_Render();
}

// Called every 10ms from CW_TimeSlice10ms() while CW_SENDING && gCW_PlaybackActive
static void CW_TxStateMachine(void)
{
    if (gCW_TxTimer > 0) {
        gCW_TxTimer--;
        return;
    }

    switch (gCW_TxState)
    {
        case CW_TX_PREAMBLE:
            // Start first character
            gCW_TxState = CW_TX_ELEMENT;
            // fall through to element selection

        case CW_TX_ELEMENT:
        {
            // Need a new character?
            if (gCW_TxMorse == NULL || gCW_TxMorse[gCW_TxMorseIdx] == '\0')
            {
                // Advance to next message character
                while (gCW_TxMsgIdx < gCW_CursorPos)
                {
                    char c = gCW_Message[gCW_TxMsgIdx];
                    if (c >= 'a' && c <= 'z')
                        c = (char)(c - 'a' + 'A');

                    if (c == ' ')
                    {
                        const bool trailing = (gCW_TxMsgIdx + 1) >= gCW_CursorPos;
                        if (!gCW_TxSentAny || gCW_TxPrevWasSpace || trailing)
                        {
                            gCW_TxMsgIdx++;
                            continue;
                        }
                        gCW_TxPrevWasSpace = true;
                        gCW_TxState = CW_TX_WORD_GAP;
                        gCW_TxTimer = (uint8_t)(gCW_InterWordMs / 10);
                        gCW_TxMsgIdx++;
                        return;
                    }

                    gCW_TxMorse = CW_CharToMorse(c);
                    gCW_TxMorseIdx = 0;
                    gCW_TxMsgIdx++;
                    if (gCW_TxMorse != NULL && gCW_TxMorse[0] != '\0')
                    {
                        gCW_TxSentAny = true;
                        gCW_TxPrevWasSpace = false;
                        break;
                    }
                    // Unknown char: skip
                    gCW_TxMorse = NULL;
                }

                if (gCW_TxMorse == NULL || gCW_TxMorseIdx >= strlen(gCW_TxMorse))
                {
                    // Done
                    gCW_TxState = CW_TX_TAIL;
                    gCW_TxTimer = (uint8_t)(gCW_InterCharMs / 10);
                    return;
                }
            }

            // Emit current element
            if (gCW_TxMorse[gCW_TxMorseIdx] == '.') {
                CW_ToneOn();
                gCW_TxTimer = (uint8_t)(gCW_DitMs / 10);
            } else if (gCW_TxMorse[gCW_TxMorseIdx] == '-') {
                CW_ToneOn();
                gCW_TxTimer = (uint8_t)(gCW_DahMs / 10);
            }
            gCW_TxState = CW_TX_ELEM_GAP;
            CW_Render();
            break;
        }

        case CW_TX_ELEM_GAP:
        {
            CW_ToneOff();
            gCW_TxMorseIdx++;
            if (gCW_TxMorse[gCW_TxMorseIdx] != '\0')
            {
                gCW_TxState = CW_TX_ELEMENT;
                gCW_TxTimer = (uint8_t)(gCW_InterElemMs / 10);
            }
            else
            {
                gCW_TxState = CW_TX_CHAR_GAP;
                gCW_TxTimer = (uint8_t)(gCW_InterCharMs / 10);
            }
            break;
        }

        case CW_TX_CHAR_GAP:
            gCW_TxState = CW_TX_ELEMENT;
            break;

        case CW_TX_WORD_GAP:
            gCW_TxState = CW_TX_ELEMENT;
            break;

        case CW_TX_TAIL:
            gCW_TxState = CW_TX_IDLE;
            gCW_PlaybackActive = false;
            gCW_State = CW_COMPOSING;
            CW_EndDedicatedTx();
            APP_StartListening(gMonitor ? FUNCTION_MONITOR : FUNCTION_RECEIVE);
            CW_Render();
            break;

        default:
            gCW_TxState = CW_TX_IDLE;
            break;
    }
}

void CW_Init(void)
{
    gCW_State = CW_IDLE;
    gCW_ActiveState = false;
    gCW_CursorPos = 0;
    gCW_Message[0] = '\0';
    gCW_WPM = CW_DEFAULT_WPM;
    gCW_ToneFreq = CW_TONE_FREQ;
    gCW_UpperCase = true;
    gCW_PrevKey = 0;
    gCW_PrevLetter = 0;
    gCW_KeyTick = 0;
    gCW_MenuLongHandled = false;
    gCW_Side2LongHandled = false;

    CW_UpdateTiming();
    CW_ResetRxDecoder(true);
    gCW_PeakRssi = -110;
    gCW_RxActive = false;
}

void CW_TimeSlice10ms(void)
{
    // Skip ALL processing during startup delay (500ms) to prevent noise transients
    if (gCW_StartupDelay)
    {
        static uint8_t startupTicks = 0;
        if (startupTicks++ >= 50)  // 500ms delay
        {
            gCW_StartupDelay = false;
            startupTicks = 0;
        }
        return;
    }

    #ifdef ENABLE_FEAT_N7SIX_CW
    if (gCW_ActiveState && gCW_PlaybackActive)
        CW_TxStateMachine();   // drive non-blocking typed-message TX
    else if (gCW_ActiveState)
        CW_AppUpdate();        // paddle/keyer FSM (when not in typed playback)
    #endif

    if (!gCW_ActiveState || gCW_State == CW_SENDING)
        return;

    if (!gMonitor) {
        gMonitor = true;
        APP_StartListening(FUNCTION_MONITOR);
    }

    const bool signalNow = CW_IsRxTonePresent();

    // Track active reception state with debounce to filter noise transients
    if (signalNow && !gCW_RxActive)
    {
        if (gCW_RxActiveDebounce < 0xFF)
            gCW_RxActiveDebounce++;
        
        if (gCW_RxActiveDebounce >= CW_RX_ACTIVATE_TICKS)
        {
            gCW_RxActive = true;
            gCW_RxActiveDebounce = 0;
        }
    }
    else if (!signalNow && gCW_RxActive)
    {
        if (gCW_RxInactiveDebounce < 0xFF)
            gCW_RxInactiveDebounce++;
        
        if (gCW_RxInactiveDebounce >= CW_RX_DEACTIVATE_TICKS)
        {
            gCW_RxActive = false;
            gCW_RxActiveDebounce = 0;
            gCW_RxInactiveDebounce = 0;
            // Preserve the decoded transcript across short pauses so the UI
            // continues to display the letters that were just decoded.
            // The buffer is only cleared on an explicit CW reset/exit path.
        }
    }
    else if (!signalNow && !gCW_RxActive)
    {
        gCW_RxActiveDebounce = 0;
        gCW_RxInactiveDebounce = 0;
    }

    // Update the traced waveform continuously during active reception so the
    // whole Morse message remains visible, not just the first element.
    const bool traceActive = signalNow || gCW_RxSignalPrev || gCW_RxActive;
    const uint16_t traceAdvanceTicks = traceActive ? 1U : MAX(2U, (gCW_RxDitTicks + 1U) / 2U);
    if (gCW_RxTraceClock >= traceAdvanceTicks)
    {
        gCW_RxTraceClock = 0;

        int16_t rssi_dBm = BK4819_GetRSSI_dBm() + dBmCorrTable[gRxVfo->Band];
        uint8_t sigLevel = (uint8_t)(rssi_dBm + 120);  // Normalize -120dBm to 0
        if (sigLevel > CW_SIGNAL_FULL) sigLevel = CW_SIGNAL_FULL;
        if (signalNow) sigLevel = CW_SIGNAL_FULL;
        else if (sigLevel > CW_NOISE_FLOOR) sigLevel = CW_NOISE_FLOOR;

        CW_PushTraceSample(sigLevel);
    }
    else
    {
        gCW_RxTraceClock++;
    }

    if (gCW_KeyTick > 0)
    {
        if (gCW_KeyTick < 0xFF)
            gCW_KeyTick++;

        if (gCW_KeyTick >= CW_MULTI_TAP_TIMEOUT_TICKS)
        {
            gCW_KeyTick = 0;
            gCW_PrevKey = 0;
            gCW_PrevLetter = 0;
            CW_Render();
        }
    }

    const uint16_t ditTicks = MAX(1U, gCW_RxDitTicks);
    const uint16_t dahThreshold = (uint16_t)((ditTicks * 5) / 2);  // 2.5 * dit
    const uint16_t charGapTicks = (uint16_t)(3 * ditTicks);
    const uint16_t wordGapTicks = (uint16_t)(7 * ditTicks);

    if (signalNow)
    {
        if (gCW_RxSignalPrev)
        {
            if (gCW_RxMarkTicks < 0xFFFE)
                gCW_RxMarkTicks++;
            return;
        }

        if (gCW_RxDecoderState == CW_RX_STATE_GAP && gCW_RxMorseLen > 0)
        {
            if (gCW_RxSpaceTicks >= wordGapTicks)
            {
                CW_FinalizeRxCharacter();
                if (gCW_DecodeCursor > 0 && gCW_DecodeText[gCW_DecodeCursor - 1] != ' ')
                    CW_AppendDecodedText(" ");
            }
            else if (gCW_RxSpaceTicks >= charGapTicks)
            {
                CW_FinalizeRxCharacter();
            }
        }

        gCW_RxSignalPrev = true;
        gCW_RxMarkTicks = 1;
        gCW_RxSpaceTicks = 0;
        gCW_RxDecoderState = CW_RX_STATE_MARK;
        return;
    }

    if (gCW_RxSignalPrev)
    {
        if (gCW_RxMarkTicks > 0 && gCW_RxMorseLen < CW_RX_MORSE_MAX_LEN)
        {
            if (CW_ClassifyMark(gCW_RxMarkTicks, ditTicks, dahThreshold))
            {
                gCW_RxMorse[gCW_RxMorseLen++] = '-';
                gCW_RxMorse[gCW_RxMorseLen] = '\0';
                CW_UpdateDecoderConfidence(true);
                if (gCW_RxMarkTicks >= 3)
                    CW_UpdateRxDitEstimate((uint16_t)((gCW_RxMarkTicks + 1) / 3));
                CW_PushTraceSample(CW_SIGNAL_FULL);
                if (gCW_ActiveState && gCW_State == CW_COMPOSING)
                    CW_Render();
            }
            else if (gCW_RxMarkTicks >= MAX(2U, ditTicks / 2U))
            {
                gCW_RxMorse[gCW_RxMorseLen++] = '.';
                gCW_RxMorse[gCW_RxMorseLen] = '\0';
                CW_UpdateDecoderConfidence(false);
                CW_UpdateRxDitEstimate(gCW_RxMarkTicks);
                CW_PushTraceSample(CW_SIGNAL_THRESHOLD);
                if (gCW_ActiveState && gCW_State == CW_COMPOSING)
                    CW_Render();
            }
        }

        gCW_RxSignalPrev = false;
        gCW_RxMarkTicks = 0;
        gCW_RxSpaceTicks = 1;
        gCW_RxDecoderState = CW_RX_STATE_GAP;
        return;
    }

    if (gCW_RxDecoderState == CW_RX_STATE_GAP)
    {
        if (gCW_RxSpaceTicks < 0xFFFE)
            gCW_RxSpaceTicks++;

        if (gCW_RxMorseLen > 0 && gCW_RxSpaceTicks >= charGapTicks)
        {
            CW_FinalizeRxCharacter();
            gCW_RxSpaceTicks = charGapTicks;
        }

        if (gCW_RxMorseLen > 0 && gCW_RxSpaceTicks >= wordGapTicks)
        {
            if (gCW_DecodeCursor == 0 || gCW_DecodeText[gCW_DecodeCursor - 1] != ' ')
                CW_AppendDecodedText(" ");
            gCW_RxSpaceTicks = wordGapTicks;
        }
    }
}

void CW_Start(void)
{
    if (gCW_ActiveState)
        return;

    if (!gCW_RxModeOverridden)
    {
        gCW_BackupDualWatch = gEeprom.DUAL_WATCH;
        gCW_BackupCrossBand = gEeprom.CROSS_BAND_RX_TX;
        gCW_RxModeOverridden = true;
    }

    gEeprom.DUAL_WATCH = DUAL_WATCH_OFF;
    gEeprom.CROSS_BAND_RX_TX = CROSS_BAND_OFF;
    gDualWatchActive = false;
    gScheduleDualWatch = false;
    RADIO_SelectVfos();

    if (!gCW_MonitorForced)
    {
        gCW_BackupMonitor = gMonitor;
        gCW_MonitorForced = true;
    }

    gRxVfoIsActive = true;
    RADIO_SetupRegisters(true);

    // Backup and disable Roger beep setting for the entire CW session
    // to prevent MDC/Roger beeps from playing at end of transmission
    if (!gCW_RogerModeBackedUp)
    {
        gCW_BackupRogerMode = gEeprom.ROGER;
        gCW_RogerModeBackedUp = true;
        gEeprom.ROGER = ROGER_MODE_OFF;
    }

    // Disable DTMF/MDC for the entire CW session to prevent any digital noise
    BK4819_DisableDTMF();

    // Open monitor path AFTER radio register setup so audio is not reset.
    gMonitor = true;
    APP_StartListening(FUNCTION_MONITOR);

    // Enable startup delay to prevent noise transients from triggering RX
    gCW_StartupDelay = true;

    CW_Init();
    gCW_State = CW_COMPOSING;
    gCW_ActiveState = true;

    memset(gFrameBuffer[CW_LINE_TX1], 0, LCD_WIDTH);
    memset(gFrameBuffer[CW_LINE_TX2], 0, LCD_WIDTH);
    memset(gFrameBuffer[CW_LINE_DECODE], 0, LCD_WIDTH);
    memset(gFrameBuffer[CW_LINE_STATUS], 0, LCD_WIDTH);

    gWasFKeyPressed = false;

    CW_Render();

    AUDIO_PlayBeep(BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL);
}

void CW_Stop(void)
{
    if (!gCW_ActiveState)
        return;

    gCW_ActiveState = false;
    gCW_State = CW_IDLE;

    memset(gFrameBuffer[CW_LINE_TX1], 0, LCD_WIDTH);
    memset(gFrameBuffer[CW_LINE_TX2], 0, LCD_WIDTH);
    memset(gFrameBuffer[CW_LINE_DECODE], 0, LCD_WIDTH);
    memset(gFrameBuffer[CW_LINE_STATUS], 0, LCD_WIDTH);

    CW_ResetRxDecoder(true);

    if (gCW_RxModeOverridden)
    {
        gEeprom.DUAL_WATCH = gCW_BackupDualWatch;
        gEeprom.CROSS_BAND_RX_TX = gCW_BackupCrossBand;
        gCW_RxModeOverridden = false;
        gScheduleDualWatch = true;
        RADIO_SelectVfos();
    }

    if (gCW_MonitorForced)
    {
        gMonitor = gCW_BackupMonitor;
        gCW_MonitorForced = false;
        APP_StartListening(gMonitor ? FUNCTION_MONITOR : FUNCTION_RECEIVE);
    }

    // Restore Roger beep setting when exiting CW mode
    if (gCW_RogerModeBackedUp)
    {
        gEeprom.ROGER = gCW_BackupRogerMode;
        gCW_RogerModeBackedUp = false;
    }

    // Ensure DTMF/MDC detection is disabled after exiting CW mode
    BK4819_DisableDTMF();

    gWasFKeyPressed = false;
    gUpdateDisplay   = true;
}

void CW_Toggle(void)
{
    if (gCW_ActiveState)
        CW_Stop();
    else
        CW_Start();
}

static void CW_DrawWrappedTxText(void)
{
    const uint16_t len = (uint16_t)strlen(gCW_Message);
    if (len == 0)
    {
        memset(gFrameBuffer[CW_LINE_TX1], 0, LCD_WIDTH);
        memset(gFrameBuffer[CW_LINE_TX2], 0, LCD_WIDTH);
        UI_PrintStringSmallNormal("TYPE MESSAGE", 2, 0, CW_LINE_TX1);
        UI_PrintStringSmallNormal("PRESS PTT", 2, 0, CW_LINE_TX2);
        return;
    }

    const uint16_t line1_limit = (uint16_t)CW_CHARS_PER_TX_LINE;
    memset(gFrameBuffer[CW_LINE_TX1], 0, LCD_WIDTH);
    memset(gFrameBuffer[CW_LINE_TX2], 0, LCD_WIDTH);

    if (len <= line1_limit)
    {
        UI_PrintStringSmallBold(gCW_Message, 2, 0, CW_LINE_TX1);
        return;
    }

    char line1[CW_CHARS_PER_TX_LINE + 1];
    memcpy(line1, gCW_Message, line1_limit);
    line1[line1_limit] = '\0';
    UI_PrintStringSmallBold(line1, 2, 0, CW_LINE_TX1);

    uint16_t remaining = len - line1_limit;
    const char *pRemain = gCW_Message + line1_limit;
    if (remaining > CW_CHARS_PER_TX_LINE)
        remaining = CW_CHARS_PER_TX_LINE;

    char line2[CW_CHARS_PER_TX_LINE + 1];
    memcpy(line2, pRemain, remaining);
    line2[remaining] = '\0';
    UI_PrintStringSmallBold(line2, 2, 0, CW_LINE_TX2);
}

static void CW_DrawStatusLine(void)
{
    memset(gFrameBuffer[CW_LINE_STATUS], 0, LCD_WIDTH);

    const char *mode = "MON";
    if (gCW_State == CW_SENDING)
        mode = "TX";
    else if (gCW_RxActive || gCW_RxToneState || gCW_RxSignalPrev)
        mode = "RX";

    char status[24];
    sprintf(status, "%u WPM  %s  %c", gCW_WPM, mode, gCW_UpperCase ? 'U' : 'L');
    UI_PrintStringSmallNormal(status, 2, 0, CW_LINE_STATUS);
}

void CW_Render(void)
{
    if (!gCW_ActiveState)
        return;

    switch (gCW_State)
    {
        case CW_COMPOSING:
        {
            gMonitor = true;

            const bool haveDecodedText = (gCW_DecodeText[0] != '\0');
            const bool haveActiveMorse = (gCW_RxMorseLen > 0);

            if (haveDecodedText || haveActiveMorse)
            {
                const char *textToRender = haveDecodedText ? gCW_DecodeText : gCW_RxMorse;
                const uint16_t len = (uint16_t)strlen(textToRender);
                memset(gFrameBuffer[CW_LINE_TX1], 0, LCD_WIDTH);
                memset(gFrameBuffer[CW_LINE_TX2], 0, LCD_WIDTH);

                UI_PrintStringSmallNormal("RX:", 2, 0, CW_LINE_TX1);
                if (len <= CW_CHARS_PER_TX_LINE)
                {
                    UI_PrintStringSmallNormal(textToRender, 28, 0, CW_LINE_TX1);
                }
                else
                {
                    char line1[CW_CHARS_PER_TX_LINE + 1];
                    memcpy(line1, textToRender, CW_CHARS_PER_TX_LINE);
                    line1[CW_CHARS_PER_TX_LINE] = '\0';
                    UI_PrintStringSmallNormal(line1, 28, 0, CW_LINE_TX1);

                    uint16_t remaining = len - CW_CHARS_PER_TX_LINE;
                    if (remaining > CW_CHARS_PER_TX_LINE)
                        remaining = CW_CHARS_PER_TX_LINE;
                    char line2[CW_CHARS_PER_TX_LINE + 1];
                    memcpy(line2, textToRender + CW_CHARS_PER_TX_LINE, remaining);
                    line2[remaining] = '\0';
                    UI_PrintStringSmallNormal(line2, 2, 0, CW_LINE_TX2);
                }
            }
            else
            {
                CW_DrawWrappedTxText();
            }

            // Draw timing diagram signal graph (clears the line itself)
            CW_DrawSignalGraph();
            CW_DrawStatusLine();
            break;
        }

        case CW_SENDING:
        {
            CW_DrawWrappedTxText();

            memset(gFrameBuffer[CW_LINE_DECODE], 0, LCD_WIDTH);
            CW_DrawStatusLine();
            break;
        }

        default:
            break;
    }

    gUpdateDisplay = true;
}

void CW_ProcessKeys(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld)
{
    if (!gCW_ActiveState)
        return;

    if (!bKeyPressed)
    {
        gCW_MenuLongHandled = false;
        gCW_Side2LongHandled = false;
        return;
    }

    if (Key != KEY_F)
        gWasFKeyPressed = false;

    if (bKeyHeld)
    {
        switch (Key)
        {
        case KEY_MENU:
                if (!gCW_MenuLongHandled)
                {
                    static const uint8_t wpmTable[] = {5, 10, 15, 20, 25, 30, 35, 40, 45, 50};
                    uint8_t idx;
                    for (idx = 0; idx < sizeof(wpmTable); idx++) {
                        if (wpmTable[idx] == gCW_WPM)
                            break;
                    }
                    idx = (idx + 1) % sizeof(wpmTable);
                    gCW_WPM = wpmTable[idx];
                    CW_UpdateTiming();
                    gCW_PrevKey = 0;
                    gCW_PrevLetter = 0;
                    CW_Render();
                    AUDIO_PlayBeep(BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL);
                    gCW_MenuLongHandled = true;
                }
                return;

            case KEY_SIDE2:
                if (!gCW_Side2LongHandled)
                {
                    gCW_CursorPos = 0;
                    gCW_Message[0] = '\0';
                    gCW_PrevKey = 0;
                    gCW_PrevLetter = 0;
                    CW_Render();
                    AUDIO_PlayBeep(BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL);
                    gCW_Side2LongHandled = true;
                }
                return;

            case KEY_EXIT:
                CW_Stop();
                gRequestDisplayScreen = DISPLAY_MAIN;
                return;

            case KEY_PTT:
                return;

            default:
                break;
        }
        return;
    }

    switch (Key)
    {
        case KEY_1:
        case KEY_2:
        case KEY_3:
        case KEY_4:
        case KEY_5:
        case KEY_6:
        case KEY_7:
        case KEY_8:
        case KEY_9:
        {
            uint8_t idx = (uint8_t)(Key - KEY_1);
            uint8_t count = CW_KEY_CHAR_COUNT[idx];

            if (count == 0)
                break;

            if (gCW_PrevKey == Key && gCW_CursorPos > 0)
            {
                uint8_t next = gCW_PrevLetter;
                bool found = false;
                for (uint8_t n = 0; n < count; n++)
                {
                    next = (uint8_t)((next + 1) % count);
                    if (CW_IsAllowedInputChar(CW_GetInputChar(idx, next)))
                    {
                        found = true;
                        break;
                    }
                }
                if (found)
                {
                    gCW_PrevLetter = next;
                    gCW_Message[gCW_CursorPos - 1] = CW_GetInputChar(idx, gCW_PrevLetter);
                }
                else
                {
                    AUDIO_PlayBeep(BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL);
                }
            }
            else
            {
                bool found = false;
                for (uint8_t n = 0; n < count; n++)
                {
                    char c = CW_GetInputChar(idx, n);
                    if (CW_IsAllowedInputChar(c))
                    {
                        gCW_PrevKey = Key;
                        gCW_PrevLetter = n;
                        CW_AppendChar(c);
                        found = true;
                        break;
                    }
                }
                if (!found)
                    AUDIO_PlayBeep(BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL);
            }

            gCW_KeyTick = 1;
            CW_Render();
            break;
        }

        case KEY_0:
        {
            if (gCW_PrevKey == KEY_0 && gCW_CursorPos > 0 && gCW_Message[gCW_CursorPos - 1] == ' ')
            {
                gCW_CursorPos--;
                gCW_Message[gCW_CursorPos] = '\0';
                CW_AppendChar('0');
                gCW_PrevKey = KEY_0;
            }
            else
            {
                CW_AppendChar(' ');
                gCW_PrevKey = KEY_0;
                gCW_PrevLetter = 0;
            }
            gCW_KeyTick = 1;
            CW_Render();
            break;
        }

        case KEY_UP:
            gCW_PrevKey = 0;
            gCW_PrevLetter = 0;
            gCW_KeyTick = 0;
            CW_DeleteChar();
            CW_Render();
            break;

        case KEY_DOWN:
            gCW_PrevKey = 0;
            gCW_PrevLetter = 0;
            gCW_KeyTick = 0;
            CW_AppendChar(' ');
            CW_Render();
            break;

        case KEY_SIDE2:
            gCW_UpperCase = !gCW_UpperCase;
            CW_Render();
            AUDIO_PlayBeep(BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL);
            break;

        case KEY_STAR:
            gCW_UpperCase = !gCW_UpperCase;
            CW_Render();
            AUDIO_PlayBeep(BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL);
            break;

        case KEY_MENU:
            gRequestDisplayScreen = DISPLAY_MENU;
            break;

        case KEY_SIDE1:
            gCW_CursorPos = 0;
            gCW_Message[0] = '\0';
            gCW_PrevKey = 0;
            gCW_PrevLetter = 0;
            CW_Render();
            AUDIO_PlayBeep(BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL);
            break;

        case KEY_F:
            if (!bKeyHeld)
            {
                gCW_CursorPos = 0;
                gCW_Message[0] = '\0';
                gCW_PrevKey = 0;
                gCW_PrevLetter = 0;
                CW_Render();
            }
            break;

        case KEY_EXIT:
            CW_Stop();
            gRequestDisplayScreen = DISPLAY_MAIN;
            break;

        case KEY_PTT:
            if (!bKeyHeld)
            {
                if (gCW_CursorPos > 0)
                {
                    gCW_PrevKey = 0;
                    gCW_PrevLetter = 0;
                    CW_SendMessage();
                    CW_Render();
                }
            }
            break;

        default:
            break;
    }
}

bool CW_IsActive(void)
{
    return gCW_ActiveState;
}

void APP_RunCW(void)
{
    if (gCW_ActiveState)
        CW_Stop();
    else
        CW_Start();
}

void CW_Overlay(void)
{
    CW_Render();
}
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

#define CW_ALNUM_ONLY 1

#if !CW_ALNUM_ONLY
typedef struct {
    const char *token;
    const char *morse;
} CW_ProsignMap_t;
#endif

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

#if !CW_ALNUM_ONLY
static const CW_ProsignMap_t CW_PROSIGN_MAP[] = {
    {"AR",  ".-.-."},
    {"BT",  "-...-"},
    {"SK",  "...-.-"},
    {"KN",  "-.--."},
    {"AS",  ".-..."},
    {"SN",  "...-."},
    {"SOS", "...---..."}
};
#endif

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

#define CW_RX_MORSE_MAX_LEN 10
static char     gCW_DecodeText[CW_MSG_MAX_LEN + 1] = {0};
static uint8_t  gCW_DecodeCursor = 0;
static char     gCW_RxMorse[CW_RX_MORSE_MAX_LEN + 1] = {0};
static uint8_t  gCW_RxMorseLen = 0;
static uint16_t gCW_RxMarkTicks = 0;
static uint16_t gCW_RxSpaceTicks = 0;
static bool     gCW_RxSignalPrev = false;
static uint16_t gCW_RxDitTicks = 0;
static bool     gCW_RxToneState = false;
static uint8_t  gCW_RxToneOnDebounce = 0;
static uint8_t  gCW_RxToneOffDebounce = 0;

static uint16_t gCW_DitMs = 60;
static uint16_t gCW_DahMs = 180;
static uint16_t gCW_InterElemMs = 60;
static uint16_t gCW_InterCharMs = 180;
static uint16_t gCW_InterWordMs = 420;

#define CW_OFFSET_HYSTERESIS 5
#define CW_RX_DEBOUNCE_TICKS   1
#define CW_MULTI_TAP_TIMEOUT_TICKS 80

static void CW_UpdateTiming(void)
{
    gCW_DitMs = 1200 / gCW_WPM;
    if (gCW_DitMs < 20)  gCW_DitMs = 20;
    if (gCW_DitMs > 120) gCW_DitMs = 120;

    gCW_DahMs = gCW_DitMs * 3;
    gCW_InterElemMs = gCW_DitMs;
    gCW_InterCharMs = gCW_DitMs * 3;
    gCW_InterWordMs = gCW_DitMs * 7;
}

static bool CW_IsAllowedInputChar(char c)
{
#if CW_ALNUM_ONLY
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ')
        return true;
    return false;
#else
    (void)c;
    return true;
#endif
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
            memmove(gCW_DecodeText, gCW_DecodeText + 1, CW_MSG_MAX_LEN - 1);
            gCW_DecodeCursor = CW_MSG_MAX_LEN - 1;
        }

        gCW_DecodeText[gCW_DecodeCursor++] = text[i];
        gCW_DecodeText[gCW_DecodeCursor] = '\0';
    }

    static uint8_t updateDelay = 0;
    if (updateDelay == 0) {
        gUpdateDisplay = true;
        updateDelay = 3;
    }
    if (updateDelay > 0)
        updateDelay--;
}

/* Returns pointer to static buffer; copy before next call */
static const char *CW_MorseToDecodedToken(const char *morse)
{
    static char oneChar[2];

    if (morse == NULL || morse[0] == '\0')
        return " ";

#if !CW_ALNUM_ONLY
    if (strcmp(morse, ".-.-.") == 0)     return "<AR>";
    if (strcmp(morse, "-...-") == 0)     return "<BT>";
    if (strcmp(morse, "-.--.") == 0)     return "<KN>";
    if (strcmp(morse, ".-...") == 0)     return "<AS>";
    if (strcmp(morse, "...-.-") == 0)    return "<SK>";
    if (strcmp(morse, "...-.") == 0)     return "<SN>";
    if (strcmp(morse, "...---...") == 0) return "<SOS>";
#endif

    for (uint8_t i = 0; i < ARRAY_SIZE(CW_CHAR_MAP); i++)
    {
        if (strcmp(morse, CW_CHAR_MAP[i].morse) == 0)
        {
#if CW_ALNUM_ONLY
            if (!((CW_CHAR_MAP[i].ch >= 'A' && CW_CHAR_MAP[i].ch <= 'Z')
                        || (CW_CHAR_MAP[i].ch >= '0' && CW_CHAR_MAP[i].ch <= '9')))
                return "?";
#endif
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
    CW_AppendDecodedText(CW_MorseToDecodedToken(gCW_RxMorse));
    gCW_RxMorseLen = 0;

    gUpdateDisplay = true;
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
    gCW_RxDitTicks = (uint16_t)MAX(1, (int)((gCW_DitMs + 5) / 10));
    gCW_RxToneState = false;
    gCW_RxToneOnDebounce = 0;
    gCW_RxToneOffDebounce = 0;
}

static int16_t gCW_RxLastRssi = -120;
static int16_t gCW_PeakRssi = -120;

static bool CW_IsRxTonePresent(void)
{
    int16_t rssi_dBm =
        BK4819_GetRSSI_dBm()
#ifdef ENABLE_AM_FIX
        + ((gSetting_AM_fix && gRxVfo->Modulation == MODULATION_AM) ? AM_fix_get_gain_diff() : 0)
#endif
        + dBmCorrTable[gRxVfo->Band];

    // A real CW signal exists only when the squelch has actually opened.
    // Monitor mode keeps the audio path open for listening, but we must NOT
    // decode background noise as Morse (this caused random letters at startup
    // because the dynamic threshold tracked the idle RSSI as the peak).
    if (!g_SquelchLost)
    {
        gCW_RxToneOnDebounce = 0;
        gCW_RxToneOffDebounce = 0;
        gCW_RxToneState = false;
        return false;
    }

    if (rssi_dBm > gCW_PeakRssi)
        gCW_PeakRssi = rssi_dBm;
    else if (rssi_dBm < (gCW_PeakRssi - 10))
        gCW_PeakRssi = (int16_t)(gCW_PeakRssi - 1);
    if (gCW_PeakRssi < -110)
        gCW_PeakRssi = -110;

    const int16_t openLevel  = (int16_t)(gCW_PeakRssi - 1);
    const int16_t closeLevel = (int16_t)(gCW_PeakRssi - CW_OFFSET_HYSTERESIS);

    bool signalStrong;
    if (!gCW_RxToneState)
        signalStrong = (rssi_dBm >= openLevel);
    else
        signalStrong = (rssi_dBm >= closeLevel);

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

    gCW_RxDitTicks = (uint16_t)((3 * gCW_RxDitTicks + observedMarkTicks + 1) / 4);

    if (gCW_RxDitTicks < 1)
        gCW_RxDitTicks = 1;
    if (gCW_RxDitTicks > 40)
        gCW_RxDitTicks = 40;
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
    BK4819_EnterTxMute();
    // BK4819_TurnsOffTones_TurnsOnRX() calls ExitTxMute() and sets up RX registers
    // This provides a clean RX transition without PA carrier pop
    BK4819_TurnsOffTones_TurnsOnRX();
    BK4819_ExitSubAu();
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
        c = c - 'a' + 'A';

    for (uint8_t i = 0; i < ARRAY_SIZE(CW_CHAR_MAP); i++)
    {
        if (CW_CHAR_MAP[i].ch == c)
        {
#if CW_ALNUM_ONLY
            if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')))
                return NULL;
#endif
            return CW_CHAR_MAP[i].morse;
        }
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
        {
    #if CW_ALNUM_ONLY
            if (!((CW_CHAR_MAP[i].ch >= 'A' && CW_CHAR_MAP[i].ch <= 'Z')
              || (CW_CHAR_MAP[i].ch >= '0' && CW_CHAR_MAP[i].ch <= '9')))
            return '?';
    #endif
            return (uint8_t)CW_CHAR_MAP[i].ch;
        }
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
            __attribute__((fallthrough));
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
    gCW_PeakRssi = -120;
}

void CW_TimeSlice10ms(void)
{
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

    const uint16_t ditTicks = gCW_RxDitTicks;
    const uint16_t dahThreshold = (uint16_t)(2 * ditTicks);
    const uint16_t charGapTicks = (uint16_t)(3 * ditTicks);
    const uint16_t wordGapTicks = (uint16_t)(7 * ditTicks);
    const uint16_t finalizeOnSignalGapTicks = charGapTicks;
    const uint16_t finalizeInSilenceGapTicks = charGapTicks;

    if (signalNow)
    {
        if (gCW_RxSignalPrev)
        {
            if (gCW_RxMarkTicks < 0xFFFE)
                gCW_RxMarkTicks++;
            return;
        }

        if (gCW_RxSpaceTicks >= wordGapTicks)
        {
            CW_FinalizeRxCharacter();
            if (gCW_DecodeCursor > 0 && gCW_DecodeText[gCW_DecodeCursor - 1] != ' ')
                CW_AppendDecodedText(" ");
        }
        else if (gCW_RxSpaceTicks >= finalizeOnSignalGapTicks)
        {
            CW_FinalizeRxCharacter();
        }

        gCW_RxSignalPrev = true;
        gCW_RxMarkTicks = 1;
        gCW_RxSpaceTicks = 0;
        return;
    }

    if (gCW_RxSignalPrev)
    {
        if (gCW_RxMarkTicks > 0 && gCW_RxMorseLen < CW_RX_MORSE_MAX_LEN)
        {
            const bool isDah = (gCW_RxMarkTicks >= dahThreshold);
            gCW_RxMorse[gCW_RxMorseLen++] = isDah ? '-' : '.';
            gCW_RxMorse[gCW_RxMorseLen] = '\0';
            if (!isDah)
                CW_UpdateRxDitEstimate(gCW_RxMarkTicks);
            else if (gCW_RxMarkTicks >= 3)
                CW_UpdateRxDitEstimate((uint16_t)((gCW_RxMarkTicks + 1) / 3));
        }

        gCW_RxSignalPrev = false;
        gCW_RxMarkTicks = 0;
        gCW_RxSpaceTicks = 1;
        return;
    }

    if (gCW_RxSpaceTicks < 0xFFFE)
        gCW_RxSpaceTicks++;

    if (gCW_RxMorseLen > 0 && gCW_RxSpaceTicks >= finalizeInSilenceGapTicks)
        CW_FinalizeRxCharacter();

    if (gCW_RxSpaceTicks > wordGapTicks * 2 && gCW_RxMorseLen > 0)
    {
        CW_FinalizeRxCharacter();
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

    // Disable DTMF/MDC for the entire CW session to prevent any digital noise
    BK4819_DisableDTMF();

    // Open monitor path AFTER radio register setup so audio is not reset.
    gMonitor = true;
    APP_StartListening(FUNCTION_MONITOR);

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

void CW_Render(void)
{
    if (!gCW_ActiveState)
        return;

    switch (gCW_State)
    {
        case CW_COMPOSING:
        {
            gMonitor = true;

            CW_DrawWrappedTxText();

            memset(gFrameBuffer[CW_LINE_DECODE], 0, LCD_WIDTH);
            UI_PrintStringSmallNormal("RX:", 2, 0, CW_LINE_DECODE);

            if (gCW_RxMorseLen > 0)
            {
                char morseBuf[CW_RX_MORSE_MAX_LEN + 1];
                uint8_t displayLen = gCW_RxMorseLen;
                if (displayLen > 5)
                    displayLen = 5;
                for (uint8_t i = 0; i < displayLen; i++)
                    morseBuf[i] = (gCW_RxMorse[i] == '.') ? '.' : '-';
                morseBuf[displayLen] = '\0';
                UI_PrintStringSmallNormal(morseBuf, 18, 0, CW_LINE_DECODE);
            }

            {
                char displayText[11];
                const size_t fullLen = strlen(gCW_DecodeText);
                size_t len = fullLen;
                if (len > 10)
                    len = 10;
                const char *tail = gCW_DecodeText + (fullLen - len);
                memcpy(displayText, tail, len);
                displayText[len] = '\0';
                UI_PrintStringSmallNormal(displayText, 52, 0, CW_LINE_DECODE);
            }

            memset(gFrameBuffer[CW_LINE_STATUS], 0, LCD_WIDTH);
            {
                char status[32];
                sprintf(status, "CW %3uw%2uR%3d",
                        gCW_WPM,
                        (unsigned)gCW_RxDitTicks,
                        (int)gCW_RxLastRssi);
                UI_PrintStringSmallNormal(status, 2, 0, CW_LINE_STATUS);
            }

            break;
        }

        case CW_SENDING:
        {
            CW_DrawWrappedTxText();

            memset(gFrameBuffer[CW_LINE_DECODE], 0, LCD_WIDTH);

            memset(gFrameBuffer[CW_LINE_STATUS], 0, LCD_WIDTH);
            {
                char status[20];
                sprintf(status, "CW TX %2u%c", gCW_WPM, gCW_UpperCase ? 'U' : 'L');
                UI_PrintStringSmallNormal(status, 2, 0, CW_LINE_STATUS);
            }
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
        if (Key == KEY_MENU && !gCW_MenuLongHandled)
        {
            CW_Stop();
            gRequestDisplayScreen = DISPLAY_MENU;
            return;
        }
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
                    // Long press: clear the message
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

            // SAFETY GUARD: Prevent division-by-zero hard fault
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
                // Toggle space <-> '0'
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
            // Toggle case
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
            break;

        case KEY_SIDE1:
            // Clear message
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
                    CW_SendMessage(); // State moves to CW_SENDING inside CW_SendMessage
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
/* Copyright 2026 Sean, N7SIX ApeX-Edition Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
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

// Temporary simplification mode: only A-Z and 0-9 are encoded/decoded.
// Prosigns and punctuation are intentionally disabled for now.
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

static uint16_t gCW_TX_Index = 0;

#define CW_RX_MORSE_MAX_LEN 8
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
static uint8_t  gCW_RxNoiseFloor = 0;
static uint8_t  gCW_RxNoiseEma = 0;

static uint16_t gCW_DitMs = 60;
static uint16_t gCW_DahMs = 180;
static uint16_t gCW_InterElemMs = 60;
static uint16_t gCW_InterCharMs = 180;
static uint16_t gCW_InterWordMs = 420;

int16_t gCW_RxThreshold = CW_RX_RSSI_THRESHOLD_DEFAULT;

// RX quality heuristics: reject implausible element durations
#define CW_RX_MARK_MAX_TICKS 400  // >4s mark = noise, not CW
#define CW_RX_MIN_DIT_TICKS   2   // <20ms = noise pulse
#define CW_RX_ELEMENT_RATIO_LIMIT 10  // dah/dit ratio >10x = not CW

// Hysteresis for noise immunity (1 RSSI unit ~= 0.5 dB)
#define CW_RX_OPEN_HYST_RSSI    4   // +2 dB above threshold to confirm signal
#define CW_RX_CLOSE_HYST_RSSI   4   // -2 dB below threshold to confirm silence
#define CW_RX_DEBOUNCE_TICKS    2   // 20ms at 10ms tick

// Modulation-aware gating (for cases where RSSI stays high but AF toggles)
#define CW_RX_NOISE_ON_DELTA_DEFAULT 4  // balance lock speed and false-trigger rejection
#define CW_RX_NOISE_ON_DELTA_MIN     2
#define CW_RX_NOISE_ON_DELTA_MAX    20

// RSSI fallback: how many 10ms ticks to wait before treating RSSI as valid signal
// Original 10 = 100ms was too long for CW (missed entire characters)
#define CW_RX_RSSI_FALLBACK_TICKS 3   // 30ms

#define CW_MULTI_TAP_TIMEOUT_TICKS 80  // 800 ms at 10 ms scheduler tick

static uint8_t gCW_RxNoiseGateOn = CW_RX_NOISE_ON_DELTA_DEFAULT;
static uint16_t gCW_RxRssiFallbackTicks = 0;

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

    // Throttle display updates to avoid flooding the UI thread when
    // noise triggers spurious dits. Only update at most every 40ms.
    static uint8_t updateDelay = 0;
    if (updateDelay == 0) {
        gUpdateDisplay = true;
        updateDelay = 3;
    }
    if (updateDelay > 0)
        updateDelay--;
}

static const char *CW_MorseToDecodedToken(const char *morse)
{
    static char oneChar[2];

    if (morse == NULL || morse[0] == '\0')
        return " ";

#if !CW_ALNUM_ONLY
    // Prosign precedence is explicit because several prosigns share the
    // same Morse pattern as punctuation characters.
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

    // Force display update immediately so decoded RX text is visible without
    // requiring a key press. CW_AppendDecodedText() already throttles
    // gUpdateDisplay to ~40ms, but that flag may not reliably reach the
    // display refresh path in all radio function states. Setting it here
    // bypasses the throttle when a character boundary is crossed.
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
    gCW_RxNoiseFloor = 0;
    gCW_RxNoiseEma = 0;
    gCW_RxRssiFallbackTicks = 0;
}

static bool CW_IsRxTonePresent(void)
{
    // Use RSSI level comparison with hysteresis, inspired by the spectrum
    // analyzer's proven listen-state machine (spectrum.c).
    // Two thresholds prevent chatter when RSSI hovers near the noise floor:
    //   - Entry: need +2 dB above threshold to turn ON
    //   - Exit:  need -2 dB below threshold to turn OFF
    // This rejects noise spikes while still catching real CW edges.
    int16_t rssi_dBm =
        BK4819_GetRSSI_dBm()
#ifdef ENABLE_AM_FIX
        + ((gSetting_AM_fix && gRxVfo->Modulation == MODULATION_AM) ? AM_fix_get_gain_diff() : 0)
#endif
        + dBmCorrTable[gRxVfo->Band];

    // Hysteresis: different entry/exit thresholds
    const int16_t openLevel   = gCW_RxThreshold + CW_RX_OPEN_HYST_RSSI;   // +2 dB
    const int16_t closeLevel  = gCW_RxThreshold - CW_RX_CLOSE_HYST_RSSI;  // -2 dB
    const uint8_t debounceTicks = CW_RX_DEBOUNCE_TICKS;

    bool signalStrongRssi;
    if (!gCW_RxToneState)
        signalStrongRssi = (rssi_dBm >= openLevel);    // harder to turn ON
    else
        signalStrongRssi = (rssi_dBm >= closeLevel);   // easier to stay ON

    // CW mode opens monitor path so decode can continue even when RSSI/squelch
    // gate would otherwise flicker closed.
    const bool rxAudioOpen = g_SquelchLost || gMonitor;

    // No open RX audio path means no valid signal to decode. Force tone state
    // OFF to suppress idle ghost characters.
    if (!rxAudioOpen)
    {
        gCW_RxToneOnDebounce = 0;
        gCW_RxToneOffDebounce = 0;
        gCW_RxToneState = false;
        return false;
    }

    // When squelch is already open, detect CW elements from modulation activity
    // rather than carrier level. This avoids a stuck-ON tone state when RSSI is
    // flat but the AF tone is keying on/off.
    const uint8_t exNoise = BK4819_GetExNoiceIndicator();
    if (gCW_RxNoiseEma == 0)
        gCW_RxNoiseEma = exNoise;
    else
        gCW_RxNoiseEma = (uint8_t)((3 * gCW_RxNoiseEma + exNoise + 1) / 4);

    if (gCW_RxNoiseFloor == 0)
        gCW_RxNoiseFloor = gCW_RxNoiseEma;
    else
    {
        if (gCW_RxNoiseEma > gCW_RxNoiseFloor)
            gCW_RxNoiseFloor = gCW_RxNoiseEma;
        else if (gCW_RxNoiseFloor > 1)
            gCW_RxNoiseFloor--;
    }

    uint8_t noiseDelta = 0;
    if (gCW_RxNoiseEma < gCW_RxNoiseFloor)
        noiseDelta = (uint8_t)(gCW_RxNoiseFloor - gCW_RxNoiseEma);

    // For CW detection: a clean CW tone reduces the ExNoiceIndicator reading,
    // so signal is present when noiseDelta is BELOW the threshold.
    // Hysteresis: when already in signal state, allow noise to rise slightly
    // before declaring the tone ended (noiseGateOff > noiseGateOn).
    uint8_t noiseGateOff = (uint8_t)MIN(0xFE, gCW_RxNoiseGateOn + 3);
    const uint8_t noiseGate = gCW_RxToneState ? noiseGateOff : gCW_RxNoiseGateOn;
    const bool signalStrongNoise = (noiseDelta < noiseGate);

    // RSSI fallback: if noise-based detection has not triggered for 30ms,
    // fall back to RSSI-based detection to catch CW tones that may not
    // produce a measurable noise delta.
    bool signalStrong;
    if (signalStrongNoise) {
        signalStrong = true;
        gCW_RxRssiFallbackTicks = 0;
    } else if (gCW_RxToneState && signalStrongRssi) {
        signalStrong = true;
        gCW_RxRssiFallbackTicks = 0;
    } else if (signalStrongRssi) {
        if (gCW_RxRssiFallbackTicks < 0xFE)
            gCW_RxRssiFallbackTicks++;
        signalStrong = (gCW_RxRssiFallbackTicks >= CW_RX_RSSI_FALLBACK_TICKS);
    } else {
        gCW_RxRssiFallbackTicks = 0;
        signalStrong = false;
    }

    if (signalStrong)
    {
        if (gCW_RxToneOnDebounce < 0xFF)
            gCW_RxToneOnDebounce++;
        gCW_RxToneOffDebounce = 0;

        if (!gCW_RxToneState && gCW_RxToneOnDebounce >= debounceTicks)
            gCW_RxToneState = true;

        return gCW_RxToneState;
    }

    if (gCW_RxToneOffDebounce < 0xFF)
        gCW_RxToneOffDebounce++;
    gCW_RxToneOnDebounce = 0;

    if (gCW_RxToneState && gCW_RxToneOffDebounce >= debounceTicks)
        gCW_RxToneState = false;

    return gCW_RxToneState;
}

static void CW_UpdateRxDitEstimate(uint16_t observedMarkTicks)
{
    if (observedMarkTicks == 0)
        return;

    // Ignore ultra-short marks so timing does not collapse and force
    // premature character boundaries.
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

static uint8_t CW_ParseTxProsignToken(const char *text, uint8_t start, uint8_t textLen, const char **morse)
{
#if CW_ALNUM_ONLY
    (void)text;
    (void)start;
    (void)textLen;
    (void)morse;
    return 0;
#else
    if (text == NULL || morse == NULL)
        return 0;

    if (start >= textLen || text[start] != '<')
        return 0;

    uint8_t end = start + 1;
    while (end < textLen && text[end] != '>')
        end++;

    if (end >= textLen || text[end] != '>')
        return 0;

    const uint8_t tokenLen = (uint8_t)(end - (start + 1));
    if (tokenLen == 0 || tokenLen > 6)
        return 0;

    char token[7];
    for (uint8_t i = 0; i < tokenLen; i++)
    {
        char c = text[start + 1 + i];
        if (c >= 'a' && c <= 'z')
            c = (char)(c - 'a' + 'A');
        token[i] = c;
    }
    token[tokenLen] = '\0';

    for (uint8_t i = 0; i < ARRAY_SIZE(CW_PROSIGN_MAP); i++)
    {
        if (strcmp(token, CW_PROSIGN_MAP[i].token) == 0)
        {
            *morse = CW_PROSIGN_MAP[i].morse;
            return (uint8_t)(end - start + 1);
        }
    }

    return 0;
#endif
}

static void CW_ToneOn(void)
{
    // Enable the tone transmitter and set AF path to BEEP so sidetone is audible.
    // BK4819_TransmitTone() configures the tone generator; BK4819_PlayToneRaw()
    // writes the frequency and handles the timed duration.
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
    BK4819_EnterTxMute();
    BK4819_ExitSubAu();
    RADIO_SetupRegisters(false);
    FUNCTION_Select(FUNCTION_FOREGROUND);
    gFlagEndTransmission = false;
    gRTTECountdown_10ms = 0;
}

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
        // A word gap must total 7 dit units. Inter-character gap (3 dit units)
        // was already inserted by the previous character, so add the remaining 4.
        // gCW_InterWordMs (7×dit) is always > gCW_InterCharMs (3×dit).
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

    #if !CW_ALNUM_ONLY
    if (strcmp(morse, ".-.-.") == 0) return '+';
    if (strcmp(morse, "-...-") == 0) return '=';
    if (strcmp(morse, "...---...") == 0) return '#';
    #endif

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

void CW_SendMessage(void)
{
    if (strlen(gCW_Message) == 0)
        return;

    if (!CW_BeginDedicatedTx())
        return;

    SYSTEM_DelayMs(50);

    bool sentAny = false;
    bool prevWasSpace = false;

    for (uint8_t i = 0; i < gCW_CursorPos && gCW_ActiveState; )
    {
        const char *morse = NULL;
        uint8_t consumed = CW_ParseTxProsignToken(gCW_Message, i, gCW_CursorPos, &morse);

        if (consumed == 0)
        {
            char txChar = gCW_Message[i];
            if (txChar >= 'a' && txChar <= 'z')
                txChar = (char)(txChar - 'a' + 'A');
            morse = CW_CharToMorse(txChar);
            consumed = 1;
        }

        if (morse == NULL)
        {
            i += consumed;
            continue;
        }

        // Normalize spaces: skip leading/repeated/trailing spaces.
        if (morse[0] == '\0')
        {
            const bool trailing = (uint8_t)(i + consumed) >= gCW_CursorPos;
            if (!sentAny || prevWasSpace || trailing)
            {
                i += consumed;
                continue;
            }
            prevWasSpace = true;
        }
        else
        {
            sentAny = true;
            prevWasSpace = false;
        }

        gCW_TX_Index = i + consumed;
        CW_PlayCharacter(morse);
        CW_Render();

        i += consumed;
    }

    gCW_TX_Index = 0;
    CW_EndDedicatedTx();

    // Return to receive mode after TX
    APP_StartListening(gMonitor ? FUNCTION_MONITOR : FUNCTION_RECEIVE);
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
    gCW_RxNoiseGateOn = CW_RX_NOISE_ON_DELTA_DEFAULT;

    CW_ResetRxDecoder(true);
    CW_UpdateTiming();
}

void CW_TimeSlice10ms(void)
{
    if (!gCW_ActiveState || gCW_State == CW_SENDING)
        return;

    // Auto-commit multi-tap selection after inactivity so repeated key presses
    // after a pause start a new character instead of cycling the old one.
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
    // Guard against false E/T commits: require extra silence before
    // finalizing one-element buffers only in no-signal idle path.
    // When a new signal edge arrives, finalize at normal char gap so
    // one-element characters are not merged into the next symbol.
    const uint16_t singleElementGapTicks = (uint16_t)(5 * ditTicks);
    const uint16_t finalizeOnSignalGapTicks = charGapTicks;
    const uint16_t finalizeInSilenceGapTicks = (gCW_RxMorseLen <= 1)
                                             ? singleElementGapTicks
                                             : charGapTicks;
    const bool signalNow = CW_IsRxTonePresent();

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
        // Signal quality heuristics: reject implausible element durations
        if (gCW_RxMarkTicks > 0)
        {
            // Reject marks shorter than half the current dit estimate, but cap
            // the floor so faster valid CW is not discarded before adaptation
            // converges.
            uint16_t minMarkTicks = (uint16_t)MAX((int)CW_RX_MIN_DIT_TICKS,
                                                  (int)((ditTicks + 1) / 2));
            if (minMarkTicks > 4)
                minMarkTicks = 4;
            if (gCW_RxMarkTicks < minMarkTicks)
            {
                gCW_RxSignalPrev = false;
                gCW_RxMarkTicks = 0;
                gCW_RxSpaceTicks = 1;
                return;
            }

            // Reject marks longer than 4 seconds (not CW, likely carrier/voice)
            if (gCW_RxMarkTicks > CW_RX_MARK_MAX_TICKS)
            {
                // Reset the entire decoder; this is not CW
                CW_ResetRxDecoder(false);
                gCW_RxSignalPrev = false;
                gCW_RxMarkTicks = 0;
                gCW_RxSpaceTicks = 1;
                return;
            }

            // Reject if element ratio exceeds limit (dah/dit > 10x = not CW)
            if (gCW_RxDitTicks > 0 && gCW_RxMarkTicks > gCW_RxDitTicks * CW_RX_ELEMENT_RATIO_LIMIT)
            {
                gCW_RxSignalPrev = false;
                gCW_RxMarkTicks = 0;
                gCW_RxSpaceTicks = 1;
                return;
            }

            if (gCW_RxMorseLen < CW_RX_MORSE_MAX_LEN)
            {
                const bool isDah = (gCW_RxMarkTicks >= dahThreshold);
                gCW_RxMorse[gCW_RxMorseLen++] = isDah ? '-' : '.';
                gCW_RxMorse[gCW_RxMorseLen] = '\0';
                // Only update the dit estimate from actual dits. Dahs are 3x longer
                // and would skew the adaptive timing if included in the EMA.
                if (!isDah)
                    CW_UpdateRxDitEstimate(gCW_RxMarkTicks);
                else if (gCW_RxMarkTicks >= 3)
                    CW_UpdateRxDitEstimate((uint16_t)((gCW_RxMarkTicks + 1) / 3));
            }
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
    
    // Safety net: if space gap exceeds 2× word gap but character was not
    // finalized (e.g. due to edge-case timing), force finalization.
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

    // Enter dedicated CW receive mode: keep monitor path open for consistent
    // segmentation and decoding.
    gMonitor = true;
    APP_StartListening(FUNCTION_MONITOR);

    // Force the radio hardware to lock exclusively onto the TX VFO frequency
    // (VFO A). This prevents the BK4819 from alternating between VFO A and B
    // during dual-watch-style operation, ensuring CW decode receives a
    // consistent signal from the selected VFO.
    gRxVfoIsActive = true;
    RADIO_SetupRegisters(true);

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
            CW_DrawWrappedTxText();

            memset(gFrameBuffer[CW_LINE_DECODE], 0, LCD_WIDTH);
            UI_PrintStringSmallNormal("RX:", 2, 0, CW_LINE_DECODE);

            // Suppress stale decoded text if no valid signal is present.
            // This prevents ghost characters from noise at startup or after
            // leaving CW mode.
            if (gCW_RxMorseLen == 0 && gCW_DecodeCursor > 0)
            {
                bool recentSignal = (gCW_RxMarkTicks > 0 || gCW_RxSpaceTicks < 100);
                if (!recentSignal && !gCW_RxToneState)
                {
                    CW_ResetRxDecoder(true);
                }
            }

            // Show real-time Morse elements (dots/dashes) as they are received.
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

            // Display newest decoded text (tail) on the right side so it does
            // not overwrite the live Morse preview region.
            {
                char displayText[11];
                const size_t fullLen = strlen(gCW_DecodeText);
                size_t len = fullLen;
                if (len > 10)
                    len = 10;
                const char *tail = gCW_DecodeText + (fullLen - len);
                memcpy(displayText, tail, len);
                displayText[len] = '\0';
                // Keep decoded text clear of the live morse preview region.
                UI_PrintStringSmallNormal(displayText, 52, 0, CW_LINE_DECODE);
            }

            memset(gFrameBuffer[CW_LINE_STATUS], 0, LCD_WIDTH);
            {
                char status[32];
                // Diagnostic: S=squelch, O=open monitor path, D=dit estimate,
                // M=Morse buffer len, G=noise gate, TX=WPM
                sprintf(status, "S%u O%u D%02u M%u G%u %2u%c",
                        g_SquelchLost ? 1u : 0u,
                        gMonitor ? 1u : 0u,
                        (unsigned)gCW_RxDitTicks,
                        (unsigned)gCW_RxMorseLen,
                        (unsigned)gCW_RxNoiseGateOn,
                        gCW_WPM,
                        gCW_UpperCase ? 'U' : 'L');
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
            if (gCW_CursorPos > 0)
            {
                const uint8_t gauge_start = 78;
                const uint8_t gauge_width = LCD_WIDTH - gauge_start - 2;
                uint8_t progress = (uint8_t)((uint16_t)gCW_TX_Index * gauge_width / gCW_CursorPos);
                for (uint8_t i = 0; i < progress && i < gauge_width; i++)
                {
                    gFrameBuffer[CW_LINE_STATUS][gauge_start + i] = 0x7F;
                }
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
                    if (gCW_WPM >= 50)
                        gCW_WPM = 10;
                    else
                        gCW_WPM += 5;
                    CW_UpdateTiming();
                    gCW_PrevKey = 0;
                    gCW_PrevLetter = 0;
                    CW_Render();
                    AUDIO_PlayBeep(BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL);
                    gCW_MenuLongHandled = true;
                }
                return;

            case KEY_SIDE2:
                // SIDE2 long: less sensitive (higher gate)
                if (!gCW_Side2LongHandled)
                {
                    if (gCW_RxNoiseGateOn < CW_RX_NOISE_ON_DELTA_MAX)
                    {
                        gCW_RxNoiseGateOn++;
                        CW_Render();
                        AUDIO_PlayBeep(BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL);
                    }
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
            // First press = space, second press = 0 (append both as new characters)
            static uint8_t zeroState = 0;
            const char c = (zeroState == 0) ? ' ' : '0';
            zeroState = !zeroState;

            gCW_PrevKey = 0;  // Reset multi-tap state so it doesn't affect next character
            CW_AppendChar(c);
            gCW_KeyTick = 1;
            CW_Render();
            break;
        }

        case KEY_UP:
            // B button = left arrow = backspace
            gCW_PrevKey = 0;
            gCW_PrevLetter = 0;
            gCW_KeyTick = 0;
            CW_DeleteChar();
            CW_Render();
            break;

        case KEY_DOWN:
            // C button = proceed to next character entry (multi-tap commit).
            gCW_PrevKey = 0;
            gCW_PrevLetter = 0;
            gCW_KeyTick = 0;
            CW_Render();
            break;

        case KEY_SIDE2:
            // SIDE2 short: more sensitive (lower gate)
            if (gCW_RxNoiseGateOn > CW_RX_NOISE_ON_DELTA_MIN)
            {
                gCW_RxNoiseGateOn--;
                CW_Render();
                AUDIO_PlayBeep(BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL);
            }
            break;

        case KEY_STAR:
            gCW_UpperCase = !gCW_UpperCase;
            CW_Render();
            AUDIO_PlayBeep(BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL);
            break;

        case KEY_MENU:
            if (!bKeyHeld)
            {
                // MENU short: exit CW and open normal menu screen.
                CW_Stop();
                gRequestDisplayScreen = DISPLAY_MENU;
                return;
            }
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
                gCW_State = CW_SENDING;
                gCW_PrevKey = 0;
                gCW_PrevLetter = 0;
                CW_Render();
                CW_SendMessage();
                gCW_State = CW_COMPOSING;
                CW_Render();
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
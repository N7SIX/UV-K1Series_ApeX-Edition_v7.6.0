/* Copyright 2026 Sean, N7SIX
 * https://github.com/N7SIX
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "app/cw.h"
#include "app/cwdecoder.h"
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

// ---- Global/shared timing ----
uint16_t gCW_DitMs = 60;
uint16_t gCW_DahMs = 180;
uint16_t gCW_InterElemMs = 60;
uint16_t gCW_InterCharMs = 180;
uint16_t gCW_InterWordMs = 420;

// ---- Morse maps ----
const CW_CharMap_t CW_CHAR_MAP[] = {
    {'A', ".-"}, {'B', "-..."}, {'C', "-.-."}, {'D', "-.."},
    {'E', "."}, {'F', "..-."}, {'G', "--."}, {'H', "...."},
    {'I', ".."}, {'J', ".---"}, {'K', "-.-"}, {'L', ".-.."},
    {'M', "--"}, {'N', "-."}, {'O', "---"}, {'P', ".--."},
    {'Q', "--.-"}, {'R', ".-."}, {'S', "..."}, {'T', "-"},
    {'U', "..-"}, {'V', "...-"}, {'W', ".--"}, {'X', "-..-"},
    {'Y', "-.--"}, {'Z', "--.."},
    {'0', "-----"}, {'1', ".----"}, {'2', "..---"}, {'3', "...--"},
    {'4', "....-"}, {'5', "....."}, {'6', "-...."}, {'7', "--..."},
    {'8', "---.."}, {'9', "----."},
    {'.', ".-.-.-"}, {',', "--..--"}, {'?', "..--.."}, {'\'', ".----."},
    {'!', "-.-.--"}, {'/', "-..-."}, {'(', "-.--."}, {')', "-.--.-"},
    {'&', ".-..."}, {':', "---..."}, {';', "-.-.-."}, {'=', "-...-"},
    {'+', ".-.-."}, {'-', "-....-"}, {'_', "..--.-"}, {'"', ".-..-."},
    {'$', "...-..-"}, {'@', ".--.-."}
};
const uint8_t CW_CHAR_MAP_COUNT = sizeof(CW_CHAR_MAP) / sizeof(CW_CHAR_MAP[0]);

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
static const uint8_t CW_KEY_CHAR_COUNT[9] = {4,4,4,4,4,4,5,4,5};

// ---- App state ----
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
extern bool     gCW_PlaybackActive;

static bool     gCW_RxModeOverridden = false;
static uint8_t  gCW_BackupDualWatch = DUAL_WATCH_OFF;
static uint8_t  gCW_BackupCrossBand = CROSS_BAND_OFF;
static bool     gCW_BackupMonitor = false;
static bool     gCW_MonitorForced = false;
static uint8_t  gCW_BackupRogerMode = ROGER_MODE_OFF;
static bool     gCW_RogerModeBackedUp = false;

static void CW_UpdateTiming(void)
{
    gCW_DitMs = 1200 / gCW_WPM;
    if (gCW_DitMs < 20)  gCW_DitMs = 20;
    if (gCW_DitMs > 240) gCW_DitMs = 240;
    gCW_DahMs = gCW_DitMs * 3;
    gCW_InterElemMs = gCW_DitMs;
    gCW_InterCharMs = gCW_DitMs * 3;
    gCW_InterWordMs = gCW_DitMs * 7;
    CW_Decoder_Reset(false);
}

const char * CW_CharToMorse(char c)
{
    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    for (uint8_t i = 0; i < ARRAY_SIZE(CW_CHAR_MAP); i++)
        if (CW_CHAR_MAP[i].ch == c) return CW_CHAR_MAP[i].morse;
    if (c == ' ') return "";
    return NULL;
}

// ---- UI helpers ----
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
    if (remaining > CW_CHARS_PER_TX_LINE) remaining = CW_CHARS_PER_TX_LINE;
    char line2[CW_CHARS_PER_TX_LINE + 1];
    memcpy(line2, pRemain, remaining);
    line2[remaining] = '\0';
    UI_PrintStringSmallBold(line2, 2, 0, CW_LINE_TX2);
}

static void CW_DrawStatusLine(void)
{
    memset(gFrameBuffer[CW_LINE_STATUS], 0, LCD_WIDTH);
    const char *mode = "MON";
    if (gCW_State == CW_SENDING) mode = "TX";
    else if (CW_Decoder_IsRxActive() || CW_Decoder_IsToneActive()) mode = "RX";

    char status[32];
    sprintf(status, "%uWPM %s C%u", gCW_WPM, mode, CW_Decoder_GetCharConfidence());
    UI_PrintStringSmallNormal(status, 2, 0, CW_LINE_STATUS);
}

void CW_Render(void)
{
    if (!gCW_ActiveState) return;
    if (gScreenToDisplay != DISPLAY_MAIN) return;

    switch (gCW_State)
    {
        case CW_COMPOSING:
        {
            const bool haveDecodedText = (CW_Decoder_GetDecodedText()[0] != '\0');
            const char *textToRender = haveDecodedText ? CW_Decoder_GetDecodedText() : CW_Decoder_GetCurrentMorse();

            if (haveDecodedText || CW_Decoder_GetCurrentMorse()[0] != '\0')
            {
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
                    // Show the T A I L (most recently decoded characters)
                    // instead of the head (oldest). This makes the display
                    // "scroll" naturally: new characters appear on the right,
                    // old ones fall off the left.
                    uint16_t offset = 0;
                    const uint16_t displayLen = (uint16_t)CW_CHARS_PER_TX_LINE * 2;
                    if (len > displayLen)
                        offset = len - displayLen;

                    char line1[CW_CHARS_PER_TX_LINE + 1];
                    memcpy(line1, textToRender + offset, CW_CHARS_PER_TX_LINE);
                    line1[CW_CHARS_PER_TX_LINE] = '\0';
                    UI_PrintStringSmallNormal(line1, 28, 0, CW_LINE_TX1);

                    uint16_t remaining = len - offset - CW_CHARS_PER_TX_LINE;
                    if (remaining > CW_CHARS_PER_TX_LINE) remaining = CW_CHARS_PER_TX_LINE;
                    char line2[CW_CHARS_PER_TX_LINE + 1];
                    memcpy(line2, textToRender + offset + CW_CHARS_PER_TX_LINE, remaining);
                    line2[remaining] = '\0';
                    UI_PrintStringSmallNormal(line2, 2, 0, CW_LINE_TX2);
                }
            }
            else
            {
                CW_DrawWrappedTxText();
            }

            CW_Decoder_DrawSignalGraph();
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
        default: break;
    }
    gUpdateDisplay = true;

    // Immediate display refresh to ensure decoded text appears without
    // waiting for the main loop. This bypasses potential display update
    // latency from other 10ms tasks (scanning, DTMF, power save).
    ST7565_BlitLine(CW_LINE_TX1);
    ST7565_BlitLine(CW_LINE_TX2);
    ST7565_BlitLine(CW_LINE_DECODE);
    ST7565_BlitLine(CW_LINE_STATUS);
}

// ---- Lifecycle ----
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
    CW_Decoder_Init();
}

void CW_Start(void)
{
    if (gCW_ActiveState) return;

    gCW_BackupDualWatch = gEeprom.DUAL_WATCH;
    gCW_BackupCrossBand = gEeprom.CROSS_BAND_RX_TX;
    if (!gCW_RxModeOverridden)
    {
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

    if (!gCW_RogerModeBackedUp)
    {
        gCW_BackupRogerMode = gEeprom.ROGER;
        gCW_RogerModeBackedUp = true;
        gEeprom.ROGER = ROGER_MODE_OFF;
    }
    BK4819_DisableDTMF();
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
    if (!gCW_ActiveState) return;
    gCW_ActiveState = false;
    gCW_State = CW_IDLE;

    memset(gFrameBuffer[CW_LINE_TX1], 0, LCD_WIDTH);
    memset(gFrameBuffer[CW_LINE_TX2], 0, LCD_WIDTH);
    memset(gFrameBuffer[CW_LINE_DECODE], 0, LCD_WIDTH);
    memset(gFrameBuffer[CW_LINE_STATUS], 0, LCD_WIDTH);

    CW_Decoder_Reset(true);

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
    if (gCW_RogerModeBackedUp)
    {
        gEeprom.ROGER = gCW_BackupRogerMode;
        gCW_RogerModeBackedUp = false;
    }
    BK4819_DisableDTMF();
    gWasFKeyPressed = false;
    gUpdateDisplay = true;
}

void CW_Toggle(void)
{
    if (gCW_ActiveState) CW_Stop(); else CW_Start();
}

// ---- Main tick ----
static void CW_TxStateMachine(void);
void CW_TimeSlice10ms(void)
{
    if (!gCW_ActiveState) return;

    // Drive TX state machine during send.
    // Two mutually exclusive TX paths exist:
    //   gCW_PlaybackActive == true  -> typed-message TX via CW_TxStateMachine()
    //   gCW_PlaybackActive == false -> keyer/paddle TX via CW_AppUpdate()
    // Running both simultaneously would cause two independent FSMs to fight
    // for the RF transmitter, corrupting the output.
    if (gCW_State == CW_SENDING)
    {
        if (gCW_PlaybackActive)
        {
            CW_TxStateMachine();
        }
        #ifdef ENABLE_FEAT_N7SIX_CW
        else
        {
            CW_AppUpdate();
        }
        #endif
        return;
    }

    // RX/compose processing
    if (!gMonitor) { gMonitor = true; APP_StartListening(FUNCTION_MONITOR); }

    int16_t rssi_dBm =
        BK4819_GetRSSI_dBm()
#ifdef ENABLE_AM_FIX
        + ((gSetting_AM_fix && gRxVfo->Modulation == MODULATION_AM) ? AM_fix_get_gain_diff() : 0)
#endif
        + dBmCorrTable[gRxVfo->Band];

    CW_Decoder_ProcessTick(rssi_dBm);

    if (gCW_KeyTick > 0)
    {
        if (gCW_KeyTick < 0xFF) gCW_KeyTick++;
        if (gCW_KeyTick >= CW_KEY_TICK_RESET_MS)
        {
            gCW_KeyTick = 0; gCW_PrevKey = 0; gCW_PrevLetter = 0; CW_Render();
        }
    }
}

// ---- TX ----
static void CW_ToneOn(void) { BK4819_TransmitTone(true, gCW_ToneFreq); }
static void CW_ToneOff(void) { BK4819_EnterTxMute(); }

static bool CW_BeginDedicatedTx(void)
{
    VfoState_t state = VFO_STATE_NORMAL;
    if (gCurrentVfo == NULL) RADIO_SelectVfos();
    if (TX_freq_check(gCurrentVfo->pTX->Frequency) != 0 && gCurrentVfo->TX_LOCK) state = VFO_STATE_TX_DISABLE;
    else if (SerialConfigInProgress()) state = VFO_STATE_TX_DISABLE;
    else if (gCurrentVfo->BUSY_CHANNEL_LOCK && gCurrentFunction == FUNCTION_RECEIVE) state = VFO_STATE_BUSY;
    else if (gBatteryDisplayLevel == 0) state = VFO_STATE_BAT_LOW;
    else if (gBatteryDisplayLevel > 6) state = VFO_STATE_VOLTAGE_HIGH;
#ifdef ENABLE_BYP_RAW_DEMODULATORS
    else if (gCurrentVfo->Modulation == MODULATION_BYP || gCurrentVfo->Modulation == MODULATION_RAW) state = VFO_STATE_TX_DISABLE;
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
    BK4819_TurnsOffTones_TurnsOnRX();
    // Return to CW monitor mode after TX completes so the receiver stays
    // active for CW decoding. Without this, the radio would remain in
    // FOREGROUND mode while the status falsely shows "MON".
    APP_StartListening(FUNCTION_MONITOR);
    gFlagEndTransmission = false;
    gRTTECountdown_10ms = 0;
    RADIO_SetVfoState(VFO_STATE_NORMAL);
    RADIO_SelectVfos();
    gUpdateDisplay = true;
}

void CW_PlayDit(void) { CW_ToneOn(); BK4819_PlayToneRaw(gCW_ToneFreq, gCW_DitMs); CW_ToneOff(); }
void CW_PlayDah(void) { CW_ToneOn(); BK4819_PlayToneRaw(gCW_ToneFreq, gCW_DahMs); CW_ToneOff(); }
void CW_PlayCharacter(const char *morse)
{
    if (morse == NULL || morse[0] == '\0') { SYSTEM_DelayMs(gCW_InterWordMs - gCW_InterCharMs); return; }
    for (uint8_t i = 0; morse[i] != '\0'; i++)
    {
        if (morse[i] == '.') CW_PlayDit(); else if (morse[i] == '-') CW_PlayDah();
        if (morse[i + 1] != '\0') SYSTEM_DelayMs(gCW_InterElemMs);
    }
    SYSTEM_DelayMs(gCW_InterCharMs);
}

typedef enum {
    CW_TX_IDLE = 0, CW_TX_PREAMBLE, CW_TX_ELEMENT, CW_TX_ELEM_GAP,
    CW_TX_CHAR_GAP, CW_TX_WORD_GAP, CW_TX_TAIL
} CW_TxState_t;

static CW_TxState_t  gCW_TxState = CW_TX_IDLE;
static uint8_t       gCW_TxMsgIdx = 0;
static const char   *gCW_TxMorse = NULL;
static uint8_t       gCW_TxMorseIdx = 0;
static uint8_t       gCW_TxTimer = 0;
static bool          gCW_TxPrevWasSpace = false;
static bool          gCW_TxSentAny = false;

static void CW_TxStateMachine(void)
{
    if (gCW_TxTimer > 0) { gCW_TxTimer--; return; }
    switch (gCW_TxState)
    {
        // Intentional fall-through: CW_TX_PREAMBLE initializes the element state
        // then immediately enters CW_TX_ELEMENT for tone generation.
        case CW_TX_PREAMBLE: gCW_TxState = CW_TX_ELEMENT;
        case CW_TX_ELEMENT:
        {
            if (gCW_TxMorse == NULL || gCW_TxMorse[gCW_TxMorseIdx] == '\0')
            {
                while (gCW_TxMsgIdx < gCW_CursorPos)
                {
                    char c = gCW_Message[gCW_TxMsgIdx];
                    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
                    if (c == ' ')
                    {
                        const bool trailing = (gCW_TxMsgIdx + 1) >= gCW_CursorPos;
                        if (!gCW_TxSentAny || gCW_TxPrevWasSpace || trailing) { gCW_TxMsgIdx++; continue; }
                        gCW_TxPrevWasSpace = true;
                        gCW_TxState = CW_TX_WORD_GAP;
                        // +5 before division rounds to nearest 10 ms tick;
                        // this intentionally adds up to ~0.5 ms to word gaps.
                        gCW_TxTimer = (uint8_t)((gCW_InterWordMs + 5) / 10);
                        gCW_TxMsgIdx++;
                        return;
                    }
                    gCW_TxMorse = CW_CharToMorse(c);
                    gCW_TxMorseIdx = 0;
                    gCW_TxMsgIdx++;
                    if (gCW_TxMorse != NULL && gCW_TxMorse[0] != '\0')
                    {
                        gCW_TxSentAny = true; gCW_TxPrevWasSpace = false; break;
                    }
                    gCW_TxMorse = NULL;
                }
                if (gCW_TxMorse == NULL || gCW_TxMorseIdx >= strlen(gCW_TxMorse))
                {
                    gCW_TxState = CW_TX_TAIL;
                    gCW_TxTimer = (uint8_t)((gCW_InterCharMs + 5) / 10);
                    return;
                }
            }
            if (gCW_TxMorse[gCW_TxMorseIdx] == '.') { CW_ToneOn(); gCW_TxTimer = (uint8_t)((gCW_DitMs + 5) / 10); }
            else if (gCW_TxMorse[gCW_TxMorseIdx] == '-') { CW_ToneOn(); gCW_TxTimer = (uint8_t)((gCW_DahMs + 5) / 10); }
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
                gCW_TxTimer = (uint8_t)((gCW_InterElemMs + 5) / 10);
            }
            else
            {
                gCW_TxState = CW_TX_CHAR_GAP;
                gCW_TxTimer = (uint8_t)((gCW_InterCharMs + 5) / 10);
            }
            break;
        }
        case CW_TX_CHAR_GAP:
        case CW_TX_WORD_GAP:
            gCW_TxState = CW_TX_ELEMENT;
        break;
        case CW_TX_TAIL:
            gCW_TxState = CW_TX_IDLE;
            gCW_PlaybackActive = false;
            gCW_TxTimer = 0;
            gCW_State = CW_COMPOSING;
            CW_EndDedicatedTx();
            CW_Render();
            break;
        default: gCW_TxState = CW_TX_IDLE; break;
    }
}

void CW_SendMessage(void)
{
    if (strlen(gCW_Message) == 0 || !CW_BeginDedicatedTx()) return;
    gCW_State = CW_SENDING;
    gCW_PlaybackActive = true;
    gCW_TxState = CW_TX_PREAMBLE;
    gCW_TxMsgIdx = 0;
    gCW_TxMorse = NULL;
    gCW_TxMorseIdx = 0;
    gCW_TxTimer = 5;
    gCW_TxPrevWasSpace = false;
    gCW_TxSentAny = false;
    CW_Render();
}

// ---- Keys ----
static bool CW_IsAllowedInputChar(char c)
{
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ') return true;
    if (c == '.' || c == ',' || c == '?' || c == '\'' || c == '!' || c == '/' ||
        c == '(' || c == ')' || c == '&' || c == ':' || c == ';' || c == '=' ||
        c == '+' || c == '-' || c == '_' || c == '"' || c == '$' || c == '@') return true;
    return false;
}

static char CW_GetInputChar(uint8_t idx, uint8_t letter)
{
    char c = CW_KEY_CHARS[idx][letter];
    if (!gCW_UpperCase && c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    return c;
}

void CW_AppendChar(char c)
{
    if (gCW_CursorPos >= CW_MSG_MAX_LEN) return;
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

void CW_ProcessKeys(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld)
{
    if (!gCW_ActiveState || !bKeyPressed) return;
    if (Key != KEY_F) gWasFKeyPressed = false;

    if (bKeyHeld)
    {
        switch (Key)
        {
            case KEY_MENU:
                if (!gCW_MenuLongHandled)
                {
                    static const uint8_t wpmTable[] = {5,10,15,20,25,30,35,40,45,50};
                    uint8_t idx;
                    for (idx = 0; idx < sizeof(wpmTable); idx++) if (wpmTable[idx] == gCW_WPM) break;
                    idx = (idx + 1) % sizeof(wpmTable);
                    gCW_WPM = wpmTable[idx];
                    CW_UpdateTiming();
                    gCW_PrevKey = 0; gCW_PrevLetter = 0;
                    CW_Render();
                    AUDIO_PlayBeep(BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL);
                    gCW_MenuLongHandled = true;
                }
                return;
            case KEY_SIDE2:
                if (!gCW_Side2LongHandled)
                {
                    gCW_CursorPos = 0; gCW_Message[0] = '\0';
                    gCW_PrevKey = 0; gCW_PrevLetter = 0;
                    CW_Render();
                    AUDIO_PlayBeep(BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL);
                    gCW_Side2LongHandled = true;
                }
                return;
            case KEY_EXIT: CW_Stop(); gRequestDisplayScreen = DISPLAY_MAIN; return;
            default: break;
        }
    }

    switch (Key)
    {
        case KEY_1: case KEY_2: case KEY_3: case KEY_4: case KEY_5:
        case KEY_6: case KEY_7: case KEY_8: case KEY_9:
        {
            uint8_t idx = (uint8_t)(Key - KEY_1);
            uint8_t count = CW_KEY_CHAR_COUNT[idx];
            if (count == 0) break;
            if (gCW_PrevKey == Key && gCW_CursorPos > 0)
            {
                gCW_PrevLetter = (gCW_PrevLetter + 1) % count;
                gCW_Message[gCW_CursorPos - 1] = CW_GetInputChar(idx, gCW_PrevLetter);
            }
            else
            {
                bool found = false;
                for (uint8_t n = 0; n < count; n++)
                {
                    char c = CW_GetInputChar(idx, n);
                    if (CW_IsAllowedInputChar(c))
                    {
                        gCW_PrevKey = Key; gCW_PrevLetter = n; CW_AppendChar(c); found = true; break;
                    }
                }
                if (!found) AUDIO_PlayBeep(BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL);
            }
            gCW_KeyTick = 1; CW_Render(); break;
        }
        case KEY_0:
        {
            if (gCW_PrevKey == KEY_0 && gCW_CursorPos > 0 && gCW_Message[gCW_CursorPos - 1] == ' ')
            {
                gCW_CursorPos--; gCW_Message[gCW_CursorPos] = '\0'; CW_AppendChar('0'); gCW_PrevKey = KEY_0;
            }
            else
            {
                CW_AppendChar(' '); gCW_PrevKey = KEY_0; gCW_PrevLetter = 0;
            }
            gCW_KeyTick = 1; CW_Render(); break;
        }
        case KEY_UP: gCW_PrevKey = 0; gCW_PrevLetter = 0; gCW_KeyTick = 0; CW_DeleteChar(); CW_Render(); break;
        case KEY_DOWN: gCW_PrevKey = 0; gCW_PrevLetter = 0; gCW_KeyTick = 0; CW_AppendChar(' '); CW_Render(); break;
        case KEY_SIDE2: gCW_UpperCase = !gCW_UpperCase; CW_Render(); AUDIO_PlayBeep(BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL); break;
        // KEY_STAR intentionally removed to avoid double-toggle with KEY_SIDE2 long-press
        case KEY_MENU: gRequestDisplayScreen = DISPLAY_MENU; break;
        case KEY_SIDE1: gCW_CursorPos = 0; gCW_Message[0] = '\0'; gCW_PrevKey = 0; gCW_PrevLetter = 0; CW_Render(); AUDIO_PlayBeep(BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL); break;
        case KEY_F:
            if (!bKeyHeld) { gCW_CursorPos = 0; gCW_Message[0] = '\0'; gCW_PrevKey = 0; gCW_PrevLetter = 0; CW_Render(); }
            break;
        case KEY_EXIT: CW_Stop(); gRequestDisplayScreen = DISPLAY_MAIN; break;
        case KEY_PTT:
            if (!bKeyHeld && gCW_CursorPos > 0)
            {
                gCW_PrevKey = 0; gCW_PrevLetter = 0;
                CW_SendMessage(); CW_Render();
            }
            break;
        default: break;
    }
}

bool CW_IsActive(void) { return gCW_ActiveState; }
void APP_RunCW(void) { if (gCW_ActiveState) CW_Stop(); else CW_Start(); }
void CW_Overlay(void) { CW_Render(); }
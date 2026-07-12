/* Simplified CW Encoder/Decoder */
/* Standalone reference implementation */

#include "app/cw.h"
#include "app/app.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

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

/* Morse lookup table */
static const struct { char ch; const char *morse; } CW_CHAR_MAP[] = {
    {'A', ".-"}, {'B', "-..."}, {'C', "-.-."}, {'D', "-.."}, {'E', "."},
    {'F', "..-."}, {'G', "--."}, {'H', "...."}, {'I', ".."}, {'J', ".---"},
    {'K', "-.-"}, {'L', ".-.."}, {'M', "--"}, {'N', "-."}, {'O', "---"},
    {'P', ".--."}, {'Q', "--.-"}, {'R', ".-."}, {'S', "..."}, {'T', "-"},
    {'U', "..-"}, {'V', "...-"}, {'W', ".--"}, {'X', "-..-"}, {'Y', "-.--"},
    {'Z', "--.."}, {'0', "-----"}, {'1', ".----"}, {'2', "..---"},
    {'3', "...--"}, {'4', "....-"}, {'5', "....."}, {'6', "-...."},
    {'7', "--..."}, {'8', "---.."}, {'9', "----."}, {'.', ".-.-.-"},
    {',', "--..--"}, {'?', "..--.."}, {'\'', ".----."}, {'!', "-.-.--"},
    {'/', "-..-."}, {' ', ""}
};

#define CW_RX_SIMPLE_THRESHOLD 8

/* Global state */
CW_State_t gCW_State = CW_IDLE;
char gCW_Message[CW_MSG_MAX_LEN + 1] = {0};
uint8_t gCW_CursorPos = 0;
uint8_t gCW_WPM = CW_DEFAULT_WPM;
uint16_t gCW_ToneFreq = CW_TONE_FREQ;

static bool gCW_ActiveState = false;
static uint8_t gCW_PrevKey = 0;
static uint8_t gCW_PrevLetter = 0;
static uint8_t gCW_KeyTick = 0;
static bool gCW_MenuLongHandled = false;
static bool gCW_Side2LongHandled = false;
static bool gCW_UpperCase = true;

static bool gCW_RxModeOverridden = false;
static uint8_t gCW_BackupDualWatch = DUAL_WATCH_OFF;
static uint8_t gCW_BackupCrossBand = CROSS_BAND_OFF;
static bool gCW_BackupMonitor = false;
static bool gCW_MonitorForced = false;
static uint8_t gCW_BackupRogerMode = ROGER_MODE_OFF;
static bool gCW_RogerModeBackedUp = false;

/* RX decoder state */
static char gCW_DecodeText[CW_MSG_MAX_LEN + 1] = {0};
static uint8_t gCW_DecodeCursor = 0;
static char gCW_RxMorse[12] = {0};
static uint8_t gCW_RxMorseLen = 0;
static uint16_t gCW_RxDitTicks = 0;
static uint32_t gCW_RxMarkStart = 0;
static uint32_t gCW_RxSpaceStart = 0;
static uint8_t gCW_RxSignalDebounce = 0;
static int16_t gCW_RxNoiseFloor = -120;
static bool gCW_StartupDelay = false;

/* Visualization */
static uint8_t gCW_RxSignalHistory[128];
static uint16_t gCW_RxTraceClock = 0;
static bool gCW_RxActive = false;
static uint8_t gCW_RxActiveDebounce = 0;
static uint8_t gCW_RxInactiveDebounce = 0;

/* TX state (advanced FSM matching cw.c) */
typedef enum {
    CW_TX_IDLE = 0,
    CW_TX_PREAMBLE,
    CW_TX_ELEMENT,
    CW_TX_ELEM_GAP,
    CW_TX_CHAR_GAP,
    CW_TX_WORD_GAP,
    CW_TX_TAIL
} CW_TxState_t;

static uint8_t       gCW_TxState = CW_TX_IDLE;
static uint8_t       gCW_TxMsgIdx = 0;
static const char   *gCW_TxMorse = NULL;
static uint8_t       gCW_TxMorseIdx = 0;
static uint8_t       gCW_TxTimer = 0;
static bool          gCW_TxPrevWasSpace = false;
static bool          gCW_TxSentAny = false;

static uint16_t gCW_TxDahMs = 0;
static uint16_t gCW_TxInterElemMs = 0;
static uint16_t gCW_TxInterCharMs = 0;
static uint16_t gCW_TxInterWordMs = 0;

/* ===== TIMING ===== */
static void CW_UpdateTiming(void)
{
    uint16_t ditMs = 1200 / gCW_WPM;
    if (ditMs < 20) ditMs = 20;
    if (ditMs > 240) ditMs = 240;
    gCW_RxDitTicks = (uint16_t)((ditMs + 5) / 10);
    gCW_TxDahMs = ditMs * 3;
    gCW_TxInterElemMs = ditMs;
    gCW_TxInterCharMs = ditMs * 3;
    gCW_TxInterWordMs = ditMs * 7;
}

/* ===== SIMPLIFIED TONE DETECTION ===== */
static bool CW_IsRxTonePresent(void)
{
    int16_t rssi = BK4819_GetRSSI_dBm() + dBmCorrTable[gRxVfo->Band];
    if (rssi < gCW_RxNoiseFloor)
        gCW_RxNoiseFloor = (int16_t)((gCW_RxNoiseFloor * 3 + rssi + 2) / 4);
    else
        gCW_RxNoiseFloor = (int16_t)((gCW_RxNoiseFloor * 7 + rssi + 4) / 8);
    return rssi >= (gCW_RxNoiseFloor + CW_RX_SIMPLE_THRESHOLD);
}

/* ===== SIMPLIFIED EDGE-BASED DECODER ===== */
static void CW_DecodeEdge(bool signalNow)
{
    static bool signalPrev = false;
    const uint16_t ditTicks = MAX(1U, gCW_RxDitTicks);
    const uint16_t dahThreshold = (uint16_t)((ditTicks * 5) / 2);

    if (signalNow != signalPrev)
    {
        if (++gCW_RxSignalDebounce < 2) return;
        gCW_RxSignalDebounce = 0;
        if (signalNow && !signalPrev)
            gCW_RxMarkStart = gGlobalSysTickCounter;
        else if (!signalNow && signalPrev)
        {
            uint16_t markDur = (uint16_t)(gGlobalSysTickCounter - gCW_RxMarkStart);
            if (markDur >= dahThreshold)
                gCW_RxMorse[gCW_RxMorseLen++] = '-';
            else if (markDur >= 2)
                gCW_RxMorse[gCW_RxMorseLen++] = '.';
            gCW_RxMorse[gCW_RxMorseLen] = '\0';
            if (markDur >= 3 && markDur <= (uint16_t)(gCW_RxDitTicks * 4))
            {
                gCW_RxDitTicks = (uint16_t)((5 * gCW_RxDitTicks + markDur + 2) / 6);
                if (gCW_RxDitTicks < 1) gCW_RxDitTicks = 1;
                if (gCW_RxDitTicks > 40) gCW_RxDitTicks = 40;
            }
            gCW_RxSpaceStart = gGlobalSysTickCounter;
        }
        signalPrev = signalNow;
    }
    if (!signalNow && gCW_RxMorseLen > 0)
    {
        uint16_t spaceDur = (uint16_t)(gGlobalSysTickCounter - gCW_RxSpaceStart);
        if (spaceDur >= (uint16_t)(7 * ditTicks))
        {
            for (uint8_t i = 0; i < ARRAY_SIZE(CW_CHAR_MAP); i++)
                if (strcmp(gCW_RxMorse, CW_CHAR_MAP[i].morse) == 0 && gCW_DecodeCursor < CW_MSG_MAX_LEN)
                    gCW_DecodeText[gCW_DecodeCursor++] = CW_CHAR_MAP[i].ch;
            gCW_DecodeText[gCW_DecodeCursor] = '\0';
            gCW_RxMorseLen = 0; gCW_RxMorse[0] = '\0';
            if (gCW_DecodeCursor == 0 || gCW_DecodeText[gCW_DecodeCursor-1] != ' ')
                gCW_DecodeText[gCW_DecodeCursor++] = ' ';
            gCW_DecodeText[gCW_DecodeCursor] = '\0';
            gUpdateDisplay = true;
        }
        else if (spaceDur >= (uint16_t)(3 * ditTicks))
        {
            for (uint8_t i = 0; i < ARRAY_SIZE(CW_CHAR_MAP); i++)
                if (strcmp(gCW_RxMorse, CW_CHAR_MAP[i].morse) == 0 && gCW_DecodeCursor < CW_MSG_MAX_LEN)
                    gCW_DecodeText[gCW_DecodeCursor++] = CW_CHAR_MAP[i].ch;
            gCW_DecodeText[gCW_DecodeCursor] = '\0';
            gCW_RxMorseLen = 0; gCW_RxMorse[0] = '\0';
            gUpdateDisplay = true;
        }
    }
}

/* ===== TX HELPERS ===== */
static void CW_ToneOn(void) { BK4819_TransmitTone(true, gCW_ToneFreq); }
static void CW_ToneOff(void) { BK4819_EnterTxMute(); }

static bool CW_BeginDedicatedTx(void)
{
    VfoState_t state = VFO_STATE_NORMAL;
    if (gCurrentVfo == NULL) RADIO_SelectVfos();
    if (TX_freq_check(gCurrentVfo->pTX->Frequency) != 0 && gCurrentVfo->TX_LOCK)
        { state = VFO_STATE_TX_DISABLE; gVfoConfigureMode = VFO_CONFIGURE; }
    else if (SerialConfigInProgress()) state = VFO_STATE_TX_DISABLE;
    else if (gCurrentVfo->BUSY_CHANNEL_LOCK && gCurrentFunction == FUNCTION_RECEIVE) state = VFO_STATE_BUSY;
    else if (gBatteryDisplayLevel == 0) state = VFO_STATE_BAT_LOW;
    else if (gBatteryDisplayLevel > 6) state = VFO_STATE_VOLTAGE_HIGH;
    if (state != VFO_STATE_NORMAL) { RADIO_SetVfoState(state); AUDIO_PlayBeep(BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL); return false; }
    BK4819_DisableDTMF(); FUNCTION_Select(FUNCTION_TRANSMIT);
    gTxTimerCountdown_500ms = ((gEeprom.TX_TIMEOUT_TIMER + 1) * 5) * 2;
    gTxTimeoutReached = false; gFlagEndTransmission = false; gRTTECountdown_10ms = 0;
    BK4819_DisableScramble(); BK4819_SetCompander(0); BK4819_ExitSubAu(); BK4819_SetAF(BK4819_AF_MUTE);
    return gCurrentFunction == FUNCTION_TRANSMIT;
}

static void CW_EndDedicatedTx(void)
{
    BK4819_DisableDTMF(); BK4819_DisableScramble();
    BK4819_TurnsOffTones_TurnsOnRX(); BK4819_ExitSubAu(); BK4819_DisableDTMF();
    FUNCTION_Select(FUNCTION_FOREGROUND);
    gFlagEndTransmission = false; gRTTECountdown_10ms = 0;
    RADIO_SetVfoState(VFO_STATE_NORMAL); RADIO_SelectVfos();
}

/* Non-static to match cw.h declaration */
const char *CW_CharToMorse(char c)
{
    if (c >= 'a' && c <= 'z') c -= 32;
    for (uint8_t i = 0; i < ARRAY_SIZE(CW_CHAR_MAP); i++)
        if (CW_CHAR_MAP[i].ch == c) return CW_CHAR_MAP[i].morse;
    return (c == ' ') ? "" : NULL;
}

/* ===== PUBLIC API ===== */
void CW_Init(void)
{
    gCW_State = CW_IDLE; gCW_ActiveState = false;
    gCW_CursorPos = 0; gCW_Message[0] = '\0';
    gCW_WPM = CW_DEFAULT_WPM; gCW_ToneFreq = CW_TONE_FREQ;
    gCW_UpperCase = true; gCW_PrevKey = 0; gCW_PrevLetter = 0; gCW_KeyTick = 0;
    gCW_MenuLongHandled = false; gCW_Side2LongHandled = false;
    CW_UpdateTiming();
    gCW_DecodeCursor = 0; gCW_DecodeText[0] = '\0';
    gCW_RxMorseLen = 0; gCW_RxMorse[0] = '\0';
    gCW_RxActive = false; gCW_RxActiveDebounce = 0; gCW_RxInactiveDebounce = 0;
    gCW_RxNoiseFloor = -120; gCW_StartupDelay = false;
    memset(gCW_RxSignalHistory, 0, 128); gCW_RxTraceClock = 0;
}

void CW_Start(void)
{
    if (gCW_ActiveState) return;
    if (!gCW_RxModeOverridden) {
        gCW_BackupDualWatch = gEeprom.DUAL_WATCH;
        gCW_BackupCrossBand = gEeprom.CROSS_BAND_RX_TX;
        gCW_RxModeOverridden = true;
    }
    gEeprom.DUAL_WATCH = DUAL_WATCH_OFF; gEeprom.CROSS_BAND_RX_TX = CROSS_BAND_OFF;
    gDualWatchActive = false; gScheduleDualWatch = false; RADIO_SelectVfos();
    if (!gCW_MonitorForced) { gCW_BackupMonitor = gMonitor; gCW_MonitorForced = true; }
    gRxVfoIsActive = true; RADIO_SetupRegisters(true);
    if (!gCW_RogerModeBackedUp) { gCW_BackupRogerMode = gEeprom.ROGER; gCW_RogerModeBackedUp = true; gEeprom.ROGER = ROGER_MODE_OFF; }
    BK4819_DisableDTMF(); gMonitor = true; APP_StartListening(FUNCTION_MONITOR);
    gCW_StartupDelay = true; CW_Init();
    gCW_State = CW_COMPOSING; gCW_ActiveState = true;
    memset(gFrameBuffer[CW_LINE_TX1], 0, LCD_WIDTH);
    memset(gFrameBuffer[CW_LINE_TX2], 0, LCD_WIDTH);
    memset(gFrameBuffer[CW_LINE_DECODE], 0, LCD_WIDTH);
    memset(gFrameBuffer[CW_LINE_STATUS], 0, LCD_WIDTH);
    gWasFKeyPressed = false; CW_Render();
    AUDIO_PlayBeep(BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL);
}

void CW_Stop(void)
{
    if (!gCW_ActiveState) return;
    gCW_ActiveState = false; gCW_State = CW_IDLE;
    memset(gFrameBuffer[CW_LINE_TX1], 0, LCD_WIDTH);
    memset(gFrameBuffer[CW_LINE_TX2], 0, LCD_WIDTH);
    memset(gFrameBuffer[CW_LINE_DECODE], 0, LCD_WIDTH);
    memset(gFrameBuffer[CW_LINE_STATUS], 0, LCD_WIDTH);
    gCW_DecodeCursor = 0; gCW_DecodeText[0] = '\0';
    gCW_RxMorseLen = 0; gCW_RxMorse[0] = '\0';
    if (gCW_RxModeOverridden) { gEeprom.DUAL_WATCH = gCW_BackupDualWatch; gEeprom.CROSS_BAND_RX_TX = gCW_BackupCrossBand; gCW_RxModeOverridden = false; gScheduleDualWatch = true; RADIO_SelectVfos(); }
    if (gCW_MonitorForced) { gMonitor = gCW_BackupMonitor; gCW_MonitorForced = false; APP_StartListening(gMonitor ? FUNCTION_MONITOR : FUNCTION_RECEIVE); }
    if (gCW_RogerModeBackedUp) { gEeprom.ROGER = gCW_BackupRogerMode; gCW_RogerModeBackedUp = false; }
    BK4819_DisableDTMF(); gWasFKeyPressed = false; gUpdateDisplay = true;
}

void CW_Toggle(void) { if (gCW_ActiveState) CW_Stop(); else CW_Start(); }
bool CW_IsActive(void) { return gCW_ActiveState; }
void APP_RunCW(void) { if (gCW_ActiveState) CW_Stop(); else CW_Start(); }
void CW_AppendChar(char c) { if (gCW_CursorPos < CW_MSG_MAX_LEN) gCW_Message[gCW_CursorPos++] = c; gCW_Message[gCW_CursorPos] = '\0'; }
void CW_DeleteChar(void) { if (gCW_CursorPos > 0) { gCW_CursorPos--; gCW_Message[gCW_CursorPos] = '\0'; } }

void CW_SendMessage(void)
{
    if (strlen(gCW_Message) == 0) return;
    if (!CW_BeginDedicatedTx()) return;
    gCW_State = CW_SENDING;
    gCW_TxState = CW_TX_PREAMBLE;
    gCW_TxMsgIdx = 0; gCW_TxMorse = NULL; gCW_TxMorseIdx = 0;
    gCW_TxTimer = 5;  /* 50ms preamble */
    gCW_TxPrevWasSpace = false; gCW_TxSentAny = false;
    CW_Render();
}

/* ===== ADVANCED TX STATE MACHINE (from cw.c) ===== */
void CW_TimeSlice10ms(void)
{
    if (gCW_StartupDelay) { static uint8_t t = 0; if (++t >= 50) { gCW_StartupDelay = false; t = 0; } return; }
    
    if (gCW_ActiveState && gCW_State == CW_SENDING)
    {
        if (gCW_TxTimer > 0) { gCW_TxTimer--; return; }
        
        switch (gCW_TxState)
        {
            case CW_TX_PREAMBLE:
                gCW_TxState = CW_TX_ELEMENT;
                
            case CW_TX_ELEMENT:
            {
                if (gCW_TxMorse == NULL || gCW_TxMorse[gCW_TxMorseIdx] == '\0')
                {
                    while (gCW_TxMsgIdx < gCW_CursorPos)
                    {
                        char c = gCW_Message[gCW_TxMsgIdx];
                        if (c >= 'a' && c <= 'z') c -= 32;
                        
                        if (c == ' ')
                        {
                            bool trailing = (gCW_TxMsgIdx + 1) >= gCW_CursorPos;
                            if (!gCW_TxSentAny || gCW_TxPrevWasSpace || trailing)
                                { gCW_TxMsgIdx++; continue; }
                            gCW_TxPrevWasSpace = true;
                            gCW_TxState = CW_TX_WORD_GAP;
                            gCW_TxTimer = (uint8_t)(gCW_TxInterWordMs / 10);
                            gCW_TxMsgIdx++;
                            return;
                        }
                        
                        gCW_TxMorse = CW_CharToMorse(c);
                        gCW_TxMorseIdx = 0; gCW_TxMsgIdx++;
                        if (gCW_TxMorse && gCW_TxMorse[0])
                            { gCW_TxSentAny = true; gCW_TxPrevWasSpace = false; break; }
                        gCW_TxMorse = NULL;
                    }
                    
                    if (!gCW_TxMorse || gCW_TxMorseIdx >= strlen(gCW_TxMorse))
                    {
                        gCW_TxState = CW_TX_TAIL;
                        gCW_TxTimer = (uint8_t)(gCW_TxInterCharMs / 10);
                        return;
                    }
                }
                
                if (gCW_TxMorse[gCW_TxMorseIdx] == '.')
                    { CW_ToneOn(); gCW_TxTimer = (uint8_t)((1200 / gCW_WPM) / 10); }
                else if (gCW_TxMorse[gCW_TxMorseIdx] == '-')
                    { CW_ToneOn(); gCW_TxTimer = (uint8_t)(gCW_TxDahMs / 10); }
                gCW_TxState = CW_TX_ELEM_GAP;
                CW_Render();
                break;
            }
            
            case CW_TX_ELEM_GAP:
                CW_ToneOff();
                gCW_TxMorseIdx++;
                if (gCW_TxMorse[gCW_TxMorseIdx] != '\0')
                {
                    gCW_TxState = CW_TX_ELEMENT;
                    gCW_TxTimer = (uint8_t)(gCW_TxInterElemMs / 10);
                }
                else
                {
                    gCW_TxState = CW_TX_CHAR_GAP;
                    gCW_TxTimer = (uint8_t)(gCW_TxInterCharMs / 10);
                }
                break;
                
            case CW_TX_CHAR_GAP:
                gCW_TxState = CW_TX_ELEMENT;
                break;
                
            case CW_TX_WORD_GAP:
                gCW_TxState = CW_TX_ELEMENT;
                break;
                
            case CW_TX_TAIL:
                gCW_TxState = CW_TX_IDLE;
                gCW_State = CW_COMPOSING;
                CW_EndDedicatedTx();
                APP_StartListening(gMonitor ? FUNCTION_MONITOR : FUNCTION_RECEIVE);
                CW_Render();
                break;
                
            default:
                gCW_TxState = CW_TX_IDLE;
                break;
        }
        return;
    }
    
    if (!gCW_ActiveState || gCW_State == CW_SENDING) return;
    if (!gMonitor) { gMonitor = true; APP_StartListening(FUNCTION_MONITOR); }
    bool sig = CW_IsRxTonePresent();
    if (sig && !gCW_RxActive) { if (++gCW_RxActiveDebounce >= 5) { gCW_RxActive = true; gCW_RxActiveDebounce = 0; } }
    else if (!sig && gCW_RxActive) { if (++gCW_RxInactiveDebounce >= 3) { gCW_RxActive = false; gCW_RxActiveDebounce = 0; gCW_RxInactiveDebounce = 0; } }
    else { gCW_RxActiveDebounce = 0; gCW_RxInactiveDebounce = 0; }
    CW_DecodeEdge(sig);
    if (++gCW_RxTraceClock >= 2)
    {
        gCW_RxTraceClock = 0;
        int16_t r = BK4819_GetRSSI_dBm() + dBmCorrTable[gRxVfo->Band];
        uint8_t l = (uint8_t)(r + 120); if (l > 64) l = 64;
        if (!sig && l > 20) l = 20; if (sig) l = 64;
        memmove(gCW_RxSignalHistory, gCW_RxSignalHistory + 1, 127);
        gCW_RxSignalHistory[127] = l;
    }
    if (gCW_KeyTick > 0) { if (++gCW_KeyTick >= 80) { gCW_KeyTick = 0; gCW_PrevKey = 0; gCW_PrevLetter = 0; CW_Render(); } }
}

/* ===== UI RENDERING (matching original cw.c style) ===== */
static void CW_DrawWrappedTxText(void)
{
    const uint16_t len = (uint16_t)strlen(gCW_Message);
    memset(gFrameBuffer[CW_LINE_TX1], 0, LCD_WIDTH);
    memset(gFrameBuffer[CW_LINE_TX2], 0, LCD_WIDTH);
    if (len == 0)
    {
        UI_PrintStringSmallNormal("TYPE MESSAGE", 2, 0, CW_LINE_TX1);
        UI_PrintStringSmallNormal("PRESS PTT", 2, 0, CW_LINE_TX2);
        return;
    }
    if (len <= CW_CHARS_PER_TX_LINE)
    {
        UI_PrintStringSmallBold(gCW_Message, 2, 0, CW_LINE_TX1);
        return;
    }
    char line1[CW_CHARS_PER_TX_LINE + 1];
    memcpy(line1, gCW_Message, CW_CHARS_PER_TX_LINE);
    line1[CW_CHARS_PER_TX_LINE] = '\0';
    UI_PrintStringSmallBold(line1, 2, 0, CW_LINE_TX1);
    uint16_t remaining = len - CW_CHARS_PER_TX_LINE;
    if (remaining > CW_CHARS_PER_TX_LINE) remaining = CW_CHARS_PER_TX_LINE;
    char line2[CW_CHARS_PER_TX_LINE + 1];
    memcpy(line2, gCW_Message + CW_CHARS_PER_TX_LINE, remaining);
    line2[remaining] = '\0';
    UI_PrintStringSmallBold(line2, 2, 0, CW_LINE_TX2);
}

static void CW_DrawStatusLine(void)
{
    memset(gFrameBuffer[CW_LINE_STATUS], 0, LCD_WIDTH);
    const char *mode = "MON";
    if (gCW_State == CW_SENDING) mode = "TX";
    else if (gCW_RxActive) mode = "RX";
    char status[24];
    sprintf(status, "%u WPM  %s  %c", gCW_WPM, mode, gCW_UpperCase ? 'U' : 'L');
    UI_PrintStringSmallNormal(status, 2, 0, CW_LINE_STATUS);
}

static void CW_DrawSignalGraph(void)
{
    memset(gFrameBuffer[CW_LINE_DECODE], 0, LCD_WIDTH);
    const uint8_t colW = 2, colGap = 1, stride = colW + colGap;
    const uint8_t cols = (uint8_t)(LCD_WIDTH / stride);
    for (uint8_t i = 0; i < cols; i++)
    {
        uint8_t idx = (uint8_t)(128u - cols + i);
        uint8_t v = gCW_RxSignalHistory[idx];
        uint8_t h = 0;
        if (v >= 64) h = 7;
        else if (v > 32) h = 5;
        else if (v > 20) h = 1;
        uint8_t m = (h > 0) ? (uint8_t)((0x7Fu << (7u - h)) & 0x7Fu) : 0x40u;
        gFrameBuffer[CW_LINE_DECODE][i * stride] = m;
        if (colW > 1) gFrameBuffer[CW_LINE_DECODE][i * stride + 1] = m;
    }
}

void CW_Render(void)
{
    if (!gCW_ActiveState) return;
    switch (gCW_State)
    {
        case CW_COMPOSING:
        {
            gMonitor = true;
            const bool haveDecoded = (gCW_DecodeText[0] != '\0');
            const bool haveMorse = (gCW_RxMorseLen > 0);
            if (haveDecoded || haveMorse)
            {
                const char *text = haveDecoded ? gCW_DecodeText : gCW_RxMorse;
                memset(gFrameBuffer[CW_LINE_TX1], 0, LCD_WIDTH);
                memset(gFrameBuffer[CW_LINE_TX2], 0, LCD_WIDTH);
                UI_PrintStringSmallNormal("RX:", 2, 0, CW_LINE_TX1);
                UI_PrintStringSmallNormal(text, 28, 0, CW_LINE_TX1);
            }
            else
            {
                CW_DrawWrappedTxText();
            }
            CW_DrawSignalGraph();
            CW_DrawStatusLine();
            break;
        }
        case CW_SENDING:
            CW_DrawWrappedTxText();
            memset(gFrameBuffer[CW_LINE_DECODE], 0, LCD_WIDTH);
            CW_DrawStatusLine();
            break;
        default: break;
    }
    gUpdateDisplay = true;
}

void CW_Overlay(void) { CW_Render(); }

/* ===== KEY PROCESSING (matching original cw.c style) ===== */
static char CW_GetInputChar(uint8_t idx, uint8_t letter)
{
    char c = CW_KEY_CHARS[idx][letter];
    if (!gCW_UpperCase && c >= 'A' && c <= 'Z')
        c = (char)(c - 'A' + 'a');
    return c;
}

static bool CW_IsAllowedInputChar(char c)
{
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ')
        return true;
    if (c == '.' || c == ',' || c == '?' || c == '\'' || c == '!' || c == '/' ||
        c == '(' || c == ')' || c == '&' || c == ':' || c == ';' || c == '=' ||
        c == '+' || c == '-' || c == '_' || c == '"' || c == '$' || c == '@')
        return true;
    return false;
}

void CW_ProcessKeys(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld)
{
    if (!gCW_ActiveState) return;
    if (!bKeyPressed) { gCW_MenuLongHandled = false; gCW_Side2LongHandled = false; return; }
    if (Key != KEY_F) gWasFKeyPressed = false;
    if (bKeyHeld)
    {
        switch (Key) {
            case KEY_MENU:
                if (!gCW_MenuLongHandled) {
                    static const uint8_t wpm[] = {5,10,15,20,25,30,35,40,45,50};
                    uint8_t idx; for (idx = 0; idx < 10; idx++) if (wpm[idx] == gCW_WPM) break;
                    gCW_WPM = wpm[(idx + 1) % 10]; CW_UpdateTiming();
                    gCW_PrevKey = 0; gCW_PrevLetter = 0; CW_Render();
                    AUDIO_PlayBeep(BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL);
                    gCW_MenuLongHandled = true;
                } return;
            case KEY_SIDE2:
                if (!gCW_Side2LongHandled) {
                    gCW_CursorPos = 0; gCW_Message[0] = '\0';
                    gCW_PrevKey = 0; gCW_PrevLetter = 0; CW_Render();
                    AUDIO_PlayBeep(BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL);
                    gCW_Side2LongHandled = true;
                } return;
            case KEY_EXIT: CW_Stop(); gRequestDisplayScreen = DISPLAY_MAIN; return;
            case KEY_PTT: return;
            default: break;
        } return;
    }
    switch (Key)
    {
        case KEY_1: case KEY_2: case KEY_3: case KEY_4: case KEY_5:
        case KEY_6: case KEY_7: case KEY_8: case KEY_9
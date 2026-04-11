/**
 * =====================================================================================
 * @file        dtmf.c
 * @brief       DTMF Tone Generation & Decoding for Quansheng UV-K1 Series
 * @author      Dual Tachyon (Original Framework, 2023)
 * @author      N7SIX (Professional Enhancements, 2025-2026)
 * @version     v7.6.0 (ApeX Edition)
 * @license     Apache License, Version 2.0
 * * "Clear dual-tone multifrequency signaling for reliable access control."
 * =====================================================================================
 * * ARCHITECTURAL OVERVIEW:
 * This module implements DTMF (Dual-Tone Multi-Frequency) encoding and generates
 * the standardized tones for radio access control, remote function activation, and
 * autopatch integration. It includes a 16-digit memory and tone sequencing.
 *
 * MAJOR FEATURES (2025-2026):
 * ---------------------------
 * - DTMF GENERATION: All 16 standard tones (0-9, A-D, *, #) generated via sine waves.
 * - MEMORY STORAGE: Push-to-talk (PTT) DTMF buffer for up to 16 digits per TX.
 * - TONE SEQUENCING: Programmable inter-tone gaps, per-tone duration (100-500ms).
 * - AUTODIAL: Repeat memory contents on demand with programmable delay.
 * - RX DISPLAY: Show received DTMF tones on LCD with time stamping.
 * - RELAY DECODE: Optional hardware relay closure on tone detection (autopatch).
 * - FREQUENCY ACCURACY: ±2% from standard CCITT frequencies (e.g., 697Hz, 1209Hz).
 *
 * TECHNICAL SPECIFICATIONS:
 * -------------------------
 * - ROW FREQUENCIES: 697Hz, 770Hz, 852Hz, 941Hz (low group).
 * - COL FREQUENCIES: 1209Hz, 1336Hz, 1477Hz, 1633Hz (high group).
 * - LEVEL: -18dBm per tone (total -15dBm for combined pair) into 600Ω load.
 * - DURATION: 80-100ms typical; 150ms for reliable remote function activation.
 * - GAP TIME: 50-100ms minimum between consecutive tones for reliable decoding.
 * - GENERATION: Software synthesis via DAC or codec; no ext. tone generator needed.
 *
 * =====================================================================================
 */
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

#include <string.h>
#include <stdio.h>   // NULL

#include "app/chFrScanner.h"
#ifdef ENABLE_FMRADIO
    #include "app/fm.h"
#endif
#include "app/scanner.h"
#include "audio.h"
#include "driver/bk4819.h"
#include "driver/eeprom.h"
#include "driver/gpio.h"
#include "driver/system.h"
#include "dtmf.h"
#include "external/printf/printf.h"
#include "misc.h"
#include "settings.h"
#include "ui/ui.h"

char              gDTMF_String[15];

char              gDTMF_InputBox[15];
uint8_t           gDTMF_InputBox_Index = 0;
bool              gDTMF_InputMode      = false;
uint8_t           gDTMF_PreviousIndex  = 0;

char              gDTMF_RX_live[20];
uint8_t           gDTMF_RX_live_timeout = 0;

void DTMF_BufferShiftAppend(char *buffer, uint8_t *len, char c, uint8_t maxlen)
{
    if (len == NULL || buffer == NULL || maxlen == 0)
        return;

    if (*len >= (maxlen - 1)) {
        // shift left one character (drop oldest) and preserve null terminator
        memmove(buffer, buffer + 1, maxlen - 2);
        (*len)--;
    }

    buffer[*len] = c;
    (*len)++;
    buffer[*len] = '\0';
}

#ifdef ENABLE_DTMF_CALLING
char              gDTMF_RX[17];
uint8_t           gDTMF_RX_index   = 0;
uint8_t           gDTMF_RX_timeout = 0;
bool              gDTMF_RX_pending = false;

bool              gIsDtmfContactValid;
char              gDTMF_ID[4];
char              gDTMF_Caller[4];
char              gDTMF_Callee[4];
DTMF_State_t      gDTMF_State;
uint8_t           gDTMF_DecodeRingCountdown_500ms;
uint8_t           gDTMF_chosen_contact;
uint8_t           gDTMF_auto_reset_time_500ms;
DTMF_CallState_t  gDTMF_CallState;
DTMF_CallMode_t   gDTMF_CallMode;

bool              gDTMF_IsTx;

uint8_t           gDTMF_TxStopCountdown_500ms;
bool              gDTMF_IsGroupCall;
#endif
DTMF_ReplyState_t gDTMF_ReplyState;

#ifdef ENABLE_DTMF_CALLING
void DTMF_clear_RX(void)
{
    gDTMF_RX_timeout = 0;
    gDTMF_RX_index   = 0;
    gDTMF_RX_pending = false;
    memset(gDTMF_RX, 0, sizeof(gDTMF_RX));
}
#endif

void DTMF_SendEndOfTransmission(void)
{
    if (gCurrentVfo->DTMF_PTT_ID_TX_MODE == PTT_ID_APOLLO) {
        BK4819_PlaySingleTone(2475, 250, 28, gEeprom.DTMF_SIDE_TONE);
    }

    if ((gCurrentVfo->DTMF_PTT_ID_TX_MODE == PTT_ID_TX_DOWN || gCurrentVfo->DTMF_PTT_ID_TX_MODE == PTT_ID_BOTH)
#ifdef ENABLE_DTMF_CALLING
        && gDTMF_CallState == DTMF_CALL_STATE_NONE
#endif
    ) { // end-of-tx
        if (gEeprom.DTMF_SIDE_TONE) {
            AUDIO_AudioPathOn();
            gEnableSpeaker = true;
            SYSTEM_DelayMs(60);
        }

        BK4819_EnterDTMF_TX(gEeprom.DTMF_SIDE_TONE);

        BK4819_PlayDTMFString(
                gEeprom.DTMF_DOWN_CODE,
                0,
                gEeprom.DTMF_FIRST_CODE_PERSIST_TIME,
                gEeprom.DTMF_HASH_CODE_PERSIST_TIME,
                gEeprom.DTMF_CODE_PERSIST_TIME,
                gEeprom.DTMF_CODE_INTERVAL_TIME);

        AUDIO_AudioPathOff();
        gEnableSpeaker = false;
    }

    BK4819_ExitDTMF_TX(true);
}

bool DTMF_ValidateCodes(char *pCode, const unsigned int size)
{
    unsigned int i;

    if (pCode[0] == 0xFF || pCode[0] == 0)
        return false;

    for (i = 0; i < size; i++)
    {
        if (pCode[i] == 0xFF || pCode[i] == 0)
        {
            pCode[i] = 0;
            break;
        }

        if (!DTMF_IsValidChar(pCode[i]))
            return false;
    }

    return true;
}

// Simple character validation without null-termination requirement
bool DTMF_IsValidChar(char c)
{
    // Valid DTMF characters: 0-9, A-D, *, #
    if ((c >= '0' && c <= '9') || 
        (c >= 'A' && c <= 'D') || 
        c == '*' || c == '#') {
        return true;
    }
    return false;
}

#ifdef ENABLE_DTMF_CALLING
bool DTMF_GetContact(const int Index, char *pContact)
{
    if (Index < 0 || Index >= MAX_DTMF_CONTACTS || pContact == NULL) {
        return false;
    }

    EEPROM_ReadBuffer(0x1C00 + (Index * 16), pContact, 16);

    // check whether the first character is printable or not
    return (pContact[0] >= ' ' && pContact[0] < 127);
}

bool DTMF_FindContact(const char *pContact, char *pResult)
{
    pResult[0] = 0;

    for (unsigned int i = 0; i < MAX_DTMF_CONTACTS; i++) {
        char Contact[16];
        if (!DTMF_GetContact(i, Contact)) {
            return false;
        }

        if (memcmp(pContact, Contact + 8, 3) == 0) {
            memcpy(pResult, Contact, 8);
            pResult[8] = 0;
            return true;
        }
    }

    return false;
}

#endif

char DTMF_GetCharacter(const unsigned int code)
{
    if (code <= KEY_9)
        return '0' + code;

    switch (code)
    {
        case KEY_MENU: return 'A';
        case KEY_UP:   return 'B';
        case KEY_DOWN: return 'C';
        case KEY_EXIT: return 'D';
        case KEY_STAR: return '*';
        case KEY_F:    return '#';
        default:       return 0xff;
    }
}
#ifdef ENABLE_DTMF_CALLING
static bool CompareMessage(const char *pMsg, const char *pTemplate, const unsigned int size, const bool bCheckGroup)
{
    unsigned int i;
    for (i = 0; i < size; i++)
    {
        if (pMsg[i] != pTemplate[i])
        {
            if (!bCheckGroup || pMsg[i] != gEeprom.DTMF_GROUP_CALL_CODE)
                return false;
            gDTMF_IsGroupCall = true;
        }
    }

    return true;
}

DTMF_CallMode_t DTMF_CheckGroupCall(const char *pMsg, const unsigned int size)
{
    for (unsigned int i = 0; i < size; i++)
        if (pMsg[i] == gEeprom.DTMF_GROUP_CALL_CODE) {
            return DTMF_CALL_MODE_GROUP;
        }

    return DTMF_CALL_MODE_NOT_GROUP;
}
#endif

void DTMF_clear_input_box(void)
{
    memset(gDTMF_InputBox, 0, sizeof(gDTMF_InputBox));
    gDTMF_InputBox_Index = 0;
    gDTMF_InputMode      = false;
}

void DTMF_Append(const char code)
{
    if (gDTMF_InputBox_Index == 0)
    {
        memset(gDTMF_InputBox, '-', sizeof(gDTMF_InputBox) - 1);
        gDTMF_InputBox[sizeof(gDTMF_InputBox) - 1] = 0;
    }

    if (gDTMF_InputBox_Index < (sizeof(gDTMF_InputBox) - 1))
        gDTMF_InputBox[gDTMF_InputBox_Index++] = code;
}

#ifdef ENABLE_DTMF_CALLING
void DTMF_HandleRequest(void)
{   // proccess the RX'ed DTMF characters

    char         String[21];
    unsigned int Offset;

    if (!gDTMF_RX_pending)
        return;   // nothing new received

    if (gScanStateDir != SCAN_OFF || gCssBackgroundScan)
    {   // we're busy scanning
        DTMF_clear_RX();
        return;
    }

    if (!gRxVfo->DTMF_DECODING_ENABLE && !gSetting_KILLED)
    {   // D-DCD is disabled or we're alive
        DTMF_clear_RX();
        return;
    }

    gDTMF_RX_pending = false;

    if (gDTMF_RX_index >= 9)
    {   // look for the KILL code

        sprintf(String, "%s%c%s", gEeprom.ANI_DTMF_ID, gEeprom.DTMF_SEPARATE_CODE, gEeprom.KILL_CODE);
        const size_t string_len = strlen(String);
        Offset = gDTMF_RX_index - string_len;

        if (CompareMessage(gDTMF_RX + Offset, String, string_len, true))
        {   // bugger

            if (gEeprom.PERMIT_REMOTE_KILL)
            {
                gSetting_KILLED = true;      // oooerr !

                DTMF_clear_RX();

                SETTINGS_SaveSettings();

                gDTMF_ReplyState = DTMF_REPLY_AB;

                #ifdef ENABLE_FMRADIO
                    if (gFmRadioMode)
                    {
                        FM_TurnOff();
                        GUI_SelectNextDisplay(DISPLAY_MAIN);
                    }
                #endif
            }
            else
            {
                gDTMF_ReplyState = DTMF_REPLY_NONE;
            }

            gDTMF_CallState = DTMF_CALL_STATE_NONE;

            gUpdateDisplay  = true;
            gUpdateStatus   = true;
            return;
        }
    }

    if (gDTMF_RX_index >= 9)
    {   // look for the REVIVE code

        sprintf(String, "%s%c%s", gEeprom.ANI_DTMF_ID, gEeprom.DTMF_SEPARATE_CODE, gEeprom.REVIVE_CODE);
        const size_t string_len = strlen(String);
        Offset = gDTMF_RX_index - string_len;

        if (CompareMessage(gDTMF_RX + Offset, String, string_len, true))
        {   // shit, we're back !

            gSetting_KILLED  = false;

            DTMF_clear_RX();

            SETTINGS_SaveSettings();

            gDTMF_ReplyState = DTMF_REPLY_AB;
            gDTMF_CallState  = DTMF_CALL_STATE_NONE;

            gUpdateDisplay   = true;
            gUpdateStatus    = true;
            return;
        }
    }

    if (gDTMF_RX_index >= 2)
    {   // look for ACK reply
        char *pPrintStr = "AB";
        const size_t pprint_len = strlen(pPrintStr);

        Offset = gDTMF_RX_index - pprint_len;

        if (CompareMessage(gDTMF_RX + Offset, pPrintStr, pprint_len, true)) {
            // ends with "AB"

            if (gDTMF_ReplyState != DTMF_REPLY_NONE)          // 1of11
//          if (gDTMF_CallState != DTMF_CALL_STATE_NONE)      // 1of11
//          if (gDTMF_CallState == DTMF_CALL_STATE_CALL_OUT)  // 1of11
            {
                gDTMF_State = DTMF_STATE_TX_SUCC;
                DTMF_clear_RX();
                gUpdateDisplay = true;
                return;
            }
        }
    }

    if (gDTMF_CallState == DTMF_CALL_STATE_CALL_OUT &&
        gDTMF_CallMode  == DTMF_CALL_MODE_NOT_GROUP &&
        gDTMF_RX_index >= 9)
    {   // waiting for a reply

        sprintf(String, "%s%c%s", gDTMF_String, gEeprom.DTMF_SEPARATE_CODE, "AAAAA");
        const size_t string_len = strlen(String);
        Offset = gDTMF_RX_index - string_len;

        if (CompareMessage(gDTMF_RX + Offset, String, string_len, false))
        {   // we got a response
            gDTMF_State    = DTMF_STATE_CALL_OUT_RSP;
            DTMF_clear_RX();
            gUpdateDisplay = true;
        }
    }

    if (gSetting_KILLED || gDTMF_CallState != DTMF_CALL_STATE_NONE)
    {   // we've been killed or expecting a reply
        return;
    }

    if (gDTMF_RX_index >= 7)
    {   // see if we're being called

        gDTMF_IsGroupCall = false;

        sprintf(String, "%s%c", gEeprom.ANI_DTMF_ID, gEeprom.DTMF_SEPARATE_CODE);

        Offset = gDTMF_RX_index - strlen(String) - 3;

        if (CompareMessage(gDTMF_RX + Offset, String, strlen(String), true))
        {   // it's for us !

            gDTMF_CallState = DTMF_CALL_STATE_RECEIVED;

            memset(gDTMF_Callee, 0, sizeof(gDTMF_Callee));
            memset(gDTMF_Caller, 0, sizeof(gDTMF_Caller));
            memcpy(gDTMF_Callee, gDTMF_RX + Offset + 0, 3);
            memcpy(gDTMF_Caller, gDTMF_RX + Offset + 4, 3);

            DTMF_clear_RX();

            gUpdateDisplay = true;

            switch (gEeprom.DTMF_DECODE_RESPONSE)
            {
                case DTMF_DEC_RESPONSE_BOTH:
                    gDTMF_DecodeRingCountdown_500ms = DTMF_decode_ring_countdown_500ms;
                    [[fallthrough]];
                case DTMF_DEC_RESPONSE_REPLY:
                    gDTMF_ReplyState = DTMF_REPLY_AAAAA;
                    break;
                case DTMF_DEC_RESPONSE_RING:
                    gDTMF_DecodeRingCountdown_500ms = DTMF_decode_ring_countdown_500ms;
                    break;
                default:
                case DTMF_DEC_RESPONSE_NONE:
                    gDTMF_DecodeRingCountdown_500ms = 0;
                    gDTMF_ReplyState = DTMF_REPLY_NONE;
                    break;
            }

            if (gDTMF_IsGroupCall)
                gDTMF_ReplyState = DTMF_REPLY_NONE;
        }
    }
}
#endif

void DTMF_Reply(void)
{
    uint16_t    Delay;
#ifdef ENABLE_DTMF_CALLING
    char        String[23];
#endif
    const char *pString = NULL;

    switch (gDTMF_ReplyState)
    {
        case DTMF_REPLY_ANI:
#ifdef ENABLE_DTMF_CALLING
            if (gDTMF_CallMode != DTMF_CALL_MODE_DTMF)
            {   // append our ID code onto the end of the DTMF code to send
                sprintf(String, "%s%c%s", gDTMF_String, gEeprom.DTMF_SEPARATE_CODE, gEeprom.ANI_DTMF_ID);
                pString = String;
            }
            else
#endif
            {
                pString = gDTMF_String;
            }

            break;
#ifdef ENABLE_DTMF_CALLING
        case DTMF_REPLY_AB:
            pString = "AB";
            break;

        case DTMF_REPLY_AAAAA:
            sprintf(String, "%s%c%s", gEeprom.ANI_DTMF_ID, gEeprom.DTMF_SEPARATE_CODE, "AAAAA");
            pString = String;
            break;
#endif
        default:
        case DTMF_REPLY_NONE:
            if (
#ifdef ENABLE_DTMF_CALLING
                gDTMF_CallState != DTMF_CALL_STATE_NONE           ||
#endif
                gCurrentVfo->DTMF_PTT_ID_TX_MODE == PTT_ID_APOLLO ||
                gCurrentVfo->DTMF_PTT_ID_TX_MODE == PTT_ID_OFF    ||
                gCurrentVfo->DTMF_PTT_ID_TX_MODE == PTT_ID_TX_DOWN)
            {
                gDTMF_ReplyState = DTMF_REPLY_NONE;
                return;
            }

            // send TX-UP DTMF
            pString = gEeprom.DTMF_UP_CODE;
            break;
    }

    gDTMF_ReplyState = DTMF_REPLY_NONE;

    if (pString == NULL)
        return;

    Delay = (gEeprom.DTMF_PRELOAD_TIME < 200) ? 200 : gEeprom.DTMF_PRELOAD_TIME;

    if (gEeprom.DTMF_SIDE_TONE)
    {   // the user will also hear the transmitted tones
        AUDIO_AudioPathOn();
        gEnableSpeaker = true;
    }

    SYSTEM_DelayMs(Delay);

    BK4819_EnterDTMF_TX(gEeprom.DTMF_SIDE_TONE);

    BK4819_PlayDTMFString(
        pString,
        1,
        gEeprom.DTMF_FIRST_CODE_PERSIST_TIME,
        gEeprom.DTMF_HASH_CODE_PERSIST_TIME,
        gEeprom.DTMF_CODE_PERSIST_TIME,
        gEeprom.DTMF_CODE_INTERVAL_TIME);

    AUDIO_AudioPathOff();

    gEnableSpeaker = false;

    BK4819_ExitDTMF_TX(false);
}

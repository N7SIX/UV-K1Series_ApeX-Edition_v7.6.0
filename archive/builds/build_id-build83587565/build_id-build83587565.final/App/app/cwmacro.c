/* Copyright 2026 NR7Y
 * https://github.com/briand
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

// CW Macro system implementation
// Ported from NR7Y UV-K5 (dp32g030) to ApeX UV-K1 (PY32F071)

#include "app/cwmacro.h"
#include "driver/eeprom.h"
#include "driver/uart.h"
#include "external/printf/printf.h"
#include "misc.h"
#include "ui/main.h"
#include <string.h>

// Debug logging control
#define CW_ENCODER_DEBUG 0
#define CW_MACRO_DEBUG 0

// Morse code lookup table
// Pattern: LSB first, 0=dit, 1=dah
typedef struct {
    char ch;
    uint8_t length;
    uint8_t pattern;
} MorseCode_t;

static const MorseCode_t MORSE_TABLE[] = {
    {'A', 2, 0b10},      // .-
    {'B', 4, 0b0001},    // -...
    {'C', 4, 0b0101},    // -.-.
    {'D', 3, 0b001},     // -..
    {'E', 1, 0b0},       // .
    {'F', 4, 0b0100},    // ..-.
    {'G', 3, 0b011},     // --.
    {'H', 4, 0b0000},    // ....
    {'I', 2, 0b00},      // ..
    {'J', 4, 0b1110},    // .---
    {'K', 3, 0b101},     // -.-
    {'L', 4, 0b0010},    // .-..
    {'M', 2, 0b11},      // --
    {'N', 2, 0b01},      // -.
    {'O', 3, 0b111},     // ---
    {'P', 4, 0b0110},    // .--.
    {'Q', 4, 0b1011},    // --.-
    {'R', 3, 0b010},     // .-.
    {'S', 3, 0b000},     // ...
    {'T', 1, 0b1},       // -
    {'U', 3, 0b100},     // ..-
    {'V', 4, 0b1000},    // ...-
    {'W', 3, 0b110},     // .--
    {'X', 4, 0b1001},    // -..-
    {'Y', 4, 0b1101},    // -.--
    {'Z', 4, 0b0011},    // --..
    {'0', 5, 0b11111},   // -----
    {'1', 5, 0b11110},   // .----
    {'2', 5, 0b11100},   // ..---
    {'3', 5, 0b11000},   // ...--
    {'4', 5, 0b10000},   // ....-
    {'5', 5, 0b00000},   // .....
    {'6', 5, 0b00001},   // -....
    {'7', 5, 0b00011},   // --...
    {'8', 5, 0b00111},   // ---..
    {'9', 5, 0b01111},   // ----.
    {'/', 5, 0b01001},   // -..-.
    {'?', 6, 0b001100},  // ..--..
    {'.', 6, 0b101010},  // .-.-.-
    {',', 6, 0b110011},  // --..--
    {'=', 5, 0b10001},   // -...-
    {'-', 6, 0b100001},  // -....-
    {'+', 5, 0b01010},   // .-.-.
    {'(', 5, 0b01101},   // -.--.
    {'&', 5, 0b00010}    // .-...
};

#define MORSE_TABLE_SIZE (sizeof(MORSE_TABLE) / sizeof(MORSE_TABLE[0]))

// Encoder state machine
static uint8_t s_encoder_pattern = 0;
static uint8_t s_encoder_length = 0;
static bool s_encoder_space_pending = false;
static bool s_encoder_overflow = false;

// Recording state
bool gCW_Recording = false;
uint8_t gCW_RecordMacroIndex = 0;
uint8_t gCW_RecordBuffer[CW_MACRO_MAX_LEN];
uint8_t gCW_RecordLength = 0;
bool gCW_RecordNewChar = false;

// Playback state
bool gCW_PlaybackActive = false;
bool gCW_PlaybackRepeat = false;
uint8_t gCW_PlaybackMacroIndex = 0;
uint16_t gCW_MessageRepeatCountdown_500ms = 0;

// TX display buffer
char gCW_TX_Display[CW_TX_DISPLAY_SIZE];
uint8_t gCW_TX_DisplayIndex = 0;
bool gCW_TX_DisplayUpdated = false;

static const uint16_t MACRO_ADDRS[CW_MACRO_COUNT] = {
    CW_MACRO1_EEPROM_ADDR,
    CW_MACRO2_EEPROM_ADDR,
    CW_MACRO3_EEPROM_ADDR,
    CW_MACRO4_EEPROM_ADDR
};

bool CW_ValidateChar(char ch)
{
    if (ch >= 'A' && ch <= 'Z') return true;
    if (ch >= '+' && ch <= '9') return true;
    if (ch == '&' || ch == '(' || ch == '=' || ch == '?') return true;
    return false;
}

uint8_t compute_macro_checksum(const uint8_t *block, uint8_t length)
{
    uint8_t sum = 0;
    for (uint8_t i = 1; i <= length; i++) {
        sum += block[i];
    }
    return sum;
}

uint8_t CW_GetMacroLength(uint8_t macroIndex)
{
    if (macroIndex >= CW_MACRO_COUNT)
        return 0;

    uint8_t raw_len;
    EEPROM_ReadBuffer(MACRO_ADDRS[macroIndex], &raw_len, 1);

    if (raw_len == 0xFF)
        return 0;

    if ((raw_len & CW_MACRO_SIG) == 0)
        return 0;

    uint8_t length = raw_len & ~CW_MACRO_SIG;
    if (length == 0 || length > CW_MACRO_MAX_LEN)
        return 0;

    uint8_t block[CW_MACRO_BLOCK_SIZE];
    EEPROM_ReadBuffer(MACRO_ADDRS[macroIndex], block, CW_MACRO_BLOCK_SIZE);
    uint8_t checksum = block[CW_MACRO_CHECKSUM_OFFSET];
    if (checksum != compute_macro_checksum(block, length))
        return 0;

    return length;
}

uint8_t CW_LoadMacro(uint8_t macroIndex, char *buffer, uint8_t bufferSize)
{
    if (macroIndex >= CW_MACRO_COUNT || buffer == NULL || bufferSize == 0)
        return 0;

    uint8_t length = CW_GetMacroLength(macroIndex);
    if (length == 0) {
        buffer[0] = '\0';
        return 0;
    }

    uint8_t block[CW_MACRO_BLOCK_SIZE];
    EEPROM_ReadBuffer(MACRO_ADDRS[macroIndex], block, CW_MACRO_BLOCK_SIZE);

    uint8_t outPos = 0;
    for (uint8_t i = 0; i < length && outPos < bufferSize - 1; i++) {
        if (CW_MACRO_HAS_SPACE(block[i+1])) {
            if (outPos < bufferSize - 1) {
                buffer[outPos++] = ' ';
            }
        }

        char ch = CW_MACRO_GET_CHAR(block[i+1]);
        if (outPos < bufferSize - 1) {
            buffer[outPos++] = ch;
        }
    }

    buffer[outPos] = '\0';
    return length;
}

void CW_SaveMacro(uint8_t macroIndex, const char *buffer, uint8_t length)
{
    if (macroIndex >= CW_MACRO_COUNT || buffer == NULL)
        return;

    if (length > CW_MACRO_MAX_LEN)
        length = CW_MACRO_MAX_LEN;

    uint8_t data[CW_MACRO_BLOCK_SIZE];
    memset(data, 0xFF, sizeof(data));

    data[0] = (length == 0) ? 0xFF : ((length & ~CW_MACRO_SIG) | CW_MACRO_SIG);

    for (uint8_t i = 0; i < length; i++) {
        data[i + 1] = buffer[i];
    }

    data[CW_MACRO_CHECKSUM_OFFSET] = compute_macro_checksum(data, length);

    for (uint8_t i = 0; i < CW_MACRO_BLOCK_SIZE; i += 8) {
        EEPROM_WriteBuffer(MACRO_ADDRS[macroIndex] + i, data + i);
    }
}

uint8_t CW_FormatMacroDisplay(uint8_t macroIndex, char *display, uint8_t maxChars)
{
    if (display == NULL || maxChars == 0)
        return 0;

    uint8_t length = CW_GetMacroLength(macroIndex);
    if (length == 0) {
        strcpy(display, "empty");
        return 0;
    }

    uint8_t data[10];
    EEPROM_ReadBuffer(MACRO_ADDRS[macroIndex] + 1, data, (length < 9) ? (length) : 9);

    uint8_t outPos = 0;
    uint8_t dispCount = 0;

    for (uint8_t i = 0; i < length && i < 9 && dispCount < 9; i++) {
        if (CW_MACRO_HAS_SPACE(data[i]) && dispCount < 9) {
            display[outPos++] = ' ';
            dispCount++;
        }
        if (dispCount < 9) {
            display[outPos++] = CW_MACRO_GET_CHAR(data[i]);
            dispCount++;
        }
    }

    outPos += sprintf_(display + outPos, "\n%u chars", length);

    return outPos;
}

static char CW_DecodePattern(uint8_t pattern, uint8_t length)
{
    if (length == 0 || length > 6)
        return 0;

    for (unsigned int i = 0; i < MORSE_TABLE_SIZE; i++) {
        if (MORSE_TABLE[i].length == length && MORSE_TABLE[i].pattern == pattern) {
            return MORSE_TABLE[i].ch;
        }
    }

    return 0;
}

bool CW_GetMorseForChar(char ch, uint8_t *pattern, uint8_t *length)
{
    if (pattern == NULL || length == NULL)
        return false;

    for (unsigned int i = 0; i < MORSE_TABLE_SIZE; i++) {
        if (MORSE_TABLE[i].ch == ch) {
            *pattern = MORSE_TABLE[i].pattern;
            *length = MORSE_TABLE[i].length;
            return true;
        }
    }
    return false;
}

void CW_EncoderProcessElement(CW_ElementType_t element)
{
    switch (element) {
    case CW_ELEMENT_DIT:
        if (s_encoder_length < 6) {
            s_encoder_length++;
        } else {
            s_encoder_overflow = true;
        }
        break;

    case CW_ELEMENT_DAH:
        if (s_encoder_length < 6) {
            s_encoder_pattern |= (1 << s_encoder_length);
            s_encoder_length++;
        } else {
            s_encoder_overflow = true;
        }
        break;

    case CW_ELEMENT_INTER_CHAR_SPACE:
        if (s_encoder_length > 0) {
            char ch = s_encoder_overflow ? 0 : CW_DecodePattern(s_encoder_pattern, s_encoder_length);
            if (ch != 0 && CW_ValidateChar(ch)) {
                const bool can_update_display = !gCW_Recording || (gCW_RecordLength < CW_MACRO_MAX_LEN);

                if (gCW_Recording && gCW_RecordLength < CW_MACRO_MAX_LEN) {
                    gCW_RecordBuffer[gCW_RecordLength++] = CW_MACRO_ENCODE(ch, s_encoder_space_pending);
                    gCW_RecordNewChar = true;
                }
                if (can_update_display) {
                    CW_AddToTxDisplay(ch, s_encoder_space_pending);
                }
            }

            s_encoder_pattern = 0;
            s_encoder_length = 0;
            s_encoder_space_pending = false;
            s_encoder_overflow = false;
        }
        break;

    case CW_ELEMENT_INTER_WORD_SPACE:
        s_encoder_pattern = 0;
        s_encoder_length = 0;
        s_encoder_space_pending = true;
        s_encoder_overflow = false;
        break;
    }
}

void CW_StartRecording(uint8_t macroIndex)
{
    if (macroIndex >= CW_MACRO_COUNT)
        return;

    gCW_RecordMacroIndex = macroIndex;
    gCW_RecordLength = 0;
    gCW_RecordNewChar = false;
    gCW_Recording = true;
    CW_ClearTxDisplay();

    s_encoder_pattern = 0;
    s_encoder_length = 0;
    s_encoder_space_pending = false;
    s_encoder_overflow = false;
}

void CW_StopRecording(void)
{
    if (!gCW_Recording)
        return;

    CW_SaveMacro(gCW_RecordMacroIndex, (const char *)gCW_RecordBuffer, gCW_RecordLength);

    gCW_Recording = false;
    gCW_RecordNewChar = false;
    CW_ClearTxDisplay();
}

void CW_AddToTxDisplay(char ch, bool hasSpace)
{
    if (hasSpace) {
        if (gCW_TX_DisplayIndex >= CW_TX_DISPLAY_SIZE - 1) {
            memmove(gCW_TX_Display, gCW_TX_Display + 1, CW_TX_DISPLAY_SIZE - 2);
            gCW_TX_DisplayIndex = CW_TX_DISPLAY_SIZE - 2;
        }
        gCW_TX_Display[gCW_TX_DisplayIndex++] = ' ';
        gCW_TX_Display[gCW_TX_DisplayIndex] = '\0';
    }

    if (gCW_TX_DisplayIndex >= CW_TX_DISPLAY_SIZE - 1) {
        memmove(gCW_TX_Display, gCW_TX_Display + 1, CW_TX_DISPLAY_SIZE - 2);
        gCW_TX_DisplayIndex = CW_TX_DISPLAY_SIZE - 2;
    }
    gCW_TX_Display[gCW_TX_DisplayIndex++] = ch;
    gCW_TX_Display[gCW_TX_DisplayIndex] = '\0';

    gUpdateDisplay = true;
}

void CW_ClearTxDisplay(void)
{
    memset(gCW_TX_Display, 0, CW_TX_DISPLAY_SIZE);
    gCW_TX_DisplayIndex = 0;
}

uint8_t CW_GetTxDisplayTail(char *display, uint8_t maxLen)
{
    if (display == NULL || maxLen == 0)
        return 0;

    const size_t len = strlen(gCW_TX_Display);
    const uint8_t copy_len = (len >= (size_t)(maxLen - 1)) ? (uint8_t)(maxLen - 1) : (uint8_t)len;
    const size_t idx = (len > copy_len) ? (len - copy_len) : 0;

    if (copy_len > 0) {
        memcpy(display, gCW_TX_Display + idx, copy_len);
    }
    display[copy_len] = '\0';
    return copy_len;
}
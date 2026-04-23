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

#include "driver/st7565.h"
#include "external/printf/printf.h"
#include "font.h"
#include "ui/helper.h"
#include "ui/inputbox.h"
#include "misc.h"

#ifndef ARRAY_SIZE
    #define ARRAY_SIZE(arr) (sizeof(arr)/sizeof((arr)[0]))
#endif

void UI_GenerateChannelString(char *pString, const uint8_t Channel)
{
    unsigned int i;

    if (gInputBoxIndex == 0)
    {
        sprintf(pString, "CH-%02u", Channel + 1);
        return;
    }

    pString[0] = 'C';
    pString[1] = 'H';
    pString[2] = '-';
    for (i = 0; i < 2; i++)
        pString[i + 3] = (gInputBox[i] == 10) ? '-' : gInputBox[i] + '0';
}

void UI_GenerateChannelStringEx(char *pString, const bool bShowPrefix, const uint8_t ChannelNumber)
{
    if (gInputBoxIndex > 0) {
        for (unsigned int i = 0; i < 3; i++) {
            pString[i] = (gInputBox[i] == 10) ? '-' : gInputBox[i] + '0';
        }

        pString[3] = 0;
        return;
    }

    if (bShowPrefix) {
        // BUG here? Prefixed NULLs are allowed
        sprintf(pString, "CH-%03u", ChannelNumber + 1);
    } else if (ChannelNumber == 0xFF) {
        // Always write 4 bytes: 'N','U','L','L' and null-terminate at pString[4]
        memcpy(pString, "NULL", 4);
        pString[4] = '\0';
    } else {
        sprintf(pString, "%03u", ChannelNumber + 1);
    }
}

void UI_PrintStringBuffer(const char *pString, uint8_t * buffer, uint32_t char_width, const uint8_t *font)
{
    const size_t Length = strlen(pString);
    const unsigned int char_spacing = char_width + 1;
    for (size_t i = 0; i < Length; i++) {
        const unsigned int index = pString[i] - ' ' - 1;
        if (pString[i] > ' ' && pString[i] < 127) {
            const uint32_t offset = i * char_spacing + 1;
            /* Defensive: ensure we don't write past the expected line width */
            if (offset + char_width <= LCD_WIDTH) {
                memcpy(buffer + offset, font + index * char_width, char_width);
            }
        }
    }
}

void UI_PrintString(const char *pString, uint8_t Start, uint8_t End, uint8_t Line, uint8_t Width)
{
    size_t i;
    size_t Length = strlen(pString);

    if (End > Start)
        Start += (((End - Start) - (Length * Width)) + 1) / 2;

    for (i = 0; i < Length; i++)
    {
        const unsigned int ofs   = (unsigned int)Start + (i * Width);
        if (pString[i] > ' ' && pString[i] < 127)
        {
            const unsigned int index = pString[i] - ' ' - 1;
            /* Bounds checks: ensure line indices and column offsets are valid */
            if (Line < FRAME_LINES && (Line + 1) < FRAME_LINES && ofs + 7 <= LCD_WIDTH) {
                memcpy(gFrameBuffer[Line + 0] + ofs, &gFontBig[index][0], 7);
                memcpy(gFrameBuffer[Line + 1] + ofs, &gFontBig[index][7], 7);
            }
        }
    }
}

void UI_PrintStringSmall(const char *pString, uint8_t Start, uint8_t End, uint8_t Line, uint8_t char_width, const uint8_t *font)
{
    const size_t Length = strlen(pString);
    const unsigned int char_spacing = char_width + 1;

    if (End > Start) {
        Start += (((End - Start) - Length * char_spacing) + 1) / 2;
    }

    UI_PrintStringBuffer(pString, gFrameBuffer[Line] + Start, char_width, font);
}


void UI_PrintStringSmallNormal(const char *pString, uint8_t Start, uint8_t End, uint8_t Line)
{
    UI_PrintStringSmall(pString, Start, End, Line, ARRAY_SIZE(gFontSmall[0]), (const uint8_t *)gFontSmall);
}

void UI_PrintStringSmallBold(const char *pString, uint8_t Start, uint8_t End, uint8_t Line)
{
#ifdef ENABLE_SMALL_BOLD
    const uint8_t *font = (uint8_t *)gFontSmallBold;
    const uint8_t char_width = ARRAY_SIZE(gFontSmallBold[0]);
#else
    const uint8_t *font = (uint8_t *)gFontSmall;
    const uint8_t char_width = ARRAY_SIZE(gFontSmall[0]);
#endif

    UI_PrintStringSmall(pString, Start, End, Line, char_width, font);
}

void UI_PrintStringSmallBufferNormal(const char *pString, uint8_t * buffer)
{
    UI_PrintStringBuffer(pString, buffer, ARRAY_SIZE(gFontSmall[0]), (uint8_t *)gFontSmall);
}

void UI_PrintStringSmallBufferBold(const char *pString, uint8_t * buffer)
{
#ifdef ENABLE_SMALL_BOLD
    const uint8_t *font = (uint8_t *)gFontSmallBold;
    const uint8_t char_width = ARRAY_SIZE(gFontSmallBold[0]);
#else
    const uint8_t *font = (uint8_t *)gFontSmall;
    const uint8_t char_width = ARRAY_SIZE(gFontSmall[0]);
#endif
    UI_PrintStringBuffer(pString, buffer, char_width, font);
}

void UI_DisplayFrequency(const char *string, uint8_t X, uint8_t Y, bool center)
{
    const unsigned int char_width  = 13;
    /* Defensive: check coordinates */
    if (Y >= FRAME_LINES || X >= LCD_WIDTH) return;

    uint8_t           *pFb0        = gFrameBuffer[Y] + X;
    uint8_t           *pFb1        = gFrameBuffer[(Y + 1) < FRAME_LINES ? (Y + 1) : Y] + X;
    bool               bCanDisplay = false;

    uint8_t len = strlen(string);
    for(int i = 0; i < len; i++) {
        char c = string[i];
        if(c=='-') c = '9' + 1;
        if (bCanDisplay || c != ' ')
        {
            bCanDisplay = true;
            if(c>='0' && c<='9' + 1) {
                /* Ensure we won't write past the line width */
                if (X + 2 + (char_width - 3) <= LCD_WIDTH && (Y + 1) < FRAME_LINES) {
                    memcpy(pFb0 + 2, gFontBigDigits[c-'0'],                  char_width - 3);
                    memcpy(pFb1 + 2, gFontBigDigits[c-'0'] + char_width - 3, char_width - 3);
                }
            }
            else if(c=='.') {
                *pFb1 = 0x60; pFb0++; pFb1++;
                *pFb1 = 0x60; pFb0++; pFb1++;
                *pFb1 = 0x60; pFb0++; pFb1++;
                continue;
            }

        }
        else if (center) {
            pFb0 -= 6;
            pFb1 -= 6;
        }
        pFb0 += char_width;
        pFb1 += char_width;
    }
}

/*
void UI_DisplayFrequency(const char *string, uint8_t X, uint8_t Y, bool center)
{
    const unsigned int char_width  = 13;
    uint8_t           *pFb0        = gFrameBuffer[Y] + X;
    uint8_t           *pFb1        = pFb0 + 128;
    bool               bCanDisplay = false;

    if (center) {
        uint8_t len = 0;
        for (const char *ptr = string; *ptr; ptr++)
            if (*ptr != ' ') len++; // Ignores spaces for centering

        X -= (len * char_width) / 2; // Centering adjustment
        pFb0 = gFrameBuffer[Y] + X;
        pFb1 = pFb0 + 128;
    }

    for (; *string; string++) {
        char c = *string;
        if (c == '-') c = '9' + 1; // Remap of '-' symbol

        if (bCanDisplay || c != ' ') {
            bCanDisplay = true;
            if (c >= '0' && c <= '9' + 1) {
                memcpy(pFb0 + 2, gFontBigDigits[c - '0'], char_width - 3);
                memcpy(pFb1 + 2, gFontBigDigits[c - '0'] + char_width - 3, char_width - 3);
            } else if (c == '.') {
                memset(pFb1, 0x60, 3); // Replaces the three assignments
                pFb0 += 3;
                pFb1 += 3;
                continue;
            }
        }
        pFb0 += char_width;
        pFb1 += char_width;
    }
}
*/

void UI_DrawPixelBuffer(uint8_t (*buffer)[128], uint8_t x, uint8_t y, bool black)
{
    /* Bounds-check coordinates to avoid out-of-bounds framebuffer writes. */
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT)
        return;

    const uint8_t pattern = 1 << (y % 8);
    if (black)
        buffer[y / 8][x] |= pattern;
    else
        buffer[y / 8][x] &= (uint8_t)~pattern;
}

static void sort(int16_t *a, int16_t *b)
{
    if(*a > *b) {
        int16_t t = *a;
        *a = *b;
        *b = t;
    }
}

#ifdef ENABLE_FEAT_N7SIX
    /*
    void UI_DrawLineDottedBuffer(uint8_t (*buffer)[128], int16_t x1, int16_t y1, int16_t x2, int16_t y2, bool black)
    {
        if(x2==x1) {
            sort(&y1, &y2);
            for(int16_t i = y1; i <= y2; i+=2) {
                UI_DrawPixelBuffer(buffer, x1, i, black);
            }
        } else {
            const int multipl = 1000;
            int a = (y2-y1)*multipl / (x2-x1);
            int b = y1 - a * x1 / multipl;

            sort(&x1, &x2);
            for(int i = x1; i<= x2; i+=2)
            {
                UI_DrawPixelBuffer(buffer, i, i*a/multipl +b, black);
            }
        }
    }
    */

    void PutPixel(uint8_t x, uint8_t y, bool fill) {
            UI_DrawPixelBuffer(gFrameBuffer, x, y, fill);
    }

    void PutPixelStatus(uint8_t x, uint8_t y, bool fill) {
            UI_DrawPixelBuffer(&gStatusLine, x, y, fill);
    }

    void GUI_DisplaySmallest(const char *pString, uint8_t x, uint8_t y,
                                    bool statusbar, bool fill) {
      uint8_t c;
      uint8_t pixels;
      const uint8_t *p = (const uint8_t *)pString;

      while ((c = *p++) && c != '\0') {
        c -= 0x20;
        for (int i = 0; i < 3; ++i) {
          pixels = gFont3x5[c][i];
          for (int j = 0; j < 6; ++j) {
            if (pixels & 1) {
              if (statusbar)
                PutPixelStatus(x + i, y + j, fill);
              else
                PutPixel(x + i, y + j, fill);
            }
            pixels >>= 1;
          }
        }
        x += 4;
      }
    }

    // Added S (index 24) and W (index 25)
    static const uint8_t gFont5x5[26][5] = {
        {0x04, 0x0A, 0x11, 0x1F, 0x11}, // A
        {0x1E, 0x11, 0x1E, 0x11, 0x1E}, // B
        {0x0E, 0x11, 0x10, 0x11, 0x0E}, // C
        {0x11, 0x11, 0x1F, 0x11, 0x11}, // H
        {0x1E, 0x11, 0x1E, 0x12, 0x11}, // R
        {0x11, 0x1B, 0x15, 0x11, 0x11}, // M
        {0x11, 0x11, 0x0A, 0x0A, 0x04}, // V
        {0x1F, 0x10, 0x1E, 0x10, 0x10}, // F
        {0x0E, 0x11, 0x11, 0x11, 0x0E}, // O
        {0x11, 0x19, 0x15, 0x13, 0x11}, // N
        {0x0E, 0x11, 0x11, 0x11, 0x0E}, // 0
        {0x04, 0x0C, 0x04, 0x04, 0x0E}, // 1 (Flipped Hook to Left) {0x04, 0x06, 0x04, 0x04, 0x0E}, // 1
        {0x0E, 0x11, 0x02, 0x04, 0x1F}, // 2
        {0x0E, 0x11, 0x06, 0x11, 0x0E}, // 3
        {0x02, 0x06, 0x0A, 0x1F, 0x02}, // 4
        {0x1F, 0x10, 0x1E, 0x01, 0x1E}, // 5
        {0x0E, 0x10, 0x1E, 0x11, 0x0E}, // 6
        {0x1F, 0x01, 0x02, 0x04, 0x04}, // 7
        {0x0E, 0x11, 0x0E, 0x11, 0x0E}, // 8
        {0x0E, 0x11, 0x0F, 0x01, 0x0E}, // 9
        {0x00, 0x04, 0x00, 0x04, 0x00}, // :
        {0x01, 0x02, 0x04, 0x08, 0x10}, // /
        {0x18, 0x19, 0x02, 0x13, 0x03}, // %
        {0x00, 0x00, 0x00, 0x0C, 0x0C},  // .
        {0x0E, 0x10, 0x0E, 0x01, 0x1E}, // S (index 24)
        {0x11, 0x11, 0x15, 0x1B, 0x11}  // W (index 25)
    };

    void UI_Draw5x5Char(char c, uint8_t x, uint8_t y, bool fill) {
        const uint8_t *glyph = NULL;
        if (c == 'A') glyph = gFont5x5[0];
        else if (c == 'B') glyph = gFont5x5[1];
        else if (c == 'C') glyph = gFont5x5[2];
        else if (c == 'H') glyph = gFont5x5[3];
        else if (c == 'R') glyph = gFont5x5[4];
        else if (c == 'a') glyph = gFont5x5[0];
        else if (c == 'h') glyph = gFont5x5[3];
        else if (c == 'M' || c == 'm') glyph = gFont5x5[5];
        else if (c == 'V') glyph = gFont5x5[6];
        else if (c == 'F') glyph = gFont5x5[7];
        else if (c == 'O') glyph = gFont5x5[8];
        else if (c == 'N') glyph = gFont5x5[9];
        else if (c >= '0' && c <= '9') glyph = gFont5x5[10 + (c - '0')];
        else if (c == ':') glyph = gFont5x5[20];
        else if (c == '/') glyph = gFont5x5[21];
        else if (c == '%') glyph = gFont5x5[22];
        else if (c == '.') glyph = gFont5x5[23];
        else if (c == 'S') glyph = gFont5x5[24];
        else if (c == 'W') glyph = gFont5x5[25];
        else if (c == ' ') return;
        if (!glyph) return;

        for (int row = 0; row < 5; ++row) {
            for (int col = 0; col < 5; ++col) {
                bool pixelOn = (glyph[row] >> (4 - col)) & 1;
                if (pixelOn) {
                    PutPixel(x + col, y + row, fill);
                }
            }
        }
    }

    void UI_Draw5x5String(const char *pString, uint8_t x, uint8_t y, bool fill) {
        while (*pString) {
            UI_Draw5x5Char(*pString, x, y, fill);
            x += 6; // 5 pixel char + 1 pixel spacing
            pString++;
        }
    }
#endif
    
void UI_DrawLineBuffer(uint8_t (*buffer)[128], int16_t x1, int16_t y1, int16_t x2, int16_t y2, bool black)
{
    if(x2==x1) {
        sort(&y1, &y2);
        for(int16_t i = y1; i <= y2; i++) {
            UI_DrawPixelBuffer(buffer, x1, i, black);
        }
    } else {
        const int multipl = 1000;
        int a = (y2-y1)*multipl / (x2-x1);
        int b = y1 - a * x1 / multipl;

        sort(&x1, &x2);
        for(int i = x1; i<= x2; i++)
        {
            UI_DrawPixelBuffer(buffer, i, i*a/multipl +b, black);
        }
    }
}

void UI_DrawRectangleBuffer(uint8_t (*buffer)[128], int16_t x1, int16_t y1, int16_t x2, int16_t y2, bool black)
{
    UI_DrawLineBuffer(buffer, x1,y1, x1,y2, black);
    UI_DrawLineBuffer(buffer, x1,y1, x2,y1, black);
    UI_DrawLineBuffer(buffer, x2,y1, x2,y2, black);
    UI_DrawLineBuffer(buffer, x1,y2, x2,y2, black);
}

void UI_FillRectangleBuffer(uint8_t (*buffer)[128], int16_t x1, int16_t y1, int16_t x2, int16_t y2, bool black)
{
    if (x1 > x2) { int16_t temp = x1; x1 = x2; x2 = temp; }
    if (y1 > y2) { int16_t temp = y1; y1 = y2; y2 = temp; }

    for (int16_t y = y1; y <= y2; y++) {
        for (int16_t x = x1; x <= x2; x++) {
            UI_DrawPixelBuffer(buffer, (uint8_t)x, (uint8_t)y, black);
        }
    }
}


void UI_DisplayPopup(const char *string)
{
    UI_DisplayClear();

    // for(uint8_t i = 1; i < 5; i++) {
    //  memset(gFrameBuffer[i]+8, 0x00, 111);
    // }

    // for(uint8_t x = 10; x < 118; x++) {
    //  UI_DrawPixelBuffer(x, 10, true);
    //  UI_DrawPixelBuffer(x, 46-9, true);
    // }

    // for(uint8_t y = 11; y < 37; y++) {
    //  UI_DrawPixelBuffer(10, y, true);
    //  UI_DrawPixelBuffer(117, y, true);
    // }
    // DrawRectangle(9,9, 118,38, true);
    UI_PrintString(string, 9, 118, 2, 8);
    UI_PrintStringSmallNormal("Press EXIT", 9, 118, 6);
}

void UI_DisplayClear()
{
    memset(gFrameBuffer, 0, sizeof(gFrameBuffer));
}

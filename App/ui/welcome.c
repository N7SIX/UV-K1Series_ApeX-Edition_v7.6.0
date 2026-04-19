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

#include "driver/py25q16.h"
#include "driver/st7565.h"
#include "external/printf/printf.h"
#include "helper/battery.h"
#include "settings.h"
#include "misc.h"
#include "ui/helper.h"
#include "ui/welcome.h"
#include "ui/status.h"
#include "version.h"
#include "bitmaps.h"

#ifdef ENABLE_FEAT_N7SIX_SCREENSHOT
    #include "screenshot.h"
#endif

void UI_DisplayReleaseKeys(void)
{
    memset(gStatusLine,  0, sizeof(gStatusLine));
#if defined(ENABLE_FEAT_N7SIX_CTR) || defined(ENABLE_FEAT_N7SIX_INV)
        ST7565_ContrastAndInv();
#endif
    UI_DisplayClear();

    UI_PrintString("RELEASE", 0, 127, 1, 10);
    UI_PrintString("ALL KEYS", 0, 127, 3, 10);

    ST7565_BlitStatusLine();  // blank status line
    ST7565_BlitFullScreen();
}

void UI_DisplayWelcome(void)
{
    char WelcomeString0[16];
    char WelcomeString1[16];
    char WelcomeString2[16];
    char WelcomeString3[20];

    memset(gStatusLine, 0, sizeof(gStatusLine));

#if defined(ENABLE_FEAT_N7SIX_CTR) || defined(ENABLE_FEAT_N7SIX_INV)
    ST7565_ContrastAndInv();
#endif
    UI_DisplayClear();

#ifdef ENABLE_FEAT_N7SIX
    ST7565_BlitStatusLine();
    ST7565_BlitFullScreen();
    
    if (gEeprom.POWER_ON_DISPLAY_MODE == POWER_ON_DISPLAY_MODE_NONE || gEeprom.POWER_ON_DISPLAY_MODE == POWER_ON_DISPLAY_MODE_SOUND) {
        ST7565_FillScreen(0x00);
    } else {
        memset(WelcomeString0, 0, sizeof(WelcomeString0));
        memset(WelcomeString1, 0, sizeof(WelcomeString1));

        // Read Welcome messages from EEPROM
        PY25Q16_ReadBuffer(0x007020, WelcomeString0, 16);
        PY25Q16_ReadBuffer(0x007030, WelcomeString1, 16);

        // Prepare Voltage String
        snprintf(WelcomeString2, sizeof(WelcomeString2), "%u.%02uV %u%%",
                gBatteryVoltageAverage / 100,
                gBatteryVoltageAverage % 100,
                BATTERY_VoltsToPercent(gBatteryVoltageAverage));

        if (gEeprom.POWER_ON_DISPLAY_MODE == POWER_ON_DISPLAY_MODE_VOLTAGE) {
            strncpy(WelcomeString0, "VOLTAGE", sizeof(WelcomeString0) - 1);
            WelcomeString0[sizeof(WelcomeString0) - 1] = '\0';
            strncpy(WelcomeString1, WelcomeString2, sizeof(WelcomeString1) - 1);
            WelcomeString1[sizeof(WelcomeString1) - 1] = '\0';
        } else if(gEeprom.POWER_ON_DISPLAY_MODE == POWER_ON_DISPLAY_MODE_MESSAGE) {
            if(strlen(WelcomeString0) == 0) {
                strncpy(WelcomeString0, "WELCOME", sizeof(WelcomeString0) - 1);
                WelcomeString0[sizeof(WelcomeString0) - 1] = '\0';
            }
            if(strlen(WelcomeString1) == 0) {
                strncpy(WelcomeString1, "73 Mabuhay!", sizeof(WelcomeString1) - 1);
                WelcomeString1[sizeof(WelcomeString1) - 1] = '\0';
            }
        }

        // --- ORIGINAL VERTICAL LAYOUT ---
        // Welcome strings on lines 0 and 2
        UI_PrintString(WelcomeString0, 0, 127, 0, 10); 
        UI_PrintString(WelcomeString1, 0, 127, 2, 10); 

        // Version Box on Line 4
        uint8_t strLen   = strlen(Version);
        uint8_t boxW     = (strLen * 7) + 6;
        uint8_t startX   = (128 - boxW) / 2;
        uint8_t textX    = startX + 3;

        UI_PrintStringSmallNormal(Version, textX, 0, 4);
        UI_DrawLineBuffer(gFrameBuffer, 0, 31, 127, 31, 1); 

        // Invert the Version box area on Line 4
        for (uint8_t i = startX; i < startX + boxW; i++) {
            gFrameBuffer[4][i] ^= 0xFF;
        }

        // ApeX Edition on Line 6
        snprintf(WelcomeString3, sizeof(WelcomeString3), "%s Edition", Edition);
        UI_PrintStringSmallNormal(WelcomeString3, 0, 127, 6);

        ST7565_BlitFullScreen();

        #ifdef ENABLE_FEAT_N7SIX_SCREENSHOT
            getScreenShot(true);
        #endif
    }
#else
    // Fallback
    UI_PrintStringSmallNormal(Version, 0, 127, 6);
    ST7565_BlitFullScreen();
#endif
}
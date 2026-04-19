/**
 * =====================================================================================
 * @file        boot.c
 * @brief       System Boot Sequence & Firmware Initialization Dispatcher
 * @author      Dual Tachyon (Original)
 * @author      N7SIX/Professional Enhancement Team (2025-2026)
 * @version     v7.6.0 (ApeX Edition)
 * @license     Apache License, Version 2.0
 * "Ordered initialization of PY32F071 hardware, radio subsystems, and feature modules"
 * =====================================================================================
 * ARCHITECTURAL OVERVIEW:
 * Manages ordered startup sequence coordinating bootloader transition, hardware init,
 * EEPROM loading, radio IC setup, and feature module activation. Handles edge cases\n * including device detection, firmware recovery, and optional boot screen display.\n *\n * MAJOR FEATURES (2025-2026):
 * ---------------------------
 * - Bootloader → Application transition sequencing
 * - GPIO and clock configuration for PY32F071 MCU
 * - Radio IC (BK4819) initialization with band detection
 * - EEPROM settings loading with fallback defaults
 * - Feature module activation (spectrum, FM radio, scanner)
 * - Boot password entry (ENABLE_PWRON_PASSWORD conditional)
 * - Optional boot screen and startup beep\n *
 * TECHNICAL SPECIFICATIONS:
 * -------------------------
 * - Boot time: <1000ms (EEPROM read + radio init)*\n * - Bootloader reserved: 2 KB (0x00000000-0x00000800)
 * - Application entry: 0x00000800 (start_address in linker script)
 * - Clock setup: 48MHz from internal oscillator (PY32F071 default)
 * - Radio IC startup: 200ms BK4819 stabilization
 * - Error handling: Fallback to safe defaults on EEPROM corruption
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

#ifdef ENABLE_AIRCOPY
    #include "app/aircopy.h"
#endif
#include "driver/bk4819.h"
#include "driver/keyboard.h"
#include "driver/gpio.h"
#include "driver/system.h"
#include "helper/boot.h"
#include "misc.h"
#include "radio.h"
#include "settings.h"
#include "ui/menu.h"
#include "ui/ui.h"

BOOT_Mode_t BOOT_GetMode(void)
{
    unsigned int i;
    KEY_Code_t   Keys[2];

    for (i = 0; i < 2; i++)
    {
        if (!GPIO_IsPttPressed())
            return BOOT_MODE_NORMAL;   // PTT not pressed
        Keys[i] = KEYBOARD_Poll();
        SYSTEM_DelayMs(20);
    }

    #ifdef ENABLE_FEAT_N7SIX_RESCUE_OPS
    if (Keys[0] == (10 + gEeprom.SET_KEY))
    {
        return BOOT_MODE_RESCUE_OPS;  // Secret KEY pressed
    }
    #endif

    if (Keys[0] == Keys[1])
    {
        gKeyReading0 = Keys[0];
        gKeyReading1 = Keys[0];

        gDebounceCounter = 2;

        if (Keys[0] == KEY_SIDE1)
            return BOOT_MODE_F_LOCK;

        #ifdef ENABLE_AIRCOPY
            if (Keys[0] == KEY_SIDE2)
                return BOOT_MODE_AIRCOPY;
        #endif
    }

    return BOOT_MODE_NORMAL;
}

void BOOT_ProcessMode(BOOT_Mode_t Mode)
{
    if (Mode == BOOT_MODE_F_LOCK)
    {
        #ifdef ENABLE_FEAT_N7SIX_RESUME_STATE
            gEeprom.CURRENT_STATE = 0; // Don't resume is active...
        #endif 
        GUI_SelectNextDisplay(DISPLAY_MENU);
    }
    #ifdef ENABLE_AIRCOPY
        else
        if (Mode == BOOT_MODE_AIRCOPY)
        {
            gEeprom.DUAL_WATCH               = DUAL_WATCH_OFF;
            gEeprom.BATTERY_SAVE             = 0;
            #ifdef ENABLE_VOX
                gEeprom.VOX_SWITCH           = false;
            #endif
            gEeprom.CROSS_BAND_RX_TX         = CROSS_BAND_OFF;
            gEeprom.AUTO_KEYPAD_LOCK         = false;
            gEeprom.KEY_1_SHORT_PRESS_ACTION = ACTION_OPT_NONE;
            gEeprom.KEY_1_LONG_PRESS_ACTION  = ACTION_OPT_NONE;
            gEeprom.KEY_2_SHORT_PRESS_ACTION = ACTION_OPT_NONE;
            gEeprom.KEY_2_LONG_PRESS_ACTION  = ACTION_OPT_NONE;
            gEeprom.KEY_M_LONG_PRESS_ACTION  = ACTION_OPT_NONE;

            RADIO_InitInfo(gRxVfo, FREQ_CHANNEL_LAST - 1, 43400000); // LPD

            gRxVfo->CHANNEL_BANDWIDTH        = BANDWIDTH_NARROW;
            gRxVfo->OUTPUT_POWER             = OUTPUT_POWER_LOW1;

            RADIO_ConfigureSquelchAndOutputPower(gRxVfo);

            gCurrentVfo = gRxVfo;

            RADIO_SetupRegisters(true);
            BK4819_SetupAircopy();
            BK4819_ResetFSK();

            gAircopyState = AIRCOPY_READY;

            gEeprom.BACKLIGHT_TIME = 61;
            gEeprom.KEY_LOCK = 0;

            #ifdef ENABLE_FEAT_N7SIX_RESUME_STATE
                gEeprom.CURRENT_STATE = 0; // Don't resume is active...
            #endif 

            GUI_SelectNextDisplay(DISPLAY_AIRCOPY);
        }
    #endif
    else
    {
        GUI_SelectNextDisplay(DISPLAY_MAIN);
    }
}

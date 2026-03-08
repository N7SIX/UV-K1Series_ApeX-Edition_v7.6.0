/* Copyright 2025 muzkr https://github.com/muzkr
 * Copyright 2023 Manuel Jinger
 * Copyright 2023 Dual Tachyon
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

#include "driver/gpio.h"
#include "driver/keyboard.h"
#include "driver/systick.h"
#include "driver/i2c.h"
#include "misc.h"

KEY_Code_t gKeyReading0     = KEY_INVALID;
KEY_Code_t gKeyReading1     = KEY_INVALID;
uint16_t   gDebounceCounter = 0;
bool       gWasFKeyPressed  = false;

#define GPIOx               GPIOB
#define PIN_MASK_COLS       (LL_GPIO_PIN_6 | LL_GPIO_PIN_5 | LL_GPIO_PIN_4 | LL_GPIO_PIN_3)
#define PIN_COLS            GPIO_MAKE_PIN(GPIOx, PIN_MASK_COLS)
#define PIN_COL(n)          GPIO_MAKE_PIN(GPIOx, 1u << (6 - (n)))

#define PIN_MASK_ROWS       (LL_GPIO_PIN_15 | LL_GPIO_PIN_14 | LL_GPIO_PIN_13 | LL_GPIO_PIN_12)
#define PIN_MASK_ROW(n)     (1u << (15 - (n)))

static inline uint32_t read_rows()
{
    return PIN_MASK_ROWS & LL_GPIO_ReadInputPort(GPIOx);
}

static const KEY_Code_t keyboard[5][4] = {
    {   // Zero col
        // Set to zero to handle special case of nothing pulled down
        KEY_SIDE1, 
        KEY_SIDE2, 

        // Duplicate to fill the array with valid values
        KEY_INVALID, 
        KEY_INVALID, 
    },
    {   // First col
        KEY_MENU, 
        KEY_1, 
        KEY_4, 
        KEY_7, 
    },
    {   // Second col
        KEY_UP, 
        KEY_2 , 
        KEY_5 , 
        KEY_8 , 
    },
    {   // Third col
        KEY_DOWN, 
        KEY_3   , 
        KEY_6   , 
        KEY_9   , 
    },
    {   // Fourth col
        KEY_EXIT, 
        KEY_STAR, 
        KEY_0   , 
        KEY_F   , 
    }
};

KEY_Code_t KEYBOARD_Poll(void)
{
    KEY_Code_t Key = KEY_INVALID;

    //  if (!GPIO_CheckBit(&GPIOC->DATA, GPIOC_PIN_PTT))
    //      return KEY_PTT;

    // *****************

    for (unsigned int j = 0; j < 5; j++)
    {
        uint32_t reg;
        unsigned int i;
        unsigned int k;

        // Set all high
        GPIO_SetOutputPin(PIN_COLS);

        // Clear the pin we are selecting
        if (j > 0)
            GPIO_ResetOutputPin(PIN_COL(j - 1));

        // Read all 4 GPIO pins at once .. with de-noise, max of 8 sample loops
        for (i = 0, k = 0, reg = 0; i < 3 && k < 8; i++, k++)
        {
            SYSTICK_DelayUs(1);
            uint32_t reg2 = read_rows();
            i *= reg == reg2;
            reg = reg2;
        }

        if (i < 3)
            break; // noise is too bad

        for (unsigned int i = 0; i < 4; i++)
        {
            if (!(reg & PIN_MASK_ROW(i)))
            {
                Key = keyboard[j][i];
                break;
            }
        }

        if (Key != KEY_INVALID)
            break;
    }

    return Key;
}

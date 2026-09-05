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

#ifdef ENABLE_FEAT_N7SIX_SCREENSHOT
// Packet types for serial key injection (K5Viewer → radio)
#define SERIAL_KEY_TYPE         0x03
#define SERIAL_KEY_TYPE_LONG    0x04

// Serial-injected key (written by UART/VCP parser, consumed by KEYBOARD_Poll).
volatile KEY_Code_t gKeyFromSerial = KEY_INVALID;

// Inject a short or long press from serial (UART or VCP).
// KEY_PTT is explicitly blocked — PTT release cannot be guaranteed over serial.
static inline void KEYBOARD_InjectKey(uint8_t keyCode, bool keyLong)
{
    (void)keyLong; // Reserved for future hold/repeat support
    if (keyCode < KEY_INVALID && keyCode != KEY_PTT) {
        gKeyFromSerial = (KEY_Code_t)keyCode;
    }
}

bool KEYBOARD_ProcessProtocolByte(ParseState_t *state, uint8_t b)
{
    bool connected = false;

    switch (*state)
    {
        case STATE_IDLE:
            if      (b == 0x55) *state = STATE_KA_1;
            else if (b == 0xAA) *state = STATE_KEY_1;
            break;
            
        case STATE_KA_1:
            *state = (b == 0xAA) ? STATE_KA_2 : STATE_IDLE;
            break;
            
        case STATE_KA_2:
            *state = (b == 0x00) ? STATE_KA_3
                       : (b == 0x05) ? STATE_KA_RFLOG
                       : STATE_IDLE;
            break;
            
        case STATE_KA_3:
            if (b == 0x00) connected = true;
            *state = STATE_IDLE;
            break;
            
        case STATE_KA_RFLOG:
            // UV Studio sends: 0x55 0xAA 0x05 <features>
            // features: 0x01=RF_LOG, 0x02=RF_LOG_HISTORY, 0x80=RESTART
            // We always respond with 0x03 (RF_LOG | RF_LOG_HISTORY) since
            // both are compiled in together under ENABLE_FEAT_N7SIX_RXTX_LOG_K5VIEWER.
            connected = true;
            *state = STATE_IDLE;
            break;
            
        case STATE_KEY_1:
            *state = (b == 0x55) ? STATE_KEY_2 : STATE_IDLE;
            break;
            
        case STATE_KEY_2:
            if      (b == SERIAL_KEY_TYPE)      *state = STATE_KEY_3;
            else if (b == SERIAL_KEY_TYPE_LONG) *state = STATE_KEY_3L;
            else                                *state = STATE_IDLE;
            break;
            
        case STATE_KEY_3:
        case STATE_KEY_3L:
            KEYBOARD_InjectKey(b, *state == STATE_KEY_3L);
            connected = true;
            *state = STATE_IDLE;
            break;

        default:
            *state = STATE_IDLE;
            break;
    }

    return connected;
}
#endif

#define GPIOx               GPIOB
#define PIN_MASK_COLS       (LL_GPIO_PIN_6 | LL_GPIO_PIN_5 | LL_GPIO_PIN_4 | LL_GPIO_PIN_3)
#define PIN_COLS            GPIO_MAKE_PIN(GPIOx, PIN_MASK_COLS)
// Column pins are inverted: col 0 = pin 6, col 1 = pin 5, col 2 = pin 4, col 3 = pin 3.
// This matches the PCB trace routing on UV-K1/K5 hardware.
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

    // Check serial-injected key first (from UART/VCP K5Viewer protocol).
    // Takes priority over physical keys for responsive remote control.
    // Note: gKeyFromSerial is NOT cleared here — it stays set so that
    // CheckKeys() can see it for multiple polls (needed for debounce).
    // CheckKeys() clears it via KEYBOARD_ConsumeSerialKey() after processing.
    if (gKeyFromSerial != KEY_INVALID) {
        return gKeyFromSerial;
    }

    // Scan all 5 columns - j=0 reads side keys (all columns high),
    // j=1..4 scans the 4x4 key matrix columns.
    for (unsigned int j = 0; j < 5; j++)
    {
        uint32_t reg;

        // Set all columns high first
        GPIO_SetOutputPin(PIN_COLS);

        // Clear the specific column we are selecting
        if (j > 0)
        {
            GPIO_ResetOutputPin(PIN_COL(j - 1));
        }

    // Settling time for line capacitance to discharge
    // Reduced from 15us to 10us — RC circuit on UV-K1/K5 PCB settles
    // within 5-8us given low trace capacitance. Saves 25us per poll.
    SYSTICK_DelayUs(10);
        reg = read_rows();

        // Check which row is pressed in this column
        for (unsigned int i = 0; i < 4; i++)
        {
            if (!(reg & PIN_MASK_ROW(i)))
            {
                Key = keyboard[j][i];
                break;
            }
        }

        if (Key != KEY_INVALID)
        {
            break;  // Found a pressed key, stop scanning
        }
    }

    // Always clean up GPIO state - set all columns high at end
    GPIO_SetOutputPin(PIN_COLS);

    return Key;
}

KEY_Code_t KEYBOARD_GetKey(void)
{
    KEY_Code_t btn = KEYBOARD_Poll();
    if (btn == KEY_INVALID && GPIO_IsPttPressed())
    {
        btn = KEY_PTT;
    }
    return btn;
}

void HideFKeyIcon(void) {
    gWasFKeyPressed       = false;
    gUpdateStatus         = true;
}

#ifdef ENABLE_FEAT_N7SIX_SCREENSHOT
// Clear the serial-injected key after CheckKeys() has processed it.
// Called by CheckKeys() after the key has passed debounce and is dispatched.
void KEYBOARD_ConsumeSerialKey(void)
{
    gKeyFromSerial = KEY_INVALID;
}
#endif
/* Copyright 2026 NR7Y
 * https://github.com/briand
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

// Hardware input helpers for CW keyer (port config, debounced reads, etc.)
// Ported from NR7Y UV-K5 (dp32g030) to ApeX UV-K1 (PY32F071)

#include <stdint.h>
#include <stdbool.h>

#include "app/cwhardware.h"
#include "app/cw.h"
#include "driver/gpio.h"
#include "driver/systick.h"

// Local state for debounce counters and debounced edge tracking
static bool     s_last_dit   = false;
static bool     s_last_dah   = false;
static uint32_t s_dit_count  = 0;
static uint32_t s_dah_count  = 0;
static bool     s_last_is_dah = false;

// Generic GPIO deglitch function using PY32F071 LL API
static bool CW_ReadGpioDeglitched(GPIO_TypeDef *gpio, uint32_t pin, bool heavy)
{
    bool result = false;
    uint32_t reg = 0, reg2;
    unsigned int i, k;
    uint32_t limit = heavy ? 250 : 50;
    uint32_t goal = heavy ? 150 : 30;

    for (i = 0, k = 0, reg = 0; i < goal && k < limit; i++, k++) {
        SYSTICK_DelayUs(5);
        reg2 = LL_GPIO_IsInputPinSet(gpio, pin) ? 1u : 0u;
        i *= (reg == reg2);
        reg = reg2;
    }

    if (i >= goal) {
        result = !reg;
    }

    return result;
}

// Read raw paddle inputs for a specific mode
bool CW_ReadKeysForMode(uint8_t mode, bool *dit_out, bool *dah_out)
{
    if ((mode & CW_KEY_FLAG_NO_KEYER) && !(mode & CW_KEY_FLAG_PORT_GROUND)) {
        return false;
    }

    bool hw_tip = false;
    bool hw_ring = false;

    hw_tip = !LL_GPIO_IsInputPinSet(GPIOB, LL_GPIO_PIN_10);

    if (mode & CW_KEY_FLAG_SIDE1) {
        hw_ring = CW_ReadGpioDeglitched(GPIOA, LL_GPIO_PIN_3, true);
    }

    if (mode & CW_KEY_FLAG_PORT_RING) {
        bool port_ring = CW_ReadGpioDeglitched(GPIOB, LL_GPIO_PIN_11, true);
        hw_ring = hw_ring || port_ring;
    }

    bool reverse = (mode & CW_KEY_FLAG_REVERSED) != 0;

    *dit_out = reverse ? hw_ring : hw_tip;
    *dah_out = reverse ? hw_tip : hw_ring;

    return true;
}

// Read GPIO inputs based on configured mode
void CW_ReadKeys(CW_Input *in)
{
    bool n_dit = false;
    bool n_dah = false;

    if (!CW_ReadKeysForMode(gEeprom.CW_KEY_INPUT, &n_dit, &n_dah)) {
        n_dit = false;
        n_dah = false;
    }

    if (n_dit) s_dit_count++; else s_dit_count = 0;
    if (n_dah) s_dah_count++; else s_dah_count = 0;

    bool deb_dit = (s_dit_count >= 3);
    bool deb_dah = (s_dah_count >= 3);

    in->dit_rise = (!s_last_dit && deb_dit);
    in->dah_rise = (!s_last_dah && deb_dah);
    in->dit      = deb_dit;
    in->dah      = deb_dah;

    if (in->dit_rise) s_last_is_dah = false;
    else if (in->dah_rise) s_last_is_dah = true;
    in->last_is_dah = s_last_is_dah;

    s_last_dit = deb_dit;
    s_last_dah = deb_dah;
}

// Reset sampled key states
void CW_HW_ResetKeySamples(void)
{
    s_last_dit   = false;
    s_last_dah   = false;
    s_dit_count  = 0;
    s_dah_count  = 0;
    s_last_is_dah = false;
}

// CEC paddle via ADC is deferred: PY32F071 SARADC HAL layer needs new integration.
// Stubs keep build/link green until HAL API is confirmed.
void CW_ReadADCkeys(bool *tip_out, bool *ring_out)
{
    if (tip_out) *tip_out = false;
    if (ring_out) *ring_out = false;
}

void CW_ConfigureADCforCECPaddles(bool enable)
{
    (void)enable;
}

// Stubs for keyer/pin config paths not needed on PY32F071 GPIO-only setup
void CW_ConfigurePortGround(bool enable)
{
    (void)enable;
}

void CW_ConfigurePortRing(bool enable)
{
    (void)enable;
}
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

#ifndef APP_CWHARDWARE_H
#define APP_CWHARDWARE_H

#include <stdint.h>
#include <stdbool.h>
#include "app/cw.h"
#include "settings.h"

#define CW_ADC_20K_MIN 100
#define CW_ADC_10K_MIN 200
#define CW_ADC_MAX 1000
#define CW_ADC_GLITCH_GUARDBAND 20
#define CW_ADC_RANGE_LIMIT 50

// Read raw inputs for a specific mode
bool CW_ReadKeysForMode(uint8_t mode, bool *dit_out, bool *dah_out);

// Read normalized paddle inputs (computes edges)
void CW_ReadKeys(CW_Input *in);

// Read raw ADC value for CEC cable input
uint16_t CW_ReadCH3(void);

// Sample ADC for CEC inputs
void CW_ReadADCkeys(bool *tip_out, bool *ring_out);

// Configure port pins for paddle interface
void CW_ConfigurePortGround(bool enable);
void CW_ConfigurePortRing(bool enable);
void CW_ConfigureADCforCECPaddles(bool enable);

// Reset hardware-sampled state
void CW_HW_ResetKeySamples(void);

#endif // APP_CWHARDWARE_H
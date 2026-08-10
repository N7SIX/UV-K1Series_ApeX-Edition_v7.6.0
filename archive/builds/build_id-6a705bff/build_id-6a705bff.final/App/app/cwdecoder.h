/* Copyright 2026 Sean, N7SIX
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

#pragma once

#include <stdint.h>
#include <stdbool.h>

#define CW_RX_ACTIVATE_TICKS   4
#define CW_RX_DEACTIVATE_TICKS 8

#ifdef __cplusplus
extern "C" {
#endif

// Call once at startup/reset
void CW_Decoder_Init(void);
void CW_Decoder_Reset(bool clearDecodedText);

// Call from CW main tick
void CW_Decoder_ProcessTick(int16_t rssi_dBm);

// Status used by UI/TX layers
bool CW_Decoder_IsToneActive(void);
bool CW_Decoder_IsRxActive(void);
const char *CW_Decoder_GetDecodedText(void);
const char *CW_Decoder_GetCurrentMorse(void);
uint8_t CW_Decoder_GetCharConfidence(void);

// Drawing owned by decoder
void CW_Decoder_DrawSignalGraph(void);

#ifdef __cplusplus
}
#endif
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

// CW application-level update loop and end-of-transmission handling
/* CW Application-level update and TX state management */
/* Adapted from NR7Y's CW implementation for ApeX-Edition PY32F071 platform */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// CW application states
typedef enum {
    CW_INACTIVE = 0,
    CW_TRANSMITTING,
    CW_SUSPENDED
} CW_State_t;

// CW action codes returned by the keyer/playback engine
typedef enum {
    CW_ACTION_NONE = 0,
    CW_ACTION_CARRIER_ON,
    CW_ACTION_CARRIER_OFF,
    CW_ACTION_CARRIER_HOLD_ON
} CW_Action_t;

// Initialize CW application state
void CW_AppInit(void);

// Main update loop - call at 1ms intervals from scheduler
void CW_AppUpdate(void);

// End CW transmission immediately
void CW_EndTxNow(void);

// Get current CW state
CW_State_t CW_GetState(void);

// Check if CW is active (transmitting or suspended)
bool CW_IsActive(void);

#ifdef __cplusplus
}
#endif
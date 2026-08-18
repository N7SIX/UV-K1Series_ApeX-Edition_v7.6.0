/* Copyright 2026 Sean, N7SIX
 * https://github.com/N7SIX
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
 *
 * Centralized host-side test shim for settings.h.
 */

#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdbool.h>
#include <stdint.h>

/* Frequency-lock modes referenced by TX_freq_check() in frequencies.c */
enum {
    F_LOCK_DEF,
    F_LOCK_FCC,
    F_LOCK_CE,
    F_LOCK_GB,
    F_LOCK_430,
    F_LOCK_438,
    F_LOCK_ALL,   // disable TX on all frequencies
    F_LOCK_NONE,  // enable TX on all frequencies
    F_LOCK_LEN
};

enum {
    TX_OFFSET_FREQUENCY_DIRECTION_OFF = 0,
    TX_OFFSET_FREQUENCY_DIRECTION_ADD,
    TX_OFFSET_FREQUENCY_DIRECTION_SUB
};

extern uint8_t gSetting_F_LOCK;
extern bool    gSetting_350EN;
extern bool    gSetting_200TX;
extern bool    gSetting_350TX;
extern bool    gSetting_500TX;

#endif
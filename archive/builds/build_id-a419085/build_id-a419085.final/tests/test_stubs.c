/* Copyright 2026 Sean, N7SIX
 * https://github.com/N7SIX
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
 *
 * Host-side test stubs.
 *
 * The pure-logic modules under test (frequencies.c, dcs.c, driver/crc.c)
 * reference a small number of globals that are normally defined in the
 * firmware's misc.c / settings.c. This file provides host-side definitions
 * so the modules can be compiled and linked into the unit-test binary.
 *
 * These stubs are ONLY used by the test build; they are never linked into
 * the firmware.
 */

#include <stdint.h>
#include <stdbool.h>

/* Referenced by TX_freq_check() in frequencies.c */
uint8_t gSetting_F_LOCK;
bool    gSetting_350EN;
bool    gSetting_200TX;
bool    gSetting_350TX;
bool    gSetting_500TX;

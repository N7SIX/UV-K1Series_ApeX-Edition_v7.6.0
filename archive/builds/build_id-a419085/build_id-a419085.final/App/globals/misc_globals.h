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
 */

#ifndef GLOBALS_MISC_H
#define GLOBALS_MISC_H

#include <stdbool.h>
#include <stdint.h>

#include "system_globals.h"   // gSerialConfigCountDown_500ms

#ifndef MAX
    #define MAX(a, b) ({ __typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a > _b ? _a : _b; })
#endif

#ifndef MIN
    #define MIN(a, b) ({ __typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a < _b ? _a : _b; })
#endif

#ifndef SWAP
    #define SWAP(a, b) ({ __typeof__ (a) _c = (a);  a = b; b = _c; })
#endif

int32_t NUMBER_AddWithWraparound(int32_t Base, int32_t Add, int32_t LowerLimit, int32_t UpperLimit);
unsigned long StrToUL(const char * str);

void FUNCTION_NOP();

static inline bool SerialConfigInProgress() { return gSerialConfigCountDown_500ms != 0; }

#endif
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

#ifndef GLOBALS_UI_H
#define GLOBALS_UI_H

#include <stdbool.h>
#include <stdint.h>

extern uint8_t               gKeyInputCountdown;
extern uint8_t               gKeyLockCountdown;
extern uint8_t               gRTTECountdown_10ms;
extern bool                  bIsInLockScreen;
extern uint8_t               gUpdateStatus;

extern uint16_t              gMenuCountdown;
extern bool                  gFlagAcceptSetting;   // accept menu setting
extern bool                  gFlagRefreshSetting;  // refresh menu display

extern bool                  gKeyBeingHeld;
extern bool                  gPttIsPressed;
extern uint8_t               gPttDebounceCounter;
extern uint8_t               gMenuListCount;

extern bool                  gUpdateDisplay;
extern bool                  gF_LOCK;
extern uint8_t               gIsLocked;

#ifdef ENABLE_FEAT_N7SIX
    extern bool                  gK5startup;
    extern bool                  gBackLight;
    extern bool                  gMute;
    extern uint8_t               gBacklightTimeOriginal;
    extern uint8_t               gBacklightBrightnessOld;
    extern uint8_t               gSquelchLevelOriginal;
    extern uint8_t               gPttOnePushCounter;
    extern uint32_t              gBlinkCounter;
#endif

#endif
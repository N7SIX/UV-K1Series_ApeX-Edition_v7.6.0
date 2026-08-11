/* Copyright 2023 Dual Tachyon
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

#ifndef MISC_H
#define MISC_H

/*
 * Aggregator header.
 *
 * The global declarations that previously lived directly in this file have
 * been split into scoped modules under App/globals/ for better organization
 * and maintainability. This header remains as a thin aggregator so that all
 * existing `#include "misc.h"` sites continue to work unchanged.
 *
 * Scoped modules:
 *   globals/channel_globals.h  - channel / memory-channel cache globals
 *   globals/radio_globals.h    - VFO, scan, squelch, dual-watch, NOAA globals
 *   globals/settings_globals.h - user settings / EEPROM-backed globals
 *   globals/system_globals.h   - timers, power-save, timeslice globals
 *   globals/ui_globals.h       - UI / keypad / display state globals
 *   globals/misc_globals.h     - utility macros, helpers, misc functions
 *
 * No declarations were changed, removed, or re-typed; this is a pure
 * organizational refactor with no behavior, EEPROM, calibration, or UX change.
 */

#include "globals/channel_globals.h"
#include "globals/radio_globals.h"
#include "globals/settings_globals.h"
#include "globals/system_globals.h"
#include "globals/ui_globals.h"
#include "globals/misc_globals.h"

#endif
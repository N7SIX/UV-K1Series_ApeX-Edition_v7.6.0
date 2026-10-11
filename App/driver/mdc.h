/* Copyright 2026 N7SIX
 * https://github.com/n7six
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
 * ApeX-only MDC roger variants (MENU > Roger: MDC-EP / MDC-2P / MDC-EL).
 *
 * Self-contained module: all new ApeX roger code lives here (and in
 * driver/mdc.c) so the upstream driver files driver/bk4829.c and
 * driver/bk4819.h keep their original copyrights pristine — they only
 * carry a minimal dispatch hook each.
 *
 * Gated by N7SIX_APEX (ApeX edition only).
 */

#ifndef DRIVER_MDC_H
#define DRIVER_MDC_H

#include "driver/bk4819.h"

#ifdef N7SIX_APEX

// Dispatcher called from BK4819_PlayRoger() (see the hook in driver/bk4829.c).
// Sole public entry: the three variant bodies are static inside driver/mdc.c.
void MDC_PlayApexRoger(BK4819_FilterBandwidth_t Bandwidth);

#endif /* N7SIX_APEX */

#endif /* DRIVER_MDC_H */

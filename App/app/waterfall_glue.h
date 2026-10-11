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
 * Integration glue that mounts app/waterfall.c onto the resident
 * spectrum app WITHOUT modifying spectrum.c (its upstream copyright
 * must stay pristine).
 *
 * sweep completion is detected from the outside by watching the
 * exported newScanStart flag; rows are read from the exported
 * rssiHistory[] buffer; rendering is driven from a hook placed at the
 * top of the external ST7565_BlitLine() (driver/st7565.c).
 *
 * Gated by ENABLE_FEAT_N7SIX_WATERFALL (ApeX only); every hook is
 * compiled out everywhere else.
 */

#ifndef WATERFALL_GLUE_H
#define WATERFALL_GLUE_H

#ifdef ENABLE_FEAT_N7SIX_WATERFALL

/* Called around APP_RunSpectrum() at its two call sites (app/main.c
 * and main.c).  Enter() initializes the waterfall history; Exit() clears the strip and waterfall pages so the next screen starts clean. */
void WATERFALL_GlueEnter(void);
void WATERFALL_GlueExit(void);

/* Called from driver/st7565.c ST7565_BlitLine(), before the page is sent to the LCD.  Safe to call on every line; does nothing until
 * WATERFALL_GlueEnter() has run. */
void WATERFALL_GluePreBlit(unsigned line);

#endif /* ENABLE_FEAT_N7SIX_WATERFALL */

#endif /* WATERFALL_GLUE_H */
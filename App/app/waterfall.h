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
 * Professional 16-level grayscale dithered waterfall for the bottom 1/4 LCD area.
 *
 * LCD geometry (ST7565, 128x64, page-mapped):
 *   gFrameBuffer[4] = page 5 = lines 40-47 = frequency strip (start | center +-step | end)
 *   gFrameBuffer[5]+[6] = pages 6-7 = lines 48-63 = 16-row waterfall (1/4 LCD)
 *
 * Waterfall history: circular buffer, 128 columns x 16 rows, 4-bit/pixel packed.
 * Grayscale via 4x4 Bayer ordered dither.
 */

#ifndef WATERFALL_H
#define WATERFALL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Constants                                                          */
/* ------------------------------------------------------------------ */

#define WATERFALL_WIDTH     128
#define WATERFALL_HEIGHT    16
#define WATERFALL_LEVELS    16
#define WATERFALL_PAGE0_IDX  5   /* gFrameBuffer[5] = lines 48-55: waterfall top page (+1 = 56-63) */
#define WATERFALL_STRIP_PAGE_IDX 4  /* gFrameBuffer[4] = lines 40-47: freq strip */
#define WATERFALL_STRIP_Y        33 /* glyph y in strip page -> LCD row 41 */
#define WATERFALL_RSSI_MAX      65535u

/* INVARIANT: waterfallHistory is ONLY accessed from the main loop
 * (waterfall render/Tick). No ISR, DMA, or nested context reads or
 * writes this buffer. If adding DMA SPI or USB ISR access, you MUST
 * add synchronization. */
#define WATERFALL_ROW_10MS      32      /* default: 32 × 10ms = 320ms */
#define WATERFALL_ROW_10MS_DEFAULT  32

/* ------------------------------------------------------------------ */
/* API                                                                 */
/* ------------------------------------------------------------------ */

/* Initialize/clear waterfall history buffer */
void WATERFALL_Init(void);

void WATERFALL_SetDbRange(int dbMin, int dbMax);
void WATERFALL_SetRowInterval(uint8_t interval_10ms);
uint8_t WATERFALL_GetRowInterval(void);

/* Push one sweep row into the waterfall history.
 * rssiRow: array of RAW RSSI measurements (may have fewer than 128 valid entries).
 * bars:    number of valid entries in rssiRow (1..128).
 *          The row is linearly interpolated across the full 128 waterfall columns
 *          so the waterfall always uses the entire LCD width regardless of zoom.
 * Called from spectrum.c FinalizeCompletedSweep(). */
void WATERFALL_PushRow(const uint16_t *rssiRow, uint16_t bars);

/* Push a listen-mode row: one column boosted to signal brightness, rest
 * kept at normal scan brightness for seamless noise-floor continuation.
 * RSSI_MAX_VALUE columns are kept black. */
void WATERFALL_PushRowListen(const uint16_t *rssiRow, uint16_t bars,
                             uint16_t peakIndex, uint16_t peakRssi);

/* Render waterfall into gFrameBuffer[5..6] using 4x4 Bayer dither.
 * Call after spectrum renders, before ST7565_BlitLine(). */
void WATERFALL_Render(void);

#ifdef __cplusplus
}
#endif

#endif /* WATERFALL_H */
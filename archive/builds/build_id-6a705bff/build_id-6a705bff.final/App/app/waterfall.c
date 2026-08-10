/* Copyright 2025 Sean, N7SIX
 * Professional 16-level grayscale dithered waterfall for the bottom 1/4 LCD area.
 * Optimized with persistence/decay effect.
 *
 * Waterfall layout (ST7565, 128x64, page-mapped):
 * gFrameBuffer[5] (page 6) = lines 48-55
 * gFrameBuffer[6] (page 7) = lines 56-63
 *
 * Rendering uses 4x4 Bayer ordered dither to simulate 16 gray levels.
 */

#include "waterfall.h"
#include "driver/st7565.h"  /* gFrameBuffer */
#include "misc.h"
#include "../radio.h"
#include <string.h>

/* ================================================================== */
/* Private data                                                       */
/* ================================================================== */

static uint8_t waterfallHistory[(WATERFALL_WIDTH * WATERFALL_HEIGHT) / 2];
static uint8_t waterfallWriteRow;
static int waterfallDbMin = -130;
static int waterfallDbMax = -50;

/* Listen-mode signal persistence: keep boosting the last signal column
 * for a few cycles after the signal drops, so weak/intermittent signals
 * are still visible. Persistence decays: first cycle keeps signal
 * brightness (capped), subsequent cycles fade to background. */
static uint8_t persistCol = 0;
static uint8_t persistCount = 0;
#define PERSIST_CYCLES 3

/* Waterfall row push interval in 10ms ticks.  Narrower bandwidth scans
 * take longer per sweep, so the caller may reduce this to keep the
 * waterfall refresh rate proportional to sweep speed.  Default 320ms. */
static uint8_t waterfallRowInterval = WATERFALL_ROW_10MS_DEFAULT;

/* Exported dB range so spectrum.c can keep waterfall synchronized. */
static int waterfallDbMinSettings = -130;
static int waterfallDbMaxSettings = -50;

static const uint8_t bayer4x4[16] = {
      0,  8,  2, 10,
     12,  4, 14,  6,
      3, 11,  1,  9,
     15,  7, 13,  5
};

/* ================================================================== */
/* Internal helpers                                                   */
/* ================================================================== */

static uint8_t dbmToLevel(int dbm)
{
    int range = waterfallDbMax - waterfallDbMin;
    if (range <= 0) return 0;
    int clamped = (dbm < waterfallDbMin) ? waterfallDbMin :
                  (dbm > waterfallDbMax) ? waterfallDbMax : dbm;
    int level = ((clamped - waterfallDbMin) * (WATERFALL_LEVELS - 1) + (range >> 1)) / range;
    return (uint8_t)(level & 0x0F);
}

static int rssiToDbm(uint16_t rssi)
{
    if (rssi == WATERFALL_RSSI_MAX)
        return waterfallDbMin - 10; /* well below range -> level 0 */
    if (gRxVfo == NULL)
        return waterfallDbMin; /* safe fallback if VFO not initialized */
    return (int)(rssi >> 1) - 160 + dBmCorrTable[gRxVfo->Band];
}

static void HistorySetPixel(uint8_t col, uint8_t row, uint8_t level)
{
    unsigned idx = ((unsigned)row * WATERFALL_WIDTH + col) >> 1;
    if (col & 1)
        waterfallHistory[idx] = (waterfallHistory[idx] & 0xF0) | (level & 0x0F);
    else
        waterfallHistory[idx] = (waterfallHistory[idx] & 0x0F) | ((level & 0x0F) << 4);
}

static uint8_t HistoryGetPixel(uint8_t col, uint8_t row)
{
    unsigned idx = ((unsigned)row * WATERFALL_WIDTH + col) >> 1;
    return (col & 1) ? (waterfallHistory[idx] & 0x0F) : (waterfallHistory[idx] >> 4);
}

/* ================================================================== */
/* Public API                                                         */
/* ================================================================== */

void WATERFALL_Init(void)
{
    memset(waterfallHistory, 0, sizeof(waterfallHistory));
    waterfallWriteRow = 0;
    waterfallDbMin = waterfallDbMinSettings;
    waterfallDbMax = waterfallDbMaxSettings;
    waterfallRowInterval = WATERFALL_ROW_10MS_DEFAULT;
}

void WATERFALL_SetRowInterval(uint8_t interval_10ms)
{
    if (interval_10ms < 4) interval_10ms = 4;    // min 40ms
    if (interval_10ms > 100) interval_10ms = 100; // max 1s
    waterfallRowInterval = interval_10ms;
}

uint8_t WATERFALL_GetRowInterval(void)
{
    return waterfallRowInterval;
}

void WATERFALL_SetDbRange(int dbMin, int dbMax)
{
    waterfallDbMinSettings = dbMin;
    waterfallDbMaxSettings = dbMax;
    waterfallDbMin = dbMin;
    waterfallDbMax = dbMax;
}

void WATERFALL_PushRow(const uint16_t *rssiRow, uint16_t bars)
{
    uint16_t step256 = (bars > 1) ? ((uint16_t)(bars - 1) << 8) / 127 : 0;
    for (uint8_t col = 0; col < WATERFALL_WIDTH; col++) {
        uint16_t pos256 = (uint16_t)col * step256;
        uint8_t i0 = pos256 >> 8;
        uint8_t frac = pos256 & 0xFF;
        if (i0 >= bars) i0 = (bars > 0) ? bars - 1 : 0;

        uint16_t rssiA = rssiRow[i0];
        uint16_t rssiB = (i0 + 1 < bars) ? rssiRow[i0 + 1] : rssiRow[i0];
        uint16_t rssi = ((uint32_t)rssiA * (256 - frac) + (uint32_t)rssiB * frac) >> 8;

        if (rssi == WATERFALL_RSSI_MAX)
            HistorySetPixel(col, waterfallWriteRow, 0);
        else
            HistorySetPixel(col, waterfallWriteRow, dbmToLevel(rssiToDbm(rssi)));
    }
    if (++waterfallWriteRow >= WATERFALL_HEIGHT) waterfallWriteRow = 0;
}

void WATERFALL_PushRowListen(const uint16_t *rssiRow, uint16_t bars,
                             uint16_t peakIndex, uint16_t peakRssi)
{
    uint16_t step256 = (bars > 1) ? ((uint16_t)(bars - 1) << 8) / 127 : 0;

    // Map peakIndex (0..bars-1) to the same 0..127 column space used for
    // interpolation, so the boosted column aligns with the spectrum curve.
    uint8_t peakCol;
    if (bars <= 1)
    {
        peakCol = 0;
    }
    else
    {
        peakCol = (uint8_t)((uint16_t)peakIndex * 127 / (bars - 1));
    }

    // Update persistence: if a real peak is present, seed the persistence
    // counter; otherwise count down so faint/intermittent signals remain
    // visible for a few cycles after they drop.
    if (peakRssi != WATERFALL_RSSI_MAX)
    {
        persistCol = peakCol;
        persistCount = PERSIST_CYCLES;
    }
    else if (persistCount > 0)
    {
        persistCount--;
    }

    for (uint8_t col = 0; col < WATERFALL_WIDTH; col++) {
        uint16_t pos256 = (uint16_t)col * step256;
        uint8_t i0 = pos256 >> 8;
        uint8_t frac = pos256 & 0xFF;
        if (i0 >= bars) i0 = (bars > 0) ? bars - 1 : 0;

        uint16_t rssiA = rssiRow[i0];
        uint16_t rssiB = (i0 + 1 < bars) ? rssiRow[i0 + 1] : rssiRow[i0];
        uint16_t rssi = ((uint32_t)rssiA * (256 - frac) + (uint32_t)rssiB * frac) >> 8;

        uint8_t level;
        if (rssi == WATERFALL_RSSI_MAX)
        {
            level = 0;
        }
        else if (col == peakCol && peakRssi != WATERFALL_RSSI_MAX)
        {
            // Full signal brightness at the exact peak column.
            level = dbmToLevel(rssiToDbm(peakRssi));
        }
        else if (col == persistCol && persistCount > 0 &&
                 peakRssi == WATERFALL_RSSI_MAX)
        {
            // Persistence: decay from full-brightness to background across
            // the remaining cycles so the signal fades smoothly instead
            // of vanishing in one step.
            uint16_t persistRssi = (peakIndex < bars) ? rssiRow[peakIndex] : WATERFALL_RSSI_MAX;
            if (persistRssi == WATERFALL_RSSI_MAX)
                persistRssi = rssiA; // fall back to interpolated value
            if (persistRssi == WATERFALL_RSSI_MAX)
                persistRssi = 0;
            uint8_t signalLevel = dbmToLevel(rssiToDbm(persistRssi));
            uint8_t fadeLevel = dbmToLevel(rssiToDbm(rssi));
            // Linearly interpolate between signal strength and background
            // based on remaining persistence cycles.
            // Use (a + b + 1) / 2 rounding to avoid the middle step
            // collapsing to the final step when levels differ by 1.
            uint8_t decay = (PERSIST_CYCLES - persistCount);
            level = (decay == 0) ? signalLevel :
                    (decay == 1) ? (fadeLevel + signalLevel + 1) / 2 :
                    fadeLevel;
        }
        else if (col >= peakCol - 1 && col <= peakCol + 1 &&
                 peakRssi != WATERFALL_RSSI_MAX)
        {
            // Adjacent falloff: ±1 column gets a brighter-than-background
            // boost so the signal has a soft 3-pixel-wide shape instead of
            // a hard 1-pixel cliff.
            uint8_t bg = dbmToLevel(rssiToDbm(rssi));
            uint8_t boost = bg + 4;
            level = (boost > 15) ? 15 : boost;
        }
        else
        {
            // Seamless background: same brightness as normal scanning so
            // the noise floor continues without a visible seam.
            level = dbmToLevel(rssiToDbm(rssi));
        }
        HistorySetPixel(col, waterfallWriteRow, level);
    }
    if (++waterfallWriteRow >= WATERFALL_HEIGHT) waterfallWriteRow = 0;
}

void WATERFALL_Render(void)
{
    // Circular offset: screen row 0 (top) = newest = waterfallWriteRow - 1 (wrapped)
    //                screen row 15 (bottom) = oldest = waterfallWriteRow
    // New rows appear at the top and fall downward, matching the "falling"
    // visual expected by the user.
    for (uint8_t screenRow = 0; screenRow < WATERFALL_HEIGHT; screenRow++) {
        uint8_t bufRow = (waterfallWriteRow + WATERFALL_HEIGHT - 1 - screenRow) % WATERFALL_HEIGHT;
        uint8_t pageIdx = (screenRow < 8) ? WATERFALL_PAGE0_IDX : WATERFALL_PAGE1_IDX;
        uint8_t bitMask = (1u << (screenRow & 7));
        uint8_t rowOffset = (screenRow & 3) << 2;

        for (uint8_t col = 0; col < WATERFALL_WIDTH; col++) {
            uint8_t level = HistoryGetPixel(col, bufRow);
            if (level > bayer4x4[rowOffset | (col & 3)]) {
                gFrameBuffer[pageIdx][col] |= bitMask;
            } else {
                gFrameBuffer[pageIdx][col] &= (uint8_t)~bitMask;
            }
        }
    }
}
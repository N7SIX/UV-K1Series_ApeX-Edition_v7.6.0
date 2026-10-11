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
 * All data needed is already exported by spectrum.c as non-static
 * globals: rssiHistory[] (the sweep row), scanInfo, settings, peak,
 * currentState, isListening and newScanStart.  spectrum.c raises
 * newScanStart at the end of every completed sweep (its static
 * FinalizeCompletedSweep()) and on the listen-exit paths; watching
 * that flag from the outside tells us exactly when a fresh row is
 * ready.  Listen exits are filtered with the isListening history,
 * and the very first rise after entry is suppressed with "armed"
 * (it can be a leftover from a previous session).
 *
 * Rendering is driven from a hook at the top of the external
 * ST7565_BlitLine(): by then spectrum's Render() for the tick has completed,
 * and waterfall.h requires the paint to happen after spectrum renders but
 * before the page reaches the LCD.  While active the glue owns gFrameBuffer[4],
 * [5] and [6]: page 4 becomes the frequency strip (start freq | center +-step/bw
 * | end freq, drawn here just before that page is sent); pages 5 and 6 stay the
 * waterfall (16 rows = 1/4 LCD), overwriting the stock ticks and peak arrow.
 *
 * Gated by ENABLE_FEAT_N7SIX_WATERFALL (ApeX only).
 */

#include "waterfall_glue.h"

#ifdef ENABLE_FEAT_N7SIX_WATERFALL

#include "driver/st7565.h"
#include "scheduler.h"
#include "spectrum.h"
#include "waterfall.h"
#include <string.h>
#ifdef ENABLE_SCAN_RANGES
#include "chFrScanner.h" /* gScanRangeStart/Stop for the center bw field */
#endif

/* Exported by spectrum.c but not declared in spectrum.h. */
extern ScanInfo scanInfo;
extern SpectrumSettings settings;
extern PeakInfo peak;
extern State currentState;
extern bool newScanStart;
extern bool isListening;
extern uint16_t rssiHistory[128];
/* Non-static spectrum.c helpers used to format the frequency strip. */
extern uint32_t GetFStart(void);
extern uint32_t GetFEnd(void);

static bool glueActive;
static bool prevNewScanStart;
static bool prevIsListening;
static bool armed;               /* false until the first newScanStart = false */
static uint32_t prevListenTick;  /* SCHEDULER_GetTick10ms() units (10 ms) */

static uint16_t WaterfallBars(void)
{
    return (scanInfo.measurementsCount < 128) ? scanInfo.measurementsCount : 128;
}

void WATERFALL_GlueEnter(void)
{
    WATERFALL_Init();
    WATERFALL_SetDbRange(settings.dbMin, settings.dbMax);
    prevNewScanStart = newScanStart;
    prevIsListening = isListening;
    armed = false;
    prevListenTick = SCHEDULER_GetTick10ms();
    glueActive = true;
}

void WATERFALL_GlueExit(void)
{
    if (!glueActive)
        return;
    glueActive = false;
    /* Drop strip and waterfall leftovers so the next screen starts from a clean
     * framebuffer (spectrum.c has already stopped using these pages). */
    memset(gFrameBuffer[WATERFALL_PAGE0_IDX], 0, WATERFALL_WIDTH);
    memset(gFrameBuffer[WATERFALL_PAGE0_IDX + 1], 0, WATERFALL_WIDTH);
    memset(gFrameBuffer[WATERFALL_STRIP_PAGE_IDX], 0, WATERFALL_WIDTH);
}

/* Replicate spectrum's DrawNums() frequency row onto the strip page.
 * Stock draws it at y=49 (bit 1 of its own page); the strip sits at fb[4],
 * so the same bit position is y=33 (LCD row 41). The page is cleared first,
 * which also drops the curve's bottom floor row (DrawingEndY = 32). The
 * strip sits between the spectrum graph and the 16-row waterfall below it. */
static void GlueDrawStrip(void)
{
    char s[24];
    uint32_t f;

    memset(gFrameBuffer[WATERFALL_STRIP_PAGE_IDX], 0, WATERFALL_WIDTH);

    f = GetFStart();
    sprintf(s, "%u.%05u", f / 100000, f % 100000);
    GUI_DisplaySmallest(s, 0, WATERFALL_STRIP_Y, false, true);

#ifdef ENABLE_SCAN_RANGES
    if (gScanRangeStart)
    {
        uint32_t bw = gScanRangeStop - gScanRangeStart;
        sprintf(s, "%u.%02uk", bw / 100, bw % 100);
    }
    else
#endif
    {
        sprintf(s, "\x7F%u.%02uk", settings.frequencyChangeStep / 100,
                settings.frequencyChangeStep % 100);
    }
    GUI_DisplaySmallest(s, 48, WATERFALL_STRIP_Y, false, true);

    f = GetFEnd();
    sprintf(s, "%u.%05u", f / 100000, f % 100000);
    GUI_DisplaySmallest(s, 93, WATERFALL_STRIP_Y, false, true);
}

void WATERFALL_GluePreBlit(unsigned line)
{
    if (!glueActive)
        return;

    /* --- Completed sweep ---------------------------------------------- */
    if (armed && newScanStart && !prevNewScanStart &&
        !prevIsListening && !isListening)
    {
        uint16_t bars = WaterfallBars();
        if (bars > 0)
        {
            /* FinalizeCompletedSweep() may have auto-adjusted dbMax in the
             * very same tick - resync before converting to gray levels. */
            WATERFALL_SetDbRange(settings.dbMin, settings.dbMax);
            WATERFALL_PushRow(rssiHistory, bars);
        }
    }
    if (!newScanStart)
        armed = true; /* InitScanPosition ran: future rises are real sweeps */
    prevNewScanStart = newScanStart;
    prevIsListening = isListening;

    /* --- Listen mode: keep rows scrolling at the configured rate ------ */
    if (armed && isListening && currentState != FREQ_INPUT)
    {
        uint32_t now = SCHEDULER_GetTick10ms();
        if ((uint32_t)(now - prevListenTick) >= WATERFALL_GetRowInterval())
        {
            uint16_t bars = WaterfallBars();
            if (bars > 0)
            {
                uint32_t peakIndex = peak.i;
                if (scanInfo.measurementsCount > 128)
                    peakIndex = (uint32_t)peak.i * 128u / scanInfo.measurementsCount;
                if (peakIndex >= bars)
                    peakIndex = bars - 1u;
                WATERFALL_SetDbRange(settings.dbMin, settings.dbMax);
                WATERFALL_PushRowListen(rssiHistory, bars,
                                        (uint16_t)peakIndex, peak.rssi);
            }
            prevListenTick = now;
        }
    }
    else
    {
        prevListenTick = SCHEDULER_GetTick10ms();
    }

    /* --- Frequency strip: start | center +-step | end, below the graph - */
    if (line == WATERFALL_STRIP_PAGE_IDX && currentState == SPECTRUM)
        GlueDrawStrip();

    /* --- Paint right before the waterfall pages travel to the LCD ------
     * Both pages are repainted just before their own blit.  The spectrum
     * Tick() blits one page per tick while Render() (UI_DisplayClear +
     * stock DrawTicks/DrawArrow into fb[5]) can run between the two blits
     * and would otherwise wipe the bottom half, leaving only 8 rows
     * visible (the "too short" waterfall).  WATERFALL_Render() is
     * idempotent (pure function of history), so repainting twice is safe
     * and keeps the full 16 rows = 1/4 LCD intact.  This also overwrites
     * the stock ticks/peak-arrow that spectrum still draws into fb[5]. */
    if (line == WATERFALL_PAGE0_IDX || line == WATERFALL_PAGE0_IDX + 1)
        WATERFALL_Render();
}

#endif /* ENABLE_FEAT_N7SIX_WATERFALL */
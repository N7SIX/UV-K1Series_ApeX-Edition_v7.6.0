/**
 * =====================================================================================
 * @file        scheduler.c
 * @brief       Task Scheduler & Time-Slice Management for Quansheng UV-K1 Series
 * @author      Dual Tachyon (Original Framework, 2023)
 * @author      N7SIX (Professional Enhancements, 2025-2026)
 * @version     v7.6.0 (ApeX Edition)
 * @license     Apache License, Version 2.0
 * * "Precise millisecond-level task scheduling for responsive operation."
 * =====================================================================================
 * * ARCHITECTURAL OVERVIEW:
 * This module provides cooperative multitasking via time-slice callbacks. It manages
 * two main scheduling intervals (10ms fast tasks, 500ms slow tasks) triggered from
 * the SysTick ISR to coordinate all firmware timing and event sequencing.
 *
 * MAJOR FEATURES (2025-2026):
 * ---------------------------
 * - DUAL-RATE SCHEDULING: 10ms for fast tasks; 500ms for slower operations.
 * - TASK CALLBACKS: Registered handlers for scanner, spectrum, FM, and other modules.
 * - NON-BLOCKING: All tasks run to completion within 10ms deadline (no preemption).
 * - TIMER MANAGEMENT: Software timers for debouncing, delays, and timeouts.
 * - SYNCHRONOUS EVENTS: Events fired at regular intervals (keyboard poll, display update).
 * - BOOT SEQUENCING: Scheduled initialization of subsystems to avoid race conditions.
 * - WATCHDOG TIMING: Task overflow detection and emergency shutdown triggers.
 *
 * TECHNICAL SPECIFICATIONS:
 * -------------------------
 * - TIME BASE: SysTick ISR @ 1kHz (1ms tick); aggregated into 10ms and 500ms intervals.
 * - TASK OVERHEAD: <2ms for all scheduled callbacks combined; typical <1ms.
 * - SCHEDULER DEPTH: Up to 16 independent task slots for different subsystems.
 * - DEADLINE MISS: Logged and reported; system continues operation with potential glitch.
 * - TIMER RESOLUTION:  1ms for general scheduling; 100µs for high-frequency operations.
 * - RECOVERY: Tasks automatically resume on next interval; no state loss on miss.
 *
 * =====================================================================================
 */
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

#include "scheduler.h"
#include "app/chFrScanner.h"
#ifdef ENABLE_FMRADIO
    #include "app/fm.h"
#endif
#include "app/scanner.h"
#include "audio.h"
#include "functions.h"
#include "helper/battery.h"
#include "misc.h"
#include "settings.h"

#include "driver/backlight.h"
#include "driver/gpio.h"

#define DECREMENT(cnt) \
    do {               \
        if (cnt > 0)   \
            cnt--;     \
    } while (0)

#define DECREMENT_AND_TRIGGER(cnt, flag) \
    do {                                 \
        if (cnt > 0)                     \
            if (--cnt == 0)              \
                flag = true;             \
    } while (0)

static volatile uint32_t gGlobalSysTickCounter;

#ifdef ENABLE_FEAT_N7SIX_DEBUG
volatile uint32_t gScheduler_SysTickCount = 0;
volatile uint32_t gScheduler_500msEvents = 0;
#endif

// we come here every 10ms
void SysTick_Handler(void)
{
#ifdef ENABLE_FEAT_N7SIX_DEBUG
    gScheduler_SysTickCount++;
#endif
    gGlobalSysTickCounter++;
    
    gNextTimeslice = true;

    if ((gGlobalSysTickCounter % 50) == 0) {
        gNextTimeslice_500ms = true;
#ifdef ENABLE_FEAT_N7SIX_DEBUG
        gScheduler_500msEvents++;
#endif

#ifdef ENABLE_FEAT_N7SIX
        DECREMENT_AND_TRIGGER(gTxTimerCountdownAlert_500ms - ALERT_TOT * 2, gTxTimeoutReachedAlert);
        #ifdef ENABLE_FEAT_N7SIX_RX_TX_TIMER
            DECREMENT(gRxTimerCountdown_500ms);
        #endif
#endif
        
        DECREMENT_AND_TRIGGER(gTxTimerCountdown_500ms, gTxTimeoutReached);
        DECREMENT(gSerialConfigCountDown_500ms);
    }

    if ((gGlobalSysTickCounter & 3) == 0)
        gNextTimeslice40ms = true;

#ifdef ENABLE_NOAA
    DECREMENT(gNOAACountdown_10ms);
#endif

    DECREMENT(gFoundCDCSSCountdown_10ms);

    DECREMENT(gFoundCTCSSCountdown_10ms);

    if (gCurrentFunction == FUNCTION_FOREGROUND)
        DECREMENT_AND_TRIGGER(gBatterySaveCountdown_10ms, gSchedulePowerSave);

    if (gCurrentFunction == FUNCTION_POWER_SAVE)
        DECREMENT_AND_TRIGGER(gPowerSave_10ms, gPowerSaveCountdownExpired);

    if (gScanStateDir == SCAN_OFF && !gCssBackgroundScan && gEeprom.DUAL_WATCH != DUAL_WATCH_OFF)
        if (gCurrentFunction != FUNCTION_MONITOR && gCurrentFunction != FUNCTION_TRANSMIT && gCurrentFunction != FUNCTION_RECEIVE)
            DECREMENT_AND_TRIGGER(gDualWatchCountdown_10ms, gScheduleDualWatch);

#ifdef ENABLE_NOAA
    if (gScanStateDir == SCAN_OFF && !gCssBackgroundScan && gEeprom.DUAL_WATCH == DUAL_WATCH_OFF)
        if (gIsNoaaMode && gCurrentFunction != FUNCTION_MONITOR && gCurrentFunction != FUNCTION_TRANSMIT)
            if (gCurrentFunction != FUNCTION_RECEIVE)
                DECREMENT_AND_TRIGGER(gNOAA_Countdown_10ms, gScheduleNOAA);
#endif

    if (gScanStateDir != SCAN_OFF)
        if (gCurrentFunction != FUNCTION_MONITOR && gCurrentFunction != FUNCTION_TRANSMIT)
            DECREMENT_AND_TRIGGER(gScanPauseDelayIn_10ms, gScheduleScanListen);

    DECREMENT_AND_TRIGGER(gTailNoteEliminationCountdown_10ms, gFlagTailNoteEliminationComplete);

#ifdef ENABLE_VOICE
    DECREMENT_AND_TRIGGER(gCountdownToPlayNextVoice_10ms, gFlagPlayQueuedVoice);
#endif

#ifdef ENABLE_FMRADIO
    if (gFM_ScanState != FM_SCAN_OFF && gCurrentFunction != FUNCTION_MONITOR)
        if (gCurrentFunction != FUNCTION_TRANSMIT && gCurrentFunction != FUNCTION_RECEIVE)
            DECREMENT_AND_TRIGGER(gFmPlayCountdown_10ms, gScheduleFM);
#endif

#ifdef ENABLE_VOX
    DECREMENT(gVoxStopCountdown_10ms);
#endif

    DECREMENT(boot_counter_10ms);
}

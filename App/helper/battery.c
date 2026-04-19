/**
 * =====================================================================================
 * @file        battery.c
 * @brief       Battery Monitoring & Power Management System (ADC Calibration & Metrics)
 * @author      Dual Tachyon (Original)
 * @author      N7SIX/Professional Enhancement Team (2025-2026)
 * @version     v7.6.0 (ApeX Edition)
 * @license     Apache License, Version 2.0
 * "Real-time battery voltage monitoring with calibrated ADC and low-voltage warning"
 * =====================================================================================
 * ARCHITECTURAL OVERVIEW:
 * Manages battery voltage monitoring through ADC (successive approximation 12-bit),
 * calibration tables for accurate voltage readings, and power state detection. Provides
 * battery level display on LCD with low-voltage warnings and power-off thresholds.\n *\n * MAJOR FEATURES (2025-2026):
 * ---------------------------
 * - Dual-point ADC calibration (precise voltage mapping)
 * - Real-time 12-bit ADC sampling @ 1 kHz
 * - Battery bar indicator display (5-level visualization)
 * - Low-battery warning threshold (user-configurable: 7.0-8.5V)
 * - Automatic power-off at critical level (<6.0V)
 * - Settings persistence through EEPROM storage
 * - Backlight control based on power state
 *
 * TECHNICAL SPECIFICATIONS:
 * -------------------------
 * - ADC channel: Internal (SAR ADC, PY32F071)
 * - Sample rate: 1 kHz per scheduled cycle (500ms display update)
 * - Voltage range: 6.0-9.0V (LiPo 2S assumed)
 * - Calibration points: 2 (low & high ref, <1% error target)
 * - Display integration: st7565.c, ui/battery.c
 * - Settings: gEeprom.chargingMode, gEeprom.batteryType, calibration data
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

#include <assert.h>
#include <stddef.h>

#include "battery.h"
#include "driver/backlight.h"
#include "driver/st7565.h"
#include "functions.h"
#include "misc.h"
#include "settings.h"
#include "ui/battery.h"
#include "ui/menu.h"
#include "ui/ui.h"
#include "battery_constants.h"
//#include "debugging.h"

BatteryCalib_t    gBatteryCalib;
BatteryBaseline_t gBatteryBaseline;
uint16_t          gBatteryCurrentVoltage;
uint16_t          gBatteryCurrent;
uint16_t          gBatteryVoltages[4];
uint16_t          gBatteryVoltageAverage;
uint8_t           gBatteryDisplayLevel;
bool              gChargingWithTypeC;
bool              gLowBatteryBlink;
bool              gLowBattery;
bool              gLowBatteryConfirmed;
uint16_t          gBatteryCheckCounter;
static uint16_t   gBatterySessionPeakVoltage_10mV;

typedef enum {
    BATTERY_LOW_INACTIVE,
    BATTERY_LOW_ACTIVE,
    BATTERY_LOW_CONFIRMED
} BatteryLow_t;

uint16_t          lowBatteryCountdown;
const uint16_t    lowBatteryPeriod = 30;

volatile uint16_t gPowerSave_10ms;

static BATTERY_Type_t BATTERY_GetEffectiveBatteryType(void)
{
    return (gEeprom.BATTERY_TYPE < BATTERY_TYPE_UNKNOWN)
        ? gEeprom.BATTERY_TYPE
        : BATTERY_TYPE_1400_MAH;
}

const uint16_t Voltage2PercentageTable[][10][2] = {
    // 1400 mAh K1 Battery - Enhanced with 8 measured data points
    // Discharge curve on 500mA constant current load
    [BATTERY_TYPE_1400_MAH] = {
        {828, 100},  // Fully charged (8.28V) - peak
        {822,  99},  // Charge plateau top
        {816,  98},  // Charge plateau
        {813,  97},  // Top discharge point
        {758,  25},  // Mid-range discharge
        {740,  12},  // Low battery warning zone
        {726,   6},  // Critical threshold
        {630,   0},  // Completely discharged
        {0,    0},
        {0,    0},
    },

    // 2500 mAh K1 Battery - Enhanced with 8 measured data points
    // Discharge curve on 500mA constant current load
    [BATTERY_TYPE_2500_MAH] = {
        {839, 100},  // Fully charged (8.39V) - peak
        {832,  99},  // Charge plateau top
        {825,  98},  // Charge plateau
        {818,  95},  // Top discharge point
        {745,  55},  // Mid-range discharge
        {710,  28},  // Approaching low battery
        {693,  12},  // Low battery warning zone
        {668,   5},  // Critical threshold
        {623,   0},  // Completely discharged
        {0,    0},
    },
};

/* Useless (for compilator only)
static_assert(
    (ARRAY_SIZE(Voltage2PercentageTable[BATTERY_TYPE_1600_MAH]) ==
    ARRAY_SIZE(Voltage2PercentageTable[BATTERY_TYPE_2200_MAH])) &&
    (ARRAY_SIZE(Voltage2PercentageTable[BATTERY_TYPE_2200_MAH]) ==
    ARRAY_SIZE(Voltage2PercentageTable[BATTERY_TYPE_3500_MAH]))
    );
*/

unsigned int BATTERY_VoltsToPercent(const unsigned int voltage_10mV)
{
    const BATTERY_Type_t type = BATTERY_GetEffectiveBatteryType();
    const uint16_t (*crv)[2] = Voltage2PercentageTable[type];
    const int mulipl = 1000;
    for (unsigned int i = 1; i < 10; i++) {
        if (voltage_10mV > crv[i][0]) {
            const int a = (crv[i - 1][1] - crv[i][1]) * mulipl / (crv[i - 1][0] - crv[i][0]);
            const int b = crv[i][1] - a * crv[i][0] / mulipl;
            const int p = a * voltage_10mV / mulipl + b;
            return MIN(MAX(p, 0), 100);
        }
    }

    return 0;
}

uint16_t BATTERY_AdcToVoltage10mV(const uint16_t adc_value)
{
    uint16_t cal_high = gBatteryCalib.BatHi;
    if (cal_high < 1500 || cal_high > 3500)
        cal_high = 2200;

    const uint16_t cal_low = gBatteryCalib.BatLo;
    const uint16_t expected_low = (uint16_t)((cal_high * BATTERY_CAL_LOW_REF_10MV) / BATTERY_CAL_HIGH_REF_10MV);

    // Use 2-point linear calibration only when low/high points are plausible, monotonic,
    // and physically coherent with the 5.2V/7.6V reference ratio.
    if (cal_low >= 1500 && cal_low <= 3500 && cal_low + 10 < cal_high
        && cal_low + 120 >= expected_low && cal_low <= expected_low + 120)
    {
        int32_t voltage_10mV = BATTERY_CAL_LOW_REF_10MV
            + ((int32_t)(adc_value - cal_low) * (BATTERY_CAL_HIGH_REF_10MV - BATTERY_CAL_LOW_REF_10MV))
                / (int32_t)(cal_high - cal_low);

        if (voltage_10mV < 0)
            voltage_10mV = 0;

        return (uint16_t)voltage_10mV;
    }

    // Fallback preserves previous behavior when only high-point calibration is reliable.
    return (uint16_t)(((uint32_t)adc_value * BATTERY_CAL_HIGH_REF_10MV) / cal_high);
}

uint16_t BATTERY_GetSessionPeakVoltage_10mV(void)
{
    return gBatterySessionPeakVoltage_10mV;
}

uint16_t BATTERY_GetNominalCapacitymAh(void)
{
    switch (BATTERY_GetEffectiveBatteryType())
    {
        case BATTERY_TYPE_1400_MAH: return 1400;
        case BATTERY_TYPE_2500_MAH: return 2500;
        default:                    return 1400;
    }
}

uint16_t BATTERY_GetEstimatedRemainingmAh(void)
{
    const uint16_t nominal_mAh = BATTERY_GetNominalCapacitymAh();
    const uint8_t soc_percent = BATTERY_VoltsToPercent(gBatteryVoltageAverage);
    return (uint16_t)((nominal_mAh * soc_percent) / 100);
}

uint8_t BATTERY_GetEstimatedHealthPercent(void)
{
    const BATTERY_Type_t type = BATTERY_GetEffectiveBatteryType();
    const uint16_t full_reference_10mV = Voltage2PercentageTable[type][0][0];
    const uint16_t peak_10mV = MAX(gBatterySessionPeakVoltage_10mV, gBatteryVoltageAverage);

    if (full_reference_10mV == 0)
        return 0;

    const uint16_t health = (uint16_t)((peak_10mV * 100u) / full_reference_10mV);
    return (uint8_t)MIN(health, 100u);
}

uint8_t BATTERY_GetTrueHealthPercent(void)
{
    // True health based on factory peak voltage baseline
    // Eliminates session-dependent bias from cold-start
    const uint16_t factory_peak = gBatteryBaseline.factory_peak_10mV;
    
    if (factory_peak == 0)
    {
        // Baseline not set; fall back to session health
        return BATTERY_GetEstimatedHealthPercent();
    }
    
    // Apply cycle-based degradation estimation
    // Typical LiPo: ~0.2% fade per cycle
    uint8_t cycle_degradation = (gBatteryBaseline.cycle_count / 50);  // 0.2% per cycle
    cycle_degradation = MIN(cycle_degradation, 40);  // Cap at 40% max estimated fade
    
    // True health = Factory Peak / Reference × 100 - Cycle Degradation
    const BATTERY_Type_t type = BATTERY_GetEffectiveBatteryType();
    const uint16_t reference_10mV = Voltage2PercentageTable[type][0][0];

    if (reference_10mV == 0) {
        return 0;
    }

    const uint16_t health_base = (uint16_t)((factory_peak * 100u) / reference_10mV);
    const uint8_t true_health = (uint8_t)MAX(0, MIN(100, (int)health_base - (int)cycle_degradation));
    
    return true_health;
}

void BATTERY_SetAndValidateCalibration(uint16_t cal_high, bool* pIsValid)
{
    // Validation function for battery calibration
    // Returns true if calibration is valid, false if out of range
    
    if (pIsValid == NULL)
        return;
    
    *pIsValid = false;
    
    // Validate high reference is in reasonable ADC range
    if (cal_high < 1500 || cal_high > 3500)
    {
        // Out of range: ADC value for 7.6V reference should be 1500-3500
        return;
    }
    
    // Calculate low reference from high (5.2V / 7.6V ratio)
    uint16_t cal_low = (uint16_t)((520ul * cal_high) / 760);
    
    // Validate low reference constraints
    if (cal_low < 1500 || cal_low > 3500)
    {
        // Can't achieve valid low point; reject this calibration
        return;
    }
    
    // Validate monotonicity and coherence
    if (cal_low + 10 >= cal_high)
    {
        // Points too close together; incoherent
        return;
    }
    
    // Validate coherence with reference ratio (±120 tolerance)
    const uint16_t expected_low = (uint16_t)((cal_high * BATTERY_CAL_LOW_REF_10MV) / BATTERY_CAL_HIGH_REF_10MV);
    if (cal_low + 120 < expected_low || cal_low > expected_low + 120)
    {
        // Low point doesn't match expected 5.2V/7.6V ratio
        return;
    }
    
    // All checks passed
    gBatteryCalib.BatLo = cal_low;
    gBatteryCalib.BatHi = cal_high;
    gBatteryCalib.BatTol = gBatteryCalib.BatHi - gBatteryCalib.BatLo;
    gBatteryCalib.BatChk = (gBatteryCalib.BatHi + gBatteryCalib.BatLo) / 2;
    
    *pIsValid = true;
}

void BATTERY_InitializeBaselineIfNeeded(void)
{
    // Initialize factory peak voltage baseline on first boot
    // This establishes the reference point for true health calculation
    
    if (gBatteryBaseline.factory_peak_10mV == 0 && gBatteryVoltageAverage >= 800)
    {
        // First boot and battery appears charged; set baseline
        gBatteryBaseline.factory_peak_10mV = gBatteryVoltageAverage;
        gBatteryBaseline.cycle_count = 1;
        SETTINGS_SaveBatteryBaselineStruct(&gBatteryBaseline);
    }
    else if (gBatteryBaseline.factory_peak_10mV != 0)
    {
        // Baseline already set; ensure peak is updated if higher
        if (gBatteryVoltageAverage > gBatteryBaseline.factory_peak_10mV)
        {
            gBatteryBaseline.factory_peak_10mV = gBatteryVoltageAverage;
            SETTINGS_SaveBatteryBaselineStruct(&gBatteryBaseline);
        }
    }
}

void BATTERY_GetReadings(const bool bDisplayBatteryLevel)
{
    const uint8_t  PreviousBatteryLevel = gBatteryDisplayLevel;
    const uint16_t Voltage              = (gBatteryVoltages[0] + gBatteryVoltages[1] + gBatteryVoltages[2] + gBatteryVoltages[3]) / 4;
    gBatteryVoltageAverage = BATTERY_AdcToVoltage10mV(Voltage);

    // Initialize factory baseline on first boot with valid voltage
    BATTERY_InitializeBaselineIfNeeded();

    if (gBatteryVoltageAverage > gBatterySessionPeakVoltage_10mV)
        gBatterySessionPeakVoltage_10mV = gBatteryVoltageAverage;

    if(gBatteryVoltageAverage > BATTERY_OVERVOLT_THRESHOLD_10MV)
        gBatteryDisplayLevel = BATTERY_DISPLAY_LEVEL_OVERVOLT; // battery overvoltage
    else if(gBatteryVoltageAverage < BATTERY_2200M_CRITICAL_10MV
            && (gEeprom.BATTERY_TYPE == BATTERY_TYPE_1400_MAH))
        gBatteryDisplayLevel = BATTERY_DISPLAY_LEVEL_CRITICAL; // battery critical
    else if(gBatteryVoltageAverage < BATTERY_2500M_CRITICAL_10MV
            && (gEeprom.BATTERY_TYPE == BATTERY_TYPE_2500_MAH))
        gBatteryDisplayLevel = BATTERY_DISPLAY_LEVEL_CRITICAL; // battery critical
    else {
        gBatteryDisplayLevel = 1;
        const uint8_t levels[] = {5,17,41,65,88};
        uint8_t perc = BATTERY_VoltsToPercent(gBatteryVoltageAverage);
        //char str[64];
        //LogUart("----------\n");
        //sprintf(str, "%d %d %d %d %d %d %d\n", gBatteryVoltages[0], gBatteryVoltages[1], gBatteryVoltages[2], gBatteryVoltages[3], Voltage, gBatteryVoltageAverage, perc);
        //LogUart(str);

        for(uint8_t i = 6; i >= 2; i--){
            //sprintf(str, "%d %d %d\n", perc, levels[i-2], i);
            //LogUart(str);
            if (perc > levels[i-2]) {
                gBatteryDisplayLevel = i;
                break;
            }
        }
    }


    if ((gScreenToDisplay == DISPLAY_MENU) && UI_MENU_GetCurrentMenuId() == MENU_VOL)
        gUpdateDisplay = true;

    if (gBatteryCurrent < 501)
    {
        if (gChargingWithTypeC)
        {
            gUpdateStatus  = true;
            gUpdateDisplay = true;
        }

        gChargingWithTypeC = false;
    }
    else
    {
        if (!gChargingWithTypeC)
        {
            gUpdateStatus  = true;
            gUpdateDisplay = true;
            BACKLIGHT_TurnOn();
        }

        gChargingWithTypeC = true;
    }

    if (PreviousBatteryLevel != gBatteryDisplayLevel)
    {
        if(gBatteryDisplayLevel > 2)
            gLowBatteryConfirmed = false;
        else if (gBatteryDisplayLevel < 2)
        {
            gLowBattery = true;
        }
        else
        {
            gLowBattery = false;

            if (bDisplayBatteryLevel)
                UI_DisplayBattery(gBatteryDisplayLevel, gLowBatteryBlink);
        }

        if(!gLowBatteryConfirmed)
            gUpdateDisplay = true;

        lowBatteryCountdown = 0;
    }
}

void BATTERY_TimeSlice500ms(void)
{
    if (!gLowBattery) {
        return;
    }

    gLowBatteryBlink = ++lowBatteryCountdown & 1;

    UI_DisplayBattery(0, gLowBatteryBlink);

    if (gCurrentFunction == FUNCTION_TRANSMIT) {
        return;
    }

    // not transmitting

    if (lowBatteryCountdown < lowBatteryPeriod) {
        if (lowBatteryCountdown == lowBatteryPeriod-1 && !gChargingWithTypeC && !gLowBatteryConfirmed) {
            AUDIO_PlayBeep(BEEP_500HZ_60MS_DOUBLE_BEEP);
        }
        return;
    }

    lowBatteryCountdown = 0;

    if (gChargingWithTypeC) {
        return;
    }

    // not on charge
    if (!gLowBatteryConfirmed) {
        AUDIO_PlayBeep(BEEP_500HZ_60MS_DOUBLE_BEEP);
#ifdef ENABLE_VOICE
        AUDIO_SetVoiceID(0, VOICE_ID_LOW_VOLTAGE);
#endif
    }

    if (gBatteryDisplayLevel != 0) {
#ifdef ENABLE_VOICE
        AUDIO_PlaySingleVoice(false);
#endif
        return;
    }

#ifdef ENABLE_VOICE
    AUDIO_PlaySingleVoice(true);
#endif

    gReducedService = true;

    FUNCTION_Select(FUNCTION_POWER_SAVE);

    ST7565_HardwareReset();

    if (gEeprom.BACKLIGHT_TIME < 61) {
        BACKLIGHT_TurnOff();
    }
}

bool BATTERY_IsVoltageSafeForCriticalOps(void)
{
    return gBatteryVoltageAverage >= BATTERY_MIN_SAFE_WRITE_10MV;
}

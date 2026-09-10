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

#ifndef BATTERY_H
#define BATTERY_H

#include <stdbool.h>
#include <stdint.h>

#include "battery_calibration.h"

extern uint16_t          gBatteryCalibration[6];
/*
 * gBatteryCalibration[] storage format (V2).
 *   [0]  raw ADC at ~6.0V   (0 = low point not calibrated -> single-point fallback)
 *   [1]  reserved
 *   [2]  BATCAL_FORMAT_V2 marker (format sentinel - legacy code never used slot 2)
 *   [3]  raw ADC at ~8.4V   (high point reference)
 *   [4]  reserved
 *   [5]  legacy, forced to 2300 on load
 * Legacy (V1) firmware stored slot3 as the raw ADC at ~7.6V under the old
 * `raw * 760 / slot3` display formula. SETTINGS_LoadCalibration() migrates
 * V1 blocks to V2 once (slot3 *= 840/760, slot0 cleared) so existing
 * calibrations keep reading correctly in the 2-point model.
 */
extern uint16_t          gBatteryCurrentVoltage;
extern uint16_t          gBatteryCurrent;
extern uint16_t          gBatteryVoltages[4];
extern uint16_t          gBatteryVoltageAverage;
extern uint8_t           gBatteryDisplayLevel;
extern bool              gChargingWithTypeC;
extern bool              gLowBatteryBlink;
extern bool              gLowBattery;
extern bool              gLowBatteryConfirmed;
extern uint16_t          gBatteryCheckCounter;

extern volatile uint16_t gPowerSave_10ms;

typedef enum {
    BATTERY_TYPE_1600_MAH,
    BATTERY_TYPE_2200_MAH,
    BATTERY_TYPE_3500_MAH,
    BATTERY_TYPE_1500_MAH,
    BATTERY_TYPE_2500_MAH,
    BATTERY_TYPE_UNKNOWN
} BATTERY_Type_t;


unsigned int BATTERY_VoltsToPercent(unsigned int voltage_10mV);
bool BATTERY_IsCriticalVoltage(BATTERY_Type_t battery_type, uint16_t voltage_10mV);
void BATTERY_GetReadings(bool bDisplayBatteryLevel);
void BATTERY_TimeSlice500ms(void);

#endif

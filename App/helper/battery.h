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

typedef struct {
    uint16_t BatHi;   // High reference ADC value (e.g., 8.4V)
    uint16_t BatLo;   // Low reference ADC value (e.g., 6.0V)
    uint16_t BatTol;  // Tolerance (user-defined or calculated)
    uint16_t BatChk;  // Check value (for verification)
} BatteryCalib_t;

extern BatteryCalib_t gBatteryCalib;
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
    BATTERY_TYPE_1400_MAH,
    BATTERY_TYPE_2500_MAH,
    BATTERY_TYPE_UNKNOWN
} BATTERY_Type_t;


unsigned int BATTERY_VoltsToPercent(unsigned int voltage_10mV);
uint16_t BATTERY_AdcToVoltage10mV(uint16_t adc_value);
uint8_t BATTERY_GetEstimatedHealthPercent(void);
uint16_t BATTERY_GetNominalCapacitymAh(void);
uint16_t BATTERY_GetEstimatedRemainingmAh(void);
uint16_t BATTERY_GetSessionPeakVoltage_10mV(void);
void BATTERY_GetReadings(bool bDisplayBatteryLevel);
void BATTERY_TimeSlice500ms(void);
bool BATTERY_IsVoltageSafeForCriticalOps(void);

#endif

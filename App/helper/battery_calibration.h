#ifndef BATTERY_CALIBRATION_H
#define BATTERY_CALIBRATION_H

#include <stdbool.h>
#include <stdint.h>

#define BATCAL_FORMAT_V2       0xB5F2u
#define BATCAL_LOW_REFERENCE   600u
#define BATCAL_HIGH_REFERENCE  840u
#define BATCAL_LOW_MIN_RAW     1000u
#define BATCAL_LOW_MAX_RAW     4000u
#define BATCAL_HIGH_MIN_RAW    1650u
#define BATCAL_HIGH_MAX_RAW    3900u

bool BATTERY_CalibrationPointsValid(uint16_t cal_lo, uint16_t cal_hi);
uint16_t BATTERY_CalibrateRaw(uint16_t raw_adc, uint16_t cal_lo, uint16_t cal_hi);
uint16_t BATTERY_CalibrationLowPreset(uint16_t cal_hi);

#endif

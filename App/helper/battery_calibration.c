#include "battery_calibration.h"

bool BATTERY_CalibrationPointsValid(const uint16_t cal_lo, const uint16_t cal_hi)
{
    return cal_lo >= BATCAL_LOW_MIN_RAW &&
           cal_lo <= BATCAL_LOW_MAX_RAW &&
           cal_hi >= BATCAL_HIGH_MIN_RAW &&
           cal_hi <= BATCAL_HIGH_MAX_RAW &&
           cal_lo < cal_hi;
}

uint16_t BATTERY_CalibrateRaw(const uint16_t raw_adc,
                              const uint16_t cal_lo,
                              const uint16_t cal_hi)
{
    int32_t voltage;

    if (BATTERY_CalibrationPointsValid(cal_lo, cal_hi))
    {
        if (raw_adc <= cal_lo)
            voltage = BATCAL_LOW_REFERENCE;
        else if (raw_adc >= cal_hi)
            voltage = BATCAL_HIGH_REFERENCE;
        else
            voltage = (int32_t)BATCAL_LOW_REFERENCE +
                      (int32_t)(raw_adc - cal_lo) *
                      ((int32_t)BATCAL_HIGH_REFERENCE - (int32_t)BATCAL_LOW_REFERENCE) /
                      (cal_hi - cal_lo);
    }
    else if (cal_hi > 0)
    {
        voltage = (int32_t)raw_adc * (int32_t)BATCAL_HIGH_REFERENCE / cal_hi;
    }
    else
    {
        voltage = 0;
    }

    return (uint16_t)voltage;
}

uint16_t BATTERY_CalibrationLowPreset(const uint16_t cal_hi)
{
    return cal_hi > 0
        ? (uint16_t)((BATCAL_LOW_REFERENCE * (uint32_t)cal_hi) / BATCAL_HIGH_REFERENCE)
        : 1357;
}

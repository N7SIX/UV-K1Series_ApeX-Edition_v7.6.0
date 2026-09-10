#include <stdio.h>
#include <stdint.h>
#include <assert.h>

#include "battery_calibration.h"

static int is_critical_voltage(unsigned int battery_type, uint16_t voltage_10mV)
{
    switch (battery_type)
    {
        case 0:
        case 1:
        case 3:
            return voltage_10mV <= 630;
        case 2:
            return voltage_10mV <= 600;
        case 4:
            return voltage_10mV <= 623;
        default:
            return 0;
    }
}

/* ---- Mirror of BATTERY_GetReadings() calibration math (helper/battery.c) ---- */
static uint16_t fake_cal[6];

static uint16_t interpolate(uint16_t raw_avg)
{
    return BATTERY_CalibrateRaw(raw_avg, fake_cal[0], fake_cal[3]);
}

/* ---- Mirror of MENU_BatCalLowPreset() ---- */
static uint16_t low_preset(void)
{
    return BATTERY_CalibrationLowPreset(fake_cal[3]);
}

/* ---- Mirror of the V1->V2 migration (settings.c) ---- */
static void migrate(void)
{
    if (fake_cal[2] != BATCAL_FORMAT_V2) {
        if (fake_cal[3] == 0 || fake_cal[3] >= 5000)
            fake_cal[3] = 2210;
        else
            fake_cal[3] = (uint16_t)((840ul * fake_cal[3]) / 760);
        fake_cal[0] = 0;
        fake_cal[2] = BATCAL_FORMAT_V2;
    }
}

int main(void)
{
    int failures = 0;
    #define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } } while (0)

    /* 1) Legacy migration: 2192 @7.6V -> raw(8.4V) (docs claim 2422) */
    fake_cal[0] = 5000; fake_cal[3] = 2192; fake_cal[2] = 0;
    migrate();
    CHECK(fake_cal[3] == 2422, "migration 2192 -> 2422");
    CHECK(fake_cal[0] == 0, "migration clears slot0");
    CHECK(fake_cal[2] == BATCAL_FORMAT_V2, "migration sets marker");
    CHECK(interpolate(fake_cal[3]) == 840, "raw==cal_hi reads 8.40V");
    /* NOTE: slot0==0 after migration -> single-point FALLBACK (through origin),
     * so there is NO 6.0V floor in this mode: raw 0 maps to 0V by design. */
    CHECK(interpolate(0) == 0, "fallback (cal_lo=0): raw=0 reads 0V, no div/0");

    /* 2) Fresh/uninitialized flash: slot3 = 0xFFFF -> 2210 default */
    fake_cal[0] = 0xFFFF; fake_cal[3] = 0xFFFF; fake_cal[2] = 0xFFFF;
    migrate();
    CHECK(fake_cal[3] == 2210, "blank flash default -> 2210");
    CHECK(fake_cal[0] == 0, "blank flash slot0 cleared");

    /* 3) Idempotent migration (marker present) */
    fake_cal[0] = 1000; fake_cal[3] = 2300; fake_cal[2] = BATCAL_FORMAT_V2;
    migrate();
    CHECK(fake_cal[3] == 2300, "marker present -> untouched");

    /* 4) Factory preset math: at 6.0V line through origin => cal_hi * 5/7 */
    fake_cal[0] = 0; fake_cal[3] = 2422; fake_cal[2] = BATCAL_FORMAT_V2;
    CHECK(low_preset() == 1730, "preset 2422*5/7 = 1730");

    /* 5) 2-point interpolation correctness */
    fake_cal[0] = 1600; fake_cal[3] = 2400;  /* 6.0V..8.4V span = 800 raw counts / 240 cV */
    CHECK(interpolate(1600) == 600, "at cal_lo -> 6.00V");
    CHECK(interpolate(2400) == 840, "at cal_hi -> 8.40V");
    CHECK(interpolate(1601) == 600, "one count above lo - still ~6.00V");
    CHECK(interpolate(2400 - 1) == 839, "one count below hi -> 8.39V");
    CHECK(interpolate(2000) == 720, "midpoint (2000) -> 7.20V");

    /* 6) Clamping outside the calibrated window */
    CHECK(interpolate(100) == 600, "below low clamps to 6.00V");
    CHECK(interpolate(5000) == 840, "above high clamps to 8.40V");

    /* 7) Single-point fallback: cal_lo=0 -> through-origin with 840 anchor */
    fake_cal[0] = 0; fake_cal[3] = 2422;
    CHECK(interpolate(1211) == 420, "fallback half raw -> 4.20V");
    CHECK(interpolate(0) == 0, "fallback cal_hi>0, raw=0 -> 0V");

    /* 8) Degenerate: cal_hi == 0 -> 0 V, no divide by zero */
    fake_cal[0] = 0; fake_cal[3] = 0;
    CHECK(interpolate(2000) == 0, "cal_hi=0 -> 0V (no div/0)");

    /* 9) Malformed 2-point: cal_lo >= cal_hi -> single-point fallback (not div/0) */
    fake_cal[0] = 3000; fake_cal[3] = 2000;
    CHECK(interpolate(1500) == 630, "invalid points fall back (1500*840/2000=630)");
    CHECK(BATTERY_CalibrationPointsValid(1600, 2400), "valid calibration points accepted");
    CHECK(!BATTERY_CalibrationPointsValid(0, 2400), "unset low point rejected as 2-point data");
    CHECK(!BATTERY_CalibrationPointsValid(2400, 1600), "reversed points rejected");
    CHECK(!BATTERY_CalibrationPointsValid(1000, 4000), "equal boundary points rejected");

    /* 10) Battery-type critical thresholds match each discharge curve. */
    CHECK(is_critical_voltage(0, 630), "1600 mAh critical at 6.30V");
    CHECK(is_critical_voltage(1, 630), "2200 mAh critical at 6.30V");
    CHECK(is_critical_voltage(2, 600), "3500 mAh critical at 6.00V");
    CHECK(is_critical_voltage(3, 630), "1500 mAh critical at 6.30V");
    CHECK(is_critical_voltage(4, 623), "2500 mAh critical at 6.23V");
    CHECK(!is_critical_voltage(4, 624), "2500 mAh above critical threshold");

    /* 11) List preview formula: avg * cal[3] / selection with selection==cal[3] */
    {
        const uint16_t avg = 765, cal3 = 2422;
        const uint16_t prev = (uint16_t)((uint32_t)avg * cal3 / cal3);
        CHECK(prev == avg, "menu-list preview with valid slot3 equals live voltage");
    }

    if (failures == 0) {
        printf("ALL PASS\n");
        return 0;
    }
    printf("%d FAILURES\n", failures);
    return 1;
}
#ifndef BATTERY_CONSTANTS_H
#define BATTERY_CONSTANTS_H

// Battery voltage levels (10mV units)
#define BATTERY_OVERVOLT_THRESHOLD_10MV    890  // 8.90 V (overvoltage condition)
#define BATTERY_2200M_CRITICAL_10MV        630  // 6.30 V (critical for 1600/2200)
#define BATTERY_3500M_CRITICAL_10MV        600  // 6.00 V (critical for 3500)

// Safety thresholds (10mV units)
#define BATTERY_MIN_SAFE_TX_10MV           700  // 7.00 V minimum safe TX threshold
#define BATTERY_MIN_SAFE_WRITE_10MV        700  // 7.00 V minimum safe EEPROM/flash write threshold

// Display levels
#define BATTERY_DISPLAY_LEVEL_OVERVOLT     7
#define BATTERY_DISPLAY_LEVEL_CRITICAL     0

// DTMF/Low battery persistance
#define BATTERY_LOWLEVEL_HYSTERESIS_LEVEL  2

#endif // BATTERY_CONSTANTS_H
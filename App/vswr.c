/*
 * vswr.c - Software-based VSWR estimation using battery voltage drop during TX
 *
 * This module estimates VSWR by observing the battery voltage drop when transmitting.
 * It does NOT require a hardware bridge, but infers relative mismatch by comparing
 * idle and TX battery voltages. This is a heuristic and not a true VSWR measurement.
 *
 * Author: N7SIX
 * Date: 2026-04-21
 */

#include <stdint.h>
#include <stdbool.h>
#include "helper/battery.h"
#include "radio.h"

// Simple VSWR estimation state
static uint16_t last_idle_voltage = 0;   // 10mV units
static uint16_t last_tx_voltage = 0;     // 10mV units
static float last_vswr = 1.0f;

// Call this when radio is idle (not transmitting)
void VSWR_SampleIdleVoltage(void) {
    last_idle_voltage = gBatteryVoltageAverage; // 10mV units
}

// Call this when radio is transmitting
void VSWR_SampleTxVoltage(void) {
    last_tx_voltage = gBatteryVoltageAverage; // 10mV units
    // Only update VSWR during TX, using the most recent idle voltage
    if (last_idle_voltage > 0 && last_tx_voltage > 0 && last_tx_voltage < last_idle_voltage) {
        float drop = (float)(last_idle_voltage - last_tx_voltage) / last_idle_voltage;
        float vswr = 1.0f + drop * 4.0f;
        if (vswr < 1.0f) vswr = 1.0f;
        if (vswr > 5.0f) vswr = 5.0f;
        last_vswr = vswr;
    }
}

// Estimate VSWR based on voltage drop
float VSWR_GetEstimate(void) {
    // Always return the last computed VSWR (updated only during TX)
    return last_vswr;
}

// Optionally, expose raw voltages for diagnostics
uint16_t VSWR_GetIdleVoltage(void) { return last_idle_voltage; }
uint16_t VSWR_GetTxVoltage(void) { return last_tx_voltage; }

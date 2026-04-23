/*
 * vswr.h - Interface for software-based VSWR estimation
 *
 * Author: N7SIX + Copilot
 * Date: 2026-04-21
 */

#ifndef VSWR_H
#define VSWR_H

#include <stdint.h>

void VSWR_SampleIdleVoltage(void);
void VSWR_SampleTxVoltage(void);
float VSWR_GetEstimate(void);
uint16_t VSWR_GetIdleVoltage(void);
uint16_t VSWR_GetTxVoltage(void);

#endif // VSWR_H

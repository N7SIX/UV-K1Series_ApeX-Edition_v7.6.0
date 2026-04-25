#ifndef VSWR_DELAY_H
#define VSWR_DELAY_H

#include <stdint.h>

void VSWR_RequestTxSampleDelay(uint16_t delay_ms);
void VSWR_Tick10ms(void);
int VSWR_IsTxSampleDue(void);

#endif // VSWR_DELAY_H

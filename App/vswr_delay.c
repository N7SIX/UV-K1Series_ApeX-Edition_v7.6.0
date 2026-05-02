#include "vswr_delay.h"

static uint16_t tx_sample_delay_10ms = 0;

void VSWR_RequestTxSampleDelay(uint16_t delay_ms) {
    tx_sample_delay_10ms = (delay_ms + 9) / 10; // round up to next 10ms
}

void VSWR_Tick10ms(void) {
    if (tx_sample_delay_10ms > 0) {
        tx_sample_delay_10ms--;
    }
}

int VSWR_IsTxSampleDue(void) {
    return tx_sample_delay_10ms == 0;
}

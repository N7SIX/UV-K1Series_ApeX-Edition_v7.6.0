#include "adc.h"

uint8_t ADC_GetChannelNumber(ADC_CH_MASK Mask)
{
    if (Mask == 0U) {
        return 0U;
    }

    return (uint8_t)__builtin_ctz((unsigned int)Mask);
}

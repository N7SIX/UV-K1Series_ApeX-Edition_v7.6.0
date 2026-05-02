#include "dtmf.h"
#include <string.h>

void DTMF_BufferShiftAppend(char *buffer, uint8_t *len, char c, uint8_t maxlen)
{
    if (len == NULL || buffer == NULL || maxlen == 0)
        return;

    if (*len >= (maxlen - 1)) {
        memmove(buffer, buffer + 1, maxlen - 2);
        (*len)--;
    }

    buffer[*len] = c;
    (*len)++;
    buffer[*len] = '\0';
}

// safe_str.h
// Macro and inline function for safe string copy with null-termination
// Usage: SAFE_STRNCPY(dest, src);

#ifndef SAFE_STR_H
#define SAFE_STR_H

#include <string.h>

#define SAFE_STRNCPY(dest, src) \
    do { \
        strncpy((dest), (src), sizeof(dest) - 1); \
        (dest)[sizeof(dest) - 1] = '\0'; \
    } while (0)

#endif // SAFE_STR_H

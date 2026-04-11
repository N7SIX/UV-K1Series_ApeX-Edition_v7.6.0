/* Minimal _sbrk implementation for newlib nano, for heap allocation (malloc, etc.) */
#include <sys/types.h>
#include <errno.h>

// Defined by linker script
extern char _end;    // End of bss
extern char _estack; // Top of stack

static char *heap_end = 0;

caddr_t _sbrk(int incr)
{
    if (heap_end == 0) {
        heap_end = &_end;
    }
    char *prev_heap_end = heap_end;
    char *stack = (char *)&_estack;
    if (heap_end + incr > stack) {
        errno = ENOMEM;
        return (caddr_t)-1;
    }
    heap_end += incr;
    return (caddr_t)prev_heap_end;
}

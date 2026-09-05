/* Minimal newlib syscall stubs for heap allocation.
 * This provides _sbrk for malloc/newlib support.
 */

#include <errno.h>
#include <sys/types.h>

extern char _end;    /* Defined by linker script */
extern char _estack; /* Defined by linker script */

static char *heap_end;

caddr_t _sbrk(int incr)
{
    char *prev_heap_end;
    char *stack;

    if (heap_end == NULL)
    {
        heap_end = &_end;
    }

    prev_heap_end = heap_end;
    stack = (char *)&_estack;

    if (heap_end + incr > stack)
    {
        errno = ENOMEM;
        return (caddr_t)-1;
    }

    heap_end += incr;
    return (caddr_t)prev_heap_end;
}

caddr_t _sbrk_r(struct _reent *r, int incr)
{
    (void)r;
    return _sbrk(incr);
}

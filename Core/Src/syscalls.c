/* Minimal _sbrk implementation for newlib nano, for heap allocation (malloc, etc.) */
#include <sys/types.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

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

// Minimal implementations for other newlib syscalls
int _write(int file, char *ptr, int len)
{
    (void)file;
    (void)ptr;
    (void)len;
    return -1; // Not implemented
}

int _read(int file, char *ptr, int len)
{
    (void)file;
    (void)ptr;
    (void)len;
    return -1; // Not implemented
}

int _close(int file)
{
    (void)file;
    return -1; // Not implemented
}

int _fstat(int file, struct stat *st)
{
    (void)file;
    st->st_mode = S_IFCHR;
    return 0;
}

int _isatty(int file)
{
    (void)file;
    return 1; // Pretend all files are TTY
}

int _lseek(int file, int ptr, int dir)
{
    (void)file;
    (void)ptr;
    (void)dir;
    return 0;
}

void _exit(int status)
{
    (void)status;
    while (1); // Infinite loop
}

int _kill(int pid, int sig)
{
    (void)pid;
    (void)sig;
    errno = EINVAL;
    return -1;
}

int _getpid(void)
{
    return 1; // Fake PID
}

#ifdef __cplusplus
}
#endif

/* Parse a GNU ld .map file and print the largest .bss (RAM) symbols. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    FILE *f;
    char line[512];
    char name[256];
    unsigned long addr, size;
    struct { char name[256]; unsigned long size; } e[4096];
    int n = 0, i, j;

    if (argc < 2) { fprintf(stderr, "usage: %s mapfile\n", argv[0]); return 1; }
    f = fopen(argv[1], "r");
    if (!f) { perror("open"); return 1; }

    while (fgets(line, sizeof(line), f)) {
        /* match: .bss.symbol   0xADDR   0xSIZE ... */
        if (strncmp(line, ".bss", 4) != 0)
            continue;
        if (sscanf(line, ".bss.%255s 0x%lx 0x%lx", name, &addr, &size) == 3 && size > 0) {
            if (n < 4096) {
                strncpy(e[n].name, name, 255);
                e[n].name[255] = 0;
                e[n].size = size;
                n++;
            }
        }
    }
    fclose(f);

    /* insertion sort by size desc */
    for (i = 1; i < n; i++) {
        char tn[256]; unsigned long ts;
        strcpy(tn, e[i].name); ts = e[i].size;
        for (j = i - 1; j >= 0 && e[j].size < ts; j--) {
            strcpy(e[j+1].name, e[j].name); e[j+1].size = e[j].size;
        }
        strcpy(e[j+1].name, tn); e[j+1].size = ts;
    }

    for (i = 0; i < n && i < 40; i++)
        printf("%6lu  %s\n", e[i].size, e[i].name);
    return 0;
}
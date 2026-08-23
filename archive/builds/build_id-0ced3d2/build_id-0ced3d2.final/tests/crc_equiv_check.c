/* One-off verification: original bit-serial MDC CRC vs new table-driven
 * implementation must produce identical results for all inputs. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

static uint16_t flip16(uint16_t crc, int bitnum)
{
    uint16_t out = 0, i, j = 1;
    for (i = (uint16_t)(1u << (bitnum - 1)); i != 0; i >>= 1) {
        if (crc & i) out |= j;
        j <<= 1;
    }
    return out;
}

static uint16_t crc_old(const uint8_t *p, size_t len)
{
    size_t i; int j, bit; uint16_t c, crc = 0;
    for (i = 0; i < len; ++i) {
        c = flip16(p[i], 8);
        for (j = 0x80; j; j >>= 1) {
            bit = crc & 0x8000;
            crc <<= 1;
            if (c & j) bit ^= 0x8000;
            if (bit) crc ^= 0x1021;
        }
    }
    crc = flip16(crc, 16);
    return (uint16_t)(crc ^ 0xffff);
}

/* The original is reflect-in/MSB-first/reflect-out, which is exactly an
 * LSB-first CRC with the reflected polynomial 0x8408. Table identity:
 *   crc = (crc >> 8) ^ T[(crc ^ byte) & 0xFF] */
static uint16_t tbl[256];
static void tbl_init(void)
{
    unsigned b; int j; uint16_t crc;
    for (b = 0; b < 256; ++b) {
        crc = (uint16_t)b;
        for (j = 0; j < 8; ++j)
            crc = (uint16_t)((crc >> 1) ^ ((crc & 1u) ? 0x8408u : 0u));
        tbl[b] = crc;
    }
}

static uint16_t crc_new(const uint8_t *p, size_t len)
{
    uint16_t crc = 0;
    tbl_init();
    while (len--) crc = (uint16_t)((crc >> 8) ^ tbl[(uint8_t)((crc ^ *p++) & 0xFF)]);
    return (uint16_t)(crc ^ 0xffff);
}

int main(void)
{
    uint8_t buf[64];
    size_t n, len;
    unsigned long seed = 12345;
    int fails = 0;

    /* deterministic pseudo-random buffers */
    for (n = 0; n < 20000; ++n) {
        for (len = 0; len < sizeof(buf); ++len) {
            seed = seed * 1103515245u + 12345u;
            buf[len] = (uint8_t)(seed >> 16);
        }
        for (len = 0; len <= sizeof(buf); ++len) {
            if (crc_old(buf, len) != crc_new(buf, len)) {
                printf("MISMATCH len=%lu\n", (unsigned long)len);
                fails++;
            }
        }
    }
    printf(fails ? "FAILED (%d)\n" : "OK: implementations identical\n", fails);
    return fails != 0;
}
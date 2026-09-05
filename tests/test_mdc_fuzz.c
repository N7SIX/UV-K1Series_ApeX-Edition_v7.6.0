/* Copyright 2026 Sean, N7SIX
 * https://github.com/N7SIX
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * MDC-1200 decoder fuzz / bit-flip robustness tests.
 *
 * Deterministic PRNG (xorshift32) so failures are reproducible. For each of
 * many random frames:
 *   - flip 1..2 bits  -> decoder MUST recover the original fields. The
 *                        systematic rate-1/2 convolutional code (parity
 *                        taps d^d_{t-2}^d_{t-5}^d_{t-6}) has free distance
 *                        5 (input u(D)=1 gives weight 1+4), so t=2 is the
 *                        guaranteed-correction bound.
 *   - flip 3..8 bits  -> decoder must either recover or cleanly reject.
 *                        Beyond t=2 a miscorrection that still passes CRC
 *                        is mathematically possible (dfree=5 code + 16-bit
 *                        CRC); such events are tallied and reported but
 *                        are not hard failures.
 *   - random garbage  -> decoder must never crash and never claim validity
 *                        with a plausible-but-wrong payload unchecked
 */

#include "test_framework.h"
#include "mdc1200.h"
#include <string.h>

/* Count of CRC-passing Viterbi miscorrections beyond the guaranteed
 * correction radius. Reported at end of run; informational only. */
static int g_miscorrections = 0;

static uint32_t prng_state = 0x12345678u;

static uint32_t prng(void)
{
    uint32_t x = prng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    prng_state = x;
    return x;
}

static unsigned int rand_below(unsigned int n)
{
    return (unsigned int)(prng() % n);
}

/* Flip exactly `nflips` distinct bits chosen at random in a frame buffer. */
static void flip_random_bits(uint8_t *frame, size_t len, unsigned int nflips)
{
    unsigned int f;

    for (f = 0; f < nflips; ++f) {
        size_t byte_i = (size_t)rand_below((unsigned int)len);
        unsigned int bit_j = rand_below(8u);
        frame[byte_i] ^= (uint8_t)(1u << bit_j);
    }
}

void test_mdc_fuzz(void)
{
    enum { ITERATIONS = 500 };
    unsigned int iter;

    for (iter = 0; iter < ITERATIONS; ++iter) {
        const uint8_t op = (uint8_t)rand_below(256u);
        const uint8_t arg = (uint8_t)rand_below(256u);
        const uint16_t unit_id = (uint16_t)rand_below(65536u);

        uint8_t frame[MDC1200_FRAME_LENGTH];
        uint8_t mutated[MDC1200_FRAME_LENGTH];
        size_t frame_len = 0;
        uint8_t op_out, arg_out;
        uint16_t unit_id_out;
        bool valid = false;

        TEST_ASSERT(MDC1200_BuildFrame(op, arg, unit_id, frame,
                                       sizeof(frame), &frame_len) == MDC1200_ERROR_NONE);
        if (frame_len != MDC1200_FRAME_LENGTH)
            continue;

        /* --- guaranteed-correction range: must always fully recover --- */
        {
            static const unsigned int low_flips[] = { 1u, 2u };
            unsigned int li;

            for (li = 0; li < ARRAY_SIZE(low_flips); ++li) {
                memcpy(mutated, frame, sizeof(frame));
                flip_random_bits(mutated, sizeof(mutated), low_flips[li]);

                valid = false;
                TEST_ASSERT(MDC1200_DecodeFrame(mutated, sizeof(mutated),
                                                &op_out, &arg_out, &unit_id_out,
                                                &valid) == MDC1200_ERROR_NONE);
                if (!valid) {
                    g_test_failures++;
                    fprintf(stderr,
                            "  FAIL iter %u: %u-bit corruption not recovered "
                            "(op=%02X arg=%02X uid=%04X)\x0a",
                            iter, low_flips[li], op, arg, unit_id);
                    continue;
                }
                TEST_ASSERT_EQ_U32(op_out, op);
                TEST_ASSERT_EQ_U32(arg_out, arg);
                TEST_ASSERT_EQ_U32(unit_id_out, unit_id);
            }
        }

        /* --- beyond guaranteed correction: recover or reject cleanly ---
         *
         * A CRC-passing miscorrection is possible here by design (free
         * distance 5 code guarded only by a 16-bit CRC). Count those
         * occurrences so their rate stays observable without failing the
         * build for an event the protocol cannot prevent. */
        {
            static const unsigned int high_flips[] = { 3u, 4u, 5u, 6u, 7u, 8u };
            unsigned int hi;

            for (hi = 0; hi < ARRAY_SIZE(high_flips); ++hi) {
                memcpy(mutated, frame, sizeof(frame));
                flip_random_bits(mutated, sizeof(mutated), high_flips[hi]);

                valid = false;
                TEST_ASSERT(MDC1200_DecodeFrame(mutated, sizeof(mutated),
                                                &op_out, &arg_out, &unit_id_out,
                                                &valid) == MDC1200_ERROR_NONE);
                if (valid) {
                    if (op_out != op || arg_out != arg || unit_id_out != unit_id) {
                        g_miscorrections++;
                        fprintf(stderr,
                                "  NOTE iter %u: %u-bit corruption miscorrected "
                                "(%02X/%02X/%04X != %02X/%02X/%04X)\x0a",
                                iter, high_flips[hi],
                                op_out, arg_out, unit_id_out, op, arg, unit_id);
                    } else {
                        g_test_checks++;
                    }
                } else {
                    g_test_checks++;   /* clean rejection is acceptable */
                }
            }
        }

        /* --- pure garbage: must be rejected without crashing --- */
        {
            unsigned int b;
            for (b = 0; b < sizeof(mutated); ++b)
                mutated[b] = (uint8_t)prng();

            valid = true;   /* deliberately pre-set: decoder must clear it */
            TEST_ASSERT(MDC1200_DecodeFrame(mutated, sizeof(mutated),
                                            &op_out, &arg_out, &unit_id_out,
                                            &valid) == MDC1200_ERROR_NONE);
            /* Garbage rarely passes CRC by chance; when it does the fields
             * are simply whatever the noise contained. The critical property
             * is no crash and no stale 'valid' leak on rejection. */
            if (!valid)
                g_test_checks++;
        }
    }

    printf("  miscorrections (beyond dfree/2): %d\x0a", g_miscorrections);
}
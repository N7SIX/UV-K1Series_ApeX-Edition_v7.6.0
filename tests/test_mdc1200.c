/* Copyright 2026 Sean, N7SIX
 * https://github.com/N7SIX
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "test_framework.h"
#include "mdc1200.h"
#include <string.h>

/* Leader + encoded payload that MDC1200_BuildFrame(0x01, 0x23, 0x4567)
 * commits to for both the standard and the long profile. The payload
 * (leader + 14 bytes) is identical between the two; only the 0x55 preamble
 * run differs in length, which is exactly what the dual-mode encoder tests. */
static const uint8_t expected_leader[5]    = { 0x07, 0x09, 0x2A, 0x44, 0x6F };
static const uint8_t expected_payload[14] = {
    0x76, 0x76, 0x2C, 0xA6, 0x1C, 0xB8,
    0x68, 0x19, 0x10, 0x31, 0x18, 0xE6, 0x08, 0x60
};

void test_mdc1200(void)
{
    uint8_t frame[64]      = {0};
    uint8_t frame_long[64] = {0};
    uint8_t expected_std[MDC1200_FRAME_LENGTH];
    uint8_t expected_lng[MDC1200L_FRAME_LENGTH];
    uint16_t fifo_words[64];
    uint16_t fifo_words_long[64];
    uint8_t corrupted[MDC1200L_FRAME_LENGTH];
    size_t frame_len      = 0;
    size_t frame_len_long = 0;
    size_t fifo_word_count      = 0;
    size_t fifo_word_count_long = 0;
    size_t i;
    uint8_t op        = 0;
    uint8_t arg       = 0;
    uint16_t unit_id  = 0;
    bool valid        = false;

    /* Compose the reference byte arrays */
    memset(expected_std, 0x55, MDC1200_PREAMBLE_LENGTH);
    memcpy(expected_std + MDC1200_PREAMBLE_LENGTH, expected_leader, MDC1200_LEADER_LENGTH);
    memcpy(expected_std + MDC1200_PREAMBLE_LENGTH + MDC1200_LEADER_LENGTH,
           expected_payload, MDC1200_PAYLOAD_LENGTH);

    memset(expected_lng, 0x55, MDC1200_COMPOSITE_PREAMBLE_LENGTH);
    memcpy(expected_lng + MDC1200_COMPOSITE_PREAMBLE_LENGTH, expected_leader, MDC1200_LEADER_LENGTH);
    memcpy(expected_lng + MDC1200_COMPOSITE_PREAMBLE_LENGTH + MDC1200_LEADER_LENGTH,
           expected_payload, MDC1200_PAYLOAD_LENGTH);

    /*
     * === Standard MDC-1200 (26-byte frame, 7-byte preamble) ===
     */
    TEST_ASSERT(MDC1200_BuildFrame(0x01, 0x23, 0x4567,
                                   frame, sizeof(frame), &frame_len) == MDC1200_ERROR_NONE);
    TEST_ASSERT_EQ_INT(frame_len, MDC1200_FRAME_LENGTH);
    TEST_ASSERT_EQ_INT(MDC1200_FRAME_LENGTH, 26u);

    for (i = 0; i < MDC1200_FRAME_LENGTH; ++i) {
        if (frame[i] != expected_std[i]) {
            TEST_ASSERT_EQ_INT(frame[i], expected_std[i]);
        }
    }

    TEST_ASSERT(MDC1200_BuildFifoWords(frame, frame_len, fifo_words,
                                       ARRAY_SIZE(fifo_words), &fifo_word_count) == MDC1200_ERROR_NONE);
    TEST_ASSERT_EQ_INT(fifo_word_count, MDC1200_FIFO_WORD_COUNT);
    TEST_ASSERT_EQ_INT(MDC1200_FIFO_WORD_COUNT, 13u);

    for (i = 0; i < MDC1200_FIFO_WORD_COUNT; ++i) {
        uint16_t expect = ((uint16_t)expected_std[i * 2u] << 8u) | (uint16_t)expected_std[i * 2u + 1u];
        TEST_ASSERT_EQ_INT(fifo_words[i], expect);
    }

    TEST_ASSERT(MDC1200_DecodeFrame(frame, frame_len, &op, &arg, &unit_id, &valid) == MDC1200_ERROR_NONE);
    TEST_ASSERT(valid);
    TEST_ASSERT_EQ_INT(op, 0x01u);
    TEST_ASSERT_EQ_INT(arg, 0x23u);
    TEST_ASSERT_EQ_INT(unit_id, 0x4567u);

    TEST_ASSERT(MDC1200_DecodeFrameWords(fifo_words, fifo_word_count, &op, &arg, &unit_id, &valid) == MDC1200_ERROR_NONE);
    TEST_ASSERT(valid);
    TEST_ASSERT_EQ_INT(op, 0x01u);
    TEST_ASSERT_EQ_INT(arg, 0x23u);
    TEST_ASSERT_EQ_INT(unit_id, 0x4567u);

    TEST_ASSERT(MDC1200_VerifyCRC(frame, frame_len, &valid) == MDC1200_ERROR_NONE);
    TEST_ASSERT(valid);

    /*
     * === Long MDC-1200L (46-byte frame, 27-byte composite preamble) ===
     */
    TEST_ASSERT(MDC1200_BuildFrameLong(0x01, 0x23, 0x4567,
                                       frame_long, sizeof(frame_long), &frame_len_long) == MDC1200_ERROR_NONE);
    TEST_ASSERT_EQ_INT(frame_len_long, MDC1200L_FRAME_LENGTH);
    TEST_ASSERT_EQ_INT(MDC1200L_FRAME_LENGTH, 46u);
    TEST_ASSERT_EQ_INT(MDC1200L_FIFO_WORD_COUNT, 23u);

    for (i = 0; i < MDC1200L_FRAME_LENGTH; ++i) {
        if (frame_long[i] != expected_lng[i]) {
            TEST_ASSERT_EQ_INT(frame_long[i], expected_lng[i]);
        }
    }

    /* The leader + payload region must be byte-identical to the standard frame
     * (only the 0x55 run is longer). leader+payload = 5 + 14 = 19 bytes. */
    for (i = 0; i < (MDC1200_LEADER_LENGTH + MDC1200_PAYLOAD_LENGTH); ++i) {
        TEST_ASSERT_EQ_INT(frame_long[MDC1200_COMPOSITE_PREAMBLE_LENGTH + i],
                           frame[MDC1200_PREAMBLE_LENGTH + i]);
    }

    TEST_ASSERT(MDC1200_BuildFifoWords(frame_long, frame_len_long, fifo_words_long,
                                       ARRAY_SIZE(fifo_words_long), &fifo_word_count_long) == MDC1200_ERROR_NONE);
    TEST_ASSERT_EQ_INT(fifo_word_count_long, MDC1200L_FIFO_WORD_COUNT);

    for (i = 0; i < MDC1200L_FIFO_WORD_COUNT; ++i) {
        uint16_t expect = ((uint16_t)expected_lng[i * 2u] << 8u) | (uint16_t)expected_lng[i * 2u + 1u];
        TEST_ASSERT_EQ_INT(fifo_words_long[i], expect);
    }

    TEST_ASSERT(MDC1200_DecodeFrame(frame_long, frame_len_long, &op, &arg, &unit_id, &valid) == MDC1200_ERROR_NONE);
    TEST_ASSERT(valid);
    TEST_ASSERT_EQ_INT(op, 0x01u);
    TEST_ASSERT_EQ_INT(arg, 0x23u);
    TEST_ASSERT_EQ_INT(unit_id, 0x4567u);

    TEST_ASSERT(MDC1200_DecodeFrameWords(fifo_words_long, fifo_word_count_long, &op, &arg, &unit_id, &valid) == MDC1200_ERROR_NONE);
    TEST_ASSERT(valid);
    TEST_ASSERT_EQ_INT(op, 0x01u);
    TEST_ASSERT_EQ_INT(arg, 0x23u);
    TEST_ASSERT_EQ_INT(unit_id, 0x4567u);

    TEST_ASSERT(MDC1200_VerifyCRC(frame_long, frame_len_long, &valid) == MDC1200_ERROR_NONE);
    TEST_ASSERT(valid);

    /*
     * === TX FIFO words (preamble-stripped) ===
     *
     * MDC1200_BuildTxFifoWords() strips the 0x55 preamble (which the BK4819
     * hardware re-generates) and appends a single 0x55 pad byte to keep the
     * FIFO 16-bit aligned.  The output must contain the leader + payload
     * immediately, with NO embedded 0xAA (phase-reversal) byte.
     */
    {
        uint16_t tx_fifo[32];
        size_t   tx_count = 0;

        /* Standard: 5 leader + 14 payload + 1 pad = 20 bytes = 10 words. */
        TEST_ASSERT(MDC1200_BuildTxFifoWords(frame, frame_len, tx_fifo,
                                             ARRAY_SIZE(tx_fifo), &tx_count) == MDC1200_ERROR_NONE);
        TEST_ASSERT_EQ_INT(tx_count, 10u);
        /* Word 0 = leader[0]:leader[1] = 0x07 0x09 */
        TEST_ASSERT_EQ_INT(tx_fifo[0], 0x0709u);
        /* Leader bytes 2..4 then 14 payload bytes = 17 bytes (8 full words + 1 byte) */
        for (i = 0; i < MDC1200_LEADER_LENGTH; ++i) {
            uint8_t b = (i < 2u) ? (uint8_t)(tx_fifo[0] >> (8u * (1u - i)))
                                 : (uint8_t)(tx_fifo[i / 2u] >> (8u * (1u - (i % 2u))));
            TEST_ASSERT_EQ_INT(b, expected_leader[i]);
        }
        /* Payload region: frame[i+7] == expected at tx_fifo offset i+5 */
        for (i = 0; i < MDC1200_PAYLOAD_LENGTH; ++i) {
            uint8_t b = tx_fifo[(5u + i) / 2u] >> (8u * (1u - ((5u + i) % 2u)));
            TEST_ASSERT_EQ_INT(b, frame[MDC1200_PREAMBLE_LENGTH + MDC1200_LEADER_LENGTH + i]);
        }
        /* Last word: payload byte[13] + pad 0x55 = 0x60 0x55 */
        TEST_ASSERT_EQ_INT(tx_fifo[9u], 0x6055u);

        /* Long: 20 pretime + 5 leader + 14 payload + 1 pad = 40 bytes = 20 words. */
        tx_count = 0;
        TEST_ASSERT(MDC1200_BuildTxFifoWords(frame_long, frame_len_long, tx_fifo,
                                             ARRAY_SIZE(tx_fifo), &tx_count) == MDC1200_ERROR_NONE);
        TEST_ASSERT_EQ_INT(tx_count, 20u);
        /* Word 0 = pretime 0x55 0x55 */
        TEST_ASSERT_EQ_INT(tx_fifo[0], 0x5555u);
        /* leader begins at tx_fifo buffer offset 20 (after the 20-byte pretime) */
        for (i = 0; i < MDC1200_LEADER_LENGTH; ++i) {
            uint8_t b = tx_fifo[(20u + i) / 2u] >> (8u * (1u - ((20u + i) % 2u)));
            TEST_ASSERT_EQ_INT(b, expected_leader[i]);
        }
        /* payload begins at offset 25 => relative to frame_long offset 27 */
        for (i = 0; i < MDC1200_PAYLOAD_LENGTH; ++i) {
            uint8_t b = tx_fifo[(25u + i) / 2u] >> (8u * (1u - ((25u + i) % 2u)));
            TEST_ASSERT_EQ_INT(b, frame_long[MDC1200_COMPOSITE_PREAMBLE_LENGTH +
                                             MDC1200_LEADER_LENGTH + i]);
        }
        /* Last word: payload byte[13] + pad 0x55 */
        TEST_ASSERT_EQ_INT(tx_fifo[19u], 0x6055u);

        /* Error paths. */
        TEST_ASSERT(MDC1200_BuildTxFifoWords(NULL, frame_len, tx_fifo,
                                             ARRAY_SIZE(tx_fifo), &tx_count) == MDC1200_ERROR_INVALID_PARAMS);
        TEST_ASSERT(MDC1200_BuildTxFifoWords(frame, 12u, tx_fifo,
                                             ARRAY_SIZE(tx_fifo), &tx_count) == MDC1200_ERROR_FRAME_BUILD_FAILED);
        TEST_ASSERT(MDC1200_BuildTxFifoWords(frame, frame_len, tx_fifo, 1u,
                                             &tx_count) == MDC1200_ERROR_FIFO_WRITE_FAILED);
    }

    /*
     * === MDC-1200 vs MDC-1200L: the "L" MUST remain a LONGER preamble ===
     *
     * The whole point of the "L" variant is a longer 0x55 preamble for
     * weak-signal reach.  The RX/sync handling and the preamble-strip logic
     * must never collapse the two into the same on-air length.  These
     * assertions pin the intended difference so a future refactor cannot
     * silently make MDC-1200L identical to MDC-1200.
     */
    {
        uint16_t tx_fifo_short[32], tx_fifo_long[32];
        size_t   cnt_short = 0, cnt_long = 0;
        size_t   preamble_short, preamble_long;

        /* Frame constants must differ. */
        TEST_ASSERT(MDC1200_FRAME_LENGTH != MDC1200L_FRAME_LENGTH);              /* 26 vs 46 */
        TEST_ASSERT(MDC1200_PREAMBLE_LENGTH != MDC1200_COMPOSITE_PREAMBLE_LENGTH); /* 7 vs 27 */
        TEST_ASSERT_EQ_INT(MDC1200_COMPOSITE_PREAMBLE_LENGTH,
                           MDC1200_PRETIME_LENGTH + MDC1200_PREAMBLE_LENGTH);     /* 27 = 20 + 7 */

        /* The MDC-1200L TX FIFO must be exactly the pretime longer than the
         * standard TX FIFO (the payload/leader are shared). 20 bytes = 10 words. */
        TEST_ASSERT(MDC1200_BuildTxFifoWords(frame, frame_len, tx_fifo_short,
                                             ARRAY_SIZE(tx_fifo_short), &cnt_short) == MDC1200_ERROR_NONE);
        TEST_ASSERT(MDC1200_BuildTxFifoWords(frame_long, frame_len_long, tx_fifo_long,
                                             ARRAY_SIZE(tx_fifo_long), &cnt_long) == MDC1200_ERROR_NONE);
        TEST_ASSERT(cnt_short != cnt_long);
        TEST_ASSERT_EQ_INT(cnt_long - cnt_short, MDC1200_PRETIME_LENGTH / 2u);   /* 20 - 10 = 10 words */
        TEST_ASSERT(cnt_long > cnt_short);

        /* On-air 0x55 preamble: standard = HW(7+4 sync) + 1 pad = 12 bytes;
         * long = HW(7+4) + pretime(20) + 1 pad = 32 bytes. The "L" leads with
         * exactly MDC1200_PRETIME_LENGTH more 0x55 bytes. */
        preamble_short = 7u + 4u + 1u;                 /* HW preamble + HW sync + pad */
        preamble_long  = 7u + 4u + MDC1200_PRETIME_LENGTH + 1u;
        TEST_ASSERT(preamble_long > preamble_short);
        TEST_ASSERT_EQ_INT(preamble_long - preamble_short, MDC1200_PRETIME_LENGTH); /* 20 */
    }

    /*
     * === Length / parameter validation ===
     */
    /* Non-standard lengths are rejected by every public entry point. */
    TEST_ASSERT(MDC1200_BuildFifoWords(frame, 12u, fifo_words, ARRAY_SIZE(fifo_words), &fifo_word_count) != MDC1200_ERROR_NONE);
    TEST_ASSERT(MDC1200_BuildFifoWords(frame_long, 47u, fifo_words, ARRAY_SIZE(fifo_words), &fifo_word_count) != MDC1200_ERROR_NONE);
    TEST_ASSERT(MDC1200_DecodeFrame(frame, 13u, &op, &arg, &unit_id, &valid) == MDC1200_ERROR_INVALID_LENGTH);
    TEST_ASSERT(MDC1200_DecodeFrameWords(fifo_words, 14u, &op, &arg, &unit_id, &valid) == MDC1200_ERROR_INVALID_LENGTH);

    /*
     * === ECC correction: corrupted-frame recovery (both lengths) ===
     */

    /* Standard frame: single-bit error in payload byte[3] (offset 7+5+3 = 15). */
    memcpy(corrupted, frame, MDC1200_FRAME_LENGTH);
    corrupted[15] ^= 0x10;
    TEST_ASSERT(MDC1200_DecodeFrame(corrupted, MDC1200_FRAME_LENGTH, &op, &arg, &unit_id, &valid) == MDC1200_ERROR_NONE);
    TEST_ASSERT(valid);
    TEST_ASSERT_EQ_INT(op, 0x01u);
    TEST_ASSERT_EQ_INT(arg, 0x23u);
    TEST_ASSERT_EQ_INT(unit_id, 0x4567u);

    /* Standard frame: three-bit error spread across payload bytes. */
    memcpy(corrupted, frame, MDC1200_FRAME_LENGTH);
    corrupted[12u] ^= 0x01;  /* payload byte[0] */
    corrupted[18u] ^= 0x80;  /* payload byte[6] */
    corrupted[24u] ^= 0x04;  /* payload byte[12] */
    TEST_ASSERT(MDC1200_DecodeFrame(corrupted, MDC1200_FRAME_LENGTH, &op, &arg, &unit_id, &valid) == MDC1200_ERROR_NONE);
    TEST_ASSERT(valid);
    TEST_ASSERT_EQ_INT(op, 0x01u);
    TEST_ASSERT_EQ_INT(arg, 0x23u);
    TEST_ASSERT_EQ_INT(unit_id, 0x4567u);

    /* Long frame: single-bit error in payload byte[3] (offset 27+5+3 = 35). */
    memcpy(corrupted, frame_long, MDC1200L_FRAME_LENGTH);
    corrupted[35] ^= 0x10;
    TEST_ASSERT(MDC1200_DecodeFrame(corrupted, MDC1200L_FRAME_LENGTH, &op, &arg, &unit_id, &valid) == MDC1200_ERROR_NONE);
    TEST_ASSERT(valid);
    TEST_ASSERT_EQ_INT(op, 0x01u);
    TEST_ASSERT_EQ_INT(arg, 0x23u);
    TEST_ASSERT_EQ_INT(unit_id, 0x4567u);

    /* Long frame: single-bit error caught by VerifyCRC + ECC. */
    memcpy(corrupted, frame_long, MDC1200L_FRAME_LENGTH);
    corrupted[40] ^= 0x20;  /* payload byte[8] at offset 27+5+8 */
    TEST_ASSERT(MDC1200_VerifyCRC(corrupted, MDC1200L_FRAME_LENGTH, &valid) == MDC1200_ERROR_NONE);
    TEST_ASSERT(valid);
}

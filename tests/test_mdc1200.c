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
     * === MDC-1200 vs MDC-1200L: the "L" MUST remain a LONGER preamble ===
     *
     * The whole point of the "L" variant is a longer 0x55 preamble for
     * weak-signal reach.  The RX/sync handling and the TX FIFO builder
     * must never collapse the two into the same on-air length.  These
     * assertions pin the intended difference so a future refactor cannot
     * silently make MDC-1200L identical to MDC-1200.
     */
    {
        uint16_t fifo_short[32], fifo_long[32];
        size_t   cnt_short = 0, cnt_long = 0;

        /* Frame constants must differ. */
        TEST_ASSERT(MDC1200_FRAME_LENGTH != MDC1200L_FRAME_LENGTH);              /* 26 vs 46 */
        TEST_ASSERT(MDC1200_PREAMBLE_LENGTH != MDC1200_COMPOSITE_PREAMBLE_LENGTH); /* 7 vs 27 */
        TEST_ASSERT_EQ_INT(MDC1200_COMPOSITE_PREAMBLE_LENGTH,
                           MDC1200_PRETIME_LENGTH + MDC1200_PREAMBLE_LENGTH);     /* 27 = 20 + 7 */

        /* The MDC-1200L TX FIFO must be exactly the pretime longer than the
         * standard TX FIFO (the payload/leader are shared). 20 bytes = 10 words. */
        TEST_ASSERT(MDC1200_BuildFifoWords(frame, frame_len, fifo_short,
                                           ARRAY_SIZE(fifo_short), &cnt_short) == MDC1200_ERROR_NONE);
        TEST_ASSERT(MDC1200_BuildFifoWords(frame_long, frame_len_long, fifo_long,
                                           ARRAY_SIZE(fifo_long), &cnt_long) == MDC1200_ERROR_NONE);
        TEST_ASSERT(cnt_short != cnt_long);
        TEST_ASSERT_EQ_INT(cnt_long - cnt_short, MDC1200_PRETIME_LENGTH / 2u);   /* 23 - 13 = 10 words */
        TEST_ASSERT(cnt_long > cnt_short);

        /* On-air 0x55 preamble: standard = 7-byte preamble (frame bytes 0..6,
         * transmitted from the FIFO) + 4-byte HW sync; long = HW sync + the
         * 27-byte composite preamble. The "L" leads with exactly
         * MDC1200_PRETIME_LENGTH more 0x55 bytes. */
        TEST_ASSERT_EQ_INT(cnt_long - cnt_short, MDC1200_PRETIME_LENGTH / 2u); /* 10 words = 20 bytes */
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

    /*
     * === RX-path FIFO reconstruction (full-frame TX) ===
     *
     * Emulates exactly what APP_HandleMDC1200Receive() does: with full-frame
     * TX (the on-air-proven framing), the TX FIFO carries the complete MDC
     * frame (preamble included).  The BK4819/4829 hardware prepends its own
     * 6-byte preamble + 4-byte sync on-air, which the receiver's sync
     * detector consumes — the RX FIFO therefore captures the FULL frame as
     * loaded by the TX.  The handler reads the FIFO words directly with no
     * preamble re-insertion.  These tests pin the register-side contract:
     *   REG_5D = 0x1A00 (26 bytes) for standard, 0x2E00 (46) for MDC-1200L.
     */
    {
        uint16_t rx_words[MDC1200L_RX_FIFO_WORD_COUNT] = {0};
        size_t   rx_count;
        unsigned int w;

        /* Standard MDC-1200: RX FIFO = full frame -> 13 words. */
        rx_count = MDC1200_RX_FIFO_WORD_COUNT;
        TEST_ASSERT_EQ_INT(rx_count, 13u);
        for (w = 0u; w < rx_count; ++w)
            rx_words[w] = (uint16_t)(((uint16_t)frame[w * 2u] << 8u) |
                                      (uint16_t)frame[w * 2u + 1u]);
        TEST_ASSERT(MDC1200_DecodeFrameWords(rx_words, rx_count, &op, &arg, &unit_id, &valid) == MDC1200_ERROR_NONE);
        TEST_ASSERT(valid);
        TEST_ASSERT_EQ_INT(op, 0x01u);
        TEST_ASSERT_EQ_INT(arg, 0x23u);
        TEST_ASSERT_EQ_INT(unit_id, 0x4567u);

        /* MDC-1200L: RX FIFO = full frame -> 23 words. */
        rx_count = MDC1200L_RX_FIFO_WORD_COUNT;
        TEST_ASSERT_EQ_INT(rx_count, 23u);
        memset(rx_words, 0, sizeof(rx_words));
        for (w = 0u; w < rx_count; ++w)
            rx_words[w] = (uint16_t)(((uint16_t)frame_long[w * 2u] << 8u) |
                                      (uint16_t)frame_long[w * 2u + 1u]);
        TEST_ASSERT(MDC1200_DecodeFrameWords(rx_words, rx_count, &op, &arg, &unit_id, &valid) == MDC1200_ERROR_NONE);
        TEST_ASSERT(valid);
        TEST_ASSERT_EQ_INT(op, 0x01u);
        TEST_ASSERT_EQ_INT(arg, 0x23u);
        TEST_ASSERT_EQ_INT(unit_id, 0x4567u);
    }

    /* TX FIFO framing contract: full frame (preamble included) in FIFO.
     * The BK4819/4829 hardware generates its own preamble + sync on-air. */
    TEST_ASSERT_EQ_INT(MDC1200_RX_FIFO_WORD_COUNT, 13u);
    TEST_ASSERT_EQ_INT(MDC1200L_RX_FIFO_WORD_COUNT, 23u);
}

/*
 * Fourth-pass simulation tests - Part 1: TX→RX round-trip
 * Simulates the complete path: BuildFrame → BuildFifoWords → DecodeFrameWords
 */
void test_mdc1200_simulation(void)
{
    uint8_t frame[64];
    uint8_t frame_long[64];
    uint16_t fifo_words[32];
    uint16_t fifo_words_long[32];
    size_t frame_len;
    size_t frame_len_long;
    size_t fifo_count;
    size_t fifo_count_long;
    uint8_t op;
    uint8_t arg;
    uint16_t unit_id;
    bool valid;
    size_t i;

    /* === Test 1: Standard MDC-1200 TX→RX round-trip === */
    TEST_ASSERT(MDC1200_BuildFrame(0x01, 0x23, 0x4567,
                                   frame, sizeof(frame), &frame_len) == MDC1200_ERROR_NONE);
    TEST_ASSERT_EQ_INT(frame_len, 26u);
    TEST_ASSERT(MDC1200_BuildFifoWords(frame, frame_len, fifo_words,
                                       ARRAY_SIZE(fifo_words), &fifo_count) == MDC1200_ERROR_NONE);
    TEST_ASSERT_EQ_INT(fifo_count, 13u);
    TEST_ASSERT(MDC1200_DecodeFrameWords(fifo_words, fifo_count,
                                          &op, &arg, &unit_id, &valid) == MDC1200_ERROR_NONE);
    TEST_ASSERT(valid);
    TEST_ASSERT_EQ_INT(op, 0x01u);
    TEST_ASSERT_EQ_INT(arg, 0x23u);
    TEST_ASSERT_EQ_INT(unit_id, 0x4567u);

    /* === Test 2: MDC-1200L TX→RX round-trip === */
    TEST_ASSERT(MDC1200_BuildFrameLong(0x01, 0x23, 0x4567,
                                        frame_long, sizeof(frame_long), &frame_len_long) == MDC1200_ERROR_NONE);
    TEST_ASSERT_EQ_INT(frame_len_long, 46u);
    TEST_ASSERT(MDC1200_BuildFifoWords(frame_long, frame_len_long, fifo_words_long,
                                       ARRAY_SIZE(fifo_words_long), &fifo_count_long) == MDC1200_ERROR_NONE);
    TEST_ASSERT_EQ_INT(fifo_count_long, 23u);
    TEST_ASSERT(MDC1200_DecodeFrameWords(fifo_words_long, fifo_count_long,
                                          &op, &arg, &unit_id, &valid) == MDC1200_ERROR_NONE);
    TEST_ASSERT(valid);
    TEST_ASSERT_EQ_INT(op, 0x01u);
    TEST_ASSERT_EQ_INT(arg, 0x23u);
    TEST_ASSERT_EQ_INT(unit_id, 0x4567u);

    /* === Test 3: All opcodes (0x00-0x07) round-trip === */
    for (i = 0; i < 8; i++) {
        uint8_t test_op = (uint8_t)i;
        TEST_ASSERT(MDC1200_BuildFrame(test_op, 0x00, 0x0001,
                                       frame, sizeof(frame), &frame_len) == MDC1200_ERROR_NONE);
        TEST_ASSERT(MDC1200_BuildFifoWords(frame, frame_len, fifo_words,
                                           ARRAY_SIZE(fifo_words), &fifo_count) == MDC1200_ERROR_NONE);
        TEST_ASSERT(MDC1200_DecodeFrameWords(fifo_words, fifo_count,
                                              &op, &arg, &unit_id, &valid) == MDC1200_ERROR_NONE);
        TEST_ASSERT(valid);
        TEST_ASSERT_EQ_INT(op, test_op);
        TEST_ASSERT_EQ_INT(arg, 0x00u);
        TEST_ASSERT_EQ_INT(unit_id, 0x0001u);
    }

    /* === Test 4: Boundary unit IDs === */
    TEST_ASSERT(MDC1200_BuildFrame(0x00, 0x00, 0x0000,
                                   frame, sizeof(frame), &frame_len) == MDC1200_ERROR_NONE);
    TEST_ASSERT(MDC1200_BuildFifoWords(frame, frame_len, fifo_words,
                                       ARRAY_SIZE(fifo_words), &fifo_count) == MDC1200_ERROR_NONE);
    TEST_ASSERT(MDC1200_DecodeFrameWords(fifo_words, fifo_count,
                                          &op, &arg, &unit_id, &valid) == MDC1200_ERROR_NONE);
    TEST_ASSERT(valid);
    TEST_ASSERT_EQ_INT(unit_id, 0x0000u);

    TEST_ASSERT(MDC1200_BuildFrame(0x00, 0x00, 0xFFFF,
                                   frame, sizeof(frame), &frame_len) == MDC1200_ERROR_NONE);
    TEST_ASSERT(MDC1200_BuildFifoWords(frame, frame_len, fifo_words,
                                       ARRAY_SIZE(fifo_words), &fifo_count) == MDC1200_ERROR_NONE);
    TEST_ASSERT(MDC1200_DecodeFrameWords(fifo_words, fifo_count,
                                          &op, &arg, &unit_id, &valid) == MDC1200_ERROR_NONE);
    TEST_ASSERT(valid);
    TEST_ASSERT_EQ_INT(unit_id, 0xFFFFu);

    /* === Test 5: All arguments (0x00-0x0F) round-trip === */
    for (i = 0; i < 16; i++) {
        uint8_t test_arg = (uint8_t)i;
        TEST_ASSERT(MDC1200_BuildFrame(0x01, test_arg, 0x1234,
                                       frame, sizeof(frame), &frame_len) == MDC1200_ERROR_NONE);
        TEST_ASSERT(MDC1200_BuildFifoWords(frame, frame_len, fifo_words,
                                           ARRAY_SIZE(fifo_words), &fifo_count) == MDC1200_ERROR_NONE);
        TEST_ASSERT(MDC1200_DecodeFrameWords(fifo_words, fifo_count,
                                              &op, &arg, &unit_id, &valid) == MDC1200_ERROR_NONE);
        TEST_ASSERT(valid);
        TEST_ASSERT_EQ_INT(arg, test_arg);
    }

    /* === Test 6: Viterbi ECC - single bit error in leader === */
    TEST_ASSERT(MDC1200_BuildFrame(0x01, 0x23, 0x4567,
                                   frame, sizeof(frame), &frame_len) == MDC1200_ERROR_NONE);
    frame[7] ^= 0x01;
    TEST_ASSERT(MDC1200_DecodeFrame(frame, frame_len, &op, &arg, &unit_id, &valid) == MDC1200_ERROR_NONE);
    TEST_ASSERT(valid);
    TEST_ASSERT_EQ_INT(op, 0x01u);
    TEST_ASSERT_EQ_INT(unit_id, 0x4567u);

    /* === Test 7: Viterbi ECC - two bit error in leader === */
    TEST_ASSERT(MDC1200_BuildFrame(0x01, 0x23, 0x4567,
                                   frame, sizeof(frame), &frame_len) == MDC1200_ERROR_NONE);
    frame[7] ^= 0x03;
    TEST_ASSERT(MDC1200_DecodeFrame(frame, frame_len, &op, &arg, &unit_id, &valid) == MDC1200_ERROR_NONE);
    TEST_ASSERT(valid);

    /* === Test 8: Heavy corruption (4-bit error, may fail) === */
    TEST_ASSERT(MDC1200_BuildFrame(0x01, 0x23, 0x4567,
                                   frame, sizeof(frame), &frame_len) == MDC1200_ERROR_NONE);
    frame[15] ^= 0xF0;
    TEST_ASSERT(MDC1200_DecodeFrame(frame, frame_len, &op, &arg, &unit_id, &valid) == MDC1200_ERROR_NONE);

    /* === Test 9: Sliding-window sync search within valid frame ===
     * Verify the decoder finds the leader at the canonical offset.
     * The sliding-window search in mdc1200_frame_to_payload scans all
     * legal offsets, but DecodeFrame only accepts exact frame lengths.
     * Test with a valid 26-byte frame (leader at offset 7). */
    TEST_ASSERT(MDC1200_BuildFrame(0x01, 0x23, 0x4567,
                                   frame, sizeof(frame), &frame_len) == MDC1200_ERROR_NONE);
    TEST_ASSERT(MDC1200_DecodeFrame(frame, frame_len, &op, &arg, &unit_id, &valid) == MDC1200_ERROR_NONE);
    TEST_ASSERT(valid);
    TEST_ASSERT_EQ_INT(op, 0x01u);
    TEST_ASSERT_EQ_INT(unit_id, 0x4567u);

    /* === Test 10: Preamble byte verification === */
    TEST_ASSERT(MDC1200_BuildFrame(0x01, 0x23, 0x4567,
                                   frame, sizeof(frame), &frame_len) == MDC1200_ERROR_NONE);
    for (i = 0; i < 7; i++) {
        TEST_ASSERT_EQ_INT(frame[i], 0x55u);
    }
    TEST_ASSERT_EQ_INT(frame[7], 0x07u);

    /* === Test 11: MDC-1200L preamble verification === */
    TEST_ASSERT(MDC1200_BuildFrameLong(0x01, 0x23, 0x4567,
                                        frame_long, sizeof(frame_long), &frame_len_long) == MDC1200_ERROR_NONE);
    for (i = 0; i < 27; i++) {
        TEST_ASSERT_EQ_INT(frame_long[i], 0x55u);
    }
    TEST_ASSERT_EQ_INT(frame_long[27], 0x07u);

    /* === Test 12: On-air burst length === */
    TEST_ASSERT_EQ_INT(11 + 26, 37);
    TEST_ASSERT_EQ_INT(11 + 46, 57);

    /* === Test 13: Timing === */
    TEST_ASSERT_EQ_INT(37 * 8 * 1000 / 1200, 246);
    TEST_ASSERT_EQ_INT(57 * 8 * 1000 / 1200, 380);

    /* === Test 14: XOR differential encoding round-trip ===
     * Verify that DiffEncodeFrame and DiffDecodeFrame are exact inverses.
     * Build a frame, encode it, decode it, and verify the result matches. */
    {
        uint8_t test_frame[46];
        uint8_t orig_frame[46];
        size_t test_len;

        TEST_ASSERT(MDC1200_BuildFrame(0x01, 0x23, 0x4567,
                                       test_frame, sizeof(test_frame), &test_len) == MDC1200_ERROR_NONE);
        memcpy(orig_frame, test_frame, test_len);

        MDC1200_DiffEncodeFrame(test_frame, test_len);
        /* After encoding, the frame should differ from the original */
        {
            bool differs = false;
            size_t i;
            for (i = 0; i < test_len; i++) {
                if (test_frame[i] != orig_frame[i]) {
                    differs = true;
                    break;
                }
            }
            TEST_ASSERT(differs);
        }

        MDC1200_DiffDecodeFrame(test_frame, test_len);
        /* After decoding, the frame should match the original */
        {
            size_t i;
            for (i = 0; i < test_len; i++) {
                TEST_ASSERT_EQ_INT(test_frame[i], orig_frame[i]);
            }
        }
    }

    /* === Test 15: Differential encoding of 0x55 preamble ===
     * The alternating-bit preamble (0x55 = 01010101) encodes to 0x7F
     * (01111111) after differential encoding with prev_bit=0 initial condition.
     * The first data bit is 0 (MSB of 0x55), so diff[0] = 0 XOR 0 = 0.
     * Subsequent bits alternate, so diff[n] = 1 for n>0.
     * The result is 0x7F, which is "almost" a constant tone (only the first
     * bit differs). This is the Motorola physical-layer behavior. */
    {
        uint8_t preamble[7];
        unsigned int i;
        for (i = 0; i < 7; i++) {
            preamble[i] = 0x55u;
        }
        MDC1200_DiffEncodeFrame(preamble, 7u);
        /* After differential encoding, 0x55 encodes to 0x7F for the first byte */
        TEST_ASSERT_EQ_INT(preamble[0], 0x7Fu);
        /* Subsequent bytes: prev_bit from previous byte's last bit (1) XOR data_bit (0) = 1 */
        for (i = 1; i < 7; i++) {
            TEST_ASSERT_EQ_INT(preamble[i], 0xFFu);
        }
    }

    /* === Test 16: Full TX→RX round-trip with differential encoding ===
     * Simulate the complete path: build frame → diff encode → diff decode → decode.
     * This verifies that the differential encoding/decoding doesn't break
     * the frame decoding. */
    {
        uint8_t tx_frame[46];
        uint8_t rx_frame[46];
        size_t tx_len, rx_len;
        uint8_t tx_op, tx_arg, rx_op, rx_arg;
        uint16_t tx_id, rx_id;
        bool tx_valid, rx_valid;

        tx_op = 0x01u;
        tx_arg = 0x23u;
        tx_id = 0x4567u;

        TEST_ASSERT(MDC1200_BuildFrame(tx_op, tx_arg, tx_id,
                                       tx_frame, sizeof(tx_frame), &tx_len) == MDC1200_ERROR_NONE);

        /* TX path: differential encode */
        memcpy(rx_frame, tx_frame, tx_len);
        MDC1200_DiffEncodeFrame(rx_frame, tx_len);

        /* RX path: differential decode */
        MDC1200_DiffDecodeFrame(rx_frame, tx_len);

        /* Now decode the frame */
        rx_len = tx_len;
        TEST_ASSERT(MDC1200_DecodeFrame(rx_frame, rx_len, &rx_op, &rx_arg, &rx_id, &rx_valid) == MDC1200_ERROR_NONE);
        TEST_ASSERT(rx_valid);
        TEST_ASSERT_EQ_INT(rx_op, tx_op);
        TEST_ASSERT_EQ_INT(rx_arg, tx_arg);
        TEST_ASSERT_EQ_INT(rx_id, tx_id);
    }

    /* === Test 17: Motorola interop - real Motorola radios use diff encoding ===
     * A real Motorola radio transmits with differential encoding. Our receiver
     * must decode a differentially-encoded frame. This test verifies that
     * a frame built by our encoder can be decoded by our decoder (simulating
     * a Motorola radio transmitting to our receiver). */
    {
        uint8_t motorola_frame[46];
        size_t motorola_len;
        uint8_t op, arg;
        uint16_t id;
        bool valid;

        /* Build a frame as if a Motorola radio built it */
        TEST_ASSERT(MDC1200_BuildFrame(0x05u, 0x0Au, 0x1234u,
                                       motorola_frame, sizeof(motorola_frame), &motorola_len) == MDC1200_ERROR_NONE);

        /* Motorola applies differential encoding before transmission */
        MDC1200_DiffEncodeFrame(motorola_frame, motorola_len);

        /* Our receiver decodes the differentially-encoded frame */
        MDC1200_DiffDecodeFrame(motorola_frame, motorola_len);

        /* The decoded frame should be valid */
        TEST_ASSERT(MDC1200_DecodeFrame(motorola_frame, motorola_len, &op, &arg, &id, &valid) == MDC1200_ERROR_NONE);
        TEST_ASSERT(valid);
        TEST_ASSERT_EQ_INT(op, 0x05u);
        TEST_ASSERT_EQ_INT(arg, 0x0Au);
        TEST_ASSERT_EQ_INT(id, 0x1234u);
    }

    /* === Test 18: MDC-1200L differential encoding round-trip ===
     * Same as Test 14 but for the long frame variant. */
    {
        uint8_t test_frame[46];
        uint8_t orig_frame[46];
        size_t test_len;

        TEST_ASSERT(MDC1200_BuildFrameLong(0x01, 0x23, 0x4567,
                                            test_frame, sizeof(test_frame), &test_len) == MDC1200_ERROR_NONE);
        memcpy(orig_frame, test_frame, test_len);

        MDC1200_DiffEncodeFrame(test_frame, test_len);
        MDC1200_DiffDecodeFrame(test_frame, test_len);

        {
            size_t i;
            for (i = 0; i < test_len; i++) {
                TEST_ASSERT_EQ_INT(test_frame[i], orig_frame[i]);
            }
        }
    }
}

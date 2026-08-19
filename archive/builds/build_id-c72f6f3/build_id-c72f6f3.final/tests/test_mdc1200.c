/* Copyright 2026 Sean, N7SIX
 * https://github.com/N7SIX
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 */

#include "test_framework.h"
#include "mdc1200.h"

void test_mdc1200(void)
{
    uint8_t frame[64] = {0};
    uint8_t expected_frame[26] = {
        0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
        0x07, 0x09, 0x2A, 0x44, 0x6F,
        0x76, 0x76, 0x2C, 0xA6, 0x1C, 0xB8,
        0x68, 0x19, 0x10, 0x31, 0x18, 0xE6, 0x08, 0x60
    };
    uint16_t expected_fifo_words[13] = {
        0x5555, 0x5555, 0x5555, 0x5507, 0x092A, 0x446F,
        0x7676, 0x2CA6, 0x1CB8, 0x6819, 0x1031, 0x18E6, 0x0860
    };
    uint8_t op = 0;
    uint8_t arg = 0;
    uint16_t unit_id = 0;
    bool valid = false;
    size_t frame_len = 0;
    uint16_t fifo_words[32];
    size_t fifo_word_count = 0;
    size_t i;

    TEST_ASSERT(MDC1200_BuildFrame(0x01, 0x23, 0x4567, frame, sizeof(frame), &frame_len) == MDC1200_ERROR_NONE);
    TEST_ASSERT_EQ_INT(frame_len, 26u);
    for (i = 0; i < 26u; ++i) {
        TEST_ASSERT_EQ_INT(frame[i], expected_frame[i]);
    }

    TEST_ASSERT(MDC1200_BuildFifoWords(frame, frame_len, fifo_words, ARRAY_SIZE(fifo_words), &fifo_word_count) == MDC1200_ERROR_NONE);
    TEST_ASSERT_EQ_INT(fifo_word_count, 13u);
    for (i = 0; i < 13u; ++i) {
        TEST_ASSERT_EQ_INT(fifo_words[i], expected_fifo_words[i]);
    }

    TEST_ASSERT(MDC1200_DecodeFrame(frame, frame_len, &op, &arg, &unit_id, &valid) == MDC1200_ERROR_NONE);
    TEST_ASSERT(valid);
    TEST_ASSERT_EQ_INT(op, 0x01u);
    TEST_ASSERT_EQ_INT(arg, 0x23u);
    TEST_ASSERT_EQ_INT(unit_id, 0x4567u);

    TEST_ASSERT(MDC1200_VerifyCRC(frame, frame_len, &valid) == MDC1200_ERROR_NONE);
    TEST_ASSERT(valid);

    /* A professional MDC implementation rejects non-standard frame lengths. */
    TEST_ASSERT(MDC1200_BuildFifoWords(frame, 12u, fifo_words, ARRAY_SIZE(fifo_words), &fifo_word_count) != MDC1200_ERROR_NONE);
}

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
 *
 * Unit tests for the pure-logic CTCSS/DCS helpers in App/dcs.c.
 */

#include "test_framework.h"
#include "dcs.h"

static void test_CTCSS_Options_Size(void)
{
    /* The CTCSS table must contain exactly 50 entries */
    TEST_ASSERT_EQ_INT(ARRAY_SIZE(CTCSS_Options), 50);
    /* The DCS table must contain exactly 104 entries */
    TEST_ASSERT_EQ_INT(ARRAY_SIZE(DCS_Options), 104);
}

static void test_CTCSS_Options_Sorted(void)
{
    /* The CTCSS table must be strictly ascending */
    for (unsigned i = 1; i < ARRAY_SIZE(CTCSS_Options); i++)
        TEST_ASSERT(CTCSS_Options[i] > CTCSS_Options[i - 1]);
}

static void test_DCS_Options_Unique(void)
{
    /* The DCS table must contain no duplicate codes */
    for (unsigned i = 0; i < ARRAY_SIZE(DCS_Options); i++)
        for (unsigned j = i + 1; j < ARRAY_SIZE(DCS_Options); j++)
            TEST_ASSERT(DCS_Options[i] != DCS_Options[j]);
}

static void test_GetCtcssCode(void)
{
    /* Exact match returns the exact index */
    TEST_ASSERT_EQ_INT(DCS_GetCtcssCode(670), 0);    // 67.0 Hz
    TEST_ASSERT_EQ_INT(DCS_GetCtcssCode(1000), 12);  // 100.0 Hz
    TEST_ASSERT_EQ_INT(DCS_GetCtcssCode(2541), 49);  // 254.1 Hz

    /* Nearest-match behavior: 671 is closest to 670 (index 0) */
    TEST_ASSERT_EQ_INT(DCS_GetCtcssCode(671), 0);
    /* 1001 is closest to 1000 (index 12) */
    TEST_ASSERT_EQ_INT(DCS_GetCtcssCode(1001), 12);
}

static void test_GetGolayCodeWord(void)
{
    /* A known DCS code word: option 0 (0x0013) normal polarity */
    uint32_t normal = DCS_GetGolayCodeWord(CODE_TYPE_DIGITAL, 0);
    /* Reverse polarity is the bitwise complement of the normal code */
    uint32_t reverse = DCS_GetGolayCodeWord(CODE_TYPE_REVERSE_DIGITAL, 0);
    TEST_ASSERT_EQ_U32(reverse, normal ^ 0x7FFFFF);
}

static void test_GetCdcssCode(void)
{
    /* Round-trip: encode a DCS option, then decode it back */
    for (uint8_t opt = 0; opt < 104; opt++) {
        uint32_t code = DCS_GetGolayCodeWord(CODE_TYPE_DIGITAL, opt);
        TEST_ASSERT_EQ_INT(DCS_GetCdcssCode(code), opt);
    }
}

static void test_GetCtcssApprovedIndex(void)
{
    /* Index 0 (67.0 Hz) is approved -> approved index 0 */
    TEST_ASSERT_EQ_INT(DCS_GetCtcssApprovedIndex(0), 0);
    /* Index 1 (69.3 Hz) is in the extra (non-homologated) list -> skipped */
    TEST_ASSERT_EQ_INT(DCS_GetCtcssApprovedIndex(1), 0xFF);
    /* Out-of-range option -> 0xFF */
    TEST_ASSERT_EQ_INT(DCS_GetCtcssApprovedIndex(50), 0xFF);
}

static void test_GetDcsApprovedIndex(void)
{
    /* Index 0 (0x0013) is approved -> approved index 0 */
    TEST_ASSERT_EQ_INT(DCS_GetDcsApprovedIndex(0), 0);
    /* Index 5 is in the extra (non-PMR446) list -> skipped */
    TEST_ASSERT_EQ_INT(DCS_GetDcsApprovedIndex(5), 0xFF);
    /* Out-of-range option -> 0xFF */
    TEST_ASSERT_EQ_INT(DCS_GetDcsApprovedIndex(104), 0xFF);
}

void test_dcs(void)
{
    TEST_RUN(test_CTCSS_Options_Size);
    TEST_RUN(test_CTCSS_Options_Sorted);
    TEST_RUN(test_DCS_Options_Unique);
    TEST_RUN(test_GetCtcssCode);
    TEST_RUN(test_GetGolayCodeWord);
    TEST_RUN(test_GetCdcssCode);
    TEST_RUN(test_GetCtcssApprovedIndex);
    TEST_RUN(test_GetDcsApprovedIndex);
}
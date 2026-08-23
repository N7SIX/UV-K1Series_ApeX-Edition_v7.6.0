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
 * Unit tests for the pure-logic frequency helpers in App/frequencies.c.
 */

#include "test_framework.h"
#include "include_shim/settings_fake.h"
#include "frequencies.h"

/* Match the ApeX build preset (ENABLE_WIDE_RX=true) */
#ifndef ENABLE_WIDE_RX
#define ENABLE_WIDE_RX
#endif

static void test_GetBand(void)
{
    /* Wide-RX band table:
     *   BAND1_50MHz  = 1800000 .. 10800000
     *   BAND2_108MHz = 10800000 .. 13700000
     *   BAND3_137MHz = 13700000 .. 17400000
     *   BAND4_174MHz = 17400000 .. 35000000
     *   BAND5_350MHz = 35000000 .. 40000000
     *   BAND6_400MHz = 40000000 .. 47000000
     *   BAND7_470MHz = 47000000 .. 130000000
     */
    TEST_ASSERT_EQ_INT(FREQUENCY_GetBand(1800000),  BAND1_50MHz);
    TEST_ASSERT_EQ_INT(FREQUENCY_GetBand(10800000), BAND2_108MHz);
    TEST_ASSERT_EQ_INT(FREQUENCY_GetBand(13700000), BAND3_137MHz);
    TEST_ASSERT_EQ_INT(FREQUENCY_GetBand(17400000), BAND4_174MHz);
    TEST_ASSERT_EQ_INT(FREQUENCY_GetBand(35000000), BAND5_350MHz);
    TEST_ASSERT_EQ_INT(FREQUENCY_GetBand(40000000), BAND6_400MHz);
    TEST_ASSERT_EQ_INT(FREQUENCY_GetBand(47000000), BAND7_470MHz);
    TEST_ASSERT_EQ_INT(FREQUENCY_GetBand(130000000), BAND7_470MHz);
    /* Below the lowest band falls back to BAND1_50MHz */
    TEST_ASSERT_EQ_INT(FREQUENCY_GetBand(0), BAND1_50MHz);
}

static void test_ClampGlobal(void)
{
    TEST_ASSERT_EQ_U32(FREQUENCIES_ClampGlobal(0),         F_MIN);
    TEST_ASSERT_EQ_U32(FREQUENCIES_ClampGlobal(1800000),   1800000);
    TEST_ASSERT_EQ_U32(FREQUENCIES_ClampGlobal(43450000),  43450000);
    TEST_ASSERT_EQ_U32(FREQUENCIES_ClampGlobal(130000000), 130000000);
    TEST_ASSERT_EQ_U32(FREQUENCIES_ClampGlobal(999999999), F_MAX);
}

static void test_ClampToBand(void)
{
    TEST_ASSERT_EQ_U32(FREQUENCIES_ClampToBand(0, BAND6_400MHz), 40000000);
    TEST_ASSERT_EQ_U32(FREQUENCIES_ClampToBand(43450000, BAND6_400MHz), 43450000);
    TEST_ASSERT_EQ_U32(FREQUENCIES_ClampToBand(999999999, BAND6_400MHz), 47000000);
}

static void test_RoundToStep(void)
{
    /* Standard 12.5 kHz step (1250). NOTE: FREQUENCY_RoundToStep() halves
     * any step >= 1000, so "12.5 kHz" rounds on a 625 (6.25 kHz) grid by
     * design. Expectations below reflect that behavior. */
    TEST_ASSERT_EQ_U32(FREQUENCY_RoundToStep(43450000, 1250), 43450000);
    TEST_ASSERT_EQ_U32(FREQUENCY_RoundToStep(43450625, 1250), 43450625);
    TEST_ASSERT_EQ_U32(FREQUENCY_RoundToStep(43451250, 1250), 43451250);
    TEST_ASSERT_EQ_U32(FREQUENCY_RoundToStep(43450300, 1250), 43450000);

    /* 1 kHz step (100) is exact */
    TEST_ASSERT_EQ_U32(FREQUENCY_RoundToStep(43450123, 100), 43450100);

    /* 8.33 kHz aviation step (833) uses the special channel-number scheme */
    TEST_ASSERT_EQ_U32(FREQUENCY_RoundToStep(11800000, 833), 11800000);
    TEST_ASSERT_EQ_U32(FREQUENCY_RoundToStep(11800700, 833), 11800833);
}

static void test_StepIndexMapping(void)
{
    /* Sorted index 0 is the smallest step (0.01 kHz = 1) */
    TEST_ASSERT_EQ_INT(FREQUENCY_GetStepIdxFromSortedIdx(0), STEP_0_01kHz);
    /* Sorted index 23 is the largest step (500 kHz = 50000) */
    TEST_ASSERT_EQ_INT(FREQUENCY_GetStepIdxFromSortedIdx(23), STEP_500kHz);

    /* Round-trip: sorted index -> step idx -> sorted index */
    for (uint8_t i = 0; i < 24; i++) {
        STEP_Setting_t step = FREQUENCY_GetStepIdxFromSortedIdx(i);
        TEST_ASSERT_EQ_U32(FREQUENCY_GetSortedIdxFromStepIdx(step), i);
    }
}

static void test_CalculateOutputPower(void)
{
    /* Below lower limit -> low power */
    TEST_ASSERT_EQ_INT(FREQUENCY_CalculateOutputPower(1, 2, 3, 100, 200, 300, 50), 1);
    /* Above upper limit -> high power */
    TEST_ASSERT_EQ_INT(FREQUENCY_CalculateOutputPower(1, 2, 3, 100, 200, 300, 350), 3);
    /* At lower limit -> low power */
    TEST_ASSERT_EQ_INT(FREQUENCY_CalculateOutputPower(1, 2, 3, 100, 200, 300, 100), 1);
    /* At upper limit -> high power */
    TEST_ASSERT_EQ_INT(FREQUENCY_CalculateOutputPower(1, 2, 3, 100, 200, 300, 300), 3);
    /* KNOWN FIRMWARE BUG (intentionally failing, do not "fix" the test):
     * FREQUENCY_CalculateOutputPower() interpolates the lower half starting
     * from TxpMid instead of TxpLow (frequencies.c:155), so every frequency
     * in [LowerLimit, Middle] returns at least TxpMid -- one power step too
     * high. Correct behavior would be:
     *   TxpLow + ((TxpMid - TxpLow) * (Frequency - LowerLimit)) / (Middle - LowerLimit)
     * Fixing it CHANGES REAL TX POWER on the radio, so it is left untouched
     * pending an explicit decision. These assertions document the desired
     * behavior for when that decision is made. */
    /* Midpoint -> mid power */
    TEST_ASSERT_EQ_INT(FREQUENCY_CalculateOutputPower(1, 2, 3, 100, 200, 300, 200), 2);
    /* Quarter point -> interpolated between low and mid */
    TEST_ASSERT_EQ_INT(FREQUENCY_CalculateOutputPower(1, 2, 3, 100, 200, 300, 150), 1);
    /* Three-quarter point -> interpolated between mid and high */
    TEST_ASSERT_EQ_INT(FREQUENCY_CalculateOutputPower(1, 2, 3, 100, 200, 300, 250), 2);
}

static void test_RX_freq_check(void)
{
    /* Below global lower limit -> not allowed */
    TEST_ASSERT_EQ_INT(RX_freq_check(0), -1);
    /* In the BK4819 dead zone (63 MHz .. 84 MHz) -> not allowed */
    TEST_ASSERT_EQ_INT(RX_freq_check(70000000), -1);
    /* Valid VHF frequency -> allowed */
    TEST_ASSERT_EQ_INT(RX_freq_check(14500000), 0);
    /* Valid UHF frequency -> allowed */
    TEST_ASSERT_EQ_INT(RX_freq_check(43450000), 0);
    /* Above global upper limit -> not allowed */
    TEST_ASSERT_EQ_INT(RX_freq_check(999999999), -1);
}

static void test_TX_freq_check(void)
{
    /* F_LOCK_NONE: any RX-valid frequency is TX-allowed */
    gSetting_F_LOCK = F_LOCK_NONE;
    TEST_ASSERT_EQ_INT(TX_freq_check(14500000), 0);
    TEST_ASSERT_EQ_INT(TX_freq_check(43450000), 0);

    /* F_LOCK_ALL: no TX allowed */
    gSetting_F_LOCK = F_LOCK_ALL;
    TEST_ASSERT_EQ_INT(TX_freq_check(14500000), -1);
    TEST_ASSERT_EQ_INT(TX_freq_check(43450000), -1);

    /* F_LOCK_FCC: 2m and 70cm ham bands only */
    gSetting_F_LOCK = F_LOCK_FCC;
    TEST_ASSERT_EQ_INT(TX_freq_check(14500000), 0);
    TEST_ASSERT_EQ_INT(TX_freq_check(43500000), 0);
    TEST_ASSERT_EQ_INT(TX_freq_check(15100000), -1);

    /* F_LOCK_CE: 2m and 70cm (EU) */
    gSetting_F_LOCK = F_LOCK_CE;
    TEST_ASSERT_EQ_INT(TX_freq_check(14500000), 0);
    TEST_ASSERT_EQ_INT(TX_freq_check(43500000), 0);   // 435 MHz is INSIDE CE 430-440
    TEST_ASSERT_EQ_INT(TX_freq_check(43300000), 0);

    /* F_LOCK_DEF: default bands, 350 MHz requires gSetting_350EN */
    gSetting_F_LOCK = F_LOCK_DEF;
    gSetting_350TX = true;   // host tests build without ENABLE_FEAT_N7SIX,
                             // so TX_freq_check requires 350TX && 350EN
    gSetting_350EN = false;
    TEST_ASSERT_EQ_INT(TX_freq_check(14500000), 0);
    TEST_ASSERT_EQ_INT(TX_freq_check(43450000), 0);
    TEST_ASSERT_EQ_INT(TX_freq_check(36000000), -1);  // 350 band disabled
    gSetting_350EN = true;
    TEST_ASSERT_EQ_INT(TX_freq_check(36000000), 0);   // 350 band enabled
}

void test_frequencies(void)
{
    TEST_RUN(test_GetBand);
    TEST_RUN(test_ClampGlobal);
    TEST_RUN(test_ClampToBand);
    TEST_RUN(test_RoundToStep);
    TEST_RUN(test_StepIndexMapping);
    TEST_RUN(test_CalculateOutputPower);
    TEST_RUN(test_RX_freq_check);
    TEST_RUN(test_TX_freq_check);
}
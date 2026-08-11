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
 * Unit tests for the pure-logic CRC-16/CCITT helper in App/driver/crc.c.
 */

#include "test_framework.h"
#include "crc.h"

static void test_CRC_Empty(void)
{
    /* CRC of an empty buffer is 0 */
    TEST_ASSERT_EQ_U32(CRC_Calculate("", 0), 0x0000);
}

static void test_CRC_KnownVectors(void)
{
    /* CRC-16/CCITT (poly 0x1021, init 0x0000) known vectors */
    TEST_ASSERT_EQ_U32(CRC_Calculate("123456789", 9), 0x31C3);
    TEST_ASSERT_EQ_U32(CRC_Calculate("A", 1), 0xE5CC);
    TEST_ASSERT_EQ_U32(CRC_Calculate("123456789", 9), 0x31C3);
}

static void test_CRC_Deterministic(void)
{
    /* Same input always yields the same CRC */
    const uint8_t data[] = { 0xAA, 0x55, 0x01, 0x02, 0x03, 0x0A };
    uint16_t first = CRC_Calculate(data, sizeof(data));
    uint16_t second = CRC_Calculate(data, sizeof(data));
    TEST_ASSERT_EQ_U32(first, second);
}

static void test_CRC_ByteSensitivity(void)
{
    /* A single-byte change must change the CRC */
    const uint8_t a[] = { 0x01, 0x02, 0x03, 0x04 };
    const uint8_t b[] = { 0x01, 0x02, 0x03, 0x05 };
    TEST_ASSERT(CRC_Calculate(a, sizeof(a)) != CRC_Calculate(b, sizeof(b)));
}

static void test_CRC_LengthSensitivity(void)
{
    /* Appending a byte must change the CRC */
    const uint8_t a[] = { 0x01, 0x02, 0x03 };
    const uint8_t b[] = { 0x01, 0x02, 0x03, 0x04 };
    TEST_ASSERT(CRC_Calculate(a, sizeof(a)) != CRC_Calculate(b, sizeof(b)));
}

void test_crc(void)
{
    TEST_RUN(test_CRC_Empty);
    TEST_RUN(test_CRC_KnownVectors);
    TEST_RUN(test_CRC_Deterministic);
    TEST_RUN(test_CRC_ByteSensitivity);
    TEST_RUN(test_CRC_LengthSensitivity);
}
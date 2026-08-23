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
 * Unit-test runner for the host-side pure-logic tests.
 */

#include "test_framework.h"

/* Single shared definition of the test counters. Declared extern in
 * test_framework.h so all test translation units share one instance. */
int g_test_failures = 0;
int g_test_checks   = 0;

void test_frequencies(void);
void test_dcs(void);
void test_crc(void);
void test_mdc1200(void);
void test_mdc_fuzz(void);

int main(void)
{
    printf("Running unit tests...\n");

    printf("frequencies:\n");
    test_frequencies();

    printf("dcs:\n");
    test_dcs();

    printf("crc:\n");
    test_crc();

    printf("mdc1200:\n");
    test_mdc1200();

    printf("mdc fuzz:\x0a");
    test_mdc_fuzz();

    TEST_SUMMARY();
    return 0;
}
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
 * Minimal dependency-free unit-test framework for host-side testing of
 * pure-logic firmware modules. No external test library required.
 */

#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>
#include <stdlib.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#endif

static int g_test_failures = 0;
static int g_test_checks   = 0;

#define TEST_ASSERT(cond)                                                      \
    do {                                                                       \
        g_test_checks++;                                                       \
        if (!(cond)) {                                                         \
            g_test_failures++;                                                 \
            fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);  \
        }                                                                      \
    } while (0)

#define TEST_ASSERT_EQ_INT(a, b)                                               \
    do {                                                                       \
        long _a = (long)(a);                                                   \
        long _b = (long)(b);                                                   \
        g_test_checks++;                                                       \
        if (_a != _b) {                                                        \
            g_test_failures++;                                                 \
            fprintf(stderr, "  FAIL %s:%d: %s == %s (%ld != %ld)\n",           \
                    __FILE__, __LINE__, #a, #b, _a, _b);                       \
        }                                                                      \
    } while (0)

#define TEST_ASSERT_EQ_U32(a, b)                                               \
    do {                                                                       \
        unsigned long _a = (unsigned long)(a);                                 \
        unsigned long _b = (unsigned long)(b);                                 \
        g_test_checks++;                                                       \
        if (_a != _b) {                                                        \
            g_test_failures++;                                                 \
            fprintf(stderr, "  FAIL %s:%d: %s == %s (%lu != %lu)\n",           \
                    __FILE__, __LINE__, #a, #b, _a, _b);                       \
        }                                                                      \
    } while (0)

#define TEST_RUN(fn)                                                           \
    do {                                                                       \
        int _before = g_test_failures;                                         \
        printf("  %s\n", #fn);                                                 \
        fn();                                                                  \
        if (g_test_failures == _before)                                        \
            printf("    OK\n");                                                \
    } while (0)

#define TEST_SUMMARY()                                                         \
    do {                                                                       \
        printf("\n%d checks, %d failures\n", g_test_checks, g_test_failures);  \
        if (g_test_failures)                                                   \
            exit(1);                                                           \
    } while (0)

#endif
/* Copyright 2023 Dual Tachyon
 * https://github.com/DualTachyon
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

#ifndef VERSION_H
#define VERSION_H

/** @brief Firmware version string. */
extern const char Version[];

/** @brief UART/reporting version string. */
extern const char UART_Version[];

#ifdef ENABLE_FEAT_N7SIX
    /** @brief Edition string. */
    extern const char Edition[];

    /** @brief Build date string. */
    extern const char BuildDate[];

    /** @brief Build time string. */
    extern const char BuildTime[];

    /** @brief Build commit identifier. */
    extern const char BuildCommit[];
#endif

#endif

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
 *
 * Minimal host-side shim for driver/bk4819-regs.h during unit tests.
 */

#ifndef DRIVER_BK4819_REGS_H
#define DRIVER_BK4819_REGS_H

#include <stdint.h>
#include <stdbool.h>

/* Minimal register map shim for host-side compilation. */
#define BK4819_REG_02  (0x02)
#define BK4819_REG_0C  (0x0C)
#define BK4819_REG_2B  (0x2B)
#define BK4819_REG_2F  (0x2F)
#define BK4819_REG_30  (0x30)
#define BK4819_REG_31  (0x31)
#define BK4819_REG_3D  (0x3D)
#define BK4819_REG_3F  (0x3F)
#define BK4819_REG_42  (0x42)
#define BK4819_REG_47  (0x47)
#define BK4819_REG_48  (0x48)
#define BK4819_REG_54  (0x54)
#define BK4819_REG_55  (0x55)
#define BK4819_REG_7D  (0x7D)

#endif
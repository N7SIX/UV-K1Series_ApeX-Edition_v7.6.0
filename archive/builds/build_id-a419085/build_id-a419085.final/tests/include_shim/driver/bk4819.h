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
 * Minimal host-side shim for driver/bk4819.h during unit tests.
 */

#ifndef DRIVER_BK4819_H
#define DRIVER_BK4819_H

#include <stdint.h>
#include <stdbool.h>

/* Minimal BK4819 shim: radio logic under test calls BK4819_* helpers. */

typedef uint16_t BK4819_REGISTER_t;
typedef void *RegisterSpec;
typedef enum {
    BK4819_GPIO_PIN_INVALID = 0,
    BK4819_GPIO5_PIN1_RED = 5,
    BK4819_GPIO6_PIN2_GREEN = 6,
    BK4819_GPIO0_PIN28_RX_ENABLE = 0,
    BK4819_GPIO1_PIN29_PA_ENABLE = 1
} BK4819_GPIO_PIN_t;

typedef enum {
    BK4819_FILTER_BW_WIDE = 0,
    BK4819_FILTER_BW_NARROW,
    BK4819_FILTER_BW_AM,
    BK4819_FILTER_BW_NARROWER
} BK4819_FilterBandwidth_t;

typedef enum {
    BK4819_AF_FM = 0,
    BK4819_AF_BASEBAND2
} BK4819_AF_Type_t;

#define BK4819_GPIO5_PIN1_RED       (5)
#define BK4819_GPIO6_PIN2_GREEN     (6)
#define BK4819_GPIO0_PIN28_RX_ENABLE (0)
#define BK4819_GPIO1_PIN29_PA_ENABLE (1)

/* Register map subset used by radio.c */
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

/* Interrupt mask bits */
#define BK4819_REG_3F_SQUELCH_FOUND     (1u << 0)
#define BK4819_REG_3F_SQUELCH_LOST      (1u << 1)
#define BK4819_REG_3F_CxCSS_TAIL        (1u << 2)
#define BK4819_REG_3F_CTCSS_FOUND       (1u << 3)
#define BK4819_REG_3F_CTCSS_LOST        (1u << 4)
#define BK4819_REG_3F_CDCSS_FOUND       (1u << 5)
#define BK4819_REG_3F_CDCSS_LOST        (1u << 6)
#define BK4819_REG_3F_VOX_FOUND         (1u << 7)
#define BK4819_REG_3F_VOX_LOST          (1u << 8)
#define BK4819_REG_3F_DTMF_5TONE_FOUND  (1u << 9)

static inline void BK4819_WriteRegister(uint16_t reg, uint16_t val) { (void)reg; (void)val; }
static inline uint16_t BK4819_ReadRegister(uint16_t reg) { (void)reg; return 0; }
static inline void BK4819_SetFrequency(uint32_t freq) { (void)freq; }
static inline void BK4819_SetupPowerAmplifier(uint8_t pwr, uint32_t freq) { (void)pwr; (void)freq; }
static inline void BK4819_ToggleGpioOut(uint8_t pin, bool on) { (void)pin; (void)on; }
static inline void BK4819_SetFilterBandwidth(BK4819_FilterBandwidth_t bw, bool am) { (void)bw; (void)am; }
static inline void BK4819_PickRXFilterPathBasedOnFrequency(uint32_t freq) { (void)freq; }
static inline void BK4819_SetRxAudioGain(void) {}
static inline void BK4819_SetupSquelch(uint8_t openRssi, uint8_t closeRssi, uint8_t openNoise, uint8_t closeNoise, uint8_t openGlitch, uint8_t closeGlitch) { (void)openRssi; (void)closeRssi; (void)openNoise; (void)closeNoise; (void)openGlitch; (void)closeGlitch; }
static inline void BK4819_SetCTCSSFrequency(uint16_t hz10) { (void)hz10; }
static inline void BK4819_SetTailDetection(uint16_t hz10) { (void)hz10; }
static inline void BK4819_SetCDCSSCodeWord(uint32_t word) { (void)word; }
static inline void BK4819_EnableVox(uint8_t th1, uint8_t th0) { (void)th1; (void)th0; }
static inline void BK4819_DisableVox(void) {}
static inline void BK4819_SetCompander(uint8_t mode) { (void)mode; }
static inline void BK4819_EnableDTMF(void) {}
static inline void BK4819_DisableScramble(void) {}
static inline void BK4819_EnableScramble(uint8_t mode) { (void)mode; }
static inline void BK4819_EnterBypass(void) {}
static inline void BK4819_EnterRaw(void) {}
static inline void BK4819_ExitBypass(void) {}
static inline void BK4819_ExitSubAu(void) {}
static inline void BK4819_PlayCDCSSTail(void) {}
static inline void BK4819_PlayCTCSSTail(void) {}
static inline void BK4819_PlayRoger(BK4819_FilterBandwidth_t bw) { (void)bw; }
static inline void BK4819_PrepareTransmit(void) {}
static inline void BK4819_SetAF(BK4819_AF_Type_t mod) { (void)mod; }
static inline void BK4819_SetAGC(bool disable) { (void)disable; }
static inline void BK4819_InitAGC(bool listeningAM) { (void)listeningAM; }
static inline void BK4819_SetRegValue(const void *spec, uint16_t val) { (void)spec; (void)val; }
static inline void BK4819_Enable_AfDac_DiscMode_TxDsp(void) {}
static inline bool BK4819_IsGpioOutSet(BK4819_GPIO_PIN_t Pin) { (void)Pin; return false; }

#endif

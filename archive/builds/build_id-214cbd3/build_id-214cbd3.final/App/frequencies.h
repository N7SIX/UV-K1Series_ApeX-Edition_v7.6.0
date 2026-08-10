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

#ifndef FREQUENCIES_H
#define FREQUENCIES_H

#include <stdint.h>

#define _1GHz_in_KHz 100000000
#define DEFAULT_FREQ 43450000 // Use for Reset and Aircopy

typedef struct {
    const uint32_t lower;
    const uint32_t upper;
} freq_band_table_t;

extern const freq_band_table_t BX4819_band1;
extern const freq_band_table_t BX4819_band2;

typedef enum  {
    BAND_NONE = -1,
    BAND1_50MHz = 0,
    BAND2_108MHz,
    BAND3_137MHz,
    BAND4_174MHz,
    BAND5_350MHz,
    BAND6_400MHz,
    BAND7_470MHz,
    BAND_N_ELEM
} FREQUENCY_Band_t;

extern const freq_band_table_t frequencyBandTable[];

// Global frequency limits (derived from frequencyBandTable)
#define F_MIN  frequencyBandTable[0].lower
#define F_MAX  frequencyBandTable[BAND_N_ELEM - 1].upper

typedef enum {
// standard steps
    STEP_2_5kHz,
    STEP_5kHz,
    STEP_6_25kHz,
    STEP_10kHz,
    STEP_12_5kHz,
    STEP_25kHz,
    STEP_8_33kHz,
// custom steps
    STEP_0_01kHz,
    STEP_0_05kHz,
    STEP_0_1kHz,
    STEP_0_25kHz,
    STEP_0_5kHz,
    STEP_1kHz,
    STEP_1_25kHz,
    STEP_9kHz,
    STEP_15kHz,
    STEP_20kHz,
    STEP_30kHz,
    STEP_50kHz,
    STEP_100kHz,
    STEP_125kHz,
    STEP_200kHz,
    STEP_250kHz,
    STEP_500kHz,
    STEP_N_ELEM
} STEP_Setting_t;


extern const uint16_t gStepFrequencyTable[];

#ifdef ENABLE_NOAA
    extern const uint32_t NoaaFrequencyTable[10];
#endif

/** @brief Map a frequency (Hz) to its operating band index. */
FREQUENCY_Band_t FREQUENCY_GetBand(uint32_t Frequency);

/** @brief Interpolate output power between low/mid/high calibration points.
 *  @param TxpLow   Power level at or below LowerLimit.
 *  @param TxpMid   Power level at Middle.
 *  @param TxpHigh  Power level at or above UpperLimit.
 *  @param LowerLimit Frequency at which TxpLow is used.
 *  @param Middle   Frequency at which TxpMid is used.
 *  @param UpperLimit Frequency at which TxpHigh is used.
 *  @param Frequency Target frequency for interpolation.
 *  @return Calculated power level.
 */
uint8_t          FREQUENCY_CalculateOutputPower(uint8_t TxpLow, uint8_t TxpMid, uint8_t TxpHigh, int32_t LowerLimit, int32_t Middle, int32_t UpperLimit, int32_t Frequency);

/** @brief Round a frequency to the nearest step size (Hz). */
uint32_t         FREQUENCY_RoundToStep(uint32_t freq, uint16_t step);

/** @brief Convert a sorted step index to the corresponding STEP_Setting_t enum. */
STEP_Setting_t   FREQUENCY_GetStepIdxFromSortedIdx(uint8_t sortedIdx);

/** @brief Convert a STEP_Setting_t enum to its sorted index. */
uint32_t         FREQUENCY_GetSortedIdxFromStepIdx(uint8_t step);

/** @brief Return 0 if Frequency is allowed for TX, -1 otherwise. */
int32_t          TX_freq_check(uint32_t Frequency);

/** @brief Return 0 if Frequency is allowed for RX, -1 otherwise. */
int32_t          RX_freq_check(uint32_t Frequency);

/** @brief Clamp a frequency to the global band limits [F_MIN, F_MAX]. */
uint32_t         FREQUENCIES_ClampGlobal(uint32_t freq);

/** @brief Clamp a frequency to the limits of a specific band. */
uint32_t         FREQUENCIES_ClampToBand(uint32_t freq, FREQUENCY_Band_t band);

#endif

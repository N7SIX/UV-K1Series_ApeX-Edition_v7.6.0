/**
 * =====================================================================================
 * @file        audio_spectrum.c
 * @brief       Real-time Audio Spectrum Analyzer (FFT) — ApeX Edition
 * @author      N7SIX (2026)
 * @version     v7.6.0 ApeX Edition
 * @license     Apache License, Version 2.0
 * =====================================================================================
 *
 * Implementation notes
 * --------------------
 * FFT algorithm
 *   128-point radix-2 Decimation-In-Time (DIT) FFT with Q15 (signed 16-bit)
 *   fixed-point arithmetic.  No external library is required.
 *   Overflow prevention: each butterfly scales its outputs right by 1 bit,
 *   giving 7 cumulative scale-downs after all stages.  The final magnitudes
 *   are therefore divided by 2^7 = 128 relative to the raw Q15 input range.
 *
 * Hann window
 *   Applied before the FFT to reduce spectral leakage from non-integer-cycle
 *   signals.  Table pre-computed in Q15 (0 … 32767).
 *
 * ADC sampling
 *   Uses the PY32F071 ADC1 peripheral via the LL API already present in the
 *   project.  When audio mode is active the ADC is reconfigured from its
 *   default battery-monitoring single-shot mode to a continuous conversion on
 *   AUDIO_AF_ADC_CHANNEL.  128 samples are collected synchronously (polling)
 *   inside ProcessPending().  At the default ADC clock (12 MHz, 41.5-cycle
 *   sample time) the capture of one full block takes approximately 0.44 ms —
 *   well within the main-loop tick budget.
 *
 * Magnitude approximation
 *   |X[k]| ≈ max(|Re|, |Im|) + (3/8) × min(|Re|, |Im|)
 *   (alpha-max/beta-min, maximum error ≈ 3.96 %).
 *
 * Mapping to pseudo-RSSI
 *   rssiHistory values in the normal RF spectrum path range from ~60 (−130 dBm
 *   noise floor) to ~220 (−50 dBm strong signal) as given by:
 *       rssi = (dBm + 160) × 2
 *   For audio bins the mapping is:
 *       rssi = AUDIO_SPECTRUM_NOISE_FLOOR + (magnitude >> AUDIO_SPECTRUM_MAG_SHIFT)
 *   The MAG_SHIFT default (2) is a starting point; tune per hardware.
 */

#ifdef ENABLE_AUDIO_SPECTRUM

#include "audio_spectrum.h"

#include <stddef.h>
#include <string.h>

/* LL ADC / peripheral headers (already used by board.c) */
#include "py32f071_ll_adc.h"
#include "py32f071_ll_bus.h"
#include "py32f071_ll_rcc.h"
#include "py32f071_ll_gpio.h"

/* ============================================================
 * LOOKUP TABLES
 * ============================================================ */

/*
 * Q15 cosine table  (tw_cos[k] = round(cos(2πk/128) × 32767))
 * Q15 negative-sine table (tw_sin[k] = round(−sin(2πk/128) × 32767))
 * k = 0 … 63
 */
static const int16_t tw_cos[64] = {
      32767,   32728,   32609,   32412,   32137,   31785,   31356,   30852,
      30273,   29621,   28898,   28105,   27245,   26319,   25329,   24279,
      23170,   22005,   20787,   19519,   18204,   16846,   15446,   14010,
      12539,   11039,    9512,    7962,    6393,    4808,    3212,    1608,
          0,   -1608,   -3212,   -4808,   -6393,   -7962,   -9512,  -11039,
     -12539,  -14010,  -15446,  -16846,  -18204,  -19519,  -20787,  -22005,
     -23170,  -24279,  -25329,  -26319,  -27245,  -28105,  -28898,  -29621,
     -30273,  -30852,  -31356,  -31785,  -32137,  -32412,  -32609,  -32728,
};

static const int16_t tw_sin[64] = {
          0,   -1608,   -3212,   -4808,   -6393,   -7962,   -9512,  -11039,
     -12539,  -14010,  -15446,  -16846,  -18204,  -19519,  -20787,  -22005,
     -23170,  -24279,  -25329,  -26319,  -27245,  -28105,  -28898,  -29621,
     -30273,  -30852,  -31356,  -31785,  -32137,  -32412,  -32609,  -32728,
     -32767,  -32728,  -32609,  -32412,  -32137,  -31785,  -31356,  -30852,
     -30273,  -29621,  -28898,  -28105,  -27245,  -26319,  -25329,  -24279,
     -23170,  -22005,  -20787,  -19519,  -18204,  -16846,  -15446,  -14010,
     -12539,  -11039,   -9512,   -7962,   -6393,   -4808,   -3212,   -1608,
};

/*
 * Hann window (Q15)
 * hann_win[n] = round(32767 × 0.5 × (1 − cos(2πn/128))), n = 0 … 127
 */
static const uint16_t hann_win[128] = {
         0,     20,     79,    177,    315,    491,    705,    958,
      1247,   1573,   1935,   2331,   2761,   3224,   3719,   4244,
      4799,   5381,   5990,   6624,   7281,   7961,   8660,   9379,
     10114,  10864,  11628,  12403,  13187,  13980,  14778,  15580,
     16383,  17187,  17989,  18787,  19580,  20364,  21139,  21903,
     22653,  23388,  24107,  24806,  25486,  26143,  26777,  27386,
     27968,  28523,  29048,  29543,  30006,  30436,  30832,  31194,
     31520,  31809,  32062,  32276,  32452,  32590,  32688,  32747,
     32767,  32747,  32688,  32590,  32452,  32276,  32062,  31809,
     31520,  31194,  30832,  30436,  30006,  29543,  29048,  28523,
     27968,  27386,  26777,  26143,  25486,  24806,  24107,  23388,
     22653,  21903,  21139,  20364,  19580,  18787,  17989,  17187,
     16384,  15580,  14778,  13980,  13187,  12403,  11628,  10864,
     10114,   9379,   8660,   7961,   7281,   6624,   5990,   5381,
      4799,   4244,   3719,   3224,   2761,   2331,   1935,   1573,
      1247,    958,    705,    491,    315,    177,     79,     20,
};

/*
 * Bit-reversal permutation for N = 128 (7-bit reversal).
 * bit_rev_128[i] = 7-bit reverse of i.
 */
static const uint8_t bit_rev_128[128] = {
       0,   64,   32,   96,   16,   80,   48,  112,    8,   72,   40,  104,   24,   88,   56,  120,
       4,   68,   36,  100,   20,   84,   52,  116,   12,   76,   44,  108,   28,   92,   60,  124,
       2,   66,   34,   98,   18,   82,   50,  114,   10,   74,   42,  106,   26,   90,   58,  122,
       6,   70,   38,  102,   22,   86,   54,  118,   14,   78,   46,  110,   30,   94,   62,  126,
       1,   65,   33,   97,   17,   81,   49,  113,    9,   73,   41,  105,   25,   89,   57,  121,
       5,   69,   37,  101,   21,   85,   53,  117,   13,   77,   45,  109,   29,   93,   61,  125,
       3,   67,   35,   99,   19,   83,   51,  115,   11,   75,   43,  107,   27,   91,   59,  123,
       7,   71,   39,  103,   23,   87,   55,  119,   15,   79,   47,  111,   31,   95,   63,  127,
};

/* ============================================================
 * MODULE STATE
 * ============================================================ */

/** True while audio spectrum capture is active. */
static bool s_active = false;

/* Complex FFT buffer: re[] and im[] interleaved as Q15 values. */
static int16_t fft_re[AUDIO_SPECTRUM_FFT_SIZE];
static int16_t fft_im[AUDIO_SPECTRUM_FFT_SIZE];

/* ============================================================
 * INTERNAL HELPERS
 * ============================================================ */

/**
 * @brief Perform a single radix-2 DIT butterfly with Q15 arithmetic.
 *
 * Computes:
 *   a' = (a + b*w) >> 1      (scale-down prevents overflow)
 *   b' = (a - b*w) >> 1
 * where w = wr + j*wi (Q15).
 *
 * @param ar  Real part of upper butterfly arm (updated in-place).
 * @param ai  Imaginary part of upper butterfly arm (updated in-place).
 * @param br  Real part of lower butterfly arm (updated in-place).
 * @param bi  Imaginary part of lower butterfly arm (updated in-place).
 * @param wr  Q15 real twiddle factor (cosine).
 * @param wi  Q15 imaginary twiddle factor (−sine).
 */
static void butterfly(int16_t *ar, int16_t *ai,
                      int16_t *br, int16_t *bi,
                      int16_t  wr, int16_t  wi)
{
    /* b × w in Q30, then shift to Q15 */
    int16_t tr = (int16_t)(((int32_t)(*br) * wr - (int32_t)(*bi) * wi) >> 15);
    int16_t ti = (int16_t)(((int32_t)(*br) * wi + (int32_t)(*bi) * wr) >> 15);

    /* Butterfly with 1-bit scale-down to keep Q15 range */
    *br = (int16_t)((*ar - tr) >> 1);
    *bi = (int16_t)((*ai - ti) >> 1);
    *ar = (int16_t)((*ar + tr) >> 1);
    *ai = (int16_t)((*ai + ti) >> 1);
}

/**
 * @brief Alpha-max/beta-min magnitude approximation.
 *
 * Returns approx |re + j*im| = max(|re|,|im|) + (3/8)*min(|re|,|im|).
 * Maximum error ≈ 3.96 % compared to true Euclidean distance.
 * Inputs and output are non-negative 16-bit values.
 */
static uint16_t fast_mag(int16_t re, int16_t im)
{
    int32_t a = (re < 0) ? -(int32_t)re : (int32_t)re;
    int32_t b = (im < 0) ? -(int32_t)im : (int32_t)im;
    if (a < b) { int32_t t = a; a = b; b = t; } /* ensure a = max, b = min */
    int32_t mag = a + ((b * 3) >> 3);            /* max + 0.375*min */
    return (mag > 65535) ? 65535U : (uint16_t)mag;
}

/**
 * @brief Perform a 128-point radix-2 DIT FFT in-place.
 *
 * Inputs/outputs are the module-static fft_re[] and fft_im[] arrays.
 * Bit-reversal permutation is applied first, followed by 7 butterfly stages.
 * Each stage halves amplitude (scale-down) so the output is Q15 / 128.
 */
static void fft128(void)
{
    /* ---- 1. Bit-reversal permutation ---- */
    for (uint8_t i = 0; i < AUDIO_SPECTRUM_FFT_SIZE; i++) {
        uint8_t j = bit_rev_128[i];
        if (j > i) {
            int16_t tr = fft_re[i]; fft_re[i] = fft_re[j]; fft_re[j] = tr;
            int16_t ti = fft_im[i]; fft_im[i] = fft_im[j]; fft_im[j] = ti;
        }
    }

    /* ---- 2. FFT butterfly stages ---- */
    /*
     * Stage s spans groups of m = 2^s samples, each group has m/2
     * butterflies.  Twiddle index for butterfly j within a stage of span m
     * is:  tw_idx = j * (128 / m)   — this strides through the cos/sin
     * tables at the appropriate angular frequency.
     */
    for (uint8_t stage = 1; stage <= 7; stage++) {
        uint8_t m  = (uint8_t)(1u << stage);  /* butterfly span */
        uint8_t m2 = m >> 1;                  /* half-span (number of butterflies per group) */
        uint8_t tw_stride = (uint8_t)(AUDIO_SPECTRUM_FFT_SIZE >> stage); /* 128/m */

        for (uint8_t k = 0; k < AUDIO_SPECTRUM_FFT_SIZE; k += m) {
            for (uint8_t j = 0; j < m2; j++) {
                uint8_t tw = (uint8_t)(j * tw_stride);   /* twiddle table index */
                butterfly(
                    &fft_re[k + j],      &fft_im[k + j],
                    &fft_re[k + j + m2], &fft_im[k + j + m2],
                    tw_cos[tw],           tw_sin[tw]
                );
            }
        }
    }
}

/* ============================================================
 * ADC HELPERS
 * ============================================================ */

/**
 * @brief Configure ADC1 for continuous single-channel audio capture.
 *
 * Called by AUDIO_Spectrum_Start().  Configures the ADC to continuously
 * convert AUDIO_AF_ADC_CHANNEL (default: ADC_IN9 = PB1) so that
 * subsequent calls to LL_ADC_REG_ReadConversionData12() return fresh
 * audio samples.
 *
 * The GPIO pin is set to ANALOG mode.  Other ADC settings (resolution,
 * alignment, calibration) are preserved from BOARD_ADC_Init().
 */
static void adc_configure_audio(void)
{
    /* Ensure ADC clock is on (BOARD_ADC_Init() already does this, but be safe) */
    LL_APB1_GRP2_EnableClock(LL_APB1_GRP2_PERIPH_ADC1);

    /* Disable ADC before reconfiguring */
    LL_ADC_Disable(ADC1);

    /* Switch to continuous conversion on the audio channel */
    LL_ADC_REG_SetContinuousMode(ADC1, LL_ADC_REG_CONV_CONTINUOUS);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_1, AUDIO_AF_ADC_CHANNEL);
    LL_ADC_SetChannelSamplingTime(ADC1, AUDIO_AF_ADC_CHANNEL,
                                  LL_ADC_SAMPLINGTIME_41CYCLES_5);
    LL_ADC_REG_SetTriggerSource(ADC1, LL_ADC_REG_TRIG_SOFTWARE);

    /* Re-enable and start conversions */
    LL_ADC_Enable(ADC1);
    LL_ADC_REG_StartConversionSWStart(ADC1);
}

/**
 * @brief Restore ADC1 to the default battery-monitoring configuration.
 *
 * Called by AUDIO_Spectrum_Stop().  Mirrors the settings applied by
 * BOARD_ADC_Init() so that BOARD_ADC_GetBatteryInfo() works correctly
 * after audio mode ends.
 */
static void adc_restore_battery(void)
{
    LL_ADC_Disable(ADC1);

    LL_ADC_REG_SetContinuousMode(ADC1, LL_ADC_REG_CONV_SINGLE);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_1, LL_ADC_CHANNEL_8);
    LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_8,
                                  LL_ADC_SAMPLINGTIME_41CYCLES_5);
    LL_ADC_REG_SetTriggerSource(ADC1, LL_ADC_REG_TRIG_SOFTWARE);

    LL_ADC_Enable(ADC1);
}

/**
 * @brief Collect AUDIO_SPECTRUM_FFT_SIZE ADC samples via polling.
 *
 * In continuous mode the ADC converts back-to-back.  We spin-wait for
 * each EOS (end-of-sequence) flag, read the result, and clear the flag.
 * At a 12 MHz ADC clock with 41.5-cycle sample time the capture of 128
 * samples takes roughly 0.44 ms — safe to call from the main loop tick.
 *
 * Samples are written into fft_re[] as (raw − 2048) × 8, which centres
 * the 12-bit unsigned ADC value around zero and scales it to the upper
 * Q15 range (≈ ±16384).  fft_im[] is zeroed (real-only input).
 */
static void adc_collect_samples(void)
{
    for (uint8_t n = 0; n < AUDIO_SPECTRUM_FFT_SIZE; n++) {
        /* Wait for end of conversion */
        while (!LL_ADC_IsActiveFlag_EOS(ADC1)) { /* spin */ }
        LL_ADC_ClearFlag_EOS(ADC1);

        uint16_t raw = LL_ADC_REG_ReadConversionData12(ADC1);
        /*
         * Centre 12-bit ADC value (0…4095) around zero:
         *   (4095 - 2048) * 8 =  15976  ≤ INT16_MAX (32767) ✓
         *   (0    - 2048) * 8 = -16384  ≥ INT16_MIN (-32768) ✓
         * No overflow possible for any valid 12-bit ADC output.
         */
        fft_re[n] = (int16_t)(((int32_t)raw - 2048) * 8);
        fft_im[n] = 0;
    }
}

/* ============================================================
 * PUBLIC API
 * ============================================================ */

void AUDIO_Spectrum_Init(void)
{
    s_active = false;
    memset(fft_re, 0, sizeof(fft_re));
    memset(fft_im, 0, sizeof(fft_im));
}

void AUDIO_Spectrum_Start(void)
{
    if (s_active) return;
    adc_configure_audio();
    s_active = true;
}

void AUDIO_Spectrum_Stop(void)
{
    if (!s_active) return;
    adc_restore_battery();
    s_active = false;
}

bool AUDIO_Spectrum_IsActive(void)
{
    return s_active;
}

bool AUDIO_Spectrum_ProcessPending(uint16_t *rssiHistory_out, uint16_t numBins)
{
    if (!s_active || rssiHistory_out == NULL || numBins == 0) {
        return false;
    }

    /* Clamp to available FFT bins */
    if (numBins > AUDIO_SPECTRUM_BIN_COUNT) {
        numBins = AUDIO_SPECTRUM_BIN_COUNT;
    }

    /* ---- 1. Collect ADC samples into fft_re[], zero fft_im[] ---- */
    adc_collect_samples();

    /* ---- 2. Apply Hann window ---- */
    for (uint8_t n = 0; n < AUDIO_SPECTRUM_FFT_SIZE; n++) {
        /*
         * Multiply sample (Q15-ish, range ±16384) by Hann coefficient
         * (Q15, range 0…32767) and divide by 32768 to stay in Q15.
         */
        fft_re[n] = (int16_t)(((int32_t)fft_re[n] * hann_win[n]) >> 15);
    }

    /* ---- 3. Run 128-point FFT ---- */
    fft128();

    /* ---- 4. Compute magnitudes and map to display bins ---- */
    /*
     * FFT output: fft_re[k] + j*fft_im[k], k = 0…127.
     * Only bins 0…63 are independent for a real-valued input (the upper
     * half is the complex conjugate mirror).
     *
     * Map AUDIO_SPECTRUM_BIN_COUNT = 64 FFT bins into numBins display
     * columns using integer linear interpolation.
     */
    for (uint16_t col = 0; col < numBins; col++) {
        /*
         * Select the FFT bin for this display column.
         * Bins are numbered 0…63; bin 0 is DC and is skipped.
         * Mapping: col ∈ [0, numBins-1] → bin ∈ [1, 63].
         *
         * For the worst case (col = numBins-1, numBins = 64):
         *   bin = 1 + (63 × 63) / 64 = 1 + 62 = 63  (integer division)
         * → maximum bin index is always 63, never 64.  No out-of-bounds.
         */
        uint8_t bin = (uint8_t)(1u + (col * (AUDIO_SPECTRUM_BIN_COUNT - 1u))
                                      / numBins);

        uint16_t mag = fast_mag(fft_re[bin], fft_im[bin]);

        /* Scale and add noise floor to produce a pseudo-RSSI value */
        uint32_t rssi = (uint32_t)AUDIO_SPECTRUM_NOISE_FLOOR
                       + (uint32_t)(mag >> AUDIO_SPECTRUM_MAG_SHIFT);

        if (rssi > 65534U) rssi = 65534U; /* keep below sentinel RSSI_MAX_VALUE */

        rssiHistory_out[col] = (uint16_t)rssi;
    }

    return true;
}

#endif /* ENABLE_AUDIO_SPECTRUM */

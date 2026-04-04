/**
 * =====================================================================================
 * @file        audio_spectrum.c
 * @brief       Audio Spectrum Analyzer Implementation
 * @author      N7SIX (2026)
 * @version     v7.6.0 (ApeX Edition)
 * @license     Apache License, Version 2.0
 *
 * IMPLEMENTATION NOTES (AF mode):
 * --------------------------------
 * The audio source is BK4819 REG_0x64 (Voice Amplitude Out), which outputs the
 * demodulated audio envelope at up to 15-bit resolution. In AM mode this IS the
 * audio signal. A single-pole IIR high-pass removes the DC bias; the AC residual
 * is fed into a Goertzel DFT bank.
 *
 * GOERTZEL COEFFICIENT FORMULA:
 *   coeff[k] = 2 * cos(2π*k / FFT_SIZE) expressed in Q14 (× 16384)
 * This must be 2×cos, NOT 1×cos. Using 1×cos places the filter poles at the wrong
 * angles, mapping every "bin k" to a frequency roughly 8× higher than intended
 * and collapsing the displayed range into a narrow 1.3–2.4 kHz band.
 *
 * DC REMOVAL NOTE:
 * The derivative-based preprocessing (diff × 6 + centered) acts as a first-order
 * high-pass differentiator that adds +20 dB/decade tilt across the audio band.
 * At 100 Hz the gain is ~0.5× while at 4 kHz it is ~19×, making the spectrum
 * look like white noise rather than real audio content. Replaced with a simple
 * EMA DC-removal followed by a flat gain.
 * =====================================================================================
 */

#ifdef ENABLE_AUDIO_SPECTRUM

#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "audio_spectrum.h"
#include "driver/bk4819.h"
#include "driver/systick.h"

/* =============================================================================
 * CONFIGURATION AND CONSTANTS
 * =============================================================================*/

#define AUDIO_BUFFER_SIZE        64U    ///< One processing frame
#define FFT_SIZE                 64U    ///< Goertzel input size
#define DISPLAY_BINS             64U    ///< UI bin count
#define GOERTZEL_BINS            32U    ///< Positive-frequency bins (skip DC)
#define AUDIO_BW              4000U     ///< 0-4 kHz nominal AF bandwidth
#define HZ_PER_BIN            (AUDIO_BW / DISPLAY_BINS)

#define SAMPLE_BATCH             8U     ///< New samples per Process() call
#define SAMPLE_INTERVAL_US     125U     ///< ~8 kHz read cadence

#define DEFAULT_SMOOTHING        8U
#define DEFAULT_GAIN_SHIFT      12U    ///< Starting point for AGC; auto-adjusts each frame
#define GAIN_SHIFT_MAX          24U    ///< Upper bound allows large dynamic range headroom
#define PEAK_DECAY               6U

/* =============================================================================
 * RUNTIME STATE
 * =============================================================================*/

static struct {
    AudioSpectrumState_t state;
    SpectrumMode_t mode;
    bool enabled;
    bool paused;

    int16_t audioBuffer[AUDIO_BUFFER_SIZE];
    uint16_t bufferWriteIdx;
    uint16_t sampleCount;
    uint16_t droppedSamples;

    uint8_t magnitude[DISPLAY_BINS];
    uint8_t smoothedMagnitude[DISPLAY_BINS];

    uint8_t peakHold[DISPLAY_BINS];
    uint16_t peakFrequency;
    uint16_t peakMagnitude;
    uint8_t peakDecayCounter;

    uint8_t gainShift;
    uint16_t dcEstimate;
    uint16_t prevRaw;

    AudioSpectrumConfig_t config;
    uint8_t smoothingFactor;

    uint32_t frameCount;
    uint32_t lastFftTime;

} audioSpec = {
    .state = AUDIO_STATE_IDLE,
    .mode = SPECTRUM_MODE_RF,
    .enabled = false,
    .paused = false,
    .bufferWriteIdx = 0,
    .sampleCount = 0,
    .droppedSamples = 0,
    .smoothingFactor = DEFAULT_SMOOTHING,
    .peakDecayCounter = 0,
    .gainShift = DEFAULT_GAIN_SHIFT,
    .dcEstimate = 0,
    .prevRaw = 0,
    .frameCount = 0,
    .lastFftTime = 0,
};

static bool moduleInitialized = false;

/**
 * @brief Corrected Goertzel coefficients: 2*cos(2π*k/FFT_SIZE) in Q14 format.
 *
 * For k=1..GOERTZEL_BINS with FFT_SIZE=64 and 8 kHz sample rate:
 *   bin k=1  → 125 Hz,  bin k=8  → 1000 Hz,
 *   bin k=16 → 2000 Hz, bin k=32 → 4000 Hz
 *
 * The recurrence requires 2*cos, NOT cos. Using cos halves the effective
 * coefficient, shifts the poles away from the desired DFT frequencies, and
 * gives the wrong power estimate — all fixed by the values below.
 */
static const int16_t kGoertzelCoeffQ14[GOERTZEL_BINS + 1] = {
    /*k=0 (DC, unused)*/ 0,
    32610, 32138, 31357, 30274, 28899, 27246, 25330, 23170,
    20788, 18205, 15447, 12540,  9512,  6393,  3212,     0,
    -3212, -6393, -9512,-12540,-15447,-18205,-20788,-23170,
   -25330,-27246,-28899,-30274,-31357,-32138,-32610,-32768
};

/**
 * @brief Hann window lookup table in Q6 format (max=64 ≙ 1.0).
 *
 * w[n] = round(0.5 * (1 − cos(2π·n / (FFT_SIZE−1))) * 64), n = 0..63
 * Tapers to zero at both ends, suppressing spectral leakage from sharp
 * frame edges. Sidelobe attenuation ≈ −31 dB vs. −26 dB for triangular.
 */
static const uint8_t kHannWindowQ6[FFT_SIZE] = {
     0,  0,  1,  1,  3,  4,  6,  7, 10, 12, 15, 17, 20, 23, 26, 30,
    33, 36, 39, 42, 45, 48, 51, 53, 55, 58, 59, 61, 62, 63, 64, 64,
    64, 64, 63, 62, 61, 59, 58, 55, 53, 51, 48, 45, 42, 39, 36, 33,
    30, 26, 23, 20, 17, 15, 12, 10,  7,  6,  4,  3,  1,  1,  0,  0
};

/* =============================================================================
 * INTERNAL HELPERS
 * =============================================================================*/

static int16_t ReadAudioSample(void)
{
    uint16_t raw = BK4819_GetVoiceAmplitudeOut();

    /* Initialise DC estimate from the very first sample so the ring buffer
     * starts in a settled state without a large transient. */
    if (audioSpec.frameCount == 0 && audioSpec.sampleCount == 0) {
        audioSpec.dcEstimate = raw;
        audioSpec.prevRaw    = raw;
    } else {
        /* Single-pole IIR high-pass: α = 1/256 ≈ 0.004  (corner ≈ 5 Hz @8kHz).
         * This removes the DC bias present in REG_0x64 while keeping the full
         * voice band (>20 Hz) intact.
         * The previous derivative-based approach (diff×6 + slow-AC) acted as
         * a first-order differentiator (+20 dB/decade tilt) that suppressed bass
         * and made the spectrum look like spectrally flat noise. */
        int32_t err = (int32_t)raw - (int32_t)audioSpec.dcEstimate;
        audioSpec.dcEstimate = (uint16_t)((int32_t)audioSpec.dcEstimate + (err >> 8));
    }

    audioSpec.prevRaw = raw;

    /* AC-coupled sample with moderate gain so the Goertzel accumulator stays
     * well below int32_t saturation for typical radio audio levels. */
    int32_t sample = ((int32_t)raw - (int32_t)audioSpec.dcEstimate) << 2;  /* ×4 gain */
    if (sample >  32767) sample =  32767;
    if (sample < -32768) sample = -32768;
    return (int16_t)sample;
}

static void BufferAddSample(int16_t sample)
{
    if (audioSpec.sampleCount >= AUDIO_BUFFER_SIZE) {
        audioSpec.droppedSamples++;
        return;
    }

    audioSpec.audioBuffer[audioSpec.bufferWriteIdx++] = sample;
    if (audioSpec.bufferWriteIdx >= AUDIO_BUFFER_SIZE) {
        audioSpec.bufferWriteIdx = 0;
    }
    audioSpec.sampleCount++;
}

static bool BufferExtractFFTFrame(int16_t *fftInput)
{
    if (audioSpec.sampleCount < FFT_SIZE) {
        return false;
    }

    uint16_t readIdx = (audioSpec.bufferWriteIdx + AUDIO_BUFFER_SIZE - FFT_SIZE) % AUDIO_BUFFER_SIZE;
    for (uint16_t i = 0; i < FFT_SIZE; i++) {
        fftInput[i] = audioSpec.audioBuffer[readIdx++];
        if (readIdx >= AUDIO_BUFFER_SIZE) readIdx = 0;
    }

    audioSpec.sampleCount = FFT_SIZE / 2U;
    return true;
}

static void ComputeFFT(void)
{
    int16_t frame[FFT_SIZE];
    if (!BufferExtractFFTFrame(frame)) {
        return;
    }

    uint16_t binMag[GOERTZEL_BINS + 1];
    uint16_t peakMag = 0;

    for (uint16_t k = 1; k <= GOERTZEL_BINS; k++) {
        int16_t coeff = kGoertzelCoeffQ14[k];
        int32_t s1 = 0;
        int32_t s2 = 0;

        for (uint16_t n = 0; n < FFT_SIZE; n++) {
            /* Apply Hann window (Q6, max=64): reduces spectral leakage.
             * The >>6 normalises so the effective input amplitude equals
             * the unwindowed sample at the window peak. */
            int32_t x = ((int32_t)frame[n] * (int32_t)kHannWindowQ6[n]) >> 6;

            /* Goertzel recurrence: s[n] = x[n] + 2cos(ω₀)·s[n-1] - s[n-2]
             * The coefficient is stored as 2cos(ω₀)·16384 (Q14), so a >>14
             * after the multiply gives the exact 2cos(ω₀)·s1 product.
             *
             * CRITICAL: use int64_t for coeff×s1 before the shift to prevent
             * int32_t overflow at typical audio levels (coeff can reach ±32610
             * and s1 can reach ~10^6, giving a product of ~3×10^10 which
             * exceeds INT32_MAX ≈ 2.1×10^9). */
            int32_t s0 = x + (int32_t)(((int64_t)coeff * (int64_t)s1) >> 14) - s2;
            s2 = s1;
            s1 = s0;
        }

        /* Goertzel power: |X(k)|² ∝ s1² + s2² - 2cos(ω₀)·s1·s2
         * With coeff = 2cos(ω₀)·16384 stored in the table, the p3 term
         * (coeff·s1·s2 >> 14) gives exactly 2cos(ω₀)·s1·s2. */
        int64_t p1 = (int64_t)s1 * (int64_t)s1;
        int64_t p2 = (int64_t)s2 * (int64_t)s2;
        int64_t p3 = ((int64_t)coeff * (int64_t)s1 * (int64_t)s2) >> 14;
        int64_t power = p1 + p2 - p3;
        if (power < 0) power = 0;

        uint32_t scaled = (uint32_t)(power >> audioSpec.gainShift);
        uint16_t compressed = 0;
        uint16_t boost = 0;

        while (scaled > 255U && boost < 224U) {
            scaled >>= 1;
            boost += 32U;
        }
        compressed = (uint16_t)scaled + boost;
        if (compressed > 255U) compressed = 255U;

        binMag[k] = compressed;
        if (binMag[k] > peakMag) peakMag = binMag[k];
    }

    /* Adaptive gain: keep the spectral peak in the range 60–220.
     * GAIN_SHIFT_MAX (24) gives headroom for the corrected 2cos coefficients
     * which produce ~4× more power than the previous (erroneous) cos table. */
    if (peakMag > 220 && audioSpec.gainShift < GAIN_SHIFT_MAX) {
        audioSpec.gainShift++;
    } else if (peakMag < 60 && audioSpec.gainShift > 6) {
        audioSpec.gainShift--;
    }

    for (uint16_t i = 0; i < DISPLAY_BINS; i++) {
        uint16_t src = (uint16_t)((i * (GOERTZEL_BINS - 1U)) / (DISPLAY_BINS - 1U)) + 1U;
        audioSpec.magnitude[i] = (uint8_t)binMag[src];
    }

    for (uint16_t i = 0; i < DISPLAY_BINS; i++) {
        uint16_t alpha = audioSpec.smoothingFactor;
        audioSpec.smoothedMagnitude[i] =
            (audioSpec.smoothedMagnitude[i] * (16 - alpha) + audioSpec.magnitude[i] * alpha) / 16;
    }

    audioSpec.peakDecayCounter++;
    if (audioSpec.peakDecayCounter > 4) {
        audioSpec.peakDecayCounter = 0;
        for (uint16_t i = 0; i < DISPLAY_BINS; i++) {
            if (audioSpec.peakHold[i] >= PEAK_DECAY) {
                audioSpec.peakHold[i] -= PEAK_DECAY;
            } else {
                audioSpec.peakHold[i] = 0;
            }
        }
    }

    uint16_t peakIdx = 0;
    uint16_t peakVal = 0;
    for (uint16_t i = 0; i < DISPLAY_BINS; i++) {
        if (audioSpec.smoothedMagnitude[i] > audioSpec.peakHold[i]) {
            audioSpec.peakHold[i] = audioSpec.smoothedMagnitude[i];
        }
        if (audioSpec.peakHold[i] > peakVal) {
            peakVal = audioSpec.peakHold[i];
            peakIdx = i;
        }
    }

    audioSpec.peakFrequency = peakIdx * HZ_PER_BIN;
    audioSpec.peakMagnitude = peakVal;
    audioSpec.state = AUDIO_STATE_READY;
}

/* =============================================================================
 * PUBLIC API IMPLEMENTATION
 * =============================================================================*/

bool AUDIO_SPECTRUM_Init(const AudioSpectrumConfig_t *config)
{
    if (moduleInitialized) {
        return true;
    }

    if (config != NULL) {
        audioSpec.config = *config;
        audioSpec.smoothingFactor = config->smoothingFactor;
    } else {
        audioSpec.config.sampleRate = 0;
        audioSpec.config.fftSize = 0;
        audioSpec.config.enabled = false;
        audioSpec.config.waterfall = true;
        audioSpec.config.smoothingFactor = DEFAULT_SMOOTHING;
        audioSpec.config.peakHoldTime = 4;
        audioSpec.smoothingFactor = DEFAULT_SMOOTHING;
    }

    memset(audioSpec.audioBuffer, 0, sizeof(audioSpec.audioBuffer));
    memset(audioSpec.magnitude, 0, sizeof(audioSpec.magnitude));
    memset(audioSpec.smoothedMagnitude, 0, sizeof(audioSpec.smoothedMagnitude));
    memset(audioSpec.peakHold, 0, sizeof(audioSpec.peakHold));

    audioSpec.state = AUDIO_STATE_IDLE;
    audioSpec.mode = SPECTRUM_MODE_RF;
    audioSpec.bufferWriteIdx = 0;
    audioSpec.sampleCount = 0;
    audioSpec.droppedSamples = 0;
    audioSpec.peakFrequency = 0;
    audioSpec.peakMagnitude = 0;
    audioSpec.peakDecayCounter = 0;
    audioSpec.gainShift = DEFAULT_GAIN_SHIFT;
    audioSpec.dcEstimate = 0;
    audioSpec.prevRaw = 0;
    audioSpec.frameCount = 0;
    audioSpec.lastFftTime = 0;

    moduleInitialized = true;
    return true;
}

void AUDIO_SPECTRUM_Enable(bool enable)
{
    if (!moduleInitialized) return;

    audioSpec.enabled = enable;
    if (enable) {
        memset(audioSpec.audioBuffer, 0, sizeof(audioSpec.audioBuffer));
        memset(audioSpec.magnitude, 0, sizeof(audioSpec.magnitude));
        memset(audioSpec.smoothedMagnitude, 0, sizeof(audioSpec.smoothedMagnitude));
        memset(audioSpec.peakHold, 0, sizeof(audioSpec.peakHold));
        audioSpec.bufferWriteIdx = 0;
        audioSpec.sampleCount = 0;
        audioSpec.peakFrequency = 0;
        audioSpec.peakMagnitude = 0;
        audioSpec.peakDecayCounter = 0;
        audioSpec.gainShift = DEFAULT_GAIN_SHIFT;
        audioSpec.dcEstimate = 0;
        audioSpec.prevRaw = 0;
        audioSpec.state = AUDIO_STATE_CAPTURING;
    } else {
        audioSpec.state = AUDIO_STATE_IDLE;
    }
}

void AUDIO_SPECTRUM_Process(void)
{
    if (!moduleInitialized || !audioSpec.enabled || audioSpec.paused) {
        return;
    }

    for (uint16_t i = 0; i < SAMPLE_BATCH; i++) {
        int16_t sample = ReadAudioSample();
        BufferAddSample(sample);
        SYSTICK_DelayUs(SAMPLE_INTERVAL_US);
    }

    if (audioSpec.sampleCount >= FFT_SIZE) {
        audioSpec.state = AUDIO_STATE_PROCESSING;
        ComputeFFT();
    } else {
        audioSpec.state = AUDIO_STATE_BUFFERING;
    }

    audioSpec.frameCount++;
}

const uint8_t* AUDIO_SPECTRUM_GetMagnitudes(uint16_t *binCount)
{
    if (!moduleInitialized || !audioSpec.enabled || audioSpec.state != AUDIO_STATE_READY) {
        *binCount = 0;
        return NULL;
    }
    *binCount = DISPLAY_BINS;
    return audioSpec.smoothedMagnitude;
}

const uint8_t* AUDIO_SPECTRUM_GetWaterfallRow(uint8_t row)
{
    (void)row;
    return NULL;
}

AudioSpectrumStats_t AUDIO_SPECTRUM_GetStats(void)
{
    AudioSpectrumStats_t stats = {
        .state = audioSpec.state,
        .bufferLevel = audioSpec.sampleCount,
        .peakFrequency = audioSpec.peakFrequency,
        .peakMagnitude = audioSpec.peakMagnitude,
        .averagePower = 0,
        .fftComputeTime = audioSpec.lastFftTime,
        .droppedSamples = audioSpec.droppedSamples,
    };

    uint32_t sum = 0;
    for (uint16_t i = 0; i < DISPLAY_BINS; i++) {
        sum += audioSpec.smoothedMagnitude[i];
    }
    stats.averagePower = sum / DISPLAY_BINS;

    return stats;
}

void AUDIO_SPECTRUM_SetMode(SpectrumMode_t mode)
{
    audioSpec.mode = mode;
    if (mode == SPECTRUM_MODE_AUDIO) {
        AUDIO_SPECTRUM_Enable(true);
    } else {
        AUDIO_SPECTRUM_Enable(false);
    }
}

SpectrumMode_t AUDIO_SPECTRUM_GetMode(void)
{
    return audioSpec.mode;
}

void AUDIO_SPECTRUM_Pause(bool paused)
{
    audioSpec.paused = paused;
}

void AUDIO_SPECTRUM_ResetPeaks(void)
{
    memset(audioSpec.peakHold, 0, sizeof(audioSpec.peakHold));
    audioSpec.peakFrequency = 0;
    audioSpec.peakMagnitude = 0;
    audioSpec.peakDecayCounter = 0;
}

void AUDIO_SPECTRUM_UpdateConfig(const AudioSpectrumConfig_t *config)
{
    if (config != NULL) {
        audioSpec.config = *config;
        audioSpec.smoothingFactor = config->smoothingFactor;
    }
}

uint16_t AUDIO_SPECTRUM_GetMemoryUsage(void)
{
    return (uint16_t)(sizeof(audioSpec.audioBuffer) + sizeof(audioSpec.magnitude) +
                      sizeof(audioSpec.smoothedMagnitude) + sizeof(audioSpec.peakHold));
}

#endif // ENABLE_AUDIO_SPECTRUM

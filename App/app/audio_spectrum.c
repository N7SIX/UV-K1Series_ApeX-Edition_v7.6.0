/**
 * =====================================================================================
 * @file        audio_spectrum.c
 * @brief       Audio Spectrum Analyzer Implementation
 * @author      N7SIX (2026)
 * @version     v7.6.0 (ApeX Edition)
 * @license     Apache License, Version 2.0
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
#define DEFAULT_GAIN_SHIFT      10U
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

/* Goertzel resonance coefficients for a 64-point DFT.
 * 33 entries: index 0 (unused) plus k=1..32.
 * Each entry is: round( 2 * cos(2*pi*k/64) * 16384 )  (Q14 format).
 * Using 2*cos (not just cos) places filter poles exactly on the unit circle at
 * the correct DFT bin frequencies: bin k = k * Fs / N = k * 125 Hz @ 8 kHz.
 * The recurrence s0 = x + ((coeff * s1) >> 14) - s2 then resonates at k*125 Hz.
 * Array size: GOERTZEL_BINS + 1 = 33 (k=0 through k=32).
 */
static const int16_t kGoertzelCoeffQ14[GOERTZEL_BINS + 1] = {
    0,    32610, 32138, 31357, 30274, 28899, 27246, 25330, 23170,
    20788, 18205, 15447, 12540,  9512,  6393,  3212,     0,
    -3212, -6393, -9512,-12540,-15447,-18205,-20788,-23170,
   -25330,-27246,-28899,-30274,-31357,-32138,-32610,-32768
};

/* =============================================================================
 * INTERNAL HELPERS
 * =============================================================================*/

static int16_t ReadAudioSample(void)
{
    uint16_t raw = BK4819_GetVoiceAmplitudeOut();

    if (audioSpec.frameCount == 0 && audioSpec.sampleCount == 0) {
        audioSpec.dcEstimate = raw;
        audioSpec.prevRaw = raw;
    } else {
        int32_t err = (int32_t)raw - (int32_t)audioSpec.dcEstimate;
        audioSpec.dcEstimate = (uint16_t)((int32_t)audioSpec.dcEstimate + (err >> 8));
    }

    // Reg 0x64 behaves as an envelope on BK4819. Derivative + slow high-pass
    // restores motion so sustained speech does not collapse into DC.
    int32_t diff = (int32_t)raw - (int32_t)audioSpec.prevRaw;
    audioSpec.prevRaw = raw;

    int32_t centered = ((int32_t)raw - (int32_t)audioSpec.dcEstimate) >> 1;
    int32_t sample = (diff * 6) + centered;
    if (sample > 32767) sample = 32767;
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
            uint16_t w = (n <= (FFT_SIZE / 2U)) ? n : (FFT_SIZE - 1U - n);
            int32_t x = ((int32_t)frame[n] * (int32_t)w) >> 5;
            // coeff = 2*cos(2πk/N) in Q14; cast to int64_t before multiply to
            // prevent overflow (|coeff| can be up to 32768, |s1| up to ~1M).
            int32_t s0 = x + (int32_t)(((int64_t)coeff * s1) >> 14) - s2;
            s2 = s1;
            s1 = s0;
        }

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

    if (peakMag > 220 && audioSpec.gainShift < 18) {
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

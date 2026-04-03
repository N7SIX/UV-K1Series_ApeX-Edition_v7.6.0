/**
 * =====================================================================================
 * @file        audio_spectrum.h
 * @brief       Audio Spectrum Analyzer - Real-time FFT-based Audio Visualization
 * @author      N7SIX (Audio Spectrum Implementation, 2026)
 * @version     v7.6.0 (ApeX Edition)
 * @license     Apache License, Version 2.0
 * 
 * "Professional-grade audio analysis alongside RF spectrum visualization."
 * =====================================================================================
 *
 * ARCHITECTURAL OVERVIEW:
 * ----------------------
 * This module provides real-time audio frequency analysis using FFT transformation.
 * It captures demodulated audio from the BK4819 radio IC, processes it through
 * ARM CMSIS DSP Fast Fourier Transform, and generates a magnitude spectrum for
 * display alongside or alternating with RF spectrum analysis.
 *
 * KEY FEATURES:
 * - 128-point FFT @ 8kHz sample rate (audio bandwidth up to 4kHz)
 * - Exponential moving average (EMA) filtering for smooth visualization
 * - Circular buffer for seamless audio capture
 * - Waterfall display compatible with existing spectrum rendering pipeline
 * - Frequency bin mapping: 0-4kHz audio range → 64 display bins
 * - Dual-mode operation: Toggle between RF and Audio spectrum displays
 * - Low memory footprint:  ~2KB total (audio buffer + FFT workspace)
 *
 * MEMORY LAYOUT:
 * - Audio ring buffer:    512 bytes (@ 8kHz = 64ms)
 * - FFT input workspace:  256 bytes (128 samples × 2 bytes)
 * - FFT output (bins):    256 bytes (128 magnitude values)
 * - Smoothed trace:       128 bytes
 * - Waterfall history:    512 bytes (64 columns × 8 rows)
 * Total: ~1.6KB
 *
 * ACQUISITION STRATEGY:
 * --------------------
 * 1. BK4819 demodulates RF to baseband audio (FM/AM)
 * 2. Audio routed to internal codec via existing audio path
 * 3. Periodic DMA-less capture into ring buffer (via ADC polling)
 * 4. FFT computed asynchronously during APP_TimeSlice10ms()
 * 5. Display rendering uses same waterfall engine as RF spectrum
 *
 * =====================================================================================
 */

#ifndef AUDIO_SPECTRUM_H
#define AUDIO_SPECTRUM_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Spectrum display mode selector
 */
typedef enum {
    SPECTRUM_MODE_RF = 0,      ///< RF band-scope using BK4819 RSSI
    SPECTRUM_MODE_AUDIO = 1,   ///< Audio frequency analysis using FFT
} SpectrumMode_t;

/**
 * @brief Audio spectrum state machine
 */
typedef enum {
    AUDIO_STATE_IDLE = 0,          ///< Not capturing/processing
    AUDIO_STATE_CAPTURING = 1,     ///< Actively sampling audio
    AUDIO_STATE_BUFFERING = 2,     ///< Buffer fill phase
    AUDIO_STATE_PROCESSING = 3,    ///< FFT computation in progress
    AUDIO_STATE_READY = 4,         ///< Display data ready to render
} AudioSpectrumState_t;

/**
 * @brief Audio spectrum configuration
 */
typedef struct {
    uint8_t sampleRate;            ///< Sampling rate index (0=8kHz, 1=16kHz)
    uint8_t fftSize;               ///< FFT size: 0=128-point (recommended), 1=256-point
    bool    enabled;               ///< Enable/disable audio spectrum feature
    bool    waterfall;             ///< Enable waterfall visualization
    uint8_t smoothingFactor;       ///< EMA smoothing: 0-15 (higher = smoother)
    uint8_t peakHoldTime;          ///< Peak hold decay time in display updates
} AudioSpectrumConfig_t;

/**
 * @brief Audio spectrum runtime statistics
 */
typedef struct {
    AudioSpectrumState_t state;    ///< Current state of audio analyzer
    uint16_t bufferLevel;          ///< Ring buffer fill level (samples)
    uint16_t peakFrequency;        ///< Frequency (Hz) with highest magnitude
    uint16_t peakMagnitude;        ///< Magnitude of peak bin (0-65535)
    uint16_t averagePower;         ///< Average power across all bins
    uint32_t fftComputeTime;       ///< Last FFT compute time (μs)
    uint16_t droppedSamples;       ///< Count of overrun/dropped samples
} AudioSpectrumStats_t;

/**
 * @brief Initialize audio spectrum analyzer
 * Must be called during application startup
 * @param config Pointer to configuration structure (can be NULL for defaults)
 * @return true if initialization successful, false if memory/resource allocation failed
 */
bool AUDIO_SPECTRUM_Init(const AudioSpectrumConfig_t *config);

/**
 * @brief Enable/disable audio spectrum capture
 * @param enable true to start capturing, false to stop
 */
void AUDIO_SPECTRUM_Enable(bool enable);

/**
 * @brief Process audio capture and FFT during scheduler time slice
 * Called from APP_TimeSlice10ms() context
 * Non-blocking; returns immediately if no work queued
 */
void AUDIO_SPECTRUM_Process(void);

/**
 * @brief Return array of magnitude values for display
 * @param binCount Output: number of valid bins in returned array
 * @return Pointer to magnitude array (normalized 0-255 for display scaling)
 * Valid only after state transitions to READY
 */
const uint8_t* AUDIO_SPECTRUM_GetMagnitudes(uint16_t *binCount);

/**
 * @brief Get waterfall history data (for reusing spectrum waterfall pipeline)
 * @param row Waterfall row index (0-15 for 64ms history at typical refresh)
 * @return Pointer to row data (8 columns × 2 nibbles = compressed grayscale)
 */
const uint8_t* AUDIO_SPECTRUM_GetWaterfallRow(uint8_t row);

/**
 * @brief Get runtime statistics
 * @return Audio spectrum statistics structure
 */
AudioSpectrumStats_t AUDIO_SPECTRUM_GetStats(void);

/**
 * @brief Toggle between RF and Audio spectrum display modes
 * @param mode Target spectrum mode
 */
void AUDIO_SPECTRUM_SetMode(SpectrumMode_t mode);

/**
 * @brief Query current spectrum display mode
 * @return Current spectrum mode
 */
SpectrumMode_t AUDIO_SPECTRUM_GetMode(void);

/**
 * @brief Pause/resume audio capture without disabling
 * Used during TX or when user switches away from spectrum mode
 * @param paused true to pause, false to resume
 */
void AUDIO_SPECTRUM_Pause(bool paused);

/**
 * @brief Reset peak hold history and statistics
 */
void AUDIO_SPECTRUM_ResetPeaks(void);

/**
 * @brief Update audio spectrum configuration (non-blocking)
 * Changes take effect at next frame boundary
 * @param config New configuration
 */
void AUDIO_SPECTRUM_UpdateConfig(const AudioSpectrumConfig_t *config);

/**
 * @brief Return amount of RAM used by audio spectrum module (bytes)
 */
uint16_t AUDIO_SPECTRUM_GetMemoryUsage(void);

#endif // AUDIO_SPECTRUM_H

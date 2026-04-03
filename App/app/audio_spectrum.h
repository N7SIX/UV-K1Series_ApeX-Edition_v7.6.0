/**
 * =====================================================================================
 * @file        audio_spectrum.h
 * @brief       Real-time Audio Spectrum Analyzer (FFT) — ApeX Edition
 * @author      N7SIX (2026)
 * @version     v7.6.0 ApeX Edition
 * @license     Apache License, Version 2.0
 * =====================================================================================
 *
 * Feature summary
 * ---------------
 * Adds a real-time Audio Spectrum mode to the existing spectrum/waterfall UI.
 * When activated (F + MENU), the BK4819 is held at the current frequency and
 * 128 ADC samples of the demodulated AF signal are captured per tick.  A
 * 128-point radix-2 fixed-point (Q15) FFT is applied, magnitudes are computed
 * for each of the 64 output bins, and the results are written into the shared
 * rssiHistory[] array so that the existing DrawSpectrumEnhanced / DrawWaterfall
 * rendering pipeline can display them without modification.
 *
 * Guard macro
 * -----------
 * All code in this module is compiled only when ENABLE_AUDIO_SPECTRUM is
 * defined.  Non-enabled builds include this header but see only forward
 * declarations that compile away.
 *
 * Hardware requirement
 * --------------------
 * The BK4819 AF DAC output (after demodulation) must be AC-coupled into a
 * free MCU ADC pin.  The default pin is PB1 (LL_ADC_CHANNEL_9, "ADC_IN9").
 * See AUDIO_AF_ADC_CHANNEL below and Documentation/AUDIO_SPECTRUM_GUIDE.md
 * for full wiring instructions.
 *
 * Tuning macros (override at build time via target_compile_definitions)
 * ----------------------------------------------------------------------
 * AUDIO_AF_ADC_CHANNEL    LL ADC channel constant for the AF input pin.
 *                         Default: LL_ADC_CHANNEL_9 (PB1).
 * AUDIO_SPECTRUM_MAG_SHIFT  Right-shift applied to raw FFT magnitudes before
 *                         writing to rssiHistory.  Larger = smaller bars.
 *                         Default: 2.
 * AUDIO_SPECTRUM_NOISE_FLOOR  Minimum pseudo-RSSI value written for
 *                         near-zero magnitude bins (prevents blank display).
 *                         Default: 60.
 */

#pragma once

#ifdef ENABLE_AUDIO_SPECTRUM

#include <stdbool.h>
#include <stdint.h>

/* ---- Configuration macros ---- */

/** FFT size (number of real-valued input samples). Must be 128. */
#define AUDIO_SPECTRUM_FFT_SIZE     128U

/** Number of output magnitude bins (FFT_SIZE / 2). */
#define AUDIO_SPECTRUM_BIN_COUNT    (AUDIO_SPECTRUM_FFT_SIZE / 2U)

/** Nominal target audio sample rate in Hz (used for documentation / Fs label). */
#define AUDIO_SPECTRUM_SAMPLE_RATE  8000U

/**
 * ADC channel used to sample the BK4819 AF (demodulated audio) output.
 * Default is LL_ADC_CHANNEL_9 (GPIO PB1, ADC_IN9).
 * Override by passing -DAUDIO_AF_ADC_CHANNEL=LL_ADC_CHANNEL_x to the compiler.
 */
#ifndef AUDIO_AF_ADC_CHANNEL
#  include "py32f071_ll_adc.h"
#  define AUDIO_AF_ADC_CHANNEL  LL_ADC_CHANNEL_9
#endif

/**
 * Right-shift applied to 32-bit raw FFT magnitude before writing into the
 * uint16_t rssiHistory element.  Increase to compress tall bars; decrease
 * to amplify weak signals.  Default: 2  (divide magnitude by 4).
 */
#ifndef AUDIO_SPECTRUM_MAG_SHIFT
#  define AUDIO_SPECTRUM_MAG_SHIFT  2U
#endif

/**
 * Minimum pseudo-RSSI value placed in rssiHistory for nearly-silent bins
 * so that the waterfall does not render blank rows.  Should correspond
 * approximately to the noise-floor RSSI for the current band (≈60 for a
 * -130 dBm baseline with the default dbm2rssi() formula).
 */
#ifndef AUDIO_SPECTRUM_NOISE_FLOOR
#  define AUDIO_SPECTRUM_NOISE_FLOOR  60U
#endif

/* ---- Public API ---- */

/**
 * @brief Initialise the audio-spectrum subsystem.
 *
 * Must be called once (idempotent) before Start() / Stop().  Safe to call
 * during APP_RunSpectrum() initialisation.  Does not alter ADC hardware.
 */
void AUDIO_Spectrum_Init(void);

/**
 * @brief Activate audio spectrum mode.
 *
 * Reconfigures the ADC peripheral for continuous single-channel conversion
 * on AUDIO_AF_ADC_CHANNEL.  After this call, repeated calls to
 * AUDIO_Spectrum_ProcessPending() will capture audio and produce spectrum
 * data.  The BK4819 must already be in a stable RX state before calling.
 */
void AUDIO_Spectrum_Start(void);

/**
 * @brief Deactivate audio spectrum mode.
 *
 * Restores the ADC to the standard battery-monitoring configuration used
 * by BOARD_ADC_GetBatteryInfo() and DrawStatus().
 */
void AUDIO_Spectrum_Stop(void);

/**
 * @brief Capture ADC samples, run 128-point Q15 FFT, write magnitudes.
 *
 * Call once per Tick() when audio spectrum is active.  The function:
 *  1. Collects AUDIO_SPECTRUM_FFT_SIZE 12-bit ADC samples via polling.
 *  2. Applies a Hann window (Q15) to reduce spectral leakage.
 *  3. Runs a radix-2 DIT in-place FFT (Q15, 7 stages).
 *  4. Computes per-bin magnitude using an alpha-max/beta-min approximation.
 *  5. Maps bins linearly into rssiHistory_out[0..numBins-1], scaling to
 *     pseudo-RSSI units consistent with the normal RF spectrum path.
 *
 * @param rssiHistory_out  Destination array (caller-owned, e.g. rssiHistory[]).
 *                         Must have room for at least numBins uint16_t values.
 * @param numBins          Number of display columns to populate.  Values
 *                         larger than AUDIO_SPECTRUM_BIN_COUNT are clamped.
 * @return true  if audio mode is active and new data was written.
 * @return false if audio mode is not active (array is not modified).
 */
bool AUDIO_Spectrum_ProcessPending(uint16_t *rssiHistory_out, uint16_t numBins);

/**
 * @brief Query whether audio spectrum mode is currently active.
 * @return true after AUDIO_Spectrum_Start(), false after AUDIO_Spectrum_Stop()
 *         or before the first Start() call.
 */
bool AUDIO_Spectrum_IsActive(void);

#endif /* ENABLE_AUDIO_SPECTRUM */

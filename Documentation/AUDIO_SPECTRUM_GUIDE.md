# Audio Spectrum Analyzer — FFT Mode
## ApeX Edition v7.6.0 — Feature Guide

---

## Overview

The **Audio Spectrum** mode adds a real-time Fast Fourier Transform (FFT)
visualizer to the existing spectrum/waterfall UI.  When activated, the radio
stops sweeping frequencies and instead displays the **audio content** of the
demodulated signal as a spectrum + waterfall — letting you see beat frequencies,
voice pitch, DTMF tones, and other audio-domain information while the BK4819 is
locked on one channel.

| Parameter        | Value                              |
|------------------|------------------------------------|
| FFT size         | 128 points (real-valued input)     |
| Output bins      | 64 (frequency 0 … Fs/2)            |
| Window           | Hann (spectral leakage reduction)  |
| Fixed-point type | Q15 (16-bit signed integer)        |
| Algorithm        | Radix-2 DIT Cooley-Tukey           |
| ADC channel      | Default: ADC_IN9 (PB1) — see note  |
| Actual Fs        | ~289 kHz (ADC-clock limited; see §Frequency Axis) |

---

## Enabling at Build Time

### Method 1 — Use the ApeX preset (recommended)

The `ApeX` CMake preset already enables the feature:

```bash
./compile-with-docker.sh ApeX
```

Or configure manually:

```bash
cmake --preset ApeX -DENABLE_AUDIO_SPECTRUM=ON
cmake --build build/ApeX
```

### Method 2 — Enable for any custom preset

Add to your `CMakePresets.json` overrides or on the command line:

```
-DENABLE_AUDIO_SPECTRUM=ON
```

The feature is **off by default** in all presets except `ApeX`.

### Method 3 — Tune scaling via compile definitions

| Macro                        | Default              | Meaning                                   |
|------------------------------|----------------------|-------------------------------------------|
| `AUDIO_AF_ADC_CHANNEL`       | `LL_ADC_CHANNEL_9`   | LL ADC channel constant for audio input   |
| `AUDIO_SPECTRUM_MAG_SHIFT`   | `2`                  | Right-shift on magnitude (display scale)  |
| `AUDIO_SPECTRUM_NOISE_FLOOR` | `60`                 | Minimum pseudo-RSSI for silent bins       |

Example (double the bar height):

```cmake
target_compile_definitions(App INTERFACE
    AUDIO_AF_ADC_CHANNEL=LL_ADC_CHANNEL_8
    AUDIO_SPECTRUM_MAG_SHIFT=1
)
```

---

## Hardware Wiring

> **Important:** The BK4819 AF output is not connected to an MCU ADC pin on the
> stock UV-K1 PCB.  You must add a wire to use real audio input.

### Recommended wiring (PB1 / ADC_IN9)

```
BK4819 AF DAC output (AFOUT pin / speaker amplifier input)
    │
   [100 nF ceramic cap]   ← AC coupling; blocks DC bias
    │
   [100 kΩ resistor]      ← bias to MCU mid-rail (1.65 V = Vcc/2)
    │                     (connect from MCU 3.3 V through 100 kΩ)
    ├──────────────────── MCU PB1 (ADC_IN9)
    │
   [100 kΩ resistor]      ← lower half of bias divider to GND
    │
   GND
```

The AC coupling capacitor removes the speaker DC offset; the two 100 kΩ
resistors form a 1.65 V bias so the audio waveform sits at the ADC mid-range.

### Alternative: use channel 8 (PB0)

PB0 is already connected for battery monitoring.  **Do not use ADC_IN8 for
audio unless you disconnect the battery voltage divider**, as the two signals
will interfere.

### Fallback — use BK4819 VOX register

If hardware modification is not desired, the BK4819 REG_64 (VOX amplitude)
provides a single unsigned amplitude value.  That approach gives a
single-column level meter rather than a true spectrum.  The full FFT path
requires ADC samples.

---

## Runtime Usage

1. Enter **Spectrum mode** as usual.
2. Navigate to the frequency / signal you wish to analyse.
3. **Press and release F**, then **press MENU** within ~200 ms.
   - The status bar shows `AF` to indicate Audio FFT mode is active.
   - The BK4819 stops frequency-hopping and stays on the current channel.
   - The waterfall/spectrum now shows the audio FFT.
4. Repeat **F + MENU** to deactivate and resume normal RF scanning.

### What you will see

| Display element         | In Audio FFT mode                           |
|-------------------------|---------------------------------------------|
| Spectrum trace          | FFT magnitude per audio-frequency bin       |
| Waterfall               | Time history of audio spectrum (scrolling)  |
| Status bar indicator    | `AF` shown next to `PH` (peak-hold)         |
| Peak frequency label    | Shows the last RF tuning frequency          |
| dBm numbers             | Unchanged (ignore — not calibrated for AF)  |

---

## CPU and Memory Budget

The additional cost of the Audio FFT mode when active:

| Resource     | Impact                                          |
|--------------|-------------------------------------------------|
| Flash        | +~1.5 kB (FFT code + lookup tables)             |
| SRAM         | +512 B (two 128 × int16_t FFT work arrays)      |
| CPU per tick | ~0.44 ms ADC capture + ~0.2 ms FFT = ~0.64 ms  |
| Tick rate    | No change — processing replaces the scan loop  |

At 48 MHz the 128-point FFT takes approximately 200 µs.  The ADC capture of
128 samples at the 12 MHz ADC clock (41.5 cycles / sample) takes ~440 µs.
Total audio-mode tick time ≈ 640 µs vs. the normal ~1 ms scan tick — well
within budget.

---

## Expected Frequency Axis

With the default ADC sampling (41.5-cycle sample time, 12 MHz ADC clock):

```
Effective Fs ≈ 12 MHz / 41.5 ≈ 289 kHz
Bin width     ≈ Fs / 128     ≈ 2.26 kHz per bin
Freq range    ≈ 0 … 144 kHz  (bins 1–63 displayed)
```

For audio at typical voice frequencies (300 Hz – 3.4 kHz), the signals appear
in approximately the first 2–3 bins.  Increase `AUDIO_SPECTRUM_MAG_SHIFT` or
reduce the ADC sampling rate (via a TIM trigger or averaging) to spread the
audio band across more display columns if needed.

To reduce the effective sample rate to ~8 kHz (and expand the voice-frequency
display), configure TIM2 as an ADC trigger — a future enhancement that is not
part of this release.

---

## Troubleshooting

| Symptom                       | Likely cause / fix                                      |
|-------------------------------|---------------------------------------------------------|
| Bars stay flat at noise floor | No audio wired to PB1; check wiring                    |
| Bars saturate all columns     | Lower `AUDIO_SPECTRUM_MAG_SHIFT` (try 4 or 6)          |
| Display blank / no bars       | Increase magnitude (`AUDIO_SPECTRUM_MAG_SHIFT` = 0)    |
| Battery reads wrong after use | ADC not restored; report bug — `AUDIO_Spectrum_Stop()` |
| `AF` indicator not shown      | Verify `ENABLE_AUDIO_SPECTRUM` is defined at build time |
| Build error: no `LL_ADC_*`    | Include `py32f071_ll_adc.h` in your include path        |

---

## Source Files Changed / Added

| File                             | Change                                       |
|----------------------------------|----------------------------------------------|
| `App/app/audio_spectrum.h`       | **New** — public API and configuration macros |
| `App/app/audio_spectrum.c`       | **New** — FFT, ADC, processing implementation |
| `App/app/spectrum.c`             | Modified — F+MENU toggle, Tick() dispatch, status indicator |
| `App/CMakeLists.txt`             | Modified — `enable_feature(ENABLE_AUDIO_SPECTRUM ...)` |
| `CMakePresets.json`              | Modified — `ENABLE_AUDIO_SPECTRUM: true` in ApeX preset |

---

*ApeX Edition — Real-time Audio Spectrum Analyzer*
*© 2026 N7SIX — Apache License 2.0*

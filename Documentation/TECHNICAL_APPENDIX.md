# TECHNICAL APPENDIX - Professional Spectrum Analyzer in Depth

This document provides technical background for users interested in understanding the spectrum analyzer implementation, signal processing pipeline, and advanced configuration.

---

## TABLE OF CONTENTS

1. [Signal Acquisition Pipeline](#signal-acquisition-pipeline)
2. [Spectrum Display Architecture](#spectrum-display-architecture)
3. [Waterfall Rendering](#waterfall-rendering)
4. [Peak Hold Algorithm](#peak-hold-algorithm)
5. [Noise Floor Characteristics](#noise-floor-characteristics)
6. [Advanced Configuration](#advanced-configuration)
7. [Performance Tuning](#performance-tuning)
8. [Troubleshooting Deep Dive](#troubleshooting-deep-dive)

---

## SIGNAL ACQUISITION PIPELINE

### BK4819 RSSI Measurement Chain

```
┌─────────────────────────────────────────────────────────┐
│ 1. RF SIGNAL (136-480 MHz)                              │
│    Antenna → Front-end LNA → Mixer                      │
└────────────────┬────────────────────────────────────────┘
                 │
┌────────────────▼────────────────────────────────────────┐
│ 2. BK4819 IC (Receiver)                                 │
│    - RSSI register (16-bit, updated ~10kHz)             │
│    - AGC control (auto-gain optimization)               │
│    - Demodulator (FM/AM/SSB selection)                  │
└────────────────┬────────────────────────────────────────┘
                 │
┌────────────────▼────────────────────────────────────────┐
│ 3. RSSI-to-dBm Conversion                               │
│    Raw ADC (0-65535) → Logarithmic scaling              │
│    Result: -130 dBm (minimum) to -50 dBm (maximum)      │
│    Sensitivity: ~0.5 µV @ 12.5 kHz BW                  │
└────────────────┬────────────────────────────────────────┘
                 │
┌────────────────▼────────────────────────────────────────┐
│ 4. Measurement Scheduler (10 ms Tick)                   │
│    - Heavy path runs every 60 ms (6-tick throttle)      │
│    - Updates rssiHistory[128] with new measurements     │
│    - Applies noise floor smoothing (EMA filter)         │
└────────────────┬────────────────────────────────────────┘
                 │
┌────────────────▼────────────────────────────────────────┐
│ 5. Display Rendering (60 Hz Refresh)                    │
│    - Spectrum trace (3-point smoother)                  │
│    - Waterfall scrolling (new data → top row)           │
│    - Peak hold fade (exponential decay per frame)       │
│    - S-meter update (dBm → IARU standard scale)         │
└────────────────┬────────────────────────────────────────┘
                 │
┌────────────────▼────────────────────────────────────────┐
│ 6. ST7565 Display Output                                │
│    - 128 × 64 pixel framebuffer (8 pages)               │
│    - 16-shade grayscale via Bayer dithering             │
│    - ~60 Hz refresh (screen persistence)                │
└─────────────────────────────────────────────────────────┘
```

### RSSI Conversion Mechanics

The BK4819 provides raw 16-bit RSSI values. The firmware applies:

1. **Hardware AGC**: Automatic gain control optimizes receiver level
2. **Linear scaling**: Raw ADC (0-65535) → 0-100 units
3. **dBm conversion**: Calibrated lookup table
4. **Non-linear boost**: Exponential function emphasizes weak signals
5. **Noise gate threshold**: Suppress below -130 dBm (thermal noise)

**Mathematical model** (pseudocode):

```
RSSI_raw = BK4819_Measure()  // 0-65535
RSSI_linear = (RSSI_raw * 100) / 65536
dBm = -130 + (RSSI_linear * 100) / 100  // "compressed" to -130 to -50 dBm range
dBm_boosted = apply_exponential_curve(dBm)  // Emphasize weak signals
pixel_height = scale_to_display(dBm_boosted)  // 0-40 pixels
```

---

## SPECTRUM DISPLAY ARCHITECTURE

### Frequency Binning Strategy

**128 display bins** cover the scanned frequency range:

```
Step Size | Total Span | Resolution | Use Case
──────────┼───────────┼────────────┼──────────────────
2.5 kHz   | 320 kHz   | Fine detail | Narrow-band analysis
6.25 kHz  | 800 kHz   | Precise     | Single-band survey
12.5 kHz  | 1.6 MHz   | Standard    | Full band overview (default)
25 kHz    | 3.2 MHz   | Coarse      | Quick band sweep
50 kHz    | 6.4 MHz   | Very coarse | Multi-band survey
100 kHz   | 12.8 MHz  | Minimal     | Fastest refresh
Custom    | Variable  | User-defined| Purpose-specific
```

**Frequency-to-pixel mapping**:

```c
display_bin_index = (frequency - scan_start_freq) / step_size

// When sweeping 146.0 - 148.0 MHz @ 12.5 kHz steps:
// Total bins needed = (148.0 - 146.0) / 0.0125 = 160 bins
// Displayed = 128 bins (decimated: every 1.25 nth bin)
```

### Spectrum Trace Rendering Pipeline

```
Input: rssiHistory[128]  (current RSSI per bin, 0-100 scale)
       ↓
[Step 1] Non-linear scaling (exponential boost for weak signals)
       ↓
[Step 2] 3-point moving average smoothing (reduce noise)
       ↓
[Step 3] Anti-aliasing (sub-pixel interpolation)
       ↓
[Step 4] Pixel-to-Y-coordinate conversion (monochrome display)
       ↓
[Step 5] Bresenham line drawing (connect points smoothly)
       ↓
Output: Blue trace visible on display
```

**3-Point Smoother** improves visual quality without losing frequency resolution:

```
smoothed[i] = (rssiHistory[i-1] + 2*rssiHistory[i] + rssiHistory[i+1]) / 4
```

This reduces noise grass while maintaining peak accuracy.

### Color Depth & Dithering

Despite monochrome hardware limitation, the display uses **16-level grayscale**:

**Classic 4×4 Bayer pattern** (professional standard):

```
Bayer[4][4] = {
  {0,  8,  2, 10},
  {12, 4, 14,  6},
  {3, 11,  1,  9},
  {15, 7, 13,  5}
}
```

**Application**: Each pixel coordinates (x,y) dithers intensity based on:
- Grayscale level (0-15) to display
- Bayer matrix value at position (x%4, y%4)
- Threshold comparison determines black or white pixel

**Result**: 128×64 = 8,192 pixels appear as 16 distinct shades through spatial dithering.

---

## WATERFALL RENDERING

### Waterfall Buffer Layout

**16 rows × 128 columns = 2,048 bytes** (when packed as nibbles)

```
Row 0:  [████████████████] ← Latest measurements (top of display)
Row 1:  [████████████████]    ↑ Age increases downward
Row 2:  [████████████████]    │
...
Row 15: [████████████████] ← Oldest measurements (bottom of display)
```

### Frame-by-Frame Scrolling Algorithm

**Circular buffer approach** minimizes CPU overhead:

1. **No memory copying** (circular index avoids data movement)
2. **Per-tick update**: New measurements enter at logical "top"
3. **Display rendering**: Reads rows in reverse order starting from `waterfallIndex`
4. **Seamless wraparound**: Index (0-15) masks automatically via modulo

**Pseudocode**:

```c
// On every spectrum measurement:
waterfallIndex = (waterfallIndex + 1) % 16;  // Advance to next row
memcpy(waterfallHistory[waterfallIndex], rssiHistory, 128);  // New data

// During rendering:
for (row = 0; row < 16; row++) {
    display_row = (waterfallIndex - row + 16) % 16;  // Read reverse order
    render_with_bayer_dithering(waterfallHistory[display_row]);
}
```

**Visual result**: New data appears at top, older data scrolls downward, seamless wraparound.

### Dynamic Depth Fading

Recent signals appear bright; older signals fade darkly. Achieved through **grayscale level reduction**:

```
Age (rows):   0   1   2   3   4   5   6   7   8   9  10+ 
Grayscale:   15  15  14  13  12  11  10   9   7   5   3
(white)                                          (black)
```

Provides visual sense of time without explicit row timestamps.

---

## PEAK HOLD ALGORITHM

### Peak Memory Buffer

**128 entries** (one per frequency bin):

```
peakHold[i] = maximum RSSI value observed at bin i
```

### Exponential Decay Per Frame

Rather than linear fade (−1 unit per frame), exponential decay:

```
peakHold[i] = (peakHold[i] * 7 / 8) + (current_rssi[i] * 1 / 8)

// Interpretation:
// "Keep 87.5% of old peak, blend in 12.5% of current RSSI"
// Result: Natural gradual fade over ~30 seconds
```

**Time constant** (approximate):

```
e^(-t/τ) = 0.5  ⟹  τ ≈ 2.1 seconds for 50% decay
                     ~30 seconds to nearly zero

At 60 Hz display refresh (16.7 ms per frame):
30 seconds = 1800 frames
Frame-to-frame decay factor = exp(-16.7ms / 2.1s) ≈ 0.992 ≈ 7/8
```

### Visual Presentation

**Dashed line** on spectrum display:

```
┌────────────────────────────────────────────┐
│  Solid blue line = current signal           │
│  ─ ─ ─ ─ ─ line = peak hold (fading)       │
│                                            │
│  Peak peaks out, then slowly fades down    │
└────────────────────────────────────────────┘
```

---

## NOISE FLOOR CHARACTERISTICS

### Sources of Noise Observed

1. **Thermal noise**: Inherent detector noise (~-130 dBm baseline)
2. **Ambient RF**: Background radiation from local transmitters
3. **Quantization noise**: ADC resolution (16-bit = 0.1% quantization)
4. **Modulation sidebands**: Nearby transmissions spreading into viewed frequency
5. **Crystal oscillator phase noise**: VCO jitter (minimal, &lt;100 ppm)

### Noise Floor Behavior

**In quiet RF environment** (rural/shielded):
- Baseline: −125 to −130 dBm
- Variation: ±2-3 dB due to thermal fluctuations
- "Grass" pattern: Random ADC least-significant bits

**In active RF environment** (urban/congested):
- Baseline: −100 to −110 dBm
- Variation: ±5-10 dB (multipath fading, interference)
- Higher activity in lower frequencies (broadcast bleed-through)

### Interpreting Noise Floor Changes

| Observation | Likely Cause |
|-------------|--------------|
| Noise floor rises 10 dB | Nearby transmitter or RF source |
| Noise floor suddenly drops | Transmitter shut off or moved away |
| Noise floor drifts slowly upward | Temperature change (oscillator drift) |
| Noise floor unstable (flickering ±5 dB) | Atmospheric fading, multipath |

---

## ADVANCED CONFIGURATION

### Spectrum Display Scaling

**Menu → 22 DBMax** (display ceiling):

```
DBMax = -50 dBm  → "Zoomed in"   (weak signals easier to see)
DBMax = -80 dBm  → "Normal"      (good balance)
DBMax = -100 dBm → "Zoomed out"  (strong signals don't clip)
```

**Menu → 23 DBMin** (display floor):

```
DBMin = -130 dBm → Most sensitive (includes all thermal noise)
DBMin = -120 dBm → Typical setting
DBMin = -100 dBm → Least sensitive (cleaner display)
```

### Scan Range Definition

Define band boundaries to focus analysis:

```
Menu → Scan Range:
Start Freq: 146.000 MHz
Stop Freq:  148.000 MHz
Step: 12.5 kHz
Result: 160 frequency points, decimated to 128 display bins
```

### RSSI Trigger Level (Squelch)

Controls when radio locks onto signal during scan:

```
Menu → 06 Squelch: 0-9
0 = Always listening (squelch open)
5 = Moderate detection (typical repeater searching)
9 = High threshold (only strong signals)

Typical settings by use:
- Scanning (blind search):   3-4  (loose, catch all)
- Repeater hunting:         5-6  (moderate)
- Signal monitoring:        1-2  (sensitive, catch weak)
- Noise rejection:          7-8  (high, ignore interference)
```

---

## PERFORMANCE TUNING

### CPU Load Management

The spectrum analyzer runs a **heavy-path processing loop every 60 ms** (6-tick throttle) to conserve CPU:

```
Task          | Frequency    | CPU Load
──────────────┼──────────────┼─────────
Display refresh (UI)  | 60 Hz (16.7 ms) | ~15%
Heavy path (RSSI meas)| 16.7 Hz (60 ms) | ~5%
Waterfall update      | 60 Hz    | ~3%
Peak hold decay       | 60 Hz    | ~2%
───────────────────────────────────────
Total (spectrum mode) | — | ~25%
```

**Impact on user experience**:
- Spectrum trace updates every 3 frames (~50 ms), appears smooth
- Waterfall scrolls smoothly (updated every frame)
- Peak hold fades continuously
- Occasional "gaps" between updates (imperceptible to human eye)

### Memory Footprint

```
Component                 | Size      | Notes
──────────────────────────┼───────────┼──────────────────
rssiHistory[128]          | 256 bytes | Current measurements
waterfallHistory[16][128] | 2,048 bytes (packed nibbles)
peakHold[128]             | 256 bytes | Peak trace history
Display sector buffer     | 1,024 bytes | ST7565 page cache
───────────────────────────────────────────────────────
Total (spectrum mode)     | ~3.5 kB   | Of 32 kB total SRAM
```

### Optimization Strategies

**To improve spectrum update rate**:
1. Reduce `scan_step_size` (finer bins = faster computation)
2. Reduce waterfall rows (currently 16, could be 8)
3. Disable peak hold rendering if not needed
4. Increase heavy-path frequency (from 6 ticks → 3 ticks) [impacts battery]

**To reduce CPU load**:
1. Simplify 3-point smoother (use 2-point instead)
2. Skip Bayer dithering in waterfall (use simple threshold)
3. Disable display refresh during standby
4. Lock frequency (stop scanning) when not needed

---

## TROUBLESHOOTING DEEP DIVE

### Problem: Spectrum Trace Appears Flat

**Symptoms**: All frequencies show same height, no variation

**Diagnosis Steps**:
1. Check noise floor: Menu → DBMin/DBMax
   - If DBMin = -100 dBm and actual noise = -120 dBm → signals below display threshold
   - **Fix**: Set DBMin to -130 dBm
2. Check squelch: Menu → 06 Squelch
   - If squelch > 5 and signals weak → squelch cutoff hiding data
   - **Fix**: Reduce to 2-3
3. Check 3-point smoother:
   - Strong signals should show as peaks, not flat
   - If completely flat → measurement not updating
   - **Verify**: Scan in progress ([* SCAN] active?)
4. Measure RSSI directly:
   - Go to Menu → SysInf (system information)
   - Watch RSSI value change as frequency tunes
   - If RSSI frozen → measurement pipeline failure

### Problem: Waterfall Not Scrolling Smoothly

**Symptoms**: Waterfall appears jerky, rows jump, pattern distorted

**Diagnosis**:
1. **Scan paused?** Press [* SCAN] to resume
2. **Listen mode active?** Press [EXIT] to exit signal lock
3. **CPU overload?** Other tasks competing (TX activity?)
   - **Check**: Spectrum updates take ~25% CPU; if other tasks heavy, waterfall throttles
   - **Fix**: Reduce background tasks, increase refresh rate
4. **Circular buffer wraparound issue?** (Rare)
   - **Indicator**: Pattern shifts abruptly every 16 frames
   - **Root cause**: Index math overflow (firmware bug)
   - **Workaround**: Restart spectrum analyzer

### Problem: Peak Hold Disappears Instantly

**Symptoms**: Peak hold line present, then vanishes when signal ends

**Diagnosis**:
1. **Exponential decay too fast?** Check formula:
   ```
   peakHold[i] = (peakHold[i] * 7 / 8) + (rssi[i] * 1 / 8)
   ```
   If rssi[i] = 0 (no signal), then:
   ```
   peakHold decays: 0 → 0 (stays zero!)
   ```
   **This is the observed behavior.** Once signal ends, peak immediately goes to zero.

2. **Expected behavior**: Gradual fade with signal present
   - Keep scanning to maintain carrier signal
   - Peak only fades when signal actively decreasing
   
3. **Workaround**: Enable "hold" mode in spectral analysis
   - Keep last measurement in buffer
   - Slow down decay to minutes (not implemented in standard firmware)

### Problem: S-Meter Doesn't match Spectrum Peak

**Symptoms**: S-meter shows S9, but spectrum trace shows weak signal (or vice versa)

**Possible Causes**:
1. **Different RSSI sources**: S-meter might read current RX RSSI, not spectrral measurement
   - During RX: S-meter = demodulated signal strength
   - During spectrum: S-meter = measurement at scanned frequency
   - Normal if tuned to different frequency
2. **Time lag**: Spectrum updates every 60 ms, S-meter updates every 500 ms
   - Frequency might have changed between updates
   - **Verify**: Stop scanning and watch both meters stabilize
3. **AGC behavior**: Receiver gain changes for different RX modes
   - AM mode: Higher sensitivity than FM
   - SSB mode: Different gain vs FM
   - **Expected**: S-meter values vary by modulation

**Resolution**: Tune to stable test signal and verify both meters reading same level.

### Problem: Spectrum Grass Slows/Dies After 2-3 Seconds

**Symptoms**: Initial animated noise pattern → gradual slowdown → appears frozen

**Root Cause Analysis**:

This is **not a bug**, but a consequence of the **exponential moving average (EMA) noise filter**:

```c
noisePersistence[i] = (noisePersistence[i] * 7 + (baseFloor + roll)) >> 3;
// where roll ∈ [-4, +4] (zero-mean random variation)
```

**Mathematical behavior**:
1. Initial state: noisePersistence = 0
2. Frame 1: `0 * 7/8 + 4 = 0.5` → slight variation
3. Frame 2: `0.5 * 7/8 + 3.2 = 3.6` → more variation
4. ...exponential growth...
5. Frame ~12-18: `noisePersistence → baseFloor` (converges)
6. Once converged: Adding zero-mean roll does nothing → **flat**

**Why it happens**: EMA filter is designed to **smooth noise**, not generate perpetual variation. With input having zero mean, the state converges to the input mean (baseFloor).

**Workaround Options**:

**Option A** (No fix needed): Accept as normal behavior
- Spectrum is performing correctly: showing noise floor accurately
- Grass slowdown proves filter working (reducing noise jitter)
- Expected in any spectral analyzer

**Option B** (Continuous random generation):
- Replace EMA with fresh random each cycle
- Trades smoothness for "liveness"
- Requires firmware modification

**Option C** (Rely on real RSSI variations):
- Remove synthetic noise entirely
- Use actual RF environment variations
- Requires clean RF environment (quiet bands show no grass)

**Design decision**: Standard firmware uses Option A (EMA smoothing).

---

## ADVANCED MEASUREMENT TECHNIQUES

### Finding Intermittent Transmissions

1. **Enable peak hold** (should already be on)
2. **Set DBMin to -130 dBm** (maximum sensitivity)
3. **Set squelch to 2** (loose trigger)
4. **Scan slowly over suspect band** (use large step size for overview)
5. **Watch waterfall** for vertical "flashes" (indicate TX bursts)
6. **Look at peak hold line** for previous TX locations even after transmission stops

### Measuring Signal Bandwidth

**Spectrum trace width indicates modulation bandwidth**:

```
Width (dB down from peak):
6 dB        3 dB
 │           │
 ▼           ▼
  ╱╲        ╱╲
 ╱  ╲      ╱  ╲
─────────────────  ← Current trace
3 dB width indicates occupied bandwidth
```

**Typical measurements**:
- FM (12.5 kHz): ~10 kHz width
- FM (25 kHz): ~20 kHz width
- AM: ~10 kHz width (half of FM equivalent)
- SSB/USB: ~3-4 kHz width

### Identifying Modulation Type

**Visual spectrum shape**:
```
FM:     [smooth hump shape, symmetric]
AM:     [sharp double lobes, side-bands visible]
SSB:    [single sharp lobe, asymmetric]
CW:     [very narrow spike]
Digital: [multiple peaks, complex pattern]
```

**Use noise floor to classify**:
- FM (good): Noise floor relatively quiet, peaks clear
- AM (complex): Noise floor modulates with signal (AM property)
- SSB (sharp): Noise appears on one side only

---

## CALIBRATION & ACCURACY

### Frequency Calibration

BK4819 includes crystal oscillator with ~±20 ppm tolerance:

```
At 450 MHz:  ±20 ppm = ±9 kHz error
At 150 MHz:  ±20 ppm = ±3 kHz error
```

**Verification method**:
1. Tune to known frequency (broadcast station, repeater)
2. Compare displayed frequency to known value
3. Note error (e.g., "-2.5 kHz" at 146.5 MHz)
4. Apply correction factor in menu if available

### RSSI Absolute Accuracy

BK4819 RSSI accuracy: **± 3 dB typical** (professional standard)

This means:
- True signal: -100 dBm
- Measured: Could be -97 to -103 dBm
- **Don't trust single reading for absolute calibration**
- **Use trends** (rise/fall) rather than absolute values

### Relative Comparison

Frequency-to-frequency comparison is **more accurate** than absolute:
- Signal A appears 6 dB stronger than Signal B
- True relative difference likely ~6 ± 1 dB
- Useful for comparative analysis, interference detection

---

## REFERENCES

- **BK4819 Datasheet**: Receiver IC used (available from manufacturer)
-  **PY32F071 Reference Manual**: MCU specifications and timing
- **IARU Region 1 Rec R.1**: S-meter standard definition
- **Bayer Matrix**: Professional dithering standard
- **Exponential Moving Average (EMA)**: Signal processing textbooks

---

**Document Version**: 1.0  
**For Firmware**: 7.6.0+  
**Last Updated**: February 2026

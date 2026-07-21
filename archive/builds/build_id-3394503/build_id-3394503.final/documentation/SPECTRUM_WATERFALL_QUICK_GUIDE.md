# Spectrum + Waterfall Quick Guide

> **Firmware:** UV-K1Series ApeX-Edition v7.6.8  
> **Radio:** Quansheng UV-K5/K1 Series (PY32F071 + BK4819)  
> **Display:** ST7565 128×64 LCD  
> **Last Updated:** 2026-06-26

---

## 1. Entering Spectrum Mode

**From main screen:** Navigate to the menu → select **Spectrum** (or use the shortcut if configured).

On entry, the radio:
1. Saves the current BK4819 radio registers (so it can restore them on exit)
2. Centers the scan frequency on your active VFO frequency
3. Loads saved spectrum settings from SPI flash (step size, steps count, bandwidth, sensitivity profile)
4. Initializes the waterfall buffer
5. Begins **sweeping** across the frequency range, measuring RSSI at each step

Press **EXIT** to leave spectrum mode — settings are saved and the radio returns to normal operation.

---

## 2. Display Layout

```
┌──────────────────────────────────────────────────────────────────────┐
│ M -87/-92    [BATTERY ICON]   ← Status line (top 8 pixels)          │
├──────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  64x    25.00k                                                       │
│                                                                      │
│           440.00000    F SKIP                                         │
│  128.00000 ←─▽──→ 440.00000                                          │
│  ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░  │
│  ░░ ░ ░░░░░░░░░ ░ ░░░  ░░ ░ ░░░  ░░ ░  ░  ░░░ ░ ░░ ░  ░░░  ░  ░░  │
│  ░░ ░ ░░░░░ ░ ░░  ░░░  ░░ ░ ░░░  ░░ ░  ░  ░░░ ░ ░░ ░  ░░░  ░  ░░  │
│  ░░ ░ ░░░░░ ░ ░░  ░░░  ░░ ░ ░░░  ░░ ░  ░  ░░░ ░ ░░ ░  ░░░  ░  ░░  │
│  ░░ ░ ░░░░░ ░ ░░  ░░░  ░░ ░ ░░░  ░░ ░  ░  ░░░ ░ ░░ ░  ░░░  ░  ░░  │
│  ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░  │
│  ░░ ░ ░░░░░ ░ ░░  ░░░  ░░ ░ ░░░  ░░ ░  ░  ░░░ ░ ░░ ░  ░░░  ░  ░░  │
│  ░░ ░ ░░░░░ ░ ░░  ░░░  ░░ ░ ░░░  ░░ ░  ░  ░░░ ░ ░░ ░  ░░░  ░  ░░  │
│  ░░ ░ ░░░░░ ░ ░░  ░░░  ░░ ░ ░░░  ░░ ░  ░  ░░░ ░ ░░ ░  ░░░  ░  ░░  │
│  ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░  │
│  ░░ ░ ░░░░░ ░ ░░  ░░░  ░░ ░ ░░░  ░░ ░  ░  ░░░ ░ ░░ ░  ░░░  ░  ░░  │
│  ░░ ░ ░░░░░ ░ ░░  ░░░  ░░ ░ ░░░  ░░ ░  ░  ░░░ ░ ░░ ░  ░░░  ░  ░░  │
│  ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░  │
│  ░░ ░ ░░░░░ ░ ░░  ░░░  ░░ ░ ░░░  ░░ ░  ░  ░░░ ░ ░░ ░  ░░░  ░  ░░  │
│  ░░ ░ ░░░░░ ░ ░░  ░░░  ░░ ░ ░░░  ░░ ░  ░  ░░░ ░ ░░ ░  ░░░  ░  ░░  │
└──────────────────────────────────────────────────────────────────────┘
```

### Screen Regions

| Region | Lines | Content |
|--------|-------|---------|
| Status line | 0-7 | Current dBm / trigger level (or auto mode indicator) + battery icon + channel name (when listening) |
| Top info | Line 1 | Steps count (e.g. "64x") + scan step (e.g. "25.00k") |
| Spectrum curve | Lines 8-31 | Live signal crest (solid) + peak hold trace (dotted) + trigger level line (dashed horizontal) + frequency labels at bottom |
| Waterfall | Lines 48-63 | 16-row grayscale history of spectrum sweeps (newest at top, scrolls downward) |

---

## 3. Button Reference — Complete Key Map

### 3.1 Quick Reference Card

```
┌────────────┬───────────────────────────────────────────────┐
│   BUTTON   │        FUNCTION (short press)                 │
├────────────┼───────────────────────────────────────────────┤
│  ▲ (UP)    │ Move scan range UP by frequency change step  │
│  ▼ (DOWN)  │ Move scan range DOWN by frequency change step│
│  ◀ (1)     │ Decrease scan step (finer resolution)        │
│  ▶ (7)     │ Increase scan step (coarser resolution)      │
│  2         │ Decrease frequency change step               │
│  8         │ Increase frequency change step               │
│  3         │ [Manual] Decrease dB max / [Auto] Sensitivity│
│  9         │ [Manual] Increase dB max / [Auto] Sensitivity│
│  * (STAR)  │ Decrease RSSI trigger level                  │
│  F         │ Increase RSSI trigger level                  │
│  0         │ Cycle modulation (FM→AM→USB→...)¹           │
│  4         │ Cycle steps count (128→64→32→16)            │
│  5         │ Enter frequency input mode                   │
│  6         │ Cycle listening bandwidth (25k→12.5k→6.25k)  │
│  SIDE1     │ Blacklist current peak signal                │
│  SIDE2     │ Toggle backlight                             │
│  PTT       │ Tune to peak → enter STILL mode              │
│  MENU      │ [Short] Toggle manual/auto trigger           │
│  MENU      │ [Long hold] Reset spectrum to factory defaults│
│  EXIT      │ Exit spectrum mode                           │
└────────────┴───────────────────────────────────────────────┘
¹ Modulation types: FM (0), AM (1), USB (2). With ENABLE_BYP_RAW_DEMODULATORS: BYP (3), RAW (4).
   "NFM" is a filter bandwidth setting (12.5 kHz) toggled via key 6, not a separate modulation type.
```

### 3.2 Detailed Explanations

---

#### ▲ UP / ▼ DOWN — Move Frequency Range

**What it does:**
Moves the entire spectrum display window up (higher frequency) or down (lower frequency) in steps of `frequencyChangeStep`.

**How it works:**
- Increments or decrements `currentFreq` by the value of `settings.frequencyChangeStep`
- If the frequency is outside the radio range (F_MIN = 5 MHz, F_MAX = 600 MHz), movement is blocked
- Immediately restarts the sweep (`RelaunchScan()`) at the new center frequency
- Resets the blacklist so previously hidden peaks reappear

**If a signal is currently being received** (green LED on):
- UP/DOWN forces the receiver to close and restarts the sweep in the pressed direction
- This lets you "skip" out of listening mode and continue scanning

**Scan Range Mode:** UP/DOWN are disabled when operating in scan-range mode (gScanRangeStart set).

---

#### 1 ◀ / 7 ▶ — Scan Step (Resolution)

**What it does:**
Changes the frequency step between individual RSSI measurements. Smaller steps = finer detail but narrower total bandwidth. Larger steps = wider coverage but coarser detail.

**How it works:**
- Cycles through 15 predefined step sizes: 0.01, 0.1, 0.5, 1.0, 2.5, 5.0, 6.25, 8.33, 10.0, 12.5, 15.0, 20.0, 25.0, 50.0, 100.0 kHz
- The total displayed bandwidth = `stepsCount × scanStep`
- Automatically recalculates `frequencyChangeStep = totalBW / 2`
- Restarts the sweep and clears the blacklist

**Example:**
- Steps=64, Step=25 kHz → total span = 1.6 MHz
- Steps=128, Step=50 kHz → total span = 6.4 MHz

---

#### 2 / 8 — Frequency Change Step

**What it does:**
Adjusts the amount the spectrum window moves when you press UP/DOWN.

**How it works:**
- Changes `frequencyChangeStep` in increments of `scanStep × 4`
- Range: minimum 10 kHz, maximum 200 kHz
- There is a 100ms delay on each press to prevent rapid runaway

**Visual indicator:** Displayed as "△XX.XXk" (center mode) or "△XX.XXk" below the end frequency (edge mode).

---

#### 3 / 9 — dB Max (Manual) / Sensitivity Profile (Auto)

**In Manual Mode** (after pressing MENU short — status shows "M -xx/-xx"):
- **3:** Decreases the dB ceiling by 5 dB (compresses the display, signals appear higher)
- **9:** Increases the dB ceiling by 5 dB (expands the display, signals appear lower)
- Range: from `dbMin + 10` to +10 dBm
- After adjustment, manual control lasts for 2 sweeps before auto-scaling resumes

**In Auto Mode** (status shows "A:WEAK/NORM/STRG"):
- **3:** Cycles sensitivity profile backward: STRG → NORM → WEAK
- **9:** Cycles sensitivity profile forward: WEAK → NORM → STRG

| Profile | Margin above noise | Best for |
|---------|-------------------|----------|
| WEAK | +12 dB (24 RSSI units) | Noisy environments, squelch less likely to open |
| NORM | +8 dB (16 RSSI units) | Everyday use, legacy behavior |
| STRG | +5 dB (10 RSSI units) | Quiet environments, finds weaker signals |

---

#### * (STAR) / F — RSSI Trigger Level

**What it does:**
Adjusts the squelch threshold — the signal strength that must be exceeded for the radio to stop scanning and open the receiver.

**How it works:**
- Each press adjusts by ±2 RSSI units (≈ ±1 dB)
- The trigger level is clamped between `dbMin` and `dbMax`
- If the trigger exceeds dbMax, dbMax is automatically raised
- The trigger line is drawn as a dashed horizontal line on the spectrum display
- When the spectrum curve crosses this line, the radio tunes to that peak and opens the squelch

**The trigger line appears** as a horizontal dotted line across the spectrum. When a signal peaks above this line, the radio stops scanning and enters listen mode.

---

#### 0 — Modulation

**What it does:**
Cycles through available demodulation modes.

**Sequence:** FM (0) → AM (1) → USB (2) → (back to FM)

If `ENABLE_BYP_RAW_DEMODULATORS` is active: FM → AM → USB → BYP → RAW → (back to FM).

**How it works:**
- Forces the receiver closed (toggle RX off) before changing demodulator — prevents carrying stale RX state across modes
- Each mode has its own RSSI floor characteristics — the trigger level is recalibrated automatically when switching
- The RSSI history and peak hold are cleared (different modes can have very different noise floors)
- The current mode appears on screen as "FM", "AM", "USB", etc. (from `gModulationStr[]`)
- **Important:** "NFM" (Narrow FM) is NOT a separate modulation type. It is a **receive filter bandwidth setting** (12.5 kHz) toggled via key 6.

---

#### 4 — Steps Count (Zoom)

**What it does:**
Changes the number of measurement steps per sweep, which changes the total displayed bandwidth.

**Cycle:** 128 → 64 → 32 → 16 → (back to 128)

**How it works:**
- Each step takes one RSSI measurement
- The total bandwidth = `stepsCount × scanStep`
- `frequencyChangeStep` is recalculated as total bandwidth / 2
- The sweep is restarted and blacklist cleared

**Examples with 25 kHz scan step:**
| Steps | Bandwidth | Use Case |
|-------|-----------|----------|
| 128 | 3.2 MHz | Wide overview, fast scan |
| 64 | 1.6 MHz | Default, good balance |
| 32 | 800 kHz | Moderate zoom |
| 16 | 400 kHz | Deep zoom, detailed view |

---

#### 5 — Frequency Input

**What it does:**
Opens a keypad for direct frequency entry, allowing you to jump to any frequency within the radio's range.

**How it works:**
1. Screen clears, shows 10 dashes: `----------`
2. Enter digits (0-9) for the frequency, use * (STAR) as the decimal point
3. Example: `4 4 0 * 1 2 3 4 5` → frequency 440.12345 MHz
4. Press **MENU** to confirm and jump to that frequency
5. Press **EXIT** at the start to cancel (returns to previous mode)
6. Press **EXIT** during entry to backspace

The frequency is parsed as:
- Digits before the decimal point: MHz (each position × 100000, × 10000, × 1000, etc.)
- Digits after the decimal point: kHz/Hz fractions (× 10000, × 1000, × 100, × 10)

---

#### 6 — Listening Bandwidth

**What it does:**
Changes the IF filter bandwidth used while listening to a received signal.

**Cycle:** 25.0 kHz → 12.5 kHz → 6.25 kHz → (back to 25.0 kHz)

**How it works:**
- This setting only affects the **listening phase** (when the squelch is open and audio is playing)
- During scanning, the bandwidth reverts to the scan bandwidth (determined by scan step)
- Narrower bandwidth = less noise but more audio distortion
- Wider bandwidth = clearer audio but more background noise
- Displayed on screen as "25k", "12.5k", or "6.25k"

---

#### SIDE1 — Blacklist Peak

**What it does:**
Marks the current strongest peak so the scanner ignores it on future sweeps. The signal is hidden from both the spectrum curve and the waterfall.

**How it works:**
1. Records the current peak index into a blacklist array (up to 15 entries with `ENABLE_SCAN_RANGES`)
2. Sets that column's RSSI history to `RSSI_MAX_VALUE` (65535) — the renderer skips this value entirely
3. Resets peak detection so the next strongest signal becomes the new peak
4. Closes the receiver if it was listening
5. Resets scan statistics and continues sweeping

**Blacklist is cleared** when:
- The frequency range is moved (UP/DOWN)
- The scan step is changed (1/7)
- The steps count is changed (4)
- The modulation is changed (0)

---

#### SIDE2 — Backlight Toggle

**What it does:**
Toggles the LCD backlight between full brightness and minimum brightness.

**How it works:**
- Reads and writes the backlight brightness register
- Does NOT change the backlight timeout setting in EEPROM
- The default backlight state on entry comes from `BACKLIGHT_TIME` in EEPROM (if =0, off; otherwise on)

---

#### PTT — Tune to Peak / STILL Mode

**What it does:**
Immediately tunes the radio to the strongest detected peak frequency and enters **STILL mode** — a stationary receiver display with S-meter.

**How it works:**
1. Reads the current peak frequency from `peak.f`
2. Sets the BK4819 to that frequency using the full (non-optimized) path
3. Opens the receiver (squelch open)
4. Switches to STILL mode display:
   - **S-meter bar:** horizontal bar graph showing signal level
   - **S-unit readout:** "S: 5" (standard S-units)
   - **dBm readout:** "-87 dBm" (exact signal level)
   - **Register controls:** LNA, LNAs, PGA gain adjustments (see STILL mode below)
   - **Monitor toggle:** SIDE1 toggles monitor mode (forces receiver open)
5. The waterfall continues showing a single-column trace of the tuned frequency
6. During monitor mode, the receiver stays open regardless of signal level

**To return to scanning mode:** press **EXIT**.

---

#### MENU (Short Press) — Manual/Auto Toggle

**What it does:**
Toggles between **Manual** and **Automatic** trigger level control.

**How it works:**
- **Auto mode** (default, shows "A:WEAK/NORM/STRG"):
  - The trigger level is calculated automatically from the noise floor
  - The noise floor is tracked with an EMA filter: `floor = (3×old + new + 2) >> 2`
  - The trigger is set to `noiseFloor + margin` (margin depends on sensitivity profile)
  - The zoom (dbMax) is also adjusted automatically to keep signals visible

- **Manual mode** (shows "M -87/-92"):
  - The trigger level stays where you set it (via */F keys)
  - dbMax is controlled manually (via 3/9 keys)
  - The noise floor tracking continues but doesn't affect the trigger
  - The status shows current RSSI / trigger level as dBm values

---

#### MENU (Long Hold — ~1.5 seconds) — Reset to Defaults

**What it does:**
Resets ALL spectrum settings to their factory defaults while keeping the current frequency/range.

**What gets reset:**
| Setting | Default Value |
|---------|---------------|
| Scan step | 25.0 kHz (S_STEP_25_0kHz) |
| Steps count | 64 (STEPS_64) |
| Listening bandwidth | Wide (25 kHz) |
| Modulation | Active VFO's modulation |
| Trigger level | Auto (recalibrate from noise floor) |
| Auto-sensitivity | NORM |
| Manual flag | false |
| Monitor mode | false |
| Menu state | 0 |
| AGC lock | false |
| Sweep direction | Left→Right |

**What is NOT reset:** Current frequency center, saved EEPROM settings.

---

#### EXIT — Exit Spectrum

**What it does:**
Saves settings and returns to normal radio operation.

**How it works:**
1. Saves persistent settings to SPI flash (if `ENABLE_FEAT_N7SIX_SPECTRUM`):
   - Scan step index, steps count, listening bandwidth
   - Manual flag, auto-sensitivity profile
   - dbMax (as encoded offset)
   - RSSI trigger level (0xFF = auto)
2. Restores the original frequency from `initialFreq`
3. Restores all BK4819 registers from the backup stack
4. Writes the current state to EEPROM for resume-on-boot (if `ENABLE_FEAT_N7SIX_RESUME_STATE`)
5. Turns backlight on
6. Returns to the main application loop

---

## 4. STILL Mode Detail

After pressing PTT, you enter STILL mode — the radio becomes a fixed-frequency receiver with diagnostic tools.

### STILL Mode Display

```
┌─────────────────────────────────────────────────────────────────┐
│  S: 5             -87 dBm                                      │
│  ▓▓▓▓▓▓▓▓▓▓▓▓▓▓░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░  │
│  ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░  │
│  ┌──────┐  ┌──────┐  ┌──────┐                                  │
│  │ LNAs │  │ LNA  │  │ PGA  │  ← Register controls            │
│  │ -11dB│  │ -9dB │  │ -3dB │                                  │
│  └──────┘  └──────┘  └──────┘                                  │
│  [Waterfall continues with single-column trace]                │
└─────────────────────────────────────────────────────────────────┘
```

### STILL Mode Key Functions

| Button | Function |
|--------|----------|
| ▲ / ▼ | Change register value (when a register is selected) OR fine-tune frequency (when no register selected) |
| MENU | Cycle through register controls: none → LNAs → LNA → PGA → (back to none) |
| SIDE1 | Toggle monitor mode (receiver stays open) |
| 5 | Enter frequency input mode |
| EXIT | Return to scan mode (clears waterfall buffer, restart scan) |

**Frequency fine-tuning** in STILL mode uses modulation-specific steps (`modulationTypeTuneSteps[]`):
- FM: 10 kHz per press
- AM: 5 kHz per press
- USB: 1 kHz per press
- (Additional steps for BYP/RAW if `ENABLE_BYP_RAW_DEMODULATORS`)

*Note: These steps are in BK frequency units where 1 unit = 0.1 kHz. The array values {100, 50, 10} correspond to {10 kHz, 5 kHz, 1 kHz} respectively.*

### Register Controls (with `ENABLE_FEAT_N7SIX_SPECTRUM`)

| Register | BK4819 Area | Options | Function |
|----------|-------------|---------|----------|
| LNAs | REG_13 bits 8-9 | -19, -16, -11, 0 dB | Low-Noise Amplifier sensitivity (coarse) |
| LNA | REG_13 bits 5-7 | -24, -19, -14, -9, -6, -4, -2, 0 dB | Low-Noise Amplifier (fine) |
| PGA | REG_13 bits 0-2 | -33, -27, -21, -15, -9, -6, -3, 0 dB | Programmable Gain Amplifier |

The selected register is highlighted with an inverted display (white text on black background). Press ▲/▼ to adjust the gain value. When any LNA/PGA register is changed, AGC is locked to prevent the auto-gain from undoing your adjustment.

---

## 5. Waterfall Controls

The waterfall is **not directly controlled** — it's a visual history display driven by the spectrum scanner. However, understanding its behavior helps interpret the display:

| Observer Effect | Explanation |
|-----------------|-------------|
| **Scrolls downward** | Newest data at the top, oldest scrolls off the bottom |
| **Brightness = signal strength** | 16 grayscale levels (4-bit), stronger signals are brighter/whiter |
| **Black = no signal** | Blacklisted or unmeasured frequencies appear black |
| **Vertical streak during listen** | When listening, the peak column stays bright while background dims (3-pixel soft falloff) |
| **Persistence** | After a signal drops, the peak column fades over 3 sweeps before disappearing |
| **Resolution matches spectrum** | 128 waterfall columns = 128 spectrum columns (interpolated from actual measurements) |
| **dB range affects contrast** | Changing dbMin/dbMax changes how signals map to grayscale levels |

The waterfall occupies the bottom 2 display pages (lines 48-63 = 16 rows). Each row is one completed sweep (a full forward+return round trip). At typical sweep speeds, 16 rows represent approximately 10-20 seconds of history.

---

## 6. Understanding the Status Line

```
A:STRG >     ████████████░░░░░  ← Auto mode: sensitivity "STRG", sweeping right
M -87/-92    ████████████░░░░░  ← Manual mode: current -87 dBm, trigger -92 dBm
```

The status line shows:
- **Auto mode:** `A:WEAK/NORM/STRG` + sweep direction arrow (`>` right, `<` left)
- **Manual mode:** `M {current dBm}/{trigger dBm}` + sweep direction

**Battery icon** (right side, pixels 116-127):
- Segmented bar: fuller = more charge
- Updates from battery ADC every 4096 ticks (~2-3 seconds)

**Channel name** (center, during listen mode):
- When a signal opens the squelch, the spectrum tries to find a memory channel matching the tuned frequency
- If found, the channel name appears in the status bar (e.g., "REPEATER_1")

---

## 7. Tips & Tricks

### Quick Start
1. Enter Spectrum mode
2. Use **1/7** to set scan step to 25 kHz (default, good balance)
3. Use **4** to set 64 steps (1.6 MHz span)
4. Use **UP/DOWN** to find interesting frequencies
5. When you see a peak, press **PTT** to listen

### Finding Weak Signals
1. Press **MENU** to ensure you're in Auto mode
2. Press **9** repeatedly to cycle sensitivity to **STRG** (strongest sensitivity)
3. Watch for faint peaks approaching the trigger line

### Quick Power User Moves
- **Skip a busy freq:** SIDE1 while the peak is highlighted = blacklist it
- **Fine frequency entry:** Press **5**, type frequency, press **MENU**
- **Back to scan range:** EXIT from STILL mode = return to scanning
- **Reset everything:** Hold MENU for 1.5 seconds

### Understanding Sweep Direction
- The arrow in the status line (`>` or `<`) shows the current sweep direction
- Sweeps alternate left→right / right→left to avoid directional bias
- If a signal appears stronger in one direction, the bidirectional sweep averages it out

### Audio Quality During Listen
The radio intentionally **silences the SPI bus** between measurements (320 ms gaps). This reduces BK4819 register-access noise in the audio path. You may hear a faint 3 Hz tick — this is normal and far better than continuous buzzing.

---

## 8. Troubleshooting

| Problem | Likely Cause | Solution |
|---------|-------------|----------|
| No signals visible | Trigger too high or scan range too wide | Hold MENU to reset, or adjust step/range |
| Waterfall all black | dB range too narrow for signal levels | Press 3/9 to widen range |
| Waterfall all white | dB range too wide (everything clipped) | Press 3/9 to narrow range |
| Radio stays on one freq (won't scan) | Listen mode is stuck | Press UP/DOWN to force re-sweep |
| Can't move frequency | Scan range mode active (gScanRangeStart set) | Exit and disable scan ranges in menu |
| SIDE1 doesn't seem to do anything | No peak detected yet | Wait for a sweep cycle to complete |
| Screen flickers | Render timer vs sweep rate interaction | Normal — 9 Hz render rate is designed for visual stability |
| Audio hiss during scanning | SPI bus noise from BK4819 register access | Normal in scanning mode; listen mode has 320 ms quiet periods |

---

## 9. Configuration Matrix

### Scan Step × Steps Count = Bandwidth

```
                 Steps Count
              16     32     64    128
            ┌──────────────────────────┐
      0.01  │  0.16   0.32   0.64  1.28│ MHz
      0.1   │  1.6    3.2    6.4  12.8 │
      0.5   │  8     16     32    64   │
      1     │ 16     32     64   128   │
      2.5   │ 40     80    160   320   │
      5     │ 80    160    320   640   │
Step  6.25  │100    200    400   800   │   All in kHz
(kHz) 8.33  │133    267    533  1067   │
      10    │160    320    640  1280   │
      12.5  │200    400    800  1600   │
      15    │240    480    960  1920   │
      20    │320    640   1280  2560   │
      25    │400    800   1600  3200   │
      50    │800   1600   3200  6400   │
     100    │1600  3200   6400 12800   │
            └──────────────────────────┘
```

### Filter Bandwidth vs. Audio Quality

| BW Setting | BK4819 Value | Audio Quality | Noise | Use Case |
|-----------|-------------|---------------|-------|----------|
| 25 kHz | `0b0011011000101000` | Best | Most | Strong signals, clear conditions |
| 12.5 kHz | `0b0111111100001000` | Good | Moderate | Everyday use |
| 6.25 kHz | `0b0100100001011000` | Narrow | Least | Weak signals, noisy conditions |

---

## 10. Frequency Bands & Rules

The radio frequency range is defined by `frequencyBandTable[]` in `frequencies.c`. Spectrum mode respects the full table:

| Band | Range (MHz) | Notes |
|------|-------------|-------|
| BAND1_50MHz | 5.000 - 76.000 | Ham 6m band (extends down to 1.8 MHz with `ENABLE_WIDE_RX`) |
| BAND2_108MHz | 108.000 - 137.000 | Air band |
| BAND3_137MHz | 137.000 - 174.000 | VHF ham 2m band |
| BAND4_174MHz | 174.000 - 350.000 | VHF/UHF gap |
| BAND5_350MHz | 350.000 - 400.000 | Mil-air / public safety |
| BAND6_400MHz | 400.000 - 470.000 | UHF ham 70cm band |
| BAND7_470MHz | 470.000 - 600.000 | UHF TV / PMR (extends to 1300 MHz with `ENABLE_WIDE_RX`) |

### Frequency Boundaries

| Constant | Value (without WIDE_RX) | Value (with WIDE_RX) |
|----------|------------------------|----------------------|
| **F_MIN** | 5.00000 MHz | 1.80000 MHz |
| **F_MAX** | 600.00000 MHz | 1300.00000 MHz |

- F_MIN = `frequencyBandTable[0].lower` (BAND1_50MHz lower bound)
- F_MAX = `frequencyBandTable[BAND_N_ELEM-1].upper` (BAND7_470MHz upper bound)

### Cross-Band VHF/UHF Boundary

- The BK4819 RX filter path must be switched when crossing 280 MHz
- This is handled automatically in `SetFScan()` by checking:
  ```c
  if ((f < 28000000) != (fMeasure < 28000000))
      BK4819_PickRXFilterPathBasedOnFrequency(f);
  ```
- The boundary is 28 MHz (frequency in BK units), which corresponds to 280 MHz actual

---

*This guide is based on the source code analysis of spectrum.c (2,681 lines), waterfall.c (226 lines), frequencies.c, and radio.h from UV-K1Series ApeX-Edition v7.6.8.*
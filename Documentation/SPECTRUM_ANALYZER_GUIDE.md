# SPECTRUM ANALYZER USER GUIDE
## Professional Signal Analysis with Bandscope Mode

**For:** UV-K1 Series / UV-K5 V3 ApeX Edition  
**Firmware:** v7.6.4br3+ (v7.6.0+ supported; range alignment fixed in v7.6.4br3)  
**Document Version:** 1.1 (Updated March 2, 2026)

---

## TABLE OF CONTENTS

1. [Introduction](#introduction)
2. [Quick Start (5 Minutes)](#quick-start-5-minutes)
3. [Display Elements Explained](#display-elements-explained)
4. [Operating Modes](#operating-modes)
5. [Practical Analysis Techniques](#practical-analysis-techniques)
6. [Visual Signal Interpretation](#visual-signal-interpretation)
7. [Advanced Configuration](#advanced-configuration)
8. [Troubleshooting Guide](#troubleshooting-guide)
9. [Reference Tables](#reference-tables)

---

## INTRODUCTION

### What is a Spectrum Analyzer?

A spectrum analyzer is a professional instrument that displays:
- **Frequency axis** (horizontal): 136-480 MHz range
- **Amplitude axis** (vertical): Signal strength from -130 to -50 dBm
- **Time axis** (waterfall): Historical signal evolution

Unlike traditional radio "S-meter" which shows one frequency, the spectrum analyzer shows **128 frequencies simultaneously**, revealing:
- Hidden weak signals
- Interference patterns
- Modulation characteristics
- Band occupancy
- Intermittent transmissions

### Why Use Spectrum Analyzer Mode?

| Use Case | Benefit |
|----------|---------|
| **Band Scanning** | See all active frequencies at once |
| **Interference Hunting** | Identify RF pollution sources |
| **Signal Strength Mapping** | Compare signal levels across band |
| **Finding Weak TX** | Detect low-power transmitters |
| **Monitoring Activity** | Watch network repeater traffic |
| **Learning RF** | Visual understanding of RF propagation |

---

## QUICK START (5 MINUTES)

### Step 1: Activate Spectrum Analyzer
```
Press: [F] + [5]
Result: Spectrum analyzer mode opens
```

### Step 2: Start Scanning
```
Press: [* SCAN]
Result: Radio begins sweeping through frequency range
```

### Step 3: Observe the Display
```
┌─────────────────────────────────────┐
│  F: 145.500  USB  12.5k             │  ← Current frequency & mode
├─────────────────────────────────────┤
│        ╱╲                           │
│       ╱  ╲     Signal trace        │  ← SPECTRUM TRACE (blue line)
│      ╱    ╲                        │
│ ────╱──────╲──────────────────      │
├─────────────────────────────────────┤
│  ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓     │
│  ▓▓▓▓▓▓▓   ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓     │  ← WATERFALL HISTORY
│  ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓     │     (16 rows showing time)
├─────────────────────────────────────┤
│ Step: 25kHz  Rssi Trig: -95dBm      │  ← Configuration
└─────────────────────────────────────┘
```

### Step 4: Interpret What You See
- **Blue trace peaks** = Strong signals
- **Flat baseline** = Noise floor
- **Dashed line above** = Peak history (peak hold)
- **Waterfall scrolling** = Real-time activity

### Step 5: Stop Scanning
```
Press: [EXIT]
Result: Spectrum analyzer closes, returns to VFO mode
```

---

## DISPLAY ELEMENTS EXPLAINED

### 🔵 THE SPECTRUM TRACE (Blue Solid Line)

```
┌─────────────────────────────────────┐
│        ╱╲  ╱╲                       │
│       ╱  ╲╱  ╲                      │  ← Amplitude (signal strength)
│      ╱        ╲                     │
│────────────────────────────────────┘
     ↑
     └─ Frequency axis →
```

**What it shows:**
- Height = Signal strength at that frequency
- Wider base = Wider bandwidth (occupation)
- Multiple peaks = Multiple signals (interference)
- Smooth = Professional filtration/smoothing
- Jagged = Noise, weak signals

**Reading peaks:**
| Peak | Meaning |
|------|---------|
| **S9 level (top)** | Very strong signal |
| **S5-S7 height** | Good contact strength |
| **S1-S3 height** | Weak signal |
| **At baseline** | Just noise (no signal) |

---

### 📊 THE WATERFALL DISPLAY (16 Rows)

Waterfall shows **temporal evolution** of the spectrum:

```
Row  0: ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ ← NEWEST (just measured)
Row  1: ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░
Row  2: ░░░░░░ █████ ░░░░░░░░░░░░░░░░░░    Signal appearing
Row  3: ░░░░ █████████ ░░░░░░░░░░░░░░░░    Signal growing
Row  4: ░░ █████████████ ░░░░░░░░░░░░░░    Peak intensity
Row  5: ░░ █████████████ ░░░░░░░░░░░░░░    Holding
Row  6: ░░░░ █████████ ░░░░░░░░░░░░░░░░    Fading
Row  7: ░░░░░░ █████ ░░░░░░░░░░░░░░░░░░    Nearly gone
...
Row 15: ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ ← OLDEST
        └─────────── Frequency bins →────┘
        
Legend: ░░░ = Weak/noise  ▓▓▓ = Medium  ███ = Strong
```

**Key characteristics:**

| Feature | Meaning |
|---------|---------|
| **Vertical columns** | Signal at specific frequency |
| **Horizontal rows** | Signal over time |
| **Bright spot** | Recent strong signal |
| **Fading trail** | Signal ending/fading away |
| **Ghost image** | Residual from minutes ago |
| **Uniform gray** | Noise floor baseline |

**Waterfall patterns tell stories:**

```
Pattern: Continuous bright line
Meaning: Persistent transmission (repeater, beacon)

Pattern: Intermittent flashes
Meaning: Bursty transmissions (DTMF, scanner activity)

Pattern: Bright then fading
Meaning: Mobile unit transmitting then moving away

Pattern: Multiple vertical lines
Meaning: Several simultaneous signals (busy band)
```

---

### ➖ THE PEAK HOLD TRACE (Dashed Line)

```
┌─────────────────────────────────────┐
│        ╱╲  ╱╲                       │
│ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─   │ ← PEAK HOLD (dashed)
│       ╱  ╲╱  ╲                      │ ← CURRENT (solid)
│      ╱        ╲                     │
│───────────────────────────────────└
```

**What it shows:**
- Maximum signal level ever observed at each frequency
- "Ghostly" fading line showing historical peak
- Helps identify brief signal bursts that current trace might miss

**Visual behavior:**
```
SIGNAL PRESENT:
  Peak hold = at current signal level

SIGNAL ENDS:
  Peak hold fades gradually (exponential decay)
  Takes ~30 seconds to return to baseline

SIGNAL RETURNS:
  Peak hold immediately rises to new maximum
```

**Example scenario:**
```
TX starts   → Peak hold shoots up
TX ends     → Peak hold slowly fades down (visible "ghost")
New TX      → Peak hold rises to new peak
```

---

### 📏 THE NOISE FLOOR (Baseline)

The **bottom of the display** represents the minimum detectable signal:

```
Noise Floor = Inherent RF background
Range: -130 dBm (rural, quiet) to -100 dBm (urban, active)

┌─────────────────────────────────────┐
│        ╱╲                           │
│       ╱  ╲                          │
│      ╱    ╲                         │
│ ────╱──────╲──────────────────      │
│═════════════════════════════════════│ ← NOISE FLOOR
│ ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ │   (baseline ≈ -120 dBm)
└─────────────────────────────────────┘
```

**Noise floor interpretation:**

| Floor Level | RF Environment | Notes |
|-------------|-----------------|-------|
| **-130 dBm** | Heavily shielded | Excellent clean signal view |
| **-120 dBm** | Rural/quiet | Typical amateur band |
| **-110 dBm** | Suburban | Some ambient RF |
| **-100 dBm** | Urban/congested | Heavy interference |

---

### 📡 THE S-METER (Right Side)

Displays signal strength in **IARU standard** scale:

```
S0  S1  S2  S3  S4  S5  S6  S7  S8  S9  +10  +20
│───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼────┼───│
-130                                         -50  [dBm]
```

**S-Meter Scale Interpretation:**

| Range | Interpretation | Contact Quality |
|-------|-----------------|-----------------|
| **S0-S2** | Very weak | Barely detectable |
| **S3-S4** | Weak | Readable with difficulty |
| **S5-S6** | Moderate | Good contact |
| **S7-S8** | Strong | Excellent contact |
| **S9** | Very strong | Perfect signal |
| **+10/+20** | Overload | Signal too strong (clip) |

---

## OPERATING MODES

### 🔍 MODE 1: BLIND SCANNING (Default)

**Used for:** Finding active frequencies without prior knowledge

```
START:                    DURING:                     WITH SIGNAL:
Press [* SCAN]           Radio sweeps continuously    Auto-locks frequency
                         Display updates 60 Hz        (Listen mode activates)
                         Waterfall scrolls smoothly   Waterfall continues
                         Peak hold updates            
```

**Controls during scan:**
```
[* SCAN]        Start/resume scanning
[EXIT]          Stop scanning, return to VFO
[UP]/[DOWN]     Manual step (within current view)
[SIDE1]         Blacklist current frequency (skip future)
[MENU]          Pause scan, open menu
```

**Typical workflow:**
1. Press [* SCAN] to begin
2. Watch for activity (peaks rising in trace)
3. When signal detected → radio auto-locks
4. Watch modulation pattern in waterfall
5. Press [EXIT] when done observing

---

### 📍 MODE 2: LISTEN MODE (Auto-Activated)

**Triggered when:** Signal detected above squelch threshold

```
BEFORE DETECTION:    DETECTION EVENT:         AFTER DETECTION:
Scanning...          Signal >> threshold      Frequency locked
Waterfall scrolling  └─> Auto-lock            Trace updates at that freq
Up/down stepping         (Listen mode)        Waterfall shows activity
                                              Press [EXIT] to resume scan
```

**Automatic behavior:**
- Radio **holds frequency** when signal detected
- Spectrum continues updating in real-time
- Waterfall shows **temporal pattern** of TX
- **Han timer** (default 2 sec) allows gap detection
- Returns to scanning when signal ends + timer expires

**Manual control in Listen:**
```
[EXIT]          Resume scanning (exit listen mode)
[UP]/[DOWN]     Manually step frequency (within scan range)
[MENU]          Open settings
```

---

### 🎯 MODE 3: FIXED FREQUENCY ANALYSIS

**Used for:** Detailed analysis of single frequency

```
SETUP:
1. Use [UP]/[DOWN] to tune to target frequency
2. Press [F] + [5] to open spectrum analyzer
3. Zoom in on that area (adjust step size)

DISPLAY:
Shows detailed view around tuned frequency
Waterfall captures all activity at that freq
Peak hold shows historical maximum
```

**Technique:**
```
Known repeater frequency?
→ Tune to it
→ Activate spectrum analyzer
→ Watch for keeper signals (repeater input)
→ Observe input offset (±offset frequency)
→ Watch activity timeline via waterfall
```

---

### 🔧 MODE 4: RANGE SCANNING (Advanced)

**Used for:** Surveying specific frequency band

```
SETUP (Menu):
1. Menu → Scan Range
2. Set Start Freq: 146.000 MHz
3. Set Stop Freq: 148.000 MHz
4. Press [* SCAN]

RESULT:
Radio sweeps only 146-148 MHz
Finer frequency resolution
Detailed view of selected band
```

**Use cases:**
- Scout specific band (local repeaters)
- Search narrow range for interference
- Map signal strength across band
- Characterize RF environment

**📌 NEW IN v7.6.4br3: Scan Range Display Alignment**

Previous versions showed the spectrum graph offset from center when using Scan Range mode. This has been corrected:

```
BEFORE (v7.6.0):                   AFTER (v7.6.4br3+):
Range: 434.000–435.000 MHz         Range: 434.000–435.000 MHz
Center: 434.500 MHz                Center: 434.500 MHz

Display offset 3–5 pixels          Display perfectly centered
Graph misaligned with marker       Graph aligned with marker
Not all measurements visible       All measurements visible

(Visual artifact, no data issue)   (Full spectrum visible & centered)
```

**Affected scenarios:**
- Narrow scan ranges (< 5 MHz)
- Lower step counts (STEPS_16, STEPS_32)
- Frequency ranges without VFO-mode constraints

**Impact:** All display elements (spectrum trace, waterfall, frequency marker, arrow) now perfectly align regardless of range width or step size setting.

---

## PRACTICAL ANALYSIS TECHNIQUES

### 🎓 TECHNIQUE #1: FINDING HIDDEN SIGNALS

**Scenario:** Looking for weak repeater without knowing exact frequency

```
STEP 1: WIDE OVERVIEW
  • Set step size: 100 kHz (coarse, fast scan)
  • Press [* SCAN]
  • Watch for any peaks (even small ones)
  • Note region of activity

STEP 2: NARROW TO REGION
  • Exit scan [EXIT]
  • Use [UP]/[DOWN] to position over region
  • Reduce step size: 12.5 kHz (fine)
  • Resume [* SCAN] with finer resolution

STEP 3: IDENTIFY PEAK
  • Watch trace peaks
  • Look at waterfall for activity pattern
  • Listen for audio (squelch)
  • Lock frequency when found

RESULT: Found hidden weak signal! 📶
```

**Display indicators of weak signal:**
```
┌──────────────────────────────────────┐
│  ▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔  │ ← Peak Hold shows
│         ╱╲                           │    historical max
│        ╱  ╲  ← Even single pixel     │
│───────╱────╲──────────────────────   │ ← Noise floor
│ ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ │
└──────────────────────────────────────┘
```

---

### 🎓 TECHNIQUE #2: CHARACTERIZING INTERFERENCE

**Scenario:** Identifying RF pollution on your frequency

```
STEP 1: AUDIT YOUR FREQUENCY
  • Tune to your operating frequency (e.g., 145.500)
  • Activate spectrum analyzer
  • Set tight zoom (step size 2.5-6.25 kHz)
  • Watch interference pattern

STEP 2: MEASURE CHARACTERISTICS
  • Note peak width (bandwidth occupied)
  • Watch modulation pattern (steady vs. bursty)
  • Check waterfall for intermittency
  • Estimate power (peak height on S-meter)

STEP 3: IDENTIFY SOURCE
  Pattern: Steady carrier ────────────
           Type: Beacon or continuous TX

  Pattern: Bursty spikes ▐ ▌ ▐ ▌ ▐ ▌
           Type: Digital data, DTMF

  Pattern: Frequency hopping █ █ ▓ ░ ▓
           Type: Spread spectrum (rare, advanced)

RESULT: Know what's interfering and how! 🔍
```

---

### 🎓 TECHNIQUE #3: SIGNAL PROPAGATION ANALYSIS

**Scenario:** Watching signal strength change as mobile moves

```
STEP 1: TARGET MOBILE SIGNAL
  • Identify signal in spectrum trace
  • Note starting peak height
  • Observe waterfall pattern

STEP 2: WATCH AS MOBILE MOVES
  Time:     0 sec      10 sec      20 sec      30 sec
  Signal:   █████      ███         ██          █
  meaning:  [Strong]   [FadeOUT]   [Weaker]    [Distant]

STEP 3: INTERPRET PROPAGATION
  • Steep fade → LOS (line-of-sight) path loses coverage
  • Slow fade → Multipath environment (urban)
  • Sudden drop → Obstruction/tunnel entry
  • Waterfall shows fading echo pattern

RESULT: RF path behavior documented! 📉
```

---

### 🎓 TECHNIQUE #4: FINDING INTERMITTENT TRANSMISSIONS

**Scenario:** Locating brief/occasional signals

```
SETUP:
  • Enable peak hold (should be ON by default)
  • Set DBMin to -130 dBm (maximum sensitivity)
  • Start scanning via [* SCAN]
  • Walk away or let scan run unattended

DETECTION:
  Current trace:    Only noise visible
  Peak hold line:   Shows historical max even if signal ended
  Waterfall:        Vertical "flash" shows TX burst time
  
Result: Even if you missed the live signal,
        peak hold shows it was there! 👻

STEP 2: IDENTIFY EXACT FREQUENCY
  • Use peak hold position to identify frequency
  • May require retuning to narrow down
  • Note on waterfall when bursts occur (timing pattern)
  • Verify with audio monitoring
```

---

## VISUAL SIGNAL INTERPRETATION

### 📋 SIGNAL SHAPE GALLERY

#### FM (Frequency Modulation) Signal
```
┌─────────────────────────────────────┐
│            ╱╲                       │
│           ╱  ╲                      │ Characteristics:
│          ╱────╲                     │ • Smooth "hump" shape
│  ───────╱──────╲───────────         │ • Symmetric peak
│                                     │ • Width ≈ 12.5-25 kHz
│  ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░  │ • Clean noise floor
└─────────────────────────────────────┘
```

#### AM (Amplitude Modulation) Signal
```
┌─────────────────────────────────────┐
│          ╱╲      ╱╲                 │
│         ╱  ╲    ╱  ╲                │ Characteristics:
│        ╱    ╲──╱    ╲               │ • Double peaks (sidebands)
│  ──────      └────────────          │ • Wider bandwidth
│                                     │ • Asymmetric shape
│  ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░  │ • Width ≈ 8-10 kHz
└─────────────────────────────────────┘
```

#### SSB (Single Sideband) Signal
```
┌─────────────────────────────────────┐
│                  ╱╲                 │
│                 ╱  ╲                │ Characteristics:
│                ╱    ╲               │ • Single sharp peak
│  ──────────────      ───────        │ • Narrow bandwidth
│                                     │ • Asymmetric sidebands
│  ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░  │ • Width ≈ 3-4 kHz
└─────────────────────────────────────┘
```

#### CW (Morse/Carrier Wave)
```
┌─────────────────────────────────────┐
│                   ║                 │
│                   ║                 │ Characteristics:
│                   ║                 │ • Extremely narrow spike
│  ─────────────────║────────────     │ • No modulation visible
│                   ║                 │ • Pure carrier
│  ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░  │ • Width ≈ 500 Hz
└─────────────────────────────────────┘
```

#### INTERFERENCE (Multiple Signals)
```
┌─────────────────────────────────────┐
│  ╱╲  ╱╲      ╱╲     ╱╲              │
│ ╱  ╲╱  ╲    ╱  ╲   ╱  ╲             │ Characteristics:
│╱    ▐  ▌   ╱    ╲ ╱    ╲            │ • Multiple peaks
│─────────────────────────────        │ • Complex pattern
│                                     │ • Crowded band
│  ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░  │ • Width ≈ 50-100 kHz
└─────────────────────────────────────┘
```

---

## ADVANCED CONFIGURATION

### ⚙️ DISPLAY SCALING (DBMax / DBMin)

Controls the **vertical axis** dynamic range:

```
MENU → 22 DBMax:     TOP of display (ceiling)
MENU → 23 DBMin:     BOTTOM of display (floor)
```

**Adjustment effects:**

```
DEFAULT SETTING:  DBMax = -50,  DBMin = -130
┌─────────────────────────────────────┐
│  (weak signals not visible here)     │
│        ╱╲                           │  ← MAX -50 dBm
│       ╱  ╲                          │
│ ────╱──────╲──────────────────      │
│  Noise floor region                 │
└─────────────────────────────────────┘
  MIN -130 dBm = Ultra-sensitive

ZOOMED IN:  DBMax = -80,  DBMin = -130
┌─────────────────────────────────────┐
│           ╱╲                        │  ← Signal not clipped
│          ╱  ╲                       │
│         ╱────╲  (enlarged view)     │  ← MAX -80 dBm
│  ───────      ───────               │
│  Noise floor filled 50% of screen   │
└─────────────────────────────────────┘
  MIN -130 dBm = All noise visible

ZOOMED OUT:  DBMax = -50,  DBMin = -100
┴┬───────────────────────────────────┬┴
│  Strong signals compressed          │  ← MAX -50 dBm
│  Noise floor squashed              │
│        ╱╲                           │
│       ╱  ╲                          │
│ ────╱──────╲──────────────────      │
│  Noise invisible (below floor)      │
└───────────────────────────────────────┘
  MIN -100 = Weak signals not shown
```

**Practical guide:**
```
Finding weak signals:
  → Set DBMax closer to -80 dBm
  → Set DBMin to -130 dBm
  → Watch weak peaks appear

Avoiding saturation:
  → Set DBMax to -50 dBm
  → Strong signals won't clip
  → View wide dynamic range

Clean display:
  → Set DBMin to -100 dBm
  → Noise floor visually disappears
  → Easier to see real signals
```

---

### 🔧 FREQUENCY STEP SIZE

Determines **horizontal axis** resolution:

```
STEP SIZE → How wide each display bin covers
Menu → 11 Step → Select 2.5k / 6.25k / 12.5k / 25k / 50k / 100k

EXAMPLE: 145.5 MHz with different step sizes

Step = 25 kHz:   Bin width = 320 kHz total  (coarse, fast)
┌─────────────────────────────────────┐
│  145------145.5-----146------146.5  │  ← Each pixel = 25 kHz
└─────────────────────────────────────┘

Step = 12.5 kHz: Bin width = 1.6 MHz total (standard)
┌─────────────────────────────────────┐
│  145.4-145.5-145.625-145.75-145.875  │  ← Each pixel = 12.5 kHz
│  -146-146.125-146.25-146.375-146.5   │
└─────────────────────────────────────┘

Step = 2.5 kHz:  Bin width = 320 kHz total (fine, slow)
┌─────────────────────────────────────┐
│ Many pixels per 1 MHz, excellent detail  │  ← Each pixel = 2.5 kHz
└─────────────────────────────────────┘
```

**Selection guide:**
| Step Size | Scan Speed | Detail | Use Case |
|-----------|-----------|--------|----------|
| **100 kHz** | Very fast | Low | Band overview |
| **50 kHz** | Fast | Low-med | Quick survey |
| **25 kHz** | Normal | Medium | General use |
| **12.5 kHz** | Normal | Good | Default, balanced |
| **6.25 kHz** | Slow | High | Signal detail |
| **2.5 kHz** | Very slow | Excellent | Precise analysis |

---

### 📊 SQUELCH ADJUSTMENT (RSSI Trigger)

Controls what frequency signals trigger lock:

```
MENU → 06 Squelch:  0-9 level

With signal at -95 dBm:

Squelch = 0 (always listen):
  ▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄  ← All signals detected
  Catches weak signals, but noisy

Squelch = 5 (moderate):
  ▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄  ← Reasonable threshold
  Balanced detection

Squelch = 9 (tight):
  ▄▄▄▄▄▄▄▄▄▄▄  ← Only strong signals
  Clean but misses weak TX
```

**Practical suggestions:**
```
Blind scanning (search mode):
  → Set squelch 3-4 (loose, catch all activity)
  → Stop on any frequency with signal
  → Listen to verify

Repeater hunting (known band):
  → Set squelch 5-6 (moderate)
  → Typical repeater signals detected
  → Few false triggers

Weak signal monitoring:
  → Set squelch 1-2 (very sensitive)
  → Catches marginal signals
  → Higher noise, but no miss

Interference rejection:
  → Set squelch 7-8 (tight)
  → Only strong interference locked
  → May miss legitimate weak signals
```

---

## TROUBLESHOOTING GUIDE

### ❓ Problem: Spectrum Trace Looks Flat (No Variation)

**Diagnosis Steps:**

```
STEP 1: Is scan running?
  ✓ Watch waterfall: Should scroll smoothly
  ✗ If frozen → Press [* SCAN] to start

STEP 2: Check display scaling
  ✓ Go to Menu → 22 DBMax / 23 DBMin
  ✗ If DBMin = -100 dBm and noise = -120 dBm
    → Everything below -100 not displayed!
  → Solution: Set DBMin = -130 dBm

STEP 3: Check squelch
  ✓ If squelch > 5 and signals weak
    → Squelch filters out all signals!
  → Solution: Press [F]+[UP]/[DOWN] to reduce

STEP 4: Measure actual signal
  ✓ Tune to known signal (repeater, beacon)
  ✗ If spectrum still flat
    → RSSI measurement failing
    → Try: [EXIT], reboot radio, retry
```

---

### ❓ Problem: Waterfall Not Scrolling / Frozen

**Diagnosis:**

```
STEP 1: Is scan active?
  → Watch spectrum trace peak
  → Should move left-right continuously
  ✗ If frozen in place
    → Scan paused or listen locked
    → Solution: Press [EXIT] to resume scan

STEP 2: Is listen mode locked?
  → Check frequency: Should be "locked" text
  ✗ If locked on single frequency
    → Signal detected, radio waiting
    → Solution: [EXIT] to exit listen mode

STEP 3: Reload spectrum
  → Press [EXIT] (exit analyzer)
  → Press [F]+[5] (reopen analyzer)
  → Press [* SCAN]
  → Waterfall should animate
```

---

### ❓ Problem: Peak Hold Disappears Instantly

**Explanation:**

```
BEHAVIOR: Peak hold present, then vanishes when signal ends

This is EXPECTED! Here's why:

Peak hold formula: peakHold = (peakHold * 7/8) + (currentSignal * 1/8)

When signal present (e.g., 80 dB):
  peakHold = (80 * 7/8) + (80 * 1/8) = 80  ✓ Stays high

When signal ends (= 0 dB):
  peakHold = (80 * 7/8) + (0 * 1/8) = 70   → Decays gradually
              ↓ next cycle
  peakHold = (70 * 7/8) + (0 * 1/8) = 61   → More decay
  ...after ~30 seconds → reaches baseline ~= 0

SOLUTION: Keep signal present to hold peak
  • Don't press [EXIT] immediately
  • Let waterfall scroll to show activity
  • New signal will show new peak hold
```

---

### ❓ Problem: S-Meter Reading Doesn't Match Spectrum Peak

**Possible causes:**

```
Cause #1: Different frequencies
  You're looking at spectrum at 146.000 MHz
  S-meter showing current RX at 145.500 MHz
  → They're measuring different things!
  → Solution: Tune VFO to same frequency as peak

Cause #2: Time lag
  Spectrum updates every ~60 ms
  S-meter updates every 500 ms
  → Frequency changed between updates
  → Solution: Stop scanning, let both stabilize

Cause #3: Different modulation
  FM mode: Different AGC gain than AM
  SSB mode: Different sensitivity
  → Spectrum shows broadband noise
  → S-meter shows demodulated strength
  → Solution: Compare same modulation mode

Cause #4: Multipath (urban)
  Signal reflects off buildings
  → Different path strengths
  → Spectrum shows combined power
  → Solution: Expected behavior, not error
```

---

### ❓ Problem: Spectrum Grass Slows / Becomes Static After 2-3 Seconds

**Important: This is NORMAL behavior!**

```
Why it happens:
  Spectrum analyzer includes synthetic noise generation
  to show activity at quiet frequencies.
  
  This noise uses an averaging filter (EMA):
  newNoise = (oldNoise * 7/8) + (randomVariation * 1/8)
  
  With zero-mean random input:
  → Filter converges to average (baseFloor)
  → After 12-18 cycles → becomes static
  → This is CORRECT averaging behavior!

Is it broken?
  NO! The filter is working correctly.
  Showing stable noise floor = filter working.
  
Result: Clean signal display after grass stabilizes ✓
```

**If you want to reset grass animation:**
```
Press [* SCAN] to restart
→ Grass animation restarts
→ Cycling through different frequencies
→ Spectrum remains active
```

---

## REFERENCE TABLES

### 🔢 STANDARD FREQUENCY BANDS (UV-K1/K5 V3)

| Band | Range | Steps | Notes |
|------|-------|-------|-------|
| **VHF** | 136-174 MHz | 25-50 kHz | Amateur 2m, Aviation, Weather |
| **UHF** | 400-480 MHz | 25-50 kHz | Amateur 70cm, PMR, Industry |
| **Extended** | 50-940 MHz | Custom | Depends on F-Lock setting |

### 📊 TYPICAL SIGNAL LEVELS (Meter Distance)

| Distance | Frequency | Environment | S-Meter | dBm |
|----------|-----------|-------------|---------|------|
| **1 m** | 146 MHz | Free space | S9+20 | -50 |
| **10 m** | 146 MHz | Free space | S9 | -70 |
| **100 m** | 146 MHz | Free space | S5-S6 | -100 |
| **1 km** | 146 MHz | Line-of-sight | S3-S4 | -120 |
| **10 km** | 146 MHz | Urban | S0-S1 | -130 |
| **100 m** | 430 MHz | Free space | S6-S7 | -95 |
| **1 km** | 430 MHz | Free space | S2-S3 | -125 |

### 🎯 SIGNAL BANDWIDTH BY MODE

| Mode | Typical Width | Display Width |
|------|---------------|---------------|
| **FM 12.5k** | 10 kHz | 2-3 pixels |
| **FM 25k** | 20 kHz | 4-5 pixels |
| **AM** | 10 kHz | 2-3 pixels |
| **USB/LSB** | 3-4 kHz | 1 pixel |
| **CW** | 500 Hz | <1 pixel (narrow) |

### 📢 COMMON REPEATER OFFSETS (North America)

| Band | Offset | Notes |
|------|--------|-------|
| **144-148 MHz** | +600 kHz | Standard VHF repeater |
| **145.1-145.5** | -600 kHz | Popular VHF band |
| **420-450 MHz** | +5 MHz | Standard UHF repeater |
| **449-450 MHz** | -5 MHz | High-side UHF |

**To find repeater pair:**
Set OffSet in menu → Spectrum shows both TX (main) and RX (offset)

---

## QUICK REFERENCE CARD

```
ACTIVATION:           [F] + [5]
START SCANNING:       [* SCAN]
STOP/EXIT:            [EXIT]
ZOOM IN:              [F] + step down
ZOOM OUT:             [F] + step up
ADJUST SQUELCH:       [F] + [UP]/[DOWN]
BLACKLIST FREQ:       [SIDE1] during scan
MENU:                 [MENU]
LISTEN MODE:          Auto (when signal detected)
EXIT LISTEN:          [EXIT]
```

---

## CONCLUSION

The spectrum analyzer is a powerful tool for:
- ✅ Finding signals without known frequencies
- ✅ Understanding RF environment
- ✅ Identifying interference sources
- ✅ Monitoring network activity
- ✅ Learning about RF propagation
- ✅ Professional signal analysis

**Key takeaways:**
1. **Spectrum trace** = Current signal strength at each frequency
2. **Waterfall** = History over time (top = newest)
3. **Peak hold** = Maximum ever observed (fades gradually)
4. **Noise floor** = Minimum detectable signal on your radio
5. **S-meter** = Overall signal strength in standard scale

**Next steps:**
- Start with wide scan (100 kHz step) for overview
- Narrow to interesting regions (12.5 kHz step)
- Use peak hold to catch intermittent signals
- Adjust DBMax/DBMin for clear viewing
- Master squelch for targeted searching

---

**Document Version:** 1.0  
**For Firmware:** 7.6.0+  
**Last Updated:** February 28, 2026

*Professional spectrum analysis at your fingertips—Happy exploring!*

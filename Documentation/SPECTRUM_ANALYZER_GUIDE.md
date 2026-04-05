<!--
=====================================================================================
Spectrum Analyzer User Guide
Author: N7SIX, Sean
Version: v7.6.5br3 (ApeX Edition)
License: Apache License, Version 2.0
=====================================================================================
-->

# UV-K1 SERIES ApeX EDITION
# SPECTRUM ANALYZER — PROFESSIONAL USER GUIDE

**Hardware:** UV-K1 / UV-K5 V3 (PY32F071 MCU)  
**Firmware:** ApeX Edition v7.6.5br3  
**Document Version:** 2.0 (April 5, 2026)

---

> **What's new in v7.6.5br3**  
> - STILL mode now shows real dBm values (root fix: BK4819 hardware RSSI ceiling 511 separated from sentinel 65535)  
> - Live RX audio in STILL mode working reliably — helicopter noise eliminated  
> - PTT with no signal present shows a centered **NO DETECTED ACTIVE SIGNAL** overlay  
> - STILL mode register inspector: LNAs / LNA / VGA / BPF all selectable and adjustable  
> - Arrow UP / DOWN navigation is now **rotary** (wraps at both min and max)  
> - BPF selector fixed: software-state index eliminates unreliable hardware register readback  
> - BPF selection persists across every RX event (restored after ToggleRX / RADIO_SetModulation)

---

## TABLE OF CONTENTS

1. [Introduction](#1-introduction)
2. [Quick Start — 5 Minutes](#2-quick-start--5-minutes)
3. [Display Elements Explained](#3-display-elements-explained)
4. [Operating Modes](#4-operating-modes)
    - 4.1 [SPECTRUM — Scanning Mode](#41-spectrum--scanning-mode)
    - 4.2 [LISTEN — Auto-Lock Mode](#42-listen--auto-lock-mode)
    - 4.3 [STILL — Single-Frequency Monitor](#43-still--single-frequency-monitor)
    - 4.4 [Range Scanning (Advanced)](#44-range-scanning-advanced)
5. [STILL Mode — Register Inspector](#5-still-mode--register-inspector)
6. [Key Map — Complete Reference](#6-key-map--complete-reference)
7. [Practical Analysis Techniques](#7-practical-analysis-techniques)
8. [Signal Shape Gallery](#8-signal-shape-gallery)
9. [Advanced Configuration](#9-advanced-configuration)
10. [Troubleshooting Guide](#10-troubleshooting-guide)
11. [Reference Tables](#11-reference-tables)

---

## 1. INTRODUCTION

### What Is a Spectrum Analyzer?

A spectrum analyzer is a professional instrument that displays:

| Axis | Meaning |
|------|---------|
| **Horizontal (X)** | Frequency — shows many channels simultaneously |
| **Vertical (Y)** | Amplitude — signal strength at each frequency |
| **Waterfall rows** | Time — historical signal activity scrolling downward |

Unlike a traditional S-meter that shows only one frequency, the spectrum analyzer shows **128 frequencies simultaneously**, revealing:

- Hidden weak signals below audible squelch
- Interference patterns and their bandwidth
- Modulation characteristics (FM hump vs. AM sidebands)
- Band occupancy and crowding
- Intermittent or bursty transmissions

### Spectrum Analyzer Capabilities (ApeX v7.6.5br3)

| Capability | Details |
|-----------|---------|
| Real-time trace | 128-bin live spectrum updated every sweep |
| Smoothed display | Exponential moving average removes flicker |
| Waterfall history | 16-row scrolling time history |
| Peak hold | Decaying dotted-line overlay (toggle with MENU) |
| STILL monitor | Single-frequency lock with live S-meter and dBm |
| Register inspector | Live LNAs / LNA / VGA / BPF tuning in STILL mode |
| PTT to STILL | Locks to strongest detected peak with signal guard |
| Frequency input | Direct numeric frequency entry via KEY_5 |
| Monitor mode | Forced RX regardless of squelch (SIDE1 in STILL) |
| Blacklist | Skip interfering frequencies during scan |
| Scan ranges | Configurable start/stop frequency bounds |
| Autosave | Spectrum settings persisted to flash automatically |

---

## 2. QUICK START — 5 MINUTES

### Step 1 — Activate Spectrum Analyzer
```
Press: [F] + [5]
```

### Step 2 — Begin Scanning
```
Press: [* SCAN]
The radio sweeps the frequency range continuously.
```

### Step 3 — Read the Display

```
+--------------------------------------------------------------+
|  145.500  FM  12.5k                        S5  -95dBm        | <- Header row
+--------------------------------------------------------------+
|  - - - - - - - - - - - -    <- Peak Hold (dotted, fades ~5s) |
|         /\       /\         <- Spectrum trace (signal peaks)  |
|        /  \-----/  \        <- Smoothed signal line           |
| -------/------------\------ <- Trigger level marker           |
+--------------------------------------------------------------+
| .:.:.:.:X:.:.:.:.:.:.:.:                                     |
| .:.:.:.:.:XX:.:.:.:.:.:.:    <- Waterfall (16 rows)           |
| .:.:.:.:.:.:XXXX:.:.:.:.:        top = newest                 |
| .:.:.:.:.:.:.:.:.:.:.:.:         bottom = oldest              |
+--------------------------------------------------------------+
|  Step: 25kHz    Trig: -95dBm                                 | <- Footer
+--------------------------------------------------------------+
```

### Step 4 — What to Look For

| Element | Meaning |
|---------|---------|
| Tall peak in trace | Strong signal at that frequency |
| Flat baseline | Noise floor — no signal |
| Dotted line above trace | Peak hold — historical maximum (decays ~5 s after signal ends) |
| Bright column in waterfall | Persistent signal over time |
| Vertical flash in waterfall | Brief transmission burst |

### Step 5 — Exit
```
Press: [EXIT]    Returns to VFO mode.
```

---

## 3. DISPLAY ELEMENTS EXPLAINED

### 3.1 — Spectrum Trace (Solid Line)

The **solid waveform line** is the real-time signal amplitude at each of the 128 display bins:

```
Peak height  -> Signal strength (higher = stronger)
Peak width   -> Signal bandwidth (wider = more spectrum occupied)
Multiple peaks -> Multiple simultaneous signals or harmonics
Jagged line  -> Noisy environment or very weak signals
Smooth curve -> Clean, well-filtered signal path
```

Signal levels use the BK4819 chip's 0.5 dB/step RSSI scale (hardware range: 0–511 raw counts, hardware register 0x67 bits [8:0]). Calibration offsets per band are applied automatically. dBm is calculated as: `(rssi / 2) − 160 + band_offset`.

### 3.2 — Waterfall (16 Rows)

The waterfall scrolls **downward** — top row is newest, bottom row is oldest:

```
Row  0: .:.:.:X:X:X:X:.:.:.:.:  <- just measured
Row  1: .:.:.:.:X:X:X:X:X:.:.:
Row  2: .:.:.:.:.:X:X:X:X:.:.:
Row  3: .:.:.:.:.:.:X:X:.:.:.:
...
Row 15: .:.:.:.:.:.:.:.:.:.:.:  <- oldest history

Brightness:  . = noise floor   : = weak   o = moderate   X = strong
```

**Pattern recognition:**

| Waterfall pattern | Interpretation |
|-------------------|----------------|
| Continuous bright vertical column | Persistent carrier (beacon, repeater) |
| Intermittent vertical flashes | Bursty digital data or voice bursts |
| Gradually brightening column | Mobile unit approaching |
| Bright then fading trail | Mobile unit departing or path failing |
| Multiple simultaneous columns | Congested band / interference |

### 3.3 — Peak Hold (Dashed Line)

Active when `PH` appears in the status bar (toggle with `MENU` in SCAN mode):

```
Signal present   -> Peak hold rises to current signal level immediately
Signal ends      -> Dashed line decays gradually (~5 seconds to baseline)
New stronger TX  -> Peak hold rises again to new maximum
```

Use peak hold to catch **intermittent transmissions** — even if you look away and the signal has ended, the ghost line still shows the maximum height and frequency.

### 3.4 — Noise Floor

The **flat bottom region** before any peaks is the inherent RF background of the environment and receiver:

| Environment | Typical Noise Floor |
|-------------|---------------------|
| Rural / shielded | −130 to −125 dBm |
| Suburban | −120 to −110 dBm |
| Dense urban | −110 to −100 dBm |

Lower noise floor = better sensitivity = ability to see weaker signals.

### 3.5 — Trigger Level Marker

A vertical or horizontal pixel marker shows the **RSSI threshold**. Signals above this level cause the scanner to lock and enter LISTEN mode. Adjusted with `[*]` and `[F]` in STILL mode.

### 3.6 — Header / Footer Information

- **Header:** Locked/center frequency, modulation mode, listening bandwidth
- **Footer:** Current scan step size, current RSSI trigger in dBm

---

## 4. OPERATING MODES

### 4.1 SPECTRUM — Scanning Mode

The default mode. The radio continuously sweeps across a frequency range and updates the trace and waterfall.

**Entry:** Press `[* SCAN]` from the spectrum screen.

**Auto-transition to LISTEN:** When any bin's RSSI exceeds the trigger level, the scanner locks that frequency and enters LISTEN mode automatically.

**Key controls in SPECTRUM mode:**

| Key | Action |
|-----|--------|
| `[* SCAN]` | Start / resume full sweep |
| `[EXIT]` | Exit spectrum analyzer (return to VFO) |
| `[UP]` / `[DOWN]` | Step center frequency up or down |
| `[0]` | Cycle modulation: FM → USB → LSB → AM → FM |
| `[3]` | Increase dBMax (raise upper ceiling) |
| `[9]` | Decrease dBMin (lower floor) |
| `[6]` | Cycle listen bandwidth (25k / 12.5k / 6.25k / narrower) |
| `[MENU]` | Toggle Peak Hold on/off (`PH` in status bar when on) |
| `[SIDE1]` | Blacklist current peak — skip in future sweeps |
| `[SIDE2]` | Toggle backlight |
| `[PTT]` | Lock strongest peak → STILL mode (signal guard active) |
| `[5]` (hold) | Toggle grid display on/off |

---

### 4.2 LISTEN — Auto-Lock Mode

**Triggered automatically** when the scanner detects a signal above the trigger threshold.

In LISTEN mode:
- The radio stays tuned to the detected frequency
- The spectrum trace and waterfall continue updating
- A listen timer counts down; radio resumes scanning when it expires and the signal is gone
- CTCSS/DCS tail-tone detection can end listen immediately

**Key controls in LISTEN mode:**

| Key | Action |
|-----|--------|
| `[EXIT]` | Force-resume scanning immediately |
| `[SIDE1]` | Blacklist this frequency and resume scanning |
| `[PTT]` | Lock to current frequency → enter STILL mode |

---

### 4.3 STILL — Single-Frequency Monitor

STILL mode locks the radio on **one frequency** for detailed monitoring with a live S-meter, dBm readout, and access to the hardware register inspector.

**Entry methods:**
1. Press `[PTT]` from SPECTRUM or LISTEN mode — only if a valid peak above trigger level is present
2. If no valid signal: a centered overlay message appears for ~300 ms

**NO DETECTED ACTIVE SIGNAL overlay:**  
When `[PTT]` is pressed but no peak qualifies (peak.f == 0 or RSSI below trigger), the following message appears centered on screen and clears automatically:

```
+========================+
|   NO DETECTED          |
|   ACTIVE SIGNAL        |
+========================+
```

**STILL mode display layout:**

```
+--------------------------------------------------------------+
|  146.520  FM  12.5k                                          | <- Locked freq
+--------------------------------------------------------------+
| XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX............               | <- S-meter bar
|  S: 7    -94 dBm                                             | <- S-unit + dBm
+--------------------------------------------------------------+
| +--------+  +--------+  +--------+  +--------+              |
| | LNAs   |  |  LNA   |  |  VGA   |  |  BPF   |              |
| | -19dB  |  | -14dB  |  |  -9dB  |  |7.25kHz |              |
| +--------+  +--------+  +--------+  +--------+              |
+--------------------------------------------------------------+
```

The **selected register cell is highlighted** (white fill, inverted). Press `MENU` to cycle selection. Press `UP` / `DOWN` to change the value (rotary — wraps around).

**Key controls in STILL mode:**

| Key | Action |
|-----|--------|
| `[MENU]` | Cycle register selection: LNAs → LNA → VGA → BPF → LNAs |
| `[UP]` | Increase selected register value (rotary — wraps at max) |
| `[DOWN]` | Decrease selected register value (rotary — wraps at min) |
| `[EXIT]` | Deselect register (if selected) / return to SPECTRUM scan |
| `[*]` | Increase RSSI trigger level |
| `[F]` | Decrease RSSI trigger level |
| `[3]` | Increase dBMax |
| `[9]` | Decrease dBMin |
| `[0]` | Cycle modulation |
| `[6]` | Cycle listen bandwidth |
| `[SIDE1]` | Toggle monitor mode (force RX regardless of squelch) |
| `[SIDE2]` | Toggle backlight |
| `[5]` | Open direct frequency input |

---

### 4.4 Range Scanning (Advanced)

Used to survey a precise frequency band segment.

**Setup:** Via VFO menu → Scan Range → set Start and Stop frequencies.

```
Example: Start = 146.000 MHz, Stop = 148.000 MHz
-> Spectrum sweeps only that 2 MHz window
-> Finer resolution within the window
-> Display graph is centered and aligned (corrected in v7.6.4br3)
```

**Use cases:**
- Audit a specific band plan segment
- Map all repeater outputs in a local area
- Narrow interference hunt within a specific allocation
- Compare signal strengths across a defined range

---

## 5. STILL MODE — REGISTER INSPECTOR

The register inspector gives **direct hardware control** of the BK4819 radio IC receive gain chain without leaving the spectrum analyzer.

### 5.1 BK4819 Gain Chain Overview

```
Antenna
  |
  v
[ BPF — Band-Pass Filter ]
  |   Controls IF bandwidth — how wide a slice reaches the demodulator
  v
[ LNAs — Low Noise Amp Short ]
  |   First-stage coarse RF preamplification
  v
[ LNA — Low Noise Amplifier ]
  |   Main preamplifier (most impact on sensitivity)
  v
[ Mixer + IF stages ]
  |
  v
[ VGA — Variable Gain Amplifier ]
  |   Post-mixer fine gain adjustment
  v
[ Demodulator -> Audio output ]
```

The AGC (Automatic Gain Control) normally adjusts LNAs, LNA, and VGA automatically. As soon as you modify any REG_13 field (LNAs, LNA, or VGA) via the inspector, **AGC is locked** so your manual value takes permanent effect until you exit STILL mode or change modulation.

### 5.2 Register Definitions and Options

#### LNAs — Low Noise Amplifier Short (Coarse Stage)

Hardware: BK4819 REG_13 bits [9:8]

| Index | dB Display | Gain |
|-------|-----------|------|
| 0 | −19 dB | Minimum (heaviest attenuation) |
| 1 | −16 dB | — |
| 2 | −11 dB | — |
| 3 | 0 dB | Maximum (default, no attenuation) |

Navigation: Rotary over 4 steps. 0 dB → −19 dB (wrap at max); −19 dB → 0 dB (wrap at min).

**Use when:** A very strong nearby signal is overloading the front end — causing distortion or false peaks spreading across the spectrum trace. Reduce LNAs first.

---

#### LNA — Low Noise Amplifier (Main Stage)

Hardware: BK4819 REG_13 bits [7:5]

| Index | dB Display | Gain |
|-------|-----------|------|
| 0 | −24 dB | Minimum |
| 1 | −19 dB | — |
| 2 | −14 dB | — |
| 3 | −9 dB | — |
| 4 | −6 dB | — |
| 5 | −4 dB | — |
| 6 | −2 dB | — |
| 7 | 0 dB | Maximum (default) |

Navigation: Rotary over 8 steps.

**Use when:** Fine-tuning sensitivity. Reduce for strong adjacent signals; increase to pull in weak distant signals. LNA has the most impact on receive noise figure.

---

#### VGA — Variable Gain Amplifier (Post-Mixer)

Hardware: BK4819 REG_13 bits [2:0]

| Index | dB Display | Gain |
|-------|-----------|------|
| 0 | −33 dB | Minimum |
| 1 | −27 dB | — |
| 2 | −21 dB | — |
| 3 | −15 dB | — |
| 4 | −9 dB | — |
| 5 | −6 dB | — |
| 6 | −3 dB | — |
| 7 | 0 dB | Maximum (default) |

Navigation: Rotary over 8 steps.

**Use when:** RSSI reading seems inconsistent with actual signal strength, especially in AM mode. VGA is the last manual gain stage before the demodulator.

---

#### BPF — Band-Pass Filter (IF Bandwidth)

Hardware: BK4819 REG_3D (full 16-bit register, 7 calibrated presets)

| Index | Display | IF Width | Best Use |
|-------|---------|----------|----------|
| 0 | 8.46 kHz | Widest | Very wide FM, broadcasting, high-adjacent-signal environments |
| 1 | 7.25 kHz | Wide | Wide FM (25 kHz channel) |
| 2 | 6.35 kHz | Medium-wide | Standard FM (12.5 kHz channel) |
| 3 | 5.64 kHz | Medium | Default balanced |
| 4 | 5.08 kHz | Medium-narrow | Moderately congested band |
| 5 | 4.62 kHz | Narrow | Interference rejection |
| 6 | 4.23 kHz | Narrowest | Maximum adjacent-channel selectivity |

Navigation: Rotary over 7 presets.

**Implementation note:** BK4819 REG_3D does **not** reliably return the written value on SPI readback. The firmware maintains `bpfSavedIdx` in software as the authoritative state. This index is written to hardware on every change and restored after every `ToggleRX()` call and after `RADIO_SetModulation()` — ensuring the BPF selection survives the chip's internal register resets.

**Use when:**
- Adjacent channel interference → narrower BPF (index 4–6)
- Weak signal being cut off → wider BPF (index 0–2)
- AM or SSB optimum → medium-narrow (index 3–4)

### 5.3 Register Inspector Workflow

```
1. Ensure a signal is present above trigger level
2. Press [PTT] -> enters STILL mode on detected peak
3. Press [MENU] -> LNAs cell is highlighted (white/inverted)
4. Press [UP] or [DOWN] to adjust LNAs
   -> S-meter responds immediately to new gain
5. Press [MENU] -> LNA cell highlighted
6. Press [UP] or [DOWN] to adjust LNA
7. Press [MENU] -> VGA cell highlighted
8. Press [UP] or [DOWN] to adjust VGA
9. Press [MENU] -> BPF cell highlighted
10. Press [UP] or [DOWN] to cycle BPF presets
    -> 8.46 -> 7.25 -> 6.35 -> 5.64 -> 5.08 -> 4.62 -> 4.23 kHz
    -> Wraps around in both directions
11. Press [EXIT] to deselect and return to SPECTRUM scan
    -> AGC lock is cleared on return
```

**Note on rotary navigation:** All four registers wrap around at both ends.  
Example for BPF: pressing UP at index 6 (4.23 kHz) wraps to index 0 (8.46 kHz).  
Pressing DOWN at index 0 (8.46 kHz) wraps to index 6 (4.23 kHz).

---

## 6. KEY MAP — COMPLETE REFERENCE

### SPECTRUM Mode

| Key | Function |
|-----|----------|
| `[* SCAN]` | Start / resume full sweep |
| `[EXIT]` | Exit spectrum analyzer → VFO mode |
| `[UP]` | Step center frequency up |
| `[DOWN]` | Step center frequency down |
| `[0]` | Cycle modulation (FM / USB / LSB / AM) |
| `[3]` | Increase dBMax (raise ceiling) |
| `[9]` | Decrease dBMin (lower floor) |
| `[6]` | Cycle listen bandwidth |
| `[MENU]` | Toggle peak hold on/off (`PH` in status bar) |
| `[SIDE1]` | Blacklist current peak frequency |
| `[SIDE2]` | Toggle backlight |
| `[PTT]` | Lock strongest peak → STILL mode |
| `[5]` (hold) | Toggle spectrum grid display |

### STILL Mode

| Key | Function |
|-----|----------|
| `[MENU]` | Cycle register: LNAs → LNA → VGA → BPF → (back) |
| `[UP]` | Increase selected value (rotary wrap) |
| `[DOWN]` | Decrease selected value (rotary wrap) |
| `[EXIT]` | Deselect / return to SPECTRUM scan |
| `[*]` | Increase RSSI trigger level |
| `[F]` | Decrease RSSI trigger level |
| `[3]` | Increase dBMax |
| `[9]` | Decrease dBMin |
| `[0]` | Cycle modulation |
| `[6]` | Cycle listen bandwidth |
| `[SIDE1]` | Toggle monitor mode (bypass squelch) |
| `[SIDE2]` | Toggle backlight |
| `[5]` | Direct frequency entry |

---

## 7. PRACTICAL ANALYSIS TECHNIQUES

### Technique 1 — Finding Hidden Weak Signals

```
Step 1 — Wide overview
  Step size: 100 kHz (fast full-band sweep)
  [* SCAN] -> watch for any peaks above noise floor

Step 2 — Narrow to region of interest
  [EXIT] -> [UP]/[DOWN] to position over suspect region
  Step size: 12.5 kHz -> [* SCAN] (finer resolution)

Step 3 — Use peak hold
  [MENU] -> enable peak hold
  Even intermittent signals leave a ghost at their frequency

Step 4 — Lock and verify
  [PTT] -> STILL mode on detected peak
  Check S-meter reading and audio
```

---

### Technique 2 — Characterizing Interference

```
Step 1 — Tune near your frequency
  Step size: 2.5–6.25 kHz for fine detail
  [* SCAN] -> look for peaks near your operating frequency

Step 2 — Analyze characteristics
  Peak width -> interference bandwidth
  Steady vs. bursty -> continuous vs. digital protocol
  Waterfall timing -> on/off duty cycle

Step 3 — Identify by shape
  Steady flat carrier  --------   -> Beacon, unmodulated source
  Bursty spikes        | | | |    -> DTMF, data, scanner
  Wideband hump     /--------\   -> Broadband noise or switching PSU

Step 4 — Mitigate with BPF
  [PTT] -> STILL on the interfering frequency
  [MENU] x4 -> select BPF
  [DOWN] -> narrow filter -> does adjacent signal drop on S-meter?
```

---

### Technique 3 — Manual Gain Optimization

```
Overload symptoms (reduce gain):
  Multiple false peaks across band (intermodulation products)
  S-meter stuck at maximum
  Distorted audio despite strong signal
  Fix: STILL -> MENU -> LNAs -> DOWN (reduce coarse gain)
       or LNA -> DOWN (reduce main preamplifier)

Weak signal symptoms (increase gain):
  Peak visible in trace but S-meter barely moves
  Audio cuts in and out at squelch threshold
  Fix: STILL -> MENU -> LNA -> UP
       or VGA -> UP
       Also: [F] to lower trigger threshold
```

---

### Technique 4 — Catching Intermittent Transmissions

```
1. [MENU] in SPECTRUM -> enable Peak Hold (PH in status bar)
2. [F] several times to lower trigger level
3. Run scanner for several minutes unattended
4. Return and look for:
   - Dotted peak hold line at any frequency -> signal was there
   - Waterfall vertical flash -> exactly when it occurred
   - The dotted line decays ~5 seconds after signal ends
5. [PTT] if peak is still fresh -> STILL -> verify frequency
```

---

### Technique 5 — Repeater Discovery

```
1. Step size: 25 kHz (standard repeater channel spacing)
2. Modulation: FM
3. Trigger level: moderate (-100 to -95 dBm)
4. Wide scan: 144–148 MHz (VHF) or 430–440 MHz (UHF)
5. Radio auto-locks each repeater output
6. Note frequency; calculate input:
   VHF: output - 600 kHz = input
   UHF: output - 5000 kHz = input
```

---

## 8. SIGNAL SHAPE GALLERY

### FM Signal (Frequency Modulated)
```
         /\
        /  \
-------/----\-------   Width: 12.5-25 kHz, smooth symmetric hump
```

### AM Signal (Amplitude Modulated)
```
      /\    /\
-----/  \--/  \-------  Width: 8-10 kHz, double sidebands flanking carrier
```

### SSB Signal (Single Sideband)
```
            /\
-----------/  \-------  Width: 3-4 kHz, single asymmetric peak, one side only
```

### CW / Carrier Wave (Morse)
```
           |
-----------|-----------  Width: < 1 kHz, extremely narrow spike
```

### Digital / Data Burst
```
      |||||||||
------||||||||---------  Width: baud-rate dependent, square-topped envelope
```

### Interference / Intermodulation
```
  /\  /\   /\    /\
-/--\/--\-/--\--/--\---  Multiple peaks, no single center, complex pattern
```

---

## 9. ADVANCED CONFIGURATION

### 9.1 DBMax / DBMin — Vertical Scaling

`[3]` increases DBMax (raises ceiling). `[9]` decreases DBMin (lowers floor).

| Goal | Suggested DBMax | Suggested DBMin |
|------|----------------|----------------|
| Find weak signals | −80 dBm | −130 dBm |
| General monitoring | −50 dBm | −130 dBm |
| Urban noise-free | −50 dBm | −100 dBm |
| Signal shape detail | −70 dBm | −120 dBm |

---

### 9.2 Scan Step Size

| Step | Total Span (128 bins) | Use Case |
|------|-----------------------|----------|
| 2.5 kHz | 320 kHz | Precision narrow-band analysis |
| 6.25 kHz | 800 kHz | Fine signal detail |
| 12.5 kHz | 1.6 MHz | Default balanced scan |
| 25 kHz | 3.2 MHz | General VHF/UHF monitoring |
| 50 kHz | 6.4 MHz | Fast band survey |
| 100 kHz | 12.8 MHz | Full-band overview |

---

### 9.3 Listening Bandwidth

`[6]` cycles: Wide (25 kHz) → Standard (12.5 kHz) → Narrow (6.25 kHz) → Narrower.

This controls the IF filter applied **during LISTEN and STILL mode RX**. It is separate from the BPF register inspector setting. Use narrower bandwidth to reduce adjacent channel interference while monitoring a specific signal.

---

### 9.4 Modulation Mode

`[0]` cycles: FM → USB → LSB → AM → FM.

| Mode | Use |
|------|-----|
| FM | Amateur VHF/UHF, PMR, weather, repeaters |
| USB | Digital HF, SSB voice above 10 MHz |
| LSB | SSB voice below 10 MHz |
| AM | Aviation, AM broadcast monitoring |

**Note:** Changing modulation triggers `RADIO_SetModulation()` which resets REG_3D. The firmware immediately reapplies `BPFRegOptions[bpfSavedIdx]` after this write, preserving your BPF selection.

---

### 9.5 RSSI Trigger Level

Controls which signal strength causes the scanner to lock.

- `[*]` in STILL mode → raise threshold (only stronger signals lock)
- `[F]` in STILL mode → lower threshold (weaker signals can lock)

| Environment | Guidance |
|-------------|---------|
| Rural, weak signal hunt | Set low — catch everything above noise floor |
| Suburban general use | Moderate — filter ambient noise |
| Dense urban / high RF | Higher — skip intermittent noise triggers |

---

### 9.6 Blacklist

`[SIDE1]` in SPECTRUM / LISTEN mode adds the current peak to the skip list (up to 15 entries). Blacklisted frequencies are ignored for the rest of the session. Resets on scan restart or radio reboot.

---

### 9.7 Monitor Mode

`[SIDE1]` in **STILL mode** toggles monitor mode. When active, squelch is bypassed and audio is always open on the locked frequency. Useful for listening to signals below the squelch threshold, verifying channel noise, or monitoring while manually adjusting gain registers.

---

## 10. TROUBLESHOOTING GUIDE

### Problem: Spectrum Trace Looks Flat

```
Check 1 - Scan running?
  Waterfall should scroll every sweep.
  If frozen: press [* SCAN].

Check 2 - DBMin too high?
  If noise floor is at -120 dBm but DBMin = -100, nothing shows.
  Fix: press [9] to lower DBMin to -130 dBm.

Check 3 - Trigger too tight blocking display?
  High trigger with weak signals = scanner locks nothing visible.
  Fix: press [F] in STILL to lower trigger.
```

---

### Problem: STILL Mode Shows Wrong dBm (e.g., 32597 dBm)

**Root cause (pre-v7.6.5br3):** `RSSI_MAX_VALUE = 65535U` was used as both a blacklist sentinel and an arithmetic clamp ceiling. Cast to `int16_t`, `65535U` = `−1`, making every valid positive RSSI overflow the comparison and get pinned to the sentinel, producing bogus values.

**Fixed in v7.6.5br3:** Hardware ceiling is now `RSSI_HW_MAX_VALUE = 511` (BK4819 REG_67 bits [8:0]). If you still see values above +50 dBm, you are running firmware older than v7.6.5br3.

---

### Problem: BPF Shows 8.46 kHz and Cannot Be Changed

**Root cause (pre-v7.6.5br3):** `BK4819_ReadRegister(BK4819_REG_3D)` returned inconsistent values because REG_3D is not reliably readable via SPI. The nearest-match algorithm always resolved to index 0.

**Fixed in v7.6.5br3:** BPF index is stored in software (`bpfSavedIdx`). Hardware is written on every change and reapplied after every register-reset event. No hardware readback is used.

---

### Problem: PTT Does Nothing / Cannot Enter STILL Mode

In v7.6.5br3 and later, if `[PTT]` is pressed with no valid peak detected, a **NO DETECTED ACTIVE SIGNAL** overlay appears for ~300 ms. This replaces the silent-fail behavior.

**Fix:** Lower trigger level with `[F]`, wait for a full scan cycle to detect a signal, then press `[PTT]`.

---

### Problem: Arrow Keys Change the Wrong Register (Multiple Values Change at Once)

**Root cause (pre-v7.6.5br3):** `LockAGC()` was called before reading `BK4819_REG_13`. `BK4819_InitAGC()` inside `LockAGC()` wrote `0x03BE` to REG_13, overwriting all three sub-fields before the update was applied.

**Fixed in v7.6.5br3:** REG_13 is read first (pre-lock snapshot), then `LockAGC()` is called, then only the target field is modified within the captured value.

---

### Problem: Waterfall Appears Frozen

```
Check 1 - In LISTEN mode?
  Radio holds frequency when signal detected.
  Press [EXIT] to force-resume scanning.

Check 2 - In STILL mode?
  STILL does not sweep. Waterfall shows only current frequency.
  Press [EXIT] to return to SPECTRUM.

Check 3 - Full restart
  [EXIT] -> [F]+[5] -> re-enter -> [* SCAN]
```

---

### Problem: Peak Hold Line Not Visible

```
Check 1 - Is peak hold enabled?
  [MENU] in SPECTRUM to toggle. "PH" shows in status bar.

Check 2 - Did a signal occur since enabling?
  Peak hold only shows where signal exceeded noise floor.
  Enable first, then let a signal occur.

Check 3 - Has it faded already?
  Peak hold decays to baseline ~5 seconds after signal ends.
  Enable and wait for fresh peak activity.

Check 4 - DBMin set too high?
  If DBMin is high and peak hold line is at the very top pixel,
  it may fall outside the visible area.
  Lower DBMin with [9].
```

---

## 11. REFERENCE TABLES

### Frequency Band Coverage (UV-K1 / UV-K5 V3)

| Band | Range | Typical Step | Notes |
|------|-------|-------------|-------|
| VHF | 136–174 MHz | 25–50 kHz | Aviation, Marine, Amateur 2m |
| UHF | 400–480 MHz | 25–50 kHz | Amateur 70cm, PMR446, Industry |
| Extended | 50–940 MHz | Custom | Subject to regional F-Lock setting |

---

### S-Meter to dBm (Approximate, BK4819 Calibrated)

| S-Unit | dBm | Notes |
|--------|-----|-------|
| S0 | −130 | Noise floor (quiet environment) |
| S1 | −124 | Barely detectable |
| S2 | −118 | Weak |
| S3 | −112 | Readable with difficulty |
| S4 | −106 | Readable |
| S5 | −100 | Good signal |
| S6 | −94 | Strong |
| S7 | −88 | Very strong |
| S8 | −82 | Excellent |
| S9 | −73 | Full quieting |
| S9+10 | −63 | Very loud |
| S9+20 | −53 | Overload region |

---

### BPF Preset Register Values

| Index | Display | REG_3D Value (hex) |
|-------|---------|-------------------|
| 0 | 8.46 kHz | 0x0000 |
| 1 | 7.25 kHz | 0x2AAB |
| 2 | 6.35 kHz | 0x5555 |
| 3 | 5.64 kHz | 0x7FFF |
| 4 | 5.08 kHz | 0xAAA9 |
| 5 | 4.62 kHz | 0xD553 |
| 6 | 4.23 kHz | 0xFFFD |

---

### Common Repeater Offsets (North America)

| Band | Standard Offset | Notes |
|------|----------------|-------|
| 144–148 MHz | +600 kHz | VHF standard |
| 145.1–145.5 MHz | −600 kHz | |
| 420–450 MHz | +5 MHz | UHF standard |
| 449–450 MHz | −5 MHz | |

---

### Recommended BPF by Modulation and Conditions

| Modulation | Good Conditions | Adjacent Interference |
|------------|----------------|----------------------|
| FM 25 kHz | Index 1–2 | Index 3–4 |
| FM 12.5 kHz | Index 2–3 | Index 4–5 |
| AM voice | Index 3–4 | Index 5–6 |
| USB / LSB | Index 4–5 | Index 6 |
| CW / Narrow | Index 5–6 | Index 6 |

---

## QUICK REFERENCE CARD

```
ACTIVATE SPECTRUM ........ [F] + [5]
START SCAN ............... [* SCAN]
EXIT SPECTRUM ............ [EXIT]
CYCLE MODULATION ......... [0]
STEP FREQUENCY ........... [UP] / [DOWN]
ADJUST dBMax ............. [3]
ADJUST dBMin ............. [9]
CYCLE LISTEN BW .......... [6]
TOGGLE PEAK HOLD ......... [MENU]  (SPECTRUM mode; "PH" in status bar)
BLACKLIST FREQUENCY ...... [SIDE1] (SPECTRUM / LISTEN)
TOGGLE BACKLIGHT ......... [SIDE2]
PTT -> STILL MODE ........ [PTT]   (requires signal above trigger)

IN STILL MODE:
  SELECT REGISTER ........ [MENU]  (LNAs -> LNA -> VGA -> BPF -> loop)
  ADJUST VALUE ........... [UP] / [DOWN]  (rotary, wraps at both ends)
  DESELECT / EXIT ........ [EXIT]
  TOGGLE MONITOR MODE .... [SIDE1]
  TRIGGER LEVEL UP ....... [*]
  TRIGGER LEVEL DOWN ..... [F]
  DIRECT FREQUENCY ....... [5]
```

---

**Document:** SPECTRUM_ANALYZER_GUIDE.md  
**Firmware:** ApeX Edition v7.6.5br3  
**Build:** n7six.ApeX-k1.v7.6.5br3 (UV-K1 / UV-K5 V3, PY32F071)  
**Author:** N7SIX, Sean  
**License:** Apache License, Version 2.0  
**Last Updated:** April 5, 2026

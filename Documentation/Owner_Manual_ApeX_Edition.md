# UV-K1 SERIES / UV-K5 V3 TRANSCEIVER
## ApeX Edition Professional Owner's Manual

---

### TABLE OF CONTENTS

1. [Safety and Regulatory Information](#safety--regulatory-information)
2. [Getting Started](#getting-started)
3. [Front Panel Controls](#front-panel-controls)
4. [Professional Spectrum Analyzer](#professional-spectrum-analyzer)
5. [Operating Modes](#operating-modes)
6. [Menu System Reference](#menu-system-reference)
7. [Advanced Features](#advanced-features)
8. [Troubleshooting](#troubleshooting)
9. [Technical Specifications](#technical-specifications)

---

## SAFETY & REGULATORY INFORMATION

### ⚠️ CRITICAL SAFETY WARNING

**THIS FIRMWARE HAS NO WARRANTY.** Use entirely at your own risk. This custom firmware may:
- Brick your radio if any error occurs during flashing
- Void the manufacturer's warranty
- Cause unexpected behavior

**You are solely responsible for:**
- Ensuring proper frequency usage in your jurisdiction
- Compliance with local RF regulations
- Backing up your original calibration data before flashing

### Before First Use

1. **Back up your calibration data** using uvtools2 immediately after flashing
2. **Verify correct frequency bands** for your region
3. **Test transmission power limits** before extended use
4. **Know your local frequency regulations**

---

## GETTING STARTED

### Initial Setup

1. **Power On**: Press [PWR] to toggle radio on/off
2. **Adjust Volume**: Use UP/DOWN keys on keypad
3. **Select Frequency**: Use UP/DOWN arrow keys to tune
4. **Monitor Signal**: Watch S-meter on status bar for signal strength
5. **Transmit Safety**: Press [PTT] (Push-To-Talk) to transmit
   - **Release immediately** to stop transmission
   - Obey all frequency regulations in your area

### Battery Management

- **Battery Status** visible in status bar (top right)
- **Low Battery Alert**: Buzzer sounds at ~3.0V
- **Recommended Action**: Replace battery immediately at <2.8V
- **Calibration**: Available in Menu → SysInf → Battery settings

### Display Modes

Press **[F] + [UP]** or **[F] + [DOWN]** to cycle through display layouts:
- **DUAL MODE**: Main VFO + secondary channel (split screen)
- **CROSS MODE**: Dual VFO monitoring side-by-side
- **MAIN ONLY**: Single large frequency display (recommended for spectrum analysis)

---

## FRONT PANEL CONTROLS

| Button | Function | Long Press |
|--------|----------|------------|
| [PWR] | Power on/off | (none) |
| [UP]/[DOWN] | Frequency tuning | Fast scroll |
| [F] | Function modifier | (depends on context) |
| [PTT] | Transmit (activated) | (transmit lock) |
| [MENU] | Open settings menu | Scan list assignment |
| [SIDE1] | Custom function | Blacklist frequency |
| [SIDE2] | Custom function | Screen invert toggle |
| [*] SCAN | Start frequency scan | Resume scan list |
| [#] | Enter memory mode | (context dependent) |
| [0-9] | DTMF digits / freq input | (long press varies) |
| [EXIT] | Exit menu / cancel | (context dependent) |

---

## PROFESSIONAL SPECTRUM ANALYZER

### Accessing the Spectrum Analyzer

**Press [F] + [5]** to activate the professional bandscope mode.

### Main Display Elements

```
┌─────────────────────────────────────┐
│  F: 145.500  USB  12.5k             │  ← Frequency, Modulation, BW
├─────────────────────────────────────┤
│        ╱╲                           │
│       ╱  ╲     Real-time spectrum   │
│      ╱    ╲    trace (blue line)    │
│ ────╱──────╲──────────────────       │  ← S-meter scale (0-S9)
├─────────────────────────────────────┤
│        Waterfall history (16 rows)  │  ← Temporal signal data
│  ││││││││││││││││││││││││││││││││   │
│  ││    ●●●●● Signal appears here   │
│  ││││││││││││││││││││││││││││││││   │
├─────────────────────────────────────┤
│ Step: 25kHz  Rssi Trig: -95dBm      │  ← Configuration
└─────────────────────────────────────┘
```

### Display Components Explained

#### **Spectrum Trace (Top)**
- **Blue solid line**: Real-time RF amplitude at each frequency
- **Dashed line above**: Peak hold (maximum signal memory)
- **Noise floor**: Bottom of display shows ambient RF noise
- **Smooth curves**: Professional 3-point interpolation for visual clarity

#### **Waterfall Display (Center)**
- **16 rows** of frequency history
- **Top row**: Newest measurements
- **Bottom row**: Oldest measurements
- **Grayscale shading**: Signal strength (black=none, white=strong)
- **Bayer dithering**: Professional 4×4 pattern for monochrome displays
- **Auto-scrolling**: New data appears every frame during scanning

#### **Peak Hold Feature**
- **Dashed horizontal line** traces the maximum signal at each frequency
- **Exponential decay** makes peaks gradually fade (natural "ghost" effect)
- **Purpose**: Identify intermittent or brief signal bursts
- **Reset**: Automatically clears when starting a new scan

### Scanning Modes

#### **Active Scan (Continuous)**
1. Press **[* SCAN]** to start
2. Radio tunes through frequency range automatically
3. **Scan Parameters** (set via Menu):
   - **Scan Step**: 25kHz, 50kHz, 100kHz (configurable)
   - **Rssi Trigger Level**: Detection threshold for signal (-130 to -30 dBm)
   - **Scan Range**: Define start/stop frequencies
4. **During Scan**:
   - Spectrum updates smoothly
   - Waterfall scrolls continuously
   - Peak hold traces signal maxima
5. **Stop Scan**: Press **[EXIT]** or **[MENU]**

#### **Listen Mode (Signal Detected)**
When the radio detects a signal above the Rssi Trigger Level:
1. **Automatic Frequency Lock** on detected signal
2. **Spectrum freezes** at current acquisition time
3. **Waterfall continues scrolling** showing signal dynamics
4. **Peak hold remains active** for continuity
5. **Press []** to return to scanning

### Frequency Resolution

| Step Size | Use Case | Notes |
|-----------|----------|-------|
| 25 kHz | Broadcast (VHF/UHF) | Standard commercial bands |
| 50 kHz | Medium detail | Balanced resolution |
| 100 kHz | Wide surveys | Quick band overview |
| Custom | Professional analysis | User-defined precision |

### S-Meter (Signal Strength Indicator)

The S-meter follows **IARU Region 1 Technical Recommendation R.1**:

```
S0  S1  S2  S3  S4  S5  S6  S7  S8  S9  +10  +20
│───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼────┼─── [dBm]
-130                                        -50
```

- **S0-S3**: Noise/weak signals
- **S4-S6**: Good signal strength
- **S7-S9**: Very strong signals
- **+10 to +20**: Signal overload (reduce RF gain if applicable)

### Professional Analysis Features

#### **STLA (Scan-based Trigger Level Auto-adjustment)**
- Automatically adjusts squelch threshold based on band noise floor
- Prevents false triggers from background noise
- Prevents missed signals during congested bands

#### **Dynamic Range**
- **16-bit RSSI processing**: Professional-grade signal acquisition
- **Logarithmic scaling**: dBm conversion for accuracy
- **Exponential Moving Average**: Smooth trace without aliasing

#### **Noise Floor Measurement**
- Displayed as **baseline of spectrum** (typically -120 to -100 dBm at rest)
- Actual ADC thermal noise visible as
 slight variation at low signal levels
- More active on VHF (higher noise floors) than UHF

---

## OPERATING MODES

### VFO (Variable Frequency Oscillator) Mode

**Enter by pressing [F] repeatedly or directly via frequency input**

1. **Frequency Tuning**:
   - **[UP]/[DOWN]**: Change frequency by step size
   - **Long [UP]/[DOWN]**: Rapid tuning (hold down)
   - **Enter digits**: Direct frequency input (0-9 keys)

2. **Step Size Control** ([F] + [F1]):
   - 2.5 kHz (narrow)
   - 5 kHz
   - 6.25 kHz
   - **12.5 kHz** (default)
   - 25 kHz (coarse)
   - Custom value (menu)

3. **Modulation Selection** (Press [0]):
   - **FM** (narrow/wide)
   - **AM**
   - **USB**
   - **LSB**
   - **CW**

4. **Bandwidth Control** (Press [6]):
   - **12.5 kHz** (narrow), ≈-50dB BW: 10 kHz
   - **25 kHz** (wide), ≈-50dB BW: 20 kHz
   - **Auto**: System selects based on modulation

### Memory Channel Mode

**Press [#] or [F] + [*] to enter**

1. **Channel List Navigation**:
   - **[UP]/[DOWN]**: Scroll through channels
   - **Enter number**: Jump to specific channel

2. **Channel Information Display**:
   - **Name**: Up to 6 character alphanumeric
   - **Frequency**: Display precision matches VFO
   - **Modulation/BW**: Stored per-channel
   - **CTCSS/DCS**: Tone encode/decode settings

3. **Channel Management** (Menu):
   - **Create channel**: Frequency input + name entry
   - **Delete channel**: Select channel, press [EXIT] (confirm)
   - **Edit name**: Select channel, press [MENU]

### Scan Mode

**Activate via [* SCAN]**

1. **Scan Lists Available**:
   - **List 0**: All channels without list assignment
   - **List 1, 2, 3**: Specific channel groups
   - **Lists [1,2,3] combined**: Multiple lists simultaneously
   - **Scan all**: Every stored channel

2. **Scan Controls**:
   - **[UP]/[DOWN]**: Manual frequency advance/retreat within scan
   - **[SIDE1]**: **Blacklist** current frequency (skip in future scans)
   - **Long [MENU]**: Temporarily exclude channel
   - **Short [0-5]**: Change active scan list dynamically

3. **Squelch-Based Resumption**:
   - When signal detected → **Radio holds frequency** (Listen mode)
   - **Hang timer**: Configurable delay (0-10s) after signal ends
   - Auto-resume scanning when silence threshold met

---

## MENU SYSTEM REFERENCE

### Accessing the Main Menu

**Press [MENU]** to open settings. Use **[UP]/[DOWN]** to navigate, **[F]** to edit, **[EXIT]** to return.

### Core Settings (Alphabetical Quick Reference)

| Menu # | Setting | Options | Purpose |
|--------|---------|---------|---------|
| 01 | **FLck** | OFF / ON | Lock modulation to selected band |
| 02 | **TxPow** | 5W / 2W / 1W / 500mW / 250mW / 125mW / <20mW | Set transmission power |
| 03 | **RxExp** | OFF / ON | RX extension (disable to reduce battery drain) |
| 04 | **Modulation** | FM / AM / USB / LSB / CW | RX demodulation mode |
| 05 | **BandWdth** | 12.5kHz / 25kHz / Auto | Receiver bandwidth |
| 06 | **Squelch** | 0-9 | Detection threshold (0=off) |
| 07 | **ChName** | Input field | Channel name (6 chars) |
| 08 | **CTCSS** | (1-50 available) | Encode/decode tone |
| 09 | **DCS** | (23-777 available) | Digital code squelch |
| 10 | **OffSet** | ±10.00 MHz (VFO offset) | Repeater shift magnitude |
| 11 | **Step** | 2.5kHz - Custom | Frequency step size |
| 12 | **BatTxt** | Voltage / Percentage | Battery display format |
| 13 | **BatSave** | 1600mAh / 2200mAh / 3500mAh | Battery model selection |
| 14 | **BLTime** | 10s - 2m | Backlight auto-off delay |
| 15 | **BLTxRx** | OFF / TX / RX / Both | TX/RX backlight activation |
| 16 | **BLMin** | 0-15 | Minimum backlight level |
| 17 | **BLMax** | 0-15 | Maximum backlight level |
| 18 | **BLMod** | Always / Auto | Backlight mode |
| 19 | **Mic** | 0-15 | Microphone gain level |
| 20 | **Compand** | OFF / ON | Audio companding (improves clarity) |
| 21 | **Scrambler** | OFF / ON | Voice scrambling |
| 22 | **DBMax** | -50 to -80 dBm | Spectrum display top (max amplitude) |
| 23 | **DBMin** | -130 to -100 dBm | Spectrum display bottom (noise floor) |
| 24 | **BusyLed** | OFF / RED / Blue / Purple | LED busy indicator |
| 25 | **VOX** | OFF / 1-10 | Voice-activated transmit sensitivity |
| 26 | **VOXDly** | 100ms - 5s | VOX hang time before release |
| 27 | **AirCopySetting** | (varies) | Over-the-air programming options |
| 28 | **SysInf** | (read-only) | Firmware version & battery status |

### Advanced Settings

#### **Spectrum Analyzer Parameters**
- **DBMax** [-50 to -80]: Adjust top of spectrum display scale
  - Higher = "zoomed in" on weaker signals
  - Lower = "zoomed out" (shows wider dynamic range)
- **DBMin** [-130 to -100]: Adjust noise floor reference
  - Lower = more sensitive display
  - Higher = less sensitive but cleaner appearance

#### **Squelch Control** ([F] + [UP]/[DOWN] in VFO)
- **Dynamic squelch adjustment** without entering menu
- **Prevents false triggers** from band noise
- **Quick response** for operational efficiency

#### **TX/RX Timers**
- **TOT (Time Out Timer)**: Max transmission duration before auto-cutoff
- **Monitor Duration**: Automatic listen timeout
- Enable/disable via Menu → SetTmr

#### **Power Management**
- **SetOff** [1min - 2hr]: Deep sleep delay
- **Auto-powerdown** after selected idle period
- **Wakeup**: Press any key to resume

---

## ADVANCED FEATURES

### Professional Spectrum Analysis Techniques

#### **Signal Detection Methodology**
1. **Identify noise floor** (baseline at bottom of spectrum)
2. **Look for peaks** above noise floor
3. **Use peak hold** (dashed line) to trace signal envelope
4. **Watch waterfall** for signal modulation patterns
5. **Note frequency stability** via peak consistency

#### **Finding Weak Signals**
1. Increase **DBMax** range (set closer to -80 dBm)
2. Reduce **DBMin** to increase sensitivity display
3. Watch for **tiny peaks** above noise
4. Monitor **waterfall patterns** for intermittent signals
5. Reduce **step size** for finer detail

#### **Identifying Interference**
1. **Wide bandwidth increases**: Look for broad spectral spread
2. **Frequency hopping**: Watch waterfall for jumping patterns
3. **Modulation analysis**: Compare trace shape vs known signals
4. **Power measurement**: Peak height indicates relative signal strength

### Air Copy (Over-The-Air Channel Programming)

**Prerequisite**: Radio configured with Air Copy enabled in menu

1. **Receive**: Transmitter sends channel data (proprietary format)
2. **Capture**: Your radio stores frequency/settings locally
3. **Verify**: Confirm reception in Air Copy menu
4. **Apply**: Settings merged into current programming

### Custom Key Programming

Assign functions to [SIDE1] / [SIDE2] / long[MENU]:
- **RX MODE**: Force receiver-only (disable TX)
- **MUTE**: Audio off/on toggle
- **WIDE/NARROW**: BW switching shortcut
- **POWER HIGH**: Maximize TX power (RescueOps edition)
- **1750Hz TONE**: Repeater access tone

---

## TROUBLESHOOTING

### Common Issues & Solutions

| Problem | Cause | Solution |
|---------|-------|----------|
| **No signal detected** | Wrong frequency / wrong band | Verify frequency is in user's band (See Menu→FLck) |
| **Spectrum analyzer frozen** | Listen mode active | Press [EXIT] to resume scanning; check signal |
| **Waterfall not scrolling** | Scan paused | Press [* SCAN] to resume active scanning |
| **S-meter not moving** | Squelch too high | Reduce squelch value (Menu→Squelch) |
| **Battery depletes quickly** | High TX power / backlight always on | Set lower power (Menu→TxPow); disable BLTxRx |
| **Audio distorted** | Mic gain too high | Reduce mic level (Menu→Mic, reduce 0-15 value) |
| **Cannot transmit** | PTT locked or TX disabled | Check Menu→KeyLck; verify FLck band | 
| **Spectrum display too dark** | DBMax/DBMin poorly set | Reset: DBMax=-50, DBMin=-130 |
| **Peak hold not appearing** | Weak signal below threshold | Look for stronger signals or reduce DBMin |

### Factory Reset

**If radio becomes unresponsive:**

1. Power off [PWR]
2. Hold [MENU] + [EXIT] while powering on
3. Release buttons when menu appears
4. Confirm reset (lose all custom channels)

### Calibration Data Recovery

If calibration was lost after flashing:
1. Use **uvtools2** with previously backed-up calibration file
2. Or perform calibration in Menu → Calibration section
3. Verify power levels match radio specifications

---

## TECHNICAL SPECIFICATIONS

### Radio Performance (UV-K1/UV-K5 V3)

**Frequency Coverage**:
- **VHF**: 136-174 MHz
- **UHF**: 400-480 MHz
- **Other bands**: Depends on firmware F-Lock settings

**TX Power Output**:
- **&lt;20mW**: Minimum power mode
- **125mW**: Low-power setting
- **250mW**: Medium-low
- **500mW**: Medium
- **1W**: Medium-high
- **2W**: High
- **5W**: Maximum (rated)

**RX Sensitivity**:
- **VHF**: &lt;0.5µV (-130 dBm) @ 12.5kHz BW
- **UHF**: &lt;0.5µV (-130 dBm) @ 12.5kHz BW

**Spectrum Analyzer (Bandscope)**:
- **Frequency range**: Full radio band coverage
- **Dynamic range**: -130 to -50 dBm (80 dB)
- **Resolution**: 10 Hz internal, < 50 kHz display
- **Update rate**: 60 Hz (≈16ms per frame)
- **S-meter compliance**: IARU R.1 standard

**Waterfall Display**:
- **Rows**: 16 history lines
- **Columns**: 128 frequency bins
- **Color depth**: 16-level grayscale (Bayer dithering)
- **Refresh**: Continuous per-frame scrolling

### Battery Specifications

| Model | Capacity | Runtime | Note |
|-------|----------|---------|------|
| Stock | 1600 mAh | ~8-10 hrs | Original OEM |
| Extended | 2200 mAh | ~12-14 hrs | Drop-in replacement |
| High-cap | 3500 mAh | ~20-24 hrs | Larger form factor |

**Voltage**: 7.4V DC (Li-ion 2-cell nominal)
**Low Voltage**: &lt;3.0V triggers replacement alert

### Display

- **Type**: Monochrome LCD (ST7565 compatible)
- **Resolution**: 128 × 64 pixels
- **Refresh Rate**: 60 Hz (NTSC) / 50 Hz (PAL)
- **Backlight**: LED with 16-level adjustable brightness

### Audio Features

- **Audio codec**: PY32F071 internal DSP
- **Noise reduction**: SSB demodulation, companding
- **Output power**: &lt;500mW @ 8Ω speaker
- **Modulation modes**: FM, AM, USB, LSB, CW

---

## WARRANTY & LIABILITY

This firmware is provided **AS-IS** without warranty. The authors make no claims regarding:
- Fitness for any particular purpose
- Compatibility with future hardware revisions
- Compliance with local frequency regulations
- Data integrity or frequency preset preservation

**Users assume full responsibility** for lawful operation, radio safety, and backup of critical data.

---

## SUPPORT & RESOURCES

### Community Support
- **GitHub**: [https://github.com/QS-UV-K1Series](https://github.com/QS-UV-K1Series)
- **Discussion Pages**: Check the project wiki for FAQs
- **Issue Reporting**: Report bugs with detailed reproduction steps

### Related Tools
- **uvtools2**: Backup/restore calibration
- **CHIRP Driver**: UV-K5 custom driver for programming
- **Firmware Builds**: Docker-based compilation available

### Recommended References
- IARU Region 1 Recommendation R.1 (S-meter standardization)
- Your local radio frequency regulation authority
- Amateur radio licensing requirements for your country

---

## REVISION HISTORY

| Version | Date | Changes |
|---------|------|---------|
| 7.6.0 | 2026-02-28 | Initial professional manual release (ApeX Edition) |

---

**Manual prepared for the UV-K1 Series / UV-K5 V3 ApeX Edition**  
**Firmware Version 7.6.0**  
**Document Date: February 28, 2026**

**© N7SIX, F4HWN, Egzumer, DualTachyon - Licensed under Apache 2.0**

---

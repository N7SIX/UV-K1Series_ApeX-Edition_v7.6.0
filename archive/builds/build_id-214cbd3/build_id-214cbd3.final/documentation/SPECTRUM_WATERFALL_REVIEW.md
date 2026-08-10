# Comprehensive Technical Review: spectrum.c & waterfall.c

> **Project:** UV-K1Series ApeX-Edition v7.6.9D  
> **Platform:** PY32F071 (ARM Cortex-M0+) + BK4819 DSP Transceiver  
> **Display:** ST7565 128x64 LCD  
> **Date:** 2026-06-26  
> **Reviewed By:** Sean, N7SIX

---

## Table of Contents

1. [Architecture Overview](#1-architecture-overview)
2. [spectrum.c – Deep Dive](#2-spectrumc--deep-dive)
3. [waterfall.c – Deep Dive](#3-waterfallc--deep-dive)
4. [Integration Points](#4-integration-points)
5. [State Machine Analysis](#5-state-machine-analysis)
6. [Key Algorithms & Design Decisions](#6-key-algorithms--design-decisions)
7. [Performance & Optimization Notes](#7-performance--optimization-notes)
8. [Code Quality Observations](#8-code-quality-observations)
9. [Potential Issues & Recommendations](#9-potential-issues--recommendations)

---

## 1. Architecture Overview

### 1.1 Layered Component Model

```
┌─────────────────────────────────────────────────────────────────────┐
│                    APP_RunSpectrum() Main Loop (Tick)               │
├─────────────────────────────────────────────────────────────────────┤
│  ┌─────────────────────┐  ┌─────────────────┐  ┌────────────────┐  │
│  │   Scan Engine        │  │  Listen Mode    │  │  STILL Mode   │  │
│  │  (UpdateScan)        │  │ (UpdateListening)│  │ (UpdateStill) │  │
│  └──────┬──────────────┘  └────────┬────────┘  └───────┬────────┘  │
│         │                          │                    │           │
│  ┌──────┴──────────────────────────┴────────────────────┴────────┐ │
│  │                   Render Pipeline                              │ │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────────┐  │ │
│  │  │ TopY     │→ │Spectrum  │→ │Waterfall │→ │ST7565_Blit   │  │ │
│  │  │Builder   │  │Curve     │  │Render    │  │              │  │ │
│  │  └──────────┘  └──────────┘  └──────────┘  └──────────────┘  │ │
│  └────────────────────────────────────────────────────────────────┘ │
├─────────────────────────────────────────────────────────────────────┤
│                    BK4819 Radio Driver Layer                          │
│  (REG read/write, RSSI sampling, frequency tuning, filter control)   │
└─────────────────────────────────────────────────────────────────────┘
```

### 1.2 Key File Facts

| Aspect | spectrum.c | waterfall.c |
|--------|-----------|-------------|
| Lines | 2,681 | 226 |
| Author | fagci (original), N7SIX, F4HWN (extensions) | Sean, N7SIX |
| Purpose | Full-duplex spectrum analyzer app | 16-level grayscale waterfall display |
| Entry | `APP_RunSpectrum()` | Called from spectrum.c `Render()` |
| Compile Guard | `#ifdef ENABLE_SPECTRUM` | None (always compiled when included) |
| License | Apache 2.0 | Implied (no explicit header) |

### 1.3 Feature Gates

Preprocessor flags controlling behavior:

| Flag | Effect |
|------|--------|
| `ENABLE_SPECTRUM` | Enables spectrum entry from main menu |
| `ENABLE_FEAT_N7SIX` | N7SIX UI customizations (PutPixel, font, etc.) |
| `ENABLE_FEAT_N7SIX_SPECTRUM` | Extended features: LNA/PGA controls, settings persistence, CSS tail detection |
| `ENABLE_SCAN_RANGES` | Scan range subsystem (chFrScanner integration) |
| `ENABLE_AM_FIX` | AM mode RSSI correction |
| `ENABLE_FEAT_N7SIX_SCREENSHOT` | Serial screenshot capability |
| `ENABLE_FEAT_N7SIX_RESUME_STATE` | State persistence across power cycles |
| `SPECTRUM_INTERLACE_LARGE_SWEEPS` | Interlaced scanning for >128 step sweeps |
| `SPECTRUM_AUTOMATIC_SQUELCH` | Auto-squelch recalibration on restart |

---

## 2. spectrum.c – Deep Dive

### 2.1 Global State Structure

#### 2.1.1 Core State Variables

```c
State currentState = SPECTRUM, previousState = SPECTRUM;
// Three-state machine:
//   SPECTRUM  - Active scanning/sweeping
//   STILL     - Tuned to a peak (RX on, single frequency)
//   FREQ_INPUT - Manual frequency entry via keypad
```

#### 2.1.2 Major Configuration: `SpectrumSettings`

```c
SpectrumSettings settings = {
    .stepsCount = STEPS_64,          // Number of sweep steps (16/32/64/128)
    .scanStepIndex = S_STEP_25_0kHz, // Frequency step size per measurement
    .frequencyChangeStep = 80000,    // MHz/10 step when arrow keys pressed
    .scanDelay = 3200,               // Microseconds between measurements
    .rssiTriggerLevel = 150,         // Squelch open threshold (RSSI units)
    .backlightState = true,
    .bw = BK4819_FILTER_BW_WIDE,
    .listenBw = BK4819_FILTER_BW_WIDE,
    .modulationType = false,         // FM
    .dbMin = -130,                   // Display dBm floor
    .dbMax = -50                     // Display dBm ceiling
};
```

#### 2.1.3 Peak Hold System

```c
static uint8_t peakHoldY[128];    // One peak Y per column
static uint8_t peakHoldAge[64];   // Shared decay timer (1 per 2 columns)
#define PEAK_HOLD_DELAY  15       // Sweeps before decay begins
#define PEAK_HOLD_INIT   0xFF     // "No peak" sentinel
```

#### 2.1.4 Scan Info

```c
ScanInfo scanInfo;  // Tracks: rssi, rssiMin, rssiMax, i, iPeak, f, fPeak
```

#### 2.1.5 Peak Info

```c
PeakInfo peak;      // Tracks: t (age), rssi, f, i
```

### 2.2 Entry Point: `APP_RunSpectrum()`

**Location:** Line 2613

**Flow:**
1. Save backlight state from EEPROM
2. Determine VFO index from TX VFO
3. Load persisted settings (if `ENABLE_FEAT_N7SIX_SPECTRUM`)
4. Calculate `currentFreq` centered on VFO frequency, or use `gScanRangeStart`
5. Backup BK4819 radio registers via `BackupRegisters()`
6. Force RX toggle (hack to prevent noise)
7. Set modulation and filter bandwidth
8. Initialize waterfall buffer via `WATERFALL_Init()`
9. Call `RearmRuntimeState()` to reset all dynamic state
10. Enter main loop: `while (isInitialized) { Tick(); }`

**Exit:** `DeInitSpectrum()` restores original frequency and BK4819 registers, then turns backlight on.

### 2.3 The Main Loop: `Tick()`

**Location:** Line 2526

Called continuously; each call is one "tick":

```
Tick()
├── SCREENSHOT_ParseInput()         // If enabled
├── [10ms timeslice] AM_fix + Backlight
├── [500ms timeslice] Forced redraw for large scans
├── HandleUserInput()               // If not preventKeypress
├── InitScanPosition()              // If newScanStart is pending
├── UpdateListening()               // If isListening && not FREQ_INPUT
│   └── OR UpdateScan()             // If SPECTRUM state
│   └── OR UpdateStill()            // If STILL state
├── RenderStatus()                  // If redrawStatus || every 4096 ticks
├── Render()                        // If redrawScreen || every RENDER_PERIOD_TICKS (20)
└── ST7565_BlitLine(renderPage)    // Incremental blit (1 page/tick)
    └── renderPage cycles through FRAME_LINES
```

### 2.4 Scan Engine

#### 2.4.1 Sweep Direction

The sweep is **bidirectional**:
- `scanStartFromLeft` alternates each full cycle to avoid directional bias
- A full "round trip" = forward half-sweep + return half-sweep
- `scanReturnPending` tracks whether the opposite half is still due

```c
// Forward: i=0..N-1, backward: i=N-1..0
InitScanPosition() sets direction based on scanStartFromLeft
```

#### 2.4.2 Measurement Step: `SetFScan()`

**Optimized** frequency setter that:
- Caches BK4819_REG_30 value (saves 1 SPI read per step)
- Only refreshes RF path when crossing VHF/UHF boundary (280 MHz)
- Reduces SPI transactions from ~7 to 4 per step — key for audio interference reduction

#### 2.4.3 Interlaced Sweeps for Large Scans

When `ENABLE_SCAN_RANGES` produces >128 steps, `SPECTRUM_INTERLACE_LARGE_SWEEPS` kicks in:

```c
interlaceStride = (measurementsCount + 127) / 128;
```

The sweep covers every `interlaceStride`-th sample, then fills in phases. This means:
- The display updates progressively (not just the first 128 samples)
- `rssiHistory[]` only holds 128 slots regardless of total scan span
- Resolution is sacrificed for coverage on first pass, then filled in

#### 2.4.4 RSSI Measurement

```c
GetRssi():
  1. Guard loop: wait until BK4819_REG_63 < 200 (glitch settling)
  2. Discard first RSSI read (AGC transitioning)
  3. Return second RSSI read
```

RSSI is stored in `rssiHistory[128]` with:
- **Attack: instant** (new value overwrites immediately)
- **Decay: halve the gap each sweep** (smoothing for falling signals)

#### 2.4.5 Auto Trigger Level

```c
AutoTriggerLevel():
  - Tracks noise floor via EMA: floor = (3*floor + new_min + 2) >> 2
  - Target = noiseFloor + margin (weak=24, normal=16, strong=10 RSSI units)
  - Adaptive slew rate: ±4 for diff>12, ±2 for diff>6, ±1 for diff>4, dead-zone ±4
  - Recalibrates if manualSetFlag is false
```

Three sensitivity profiles: WEAK (+12 dB), NORM (+8 dB), STRONG (+5 dB).

#### 2.4.6 Peak Detection

- `UpdatePeakInfo()` updates on sweep end
- `UpdatePeakInfoForce()` forces update when current peak is stale or weaker
- Peak carries age counter (`peak.t`) — if >1024 ticks, forced refresh

#### 2.4.7 Listen Mode (Squelch Open)

When sweep finds a signal above trigger:

```
  UpdateListening():
  ├── [Phase 1] listenT countdown: 1ms delay per tick, no SPI (silence period)
  ├── [Phase 2] SPI burst: single burst every listenT=320ms ~3 Hz
  │   ├── Check tail interrupt (CSS tail squelch close)
  │   ├── Measure RSSI (fast BW switch: scanBW→measure→listenBW)
  │   └── Push listen row to waterfall
  ├── [Phase 3] Debounce:
  │   ├── Abrupt drop detect (>10 dB drop = quick PTT release)
  │   └── Gradual decay: requires 4 consecutive below-threshold reads
  └── [Exit] ToggleRX(false), reset stats, resume scan
```

### 2.5 Rendering Pipeline

#### 2.5.1 Curve Construction: `BuildSpectrumTopY()`

1. **Interpolation:** Maps `N` RSSI samples to 128 display columns using Q8 fixed-point interpolation (`InterpolateRssi()`)
2. **Invalid handling:** Blacklisted `RSSI_MAX_VALUE` samples become `SPECTRUM_TOPY_SKIP (0xFF)` — columns are not drawn
3. **Smoothing:** `SmoothTopY()` applies 3-bin moving average (skipped in manual mode for visual accuracy)

#### 2.5.2 Curves: Two-layer Spectrum Display

**Live curve** (solid crest + checkerboard body):
- Crest: solid pixels at the signal top
- Body: checkerboard pattern (every other pixel) below crest
- Half-step bridging (`CalcCrest`) smooths the visual slope between adjacent columns

**Peak hold** (dotted crest):
- Matches the live crest shape from `peakHoldY[]`
- Decay: stays for `PEAK_HOLD_DELAY` (15 sweeps), then descends 2 pixels per sweep
- Dotted rendering (checkerboard pattern) distinguishes from live curve

#### 2.5.3 Status Line: `DrawStatus()`

- **Manual mode:** "M -87/-92" (current dBm / trigger level)
- **Auto mode:** "A:STRG <" (sensitivity profile + sweep direction arrow)
- Battery icon: pixel-level drawing in `gStatusLine[116-127]`

#### 2.5.4 Frequency Display: `DrawF()`

- Frequency in MHz: `"440.12345"`
- Modulation string: FM/AM/USB (from `gModulationStr[]`)
- Bandwidth: "25k"/"12.5k"/"6.25k"
- Channel name lookup (when listening, in status bar area)

#### 2.5.5 Arrow at Peak Position

`DrawArrow()` renders a 3-pixel-tall triangular cursor at the current peak horizontal position.

#### 2.5.6 Trigger Level Line

`DrawRssiTriggerLevel()` draws a horizontal dotted line at the squelch threshold, skipping over the spectrum curve and scan step text.

### 2.6 Input Handling

#### 2.6.1 Key Map

| Key | Function |
|-----|----------|
| UP/DOWN | Move frequency band / resume sweep in direction |
| 1/7 | Change scan step (finer/coarser) |
| 2/8 | Change frequency change step |
| 3/9 | Manual: adjust dB Max / Auto: change sensitivity profile |
| 0 | Toggle modulation |
| 4 | Toggle steps count (128→64→32→16) |
| 5 | Frequency input mode |
| 6 | Toggle listening bandwidth |
| SIDE1 | Blacklist current peak |
| SIDE2 | Toggle backlight |
| PTT | Enter STILL mode, tune to peak |
| MENU (short) | Toggle manual/auto trigger |
| MENU (long) | Reset spectrum to defaults |
| */F | Adjust RSSI trigger level |
| EXIT | Exit spectrum, save settings |

#### 2.6.2 Debouncing and Repeat

```c
// counter up to 16, then auto-repeat at -3 per tick
if (kbd.counter < 16) kbd.counter++;
else kbd.counter -= 3;
// Action triggered at counter == 3 (first repeat) and counter == 16 (auto-repeat)
```

MENU key has special long-press detection:
- counter==3: mark short press pending
- counter==16: treat as long press (reset defaults)

### 2.7 Settings Persistence (`ENABLE_FEAT_N7SIX_SPECTRUM`)

**Storage:** External SPI flash (PY25Q16) at address 0x00A148, 8 bytes:

| Byte | Bits | Field |
|------|------|-------|
| 0 | 7:4 | scanStepIndex |
| 0 | 3:2 | stepsCount |
| 0 | 1:0 | listenBw |
| 1 | 0 | manualSetFlag |
| 1 | 2:1 | autoSensitivity |
| 2 | 7:0 | dbMax encoded as `(dbMax+130)/5` |
| 3 | 7:0 | rssiTriggerLevel (0xFF = auto) |
| 4-7 | - | Reserved |

### 2.8 LNA/PGA Gain Control (`ENABLE_FEAT_N7SIX_SPECTRUM`)

Three configurable stages in STILL mode (register overlay):
- **LNAs** (REG_13 bit 8-9): options -19, -16, -11, 0 dB
- **LNA** (REG_13 bit 5-7): options -24, -22, -14, -9, -6, -4, -2, 0 dB
- **PGA** (REG_13 bit 0-2): options -33, -27, -21, -15, -9, -6, -3, 0 dB

### 2.9 STILL Mode

When user presses PTT, the radio tunes to the peak frequency and enters STILL mode:

```
STILL Mode:
- Continuous RSSI measurement (one per tick)
- EMA-smoothed RSSI for display bar
- S-meter bar graph (S-units and dBm)
- Register menu overlay (LNA/PGA adjustments)
- Waterfall: single-column sweep
- monitorMode toggle (SIDE1): forces RX open
```

---

## 3. waterfall.c – Deep Dive

### 3.1 Design Summary

A **professional 16-level grayscale waterfall** occupying the bottom 1/4 of the ST7565 LCD (lines 48-63, pages 5-6).

### 3.2 Data Structure

```c
// 128 × 16 rows, 4 bits/pixel (packed 2 pixels per byte)
static uint8_t waterfallHistory[(128 * 16) / 2];  // 1024 bytes

static uint8_t waterfallWriteRow;    // Circular write pointer (0-15)
```

- **Packing:** High nibble = even columns, low nibble = odd columns
- **Total memory:** 1 KB + small overhead (~20 bytes)

### 3.3 Grayscale via Bayer Ordered Dither

4×4 Bayer matrix (16 levels):

```
 0   8   2  10
12   4  14   6
 3  11   1   9
15   7  13   5
```

**Rendering:** Each pixel's 4-bit grayscale level is compared against the Bayer threshold; if `level > bayer[row & 3][col & 3]`, the pixel is lit. This produces 16 perceptually uniform gray shades on the 1-bit ST7565.

### 3.4 Color Mapping: RSSI → Grayscale Level

```c
dbmToLevel(int dbm):
  1. range = waterfallDbMax - waterfallDbMin
  2. clamped = clamp(dbm, dbMin, dbMax)
  3. level = ((clamped - dbMin) * 15 + range/2) / range
```

Where `rssiToDbm()` converts raw RSSI: `dbm = (rssi >> 1) - 160 + corrTable[band]`

### 3.5 Public API

| Function | Purpose | Called From |
|----------|---------|-------------|
| `WATERFALL_Init()` | Clear buffer, reset pointer, refresh dB range | spectrum.c startup |
| `WATERFALL_SetDbRange()` | Set dB range for color mapping | spectrum.c, any time dB range changes |
| `WATERFALL_PushRow()` | Push one scan sweep row (linear interpolated to 128 cols) | `FinalizeCompletedSweep()` |
| `WATERFALL_PushRowListen()` | Push listen-mode row (one boosted column + background) | `UpdateListening()` |
| `WATERFALL_Render()` | Render waterfall to framebuffer pages 5-6 | spectrum.c `Render()` |

### 3.6 PushRow (Scan Mode)

```
WATERFALL_PushRow(rssiRow[], bars):
  step256 = ((bars-1) << 8) / 127    // Q8 fraction per column
  for each col 0..127:
    pos = col * step256
    interpolate rssi from rssiRow at position pos
    level = (rssi==RSSI_MAX) ? 0 : dbmToLevel(rssiToDbm(rssi))
    HistorySetPixel(col, writeRow, level)
  writeRow = (writeRow + 1) % 16
```

### 3.7 PushRowListen (Listen Mode)

Features:
- **Peak column** gets full signal brightness from `peakRssi`
- **Adjacent columns** (±1) get background +4 boost for soft falloff
- **Persistence:** When signal drops, the peak column stays dimmer (level capped at 8) for `PERSIST_CYCLES` (3 updates)
- **Background** remains at normal scan brightness for seamless noise floor

### 3.8 Render: Circular Buffer → Screen

```
WATERFALL_Render():
  for screenRow 0..15:
    bufRow = (writeRow + 15 - screenRow) % 16  // Reverse order: newest at top
    pageIdx = (screenRow < 8) ? 5 : 6           // Two framebuffer pages
    bitMask = 1 << (screenRow & 7)              // Bit position within page byte
    rowOffset = (screenRow & 3) << 2            // Bayer matrix row
    for col 0..127:
      level = HistoryGetPixel(col, bufRow)
      if level > bayer[rowOffset | (col & 3)]:
        set pixel
      else:
        clear pixel
```

Newest rows displayed at the top, producing the downward-falling visual effect.

---

## 4. Integration Points

### 4.1 spectrum.c ↔ waterfall.c

```c
// In spectrum.c (spectrum.h includes waterfall.h):
WATERFALL_Init();                    // At APP_RunSpectrum() start
WATERFALL_PushRow(rssiHistory, N);  // In FinalizeCompletedSweep()
WATERFALL_PushRowListen(...);       // In UpdateListening()
WATERFALL_SetDbRange(dbMin, dbMax); // In UpdateScanInfo() and UpdateDbMax()
                                    // In RearmRuntimeState()
// In Render():
if (currentState == SPECTRUM)
    WATERFALL_Render();
```

### 4.2 spectrum.c ↔ BK4819 Radio

Extensive register manipulation:
- `BK4819_SetFrequency()` — frequency tuning
- `BK4819_ReadRegister()` / `BK4819_WriteRegister()` — direct register access
- `BK4819_GetRSSI()` — RSSI sampling
- `BK4819_PickRXFilterPathBasedOnFrequency()` — RF path selection
- `BK4819_SetFilterBandwidth()` — IF filter bandwidth
- `RADIO_SetModulation()` — demodulator mode
- `RADIO_SetupAGC()` — AGC control

### 4.3 spectrum.c ↔ Display (ST7565)

- `gFrameBuffer[][]` — framebuffer (8 pages × 128 bytes)
- `gStatusLine[]` — dedicated status line buffer (128 bytes)
- `ST7565_BlitStatusLine()` — commit status line to display
- `ST7565_BlitLine(page)` — commit one framebuffer page (incremental rendering)

### 4.4 spectrum.c ↔ SPI Flash (PY25Q16)

- `PY25Q16_ReadBuffer()` at 0x00A148 — load settings
- `PY25Q16_WriteBuffer()` at 0x00A148 — save settings

### 4.5 Entry from main.c

```c
// main.c line 297:
#ifdef ENABLE_SPECTRUM
    case 4: case 5:
        APP_RunSpectrum();
        break;
#endif
```

The spectrum app takes over the main loop entirely until EXIT is pressed.

---

## 5. State Machine Analysis

### 5.1 State Diagram

```
                    ┌──────────┐
                    │ FREQ_INPUT│
                    │ (Freq pad)│
                    └────┬─────┘
                         │         KEY_5
                    ┌────▼─────┐   PTT      ┌─────────┐
  ┌─────────────────► SPECTRUM ├────────────►  STILL  │
  │                 │ (Sweep)  │             │ (Tuned) │
  │                 └────┬─────┘◄────────────┤         │
  │                      │   KEY_EXIT        │         │
  │                      │                   │         │
  │                      │   KEY_MENU(long)  │  ┌───┐  │
  │                      │   Reset defaults  │  │Menu│  │
  │                      │                   │  └─┬─┘  │
  │                      │   KEY_EXIT        │    │    │
  │                      │   Exit spectrum   │    │    │
  └──────────────────────┘                   └────┘────┘
```

### 5.2 Main Loop Tick Timings

An approximate timing budget per Tick() iteration:

| Phase | Time | Notes |
|-------|------|-------|
| User Input | 0-20 µs | Key read + dispatch |
| Scan Step | ~400 µs | 4 SPI writes (BK4819) + RSSI read |
| Listen Mode (active) | 1 ms | Delay + 1 SPI burst |
| Framebuffer Page Blit | ~200 µs | SPI to ST7565 display |
| **Total (scanning)** | **~600-800 µs** | ~1200-1600 Hz sweep rate |
| **Total (listening)** | **~1.2 ms** | ~830 Hz, dominated by 1ms delay |

### 5.3 State Transitions

| Transition | Trigger | Actions |
|------------|---------|---------|
| INIT→SPECTRUM | `APP_RunSpectrum()` | Backup registers, init waterfall, start sweep |
| SPECTRUM→STILL | KEY_PTT | Tune to peak, set frequency, disable sweep |
| STILL→SPECTRUM | KEY_EXIT | Clear waterfall buffer, relaunch scan |
| SPECTRUM→FREQ_INPUT | KEY_5 | Enter frequency input mode |
| FREQ_INPUT→SPECTRUM | KEY_MENU + valid freq | Set currentFreq, restart scan |
| SPECTRUM→EXIT | KEY_EXIT | Save settings, restore registers, restore frequency |

---

## 6. Key Algorithms & Design Decisions

### 6.1 Bidirectional Sweep with Interlacing

**Purpose:** Reduce directional bias in the spectrum display.

The sweep alternates between left→right and right→left on each full cycle. For large ranges (>128 steps), interlacing spreads samples across the entire range on the first pass (every N-th sample), then fills in progressively. This gives the user a "wide overview" immediately rather than watching the sweep crawl from one edge.

### 6.2 Incremental Framebuffer Blit

**Design decision:** Instead of `ST7565_BlitFullScreen()` (expensive ~1.6 ms SPI burst), only one framebuffer page is sent per tick.

```c
ST7565_BlitLine(renderPage);
renderPage = (renderPage + 1) % FRAME_LINES;
```

**Effect:** Full screen refresh completes in 8 ticks (~5-10 ms depending on sweep speed), keeping the display alive without blocking the scan engine. The ~9 Hz RENDER_PERIOD_TICKS threshold ensures flutter fusion (above ~30 Hz partial refresh, below audible hum).

### 6.3 Listen Mode SPI Quiescence

**Critical design:** During listening, SPI bursts are synchronized to ~3 Hz (every 320 ms) with zero SPI activity between bursts.

**Why:** SPI activity on the BK4819 bus generates audible interference in the audio path. By batching all register reads/writes into a single burst and then remaining silent for 320 ms, the operator hears clean audio with a faint 3 Hz tick rather than constant buzzing.

### 6.4 Abrupt Drop Detection

```c
#define LISTEN_DROP_EXIT_RSSI 20  // 10 dB threshold
if (rssi + 20 <= prevRssi)       // Sharp fall detected
    keepListening = false;
```

This handles the "quick PTT" scenario where the operator releasing PTT before the listen debounce would otherwise cause a 3-second hang. Fast drop detection (< 320 ms) exits listen mode gracefully.

### 6.5 Square-Root Compression for RSSI→Pixel Mapping

```c
uint8_t linear = ((dbm - DB_MIN) * PX_RANGE + DB_RANGE/2) / DB_RANGE;
uint8_t compressed = iSqrt(linear * PX_RANGE);
return (linear + compressed) / 2 + pxMin;
```

**Why:** Without compression, weak signals occupy very few pixels while strong signals dominate. Square-root compression spreads weak signals upward while keeping strong peaks visible. A 50/50 blend with linear mapping prevents over-compression.

### 6.6 Intermittent Signal Persistence

In listen mode, the waterfall peak column persists for 3 additional rows (`PERSIST_CYCLES=3`) after the signal drops. This prevents brief signal fades from creating visual holes in the waterfall trace.

### 6.7 EMA Smoothed RSSI for Display vs. Raw RSSI for Trigger

```c
// For STILL mode meter bar:
rssiSmoothed = (rssiSmoothed * 3 + newRssi) >> 2;  // α=0.25

// For squelch trigger:
peak.rssi = scanInfo.rssi;  // Raw value
```

This separation prevents the display from jittering while keeping the squelch detector responsive.

---

## 7. Performance & Optimization Notes

### 7.1 SPI Transaction Reduction (Key Optimization)

`SetFScan()` caches `scanReg30` (read once per sweep in `InitScan()`), eliminating one SPI register read per measurement step. Across a 128-step sweep: saves **128 SPI transactions** = ~30% reduction in bus activity.

```c
static uint16_t scanReg30 = 0;  // Cached REG_30 value

InitScan():
    scanReg30 = BK4819_ReadRegister(BK4819_REG_30) & ~(1u << 9);

SetFScan(f):
    BK4819_SetFrequency(f);
    BK4819_WriteRegister(REG_30, 0);
    BK4819_WriteRegister(REG_30, scanReg30);  // Uses cached value
```

### 7.2 Memory Footprint

| Component | RAM (bytes) | Notes |
|-----------|-------------|-------|
| waterfallHistory | 1024 | 128×16 × 4-bit packed |
| rssiHistory | 256 | 128 × uint16_t |
| registers_stack | 16 | 8 × uint16_t backup |
| peakHoldY | 128 | 128 × uint8_t |
| peakHoldAge | 64 | 64 × uint8_t |
| gFrameBuffer | 1024 | 8 × 128 (display framebuffer) |
| **Subtotal** | **~2.5 KB** | |
| Plus stack, other globals | ~1-2 KB | |
| **Total estimate** | **~4-5 KB** | Out of 8 KB SRAM (PY32F071) |

### 7.3 Integer-Only Arithmetic

All math is integer-based (no floating point):
- RSSI→dBm: `rssi / 2 - 160 + correction` (integer arithmetic)
- dBm→pixel: Fixed-point with division/rounding
- Interpolation: Q8 fixed-point (`pos256 = col * step256`)
- EMA: `(3*floor + new + 2) >> 2` (shift-based)
- Square root: Newton-Raphson integer `iSqrt()`

### 7.4 Branch Optimization

- `goto` usage in `CalcCrest()` — unusual but intentional for nested conditional flow
- `clamp()` as a macro-like inline: ternary chain (no branch mispredict penalty on Cortex-M0+ is negligible)
- Early returns in many functions for performance

---

## 8. Code Quality Observations

### 8.1 Strengths

1. **Excellent comments:** Nearly every non-trivial block has explanatory comments explaining *why* (not just *what*).
2. **Well-named constants and enums:** `ARRAY_SIZE()`, `SPECTRUM_TOPY_SKIP`, `PEAK_HOLD_INIT`, etc.
3. **Modular decomposition:** Radio ops, rendering, UI, scan logic all in well-separated functions.
4. **Careful SPI bus management:** Explicitly managing SPI activity for audio quality.
5. **Defensive state checks:** Every state transition validates preconditions.
6. **Proper EEPROM/flash wear management:** Settings persistence writes only on explicit EXIT (not every sweep).
7. **Feature flag encapsulation:** Compile-time configuration makes features optional without ifdef spaghetti inside function bodies.

### 8.2 Concerns

1. **`goto` in `CalcCrest()`:** The `goto`-based bidirectional neighbor check is unconventional. A `do/while` or indexed loop would be more maintainable.

2. **Magic numbers:** Several constants lack named defines:
   - `BK4819_REG_0C`, `BK4819_REG_02` bit checks (line 513-527) — should reference register bit defines
   - `0xFF` in PY25Q16 settings (line 214) — should be a sentinel constant
   - Line 44: `rssiHistory` array literal `128` should be `ARRAY_SIZE(rssiHistory)`

3. **Overloaded feature gates:** `ENABLE_FEAT_N7SIX` and `ENABLE_FEAT_N7SIX_SPECTRUM` overlap significantly, making it unclear which features depend on which flag.

4. **Potential dead code:**
   - `LockAGC()` (line 292) writes `lockAGC = false` unconditionally — the AGC lock mechanism appears disabled.
   - `DrawTicks()` has `#if 0` block (debug only).
   - The `BPFOptions` comment-out in `registerSpecs[]` suggests an unfinished feature.

5. **Global variable exposure:**
   - `rssiHistory[128]` is global (not static) — visible to waterfall.c via extern. Should be accessed through the `WATERFALL_PushRow()` API instead.
   - `isListening`, `monitorMode`, `redrawRedraw` are global non-static.

6. **No mutex/lock for shared state:** Not critical on single-threaded MCU, but the waterfall buffer could theoretically be read while being written.

7. **Hardcoded SPI flash addresses:** `0x00A148` for settings — if other modules use the same area, conflicts are possible.

### 8.3 Style Observations

- **Mixed indentation:** Predominantly 4 spaces, but some sections use tabs.
- **Hungarian notation:** Not used — clean descriptive names (`waterfallWriteRow`, `peakHoldAge`).
- **Comment density:** ~15-20% of lines are comments (excellent for embedded).
- **Header includes:** spectrum.h includes ~25 headers; some are redundant (e.g., `string.h` in header, also in .c).

---

## 9. Potential Issues & Recommendations

### 9.1 Critical Issues

None identified — the code appears functionally correct for its target hardware.

### 9.2 Moderate Issues

| Issue | Location | Description | Recommendation |
|-------|----------|-------------|----------------|
| Register `LockAGC` disabled | line 292-298 | `lockAGC` is always set false after call | Remove dead code or implement proper AGC lock |
| Hardcoded SPI address | line 215 | `0x00A148` may conflict with other modules | Centralize address in a config header |
| Waterfall buffer underrun | waterfall.c line 101 | If `bars == 0`, `step256` division by zero (unsigned wrap, may produce garbage) | Add explicit guard early-return if `bars == 0` |
| Render starvation | Tick() line 2608 | If Tick() is called >32800 times, `renderPage` wraps to `FRAME_LINES` (8) cleanly | Safe, but worth a comment |

### 9.3 Minor Recommendations

1. **Extract magic numbers:**
   ```c
   #define SETTINGS_SPECTRUM_ADDR   0x00A148
   #define RSSI_GLITCH_THRESHOLD    200
   #define RSSI_GLITCH_GUARD_CYCLES 50
   ```

2. **Refactor `CalcCrest()`:** Replace `goto` with indexed neighbor loop:
   ```c
   static void CalcCrest(const uint8_t *yArr, uint8_t x,
                         uint8_t *crestTop, uint8_t *crestBot) {
       int8_t neighbors[] = {-1, 1};
       *crestTop = *crestBot = yArr[x];
       if (yArr[x] == SPECTRUM_TOPY_SKIP) return;
       for (int i = 0; i < 2; i++) {
           int8_t nx = x + neighbors[i];
           if (nx < 0 || nx >= 128) continue;
           uint8_t n = yArr[nx];
           if (n != SPECTRUM_TOPY_SKIP && n <= DrawingEndY) {
               uint8_t mid = (yArr[x] + n + 1) >> 1;
               if (mid < *crestTop) *crestTop = mid;
               if (mid > *crestBot) *crestBot = mid;
           }
       }
   }
   ```

3. **Add `WATERFALL_` prefix guards:** Ensure all static waterfall functions remain truly static (not accidentally exported by linker).

4. **Document the `listenT` timing formula:**
   ```c
   // listenT = 320 → 320 ticks × 1ms delay = 320 ms between SPI bursts
   // This keeps SPI-induced audio interference below ~3 Hz
   ```

5. **Sweep re-entry optimization:** `InitScan()` calls `BK4819_PickRXFilterPathBasedOnFrequency()` on every full sweep restart — the path won't change if frequency range hasn't changed. Consider a dirty flag.

6. **Thread/ISR safety:** `waterfallWriteRow` and `waterfallHistory` are only accessed from Tick() context — document that these are NOT ISR-safe and must remain in task context.

### 9.4 Testing Considerations

| Test Case | Expected Behavior |
|-----------|------------------|
| Sweep with 0 RSSI samples | Graceful handling (all columns skip) |
| Frequency input: leading zeros + dots | Correct parsing into BK frequency units |
| PTT during FREQ_INPUT | Ignored (no transition from FREQ_INPUT) |
| Long-press MENU during listen | Spectrum reset (triggers RX off, re-sweep) |
| Blacklist all visible peaks | Display clears, scan continues silently |
| Monitor mode + signal drop | RX stays open (Vox-like behavior) |
| Battery at 0% | Status bar battery icon shows empty |
| Settings EEPROM corruption | Fallback defaults on all fields |

---

## Change Log (2026-08-01)

The following modifications were applied after this review:

### waterfall.c Bug Fixes
1. **NULL pointer dereference fix** (`waterfall.c:68-71`)
   - Added `gRxVfo == NULL` check in `rssiToDbm()`
   - Prevents crash if VFO pointers not initialized (EEPROM init failure)

2. **Out-of-bounds access fix** (`waterfall.c:197`)
   - Added `(peakIndex < bars)` bounds check before `rssiRow[peakIndex]`
   - Prevents reading past array in persistence logic

### waterfall.h Documentation
3. **ISR safety invariant added** (`waterfall.h:33-36`)
   - Documents that `waterfallHistory` is only accessed from main loop

### Related Changes
- `st7565.h:27-32` - Added ISR safety invariant for `gFrameBuffer`/`gStatusLine`
- `app.c:1650-1680` - K5Viewer rate-limited to 1Hz (fixes key stutter)
- `radio.c:1331` - Tail tone reduced from 200ms to 100ms
- `frequencies.c:19-30` - Added `FREQUENCIES_ClampGlobal()` / `FREQUENCIES_ClampToBand()`
- `frequencies.h:32-33` - Added `F_MIN` / `F_MAX` macros

---

## Conclusion

**spectrum.c** and **waterfall.c** form a sophisticated, carefully optimized spectrum analyzer for resource-constrained embedded hardware. The code demonstrates deep understanding of:

1. **Real-time DSP constraints** — SPI bus scheduling for audio quality
2. **Memory optimization** — packed pixel formats, integer math, fixed-point interpolation
3. **UI/UX design** — bidirectional sweep, interlacing, peak hold, persistence, dithering
4. **Hardware register management** — BK4819 register caching, RF path selection

The implementation is **production-quality** with well-thought-out state management, efficient rendering, and clear separation of concerns. Minor style issues and dead code aside, this is a solid example of professional embedded firmware engineering on ARM Cortex-M class hardware.

**Overall score: 8.5/10** — excellent code quality, minor opportunities for refactoring and documentation improvements.
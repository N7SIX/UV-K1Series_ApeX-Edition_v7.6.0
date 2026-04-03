<!--
=====================================================================================
UV-K1/K5 Series ApeX Edition - Owner's Manual
Author: N7SIX, Sean and contributors
Version: v7.6.x (ApeX Edition)
License: Apache License, Version 2.0
=====================================================================================
-->

# UV-K1 SERIES / UV-K5 V3 TRANSCEIVER
## ApeX Edition Owner's Manual (Professional Edition)

Edition: ApeX (all-in-one build)
Firmware family: v7.6.x
Manual revision: April 3, 2026

---

## Table Of Contents

1. Safety And Regulatory Information
2. Product Scope And Build Identity
3. Quick Start
4. Front Panel Controls
5. Display And Status Icons
6. Core Operating Modes
7. Professional Spectrum Analyzer
8. FM Broadcast Receiver
9. Air Copy
10. Games And Utility Features
11. Menu And Advanced Settings
12. Persistence Model (What Is Saved)
13. Removed Or Deprecated Features
14. Troubleshooting
15. Build, Flashing, And Recovery
16. Technical Notes
17. Revision History

---

## Safety And Regulatory Information

This firmware is distributed without warranty. Use at your own risk.

Important:
- Back up calibration data immediately after flashing.
- Verify legal TX/RX frequencies for your region.
- Confirm power limits and emission rules before transmitting.
- Understand that incorrect flashing or unsupported settings can brick a device.

Recommended tools:
- UVTools2 for calibration backup and restore.
- Dedicated CHIRP driver from the project ecosystem.

---

## Product Scope And Build Identity

ApeX Edition is the all-in-one build target for UV-K1 / UV-K5 V3 (PY32F071), with spectrum, FM, Air Copy, game features, and advanced N7SIX options enabled.

ApeX preset includes (as configured in repository build presets):
- Spectrum analyzer
- FM broadcast receiver
- Air Copy
- VOX
- Screenshot support
- Game support
- PMR and GMRS/FRS/MURS locks
- RescueOps feature set
- Extended scan ranges
- Resume-state and sleep features

Design intent:
- One production build with broad feature coverage.
- Deterministic embedded behavior (no dynamic allocation in critical display/control paths).
- Field-oriented controls with low-latency UI feedback.

---

## Quick Start

Power and basic operation:
1. Power on radio.
2. Select VFO or channel mode as needed.
3. Tune with UP/DOWN.
4. Use PTT to transmit where legal.

Open spectrum analyzer:
1. Press F + 5.
2. Adjust scan step, zoom, threshold, and bandwidth from spectrum controls.
3. Press EXIT to leave spectrum and return to foreground operation.

---

## Front Panel Controls

Core keypad behavior depends on current mode.

Global/common:
- UP/DOWN: tune or move selection
- MENU: toggle peak hold on/off in scan mode; cycles register-tuning pages in Still mode
- EXIT: back/cancel/close mode
- F: function modifier
- PTT: transmit (context dependent)
- SIDE1/SIDE2: configurable utility actions depending on mode

Spectrum-mode dedicated bindings are documented in the Spectrum section below.

---

## Display And Status Icons

Representative icons are located in repository icon assets under icons_from_c.

Common indicators:
- Battery level
- Scan list state
- VFO/lock state
- Backlight state
- USB-C charge state
- NOAA and power-save indicators (when enabled)

Note:
- Exact icon composition varies by active mode and build feature flags.

---

## Core Operating Modes

Primary modes in ApeX firmware family:
- VFO operation
- Memory/channel operation
- Scanner operation
- Spectrum analyzer mode
- FM broadcast mode (build enabled)
- Air Copy mode (build enabled)
- Optional feature modes (game, rescue workflows)

Mode transitions are key-driven and routed through the main application dispatcher.

---

## Professional Spectrum Analyzer

This implementation is a real-time frequency-domain display with waterfall history and adaptive signal detection logic.

### Spectrum Display Model

Display layers:
- Frequency reference and ruler layer
- Live spectrum trace layer
- Trigger/reference overlays
- Information text layer (frequency, modulation, bandwidth, scan metrics)
- Waterfall history layer

Current behavior highlights:
- Live smoothed trace rendering
- 16-level waterfall intensity mapping with dithering
- Adaptive threshold behavior with trigger control
- Channel name support during listening contexts (when available)

Important current-state note:
- Peak-hold overlay is **active** in this build. Toggle with `MENU` in scan mode (`PH` shown in status bar). Dotted line decays and fully fades ~5 seconds after a signal goes inactive.

### Spectrum Controls (Current Implementation)

In Spectrum state:
- 1 / 7: increase/decrease scan step index
- 2 / 8: center-shift or frequency-step adjustment (mode dependent)
- 3 / 9: adjust display dB max
- UP / DOWN: move current frequency window
- STAR / F: raise/lower RSSI trigger threshold
- 4: change horizontal resolution (128x/64x/32x/16x behavior)
- 6: cycle listening bandwidth (applies immediately to hardware)
- 0: toggle modulation type
- 5: open frequency input
- SIDE1: blacklist function
- SIDE2: backlight toggle
- MENU: toggle peak hold (scan mode); cycles register-tuning pages (Still mode)
- EXIT: leave spectrum (saves settings)

In Still state:
- Similar dB/threshold/modulation/bandwidth controls
- MENU cycles register-tuning pages
- EXIT returns to spectrum state (or closes page context)

### Step, Span, And Resolution Semantics

Three distinct spectrum controls:
1. Scan step (for example 25.00k / 50.00k / 100.00k)
2. Frequency movement offset (displayed as plus/minus k value; capped at ±2500.00k)
3. Horizontal resolution multiplier (16x to 128x display density behavior)

What they affect:
- Scan step directly controls sweep frequency increment.
- Movement offset controls how far center/window shifts per movement action.
- Resolution multiplier affects both spectrum trace density and waterfall bin mapping.

### Bandwidth And Modulation Behavior

Bandwidth:
- Displayed listen bandwidth and hardware filter are synchronized.
- Bandwidth changes now apply immediately on key action.

Modulation:
- Modulation toggle applies immediately.
- Startup preserves user-selected modulation when valid.
- Safety sanitization still prevents invalid mode combinations.

### Persistence And Reliability

Spectrum settings persistence includes:
- scan step index
- steps count / resolution
- frequency movement step
- trigger level
- dB min/max context
- scan delay
- backlight state
- listen bandwidth
- modulation type
- spectrum grid/ticks and listen timing fields

Save behavior:
- Explicit save on spectrum exit/deinit
- Dirty-flag delayed autosave while spectrum runs

Benefit:
- Better recovery of last-used spectrum settings across power cycles.

---

## FM Broadcast Receiver

ApeX preset enables FM radio support.

Capabilities include:
- FM broadcast receive mode
- Integrated UI interaction with standard key processing path
- Shared status and display infrastructure with main firmware

Operational note:
- FM mode availability depends on build flags and mode routing in the active firmware image.

---

## Air Copy

ApeX includes Air Copy support with improved user feedback in project feature set.

Functional role:
- Over-the-air channel/program data exchange workflows
- Progress-aware UI handling in the application layer

---

## Games And Utility Features

ApeX enables game feature support and screenshot support in the preset.

Included:
- Breakout feature path (when enabled in build)
- Screenshot pipeline integration (when enabled)

---

## Menu And Advanced Settings

The firmware exposes both standard radio settings and extended ApeX options.

Commonly relevant advanced controls:
- power profile/user power settings
- timeout and end-of-transmission alerts
- display contrast and invert behavior
- lock behavior (keys, keys plus PTT)
- GUI style and status line options
- scan and scan-list controls
- battery display and calibration helpers

Notes:
- Exact menu numbering and labels can vary between revisions.
- Build-time feature flags influence visible menu entries.

---

## Persistence Model (What Is Saved)

General system settings:
- Saved through the firmware settings subsystem.

Spectrum-specific settings:
- Saved through spectrum persistence blocks (EEPROM and compatibility save path where enabled).
- Exit path guarantees save.
- Runtime autosave reduces loss risk for power-off without explicit exit.

Practical guidance:
- After changing multiple spectrum parameters, allow a short idle period for delayed autosave to commit if you are not exiting explicitly.

---

## Removed Or Deprecated Features

Removed from current spectrum implementation:
- Contest marker overlays
- DX marker overlays
- Digital mode marker overlays

(Peak-hold dashed line was previously removed but has been re-implemented as a decaying dotted overlay. See Spectrum Controls section.)

Why removed:
- Cleaner visual interpretation
- Reduced UI clutter
- Better consistency with current trace rendering model

If older screenshots/manual fragments reference these overlays, treat them as historical behavior.

---

## Troubleshooting

### Spectrum appears unchanged after changing movement offset

Cause:
- Offset changes influence sweep window position, not trace style.
- In center mode, some keys shift center directly rather than editing stored offset.

Check:
- Tune near known active signal and re-test.
- Verify mode (center/span behavior).

### Bandwidth value changes but effect seems delayed

Current behavior:
- Immediate hardware apply is implemented.

If effect is still unclear:
- Verify listening context and signal type.
- Confirm modulation and filter assumptions for target signal.

### Modulation seems to reset on entering spectrum

Current behavior:
- User modulation is preserved when valid, with safety sanitization.

If fallback occurs:
- The selected modulation may be invalid for the active VFO/band context.

### Settings lost after abrupt power cut

Current behavior:
- Exit saves immediately.
- Runtime autosave is delayed to reduce write wear.

Best practice:
- Use EXIT when possible after major spectrum changes.

---

## Build, Flashing, And Recovery

Build (Docker helper):
- ./compile-with-docker.sh ApeX

Artifacts:
- build/ApeX/ with elf, bin, hex outputs

Flashing:
- Use UVTools2 flash mode
- Ensure device is in proper DFU/flash state per tool guidance

Recovery best practice:
- Always keep calibration backup before testing firmware changes.

---

## Technical Notes

Platform:
- PY32F071 (ARM Cortex-M0+)

Display:
- ST7565 class monochrome LCD

Radio IC:
- BK4819 control path with firmware-managed filtering, modulation, and scan behavior

Software architecture:
- Application, driver, UI, and settings modules with compile-time feature gating via CMake presets

---

---

## Appendix A — Spectrum Analyzer Key Reference

### Scan Mode Keys

| Key | Action |
|-----|--------|
| `1` | Scan step size — increase |
| `7` | Scan step size — decrease |
| `2` | Frequency shift step — increase |
| `8` | Frequency shift step — decrease |
| `3` | dB ceiling — raise |
| `9` | dB ceiling — lower |
| `UP` | Shift center frequency up by current step |
| `DOWN` | Shift center frequency down by current step |
| `PTT` | Lock onto peak signal → enter Still mode |
| `SIDE1` | Blacklist (skip) current peak frequency |
| `SIDE2` | Toggle backlight |
| `0` | Cycle modulation (FM → AM → USB) |
| `4` | Cycle resolution (16× / 32× / 64× / 128× steps) |
| `5` | Open frequency input (type center frequency directly) |
| `6` | Cycle listen bandwidth (25k → 12.5k → 6.25k) |
| `★` | RSSI trigger level — raise |
| `F` | RSSI trigger level — lower |
| `MENU` | Toggle peak hold on/off (`PH` shown in status bar when active) |
| `EXIT` | Exit spectrum analyzer → return to main VFO |

### Still Mode Keys

| Key | Action |
|-----|--------|
| `3` | dB ceiling — raise |
| `9` | dB ceiling — lower |
| `UP` | Tune up (or increase register value if register menu open) |
| `DOWN` | Tune down (or decrease register value if register menu open) |
| `SIDE1` | Toggle monitor mode (force audio open) |
| `SIDE2` | Toggle backlight |
| `0` | Cycle modulation |
| `5` | Open frequency input |
| `6` | Cycle listen bandwidth |
| `★` | RSSI trigger level — raise |
| `F` | RSSI trigger level — lower |
| `MENU` | Cycle register inspector (LNAs → LNA → VGA → BPF → wrap) |
| `EXIT` | Return to scan mode (or close register menu if open) |

### Frequency Input Mode Keys

| Key | Action |
|-----|--------|
| `0`–`9` | Type digit |
| `★` | Type decimal point |
| `MENU` | Confirm and tune to entered frequency |
| `EXIT` | Delete last digit; if empty, cancel and return |

---

## Appendix B — Spectrum Settings Storage Map

All spectrum settings are stored at EEPROM address `0x1E80` (20 bytes, last byte is checksum).

| Byte(s) | Bits | Setting | Notes |
|---------|------|---------|-------|
| 0 | [3:0] | `scanStepIndex` | 0=25k, 1=50k, 2=100k, … (see ScanStep enum) |
| 0 | [5:4] | `stepsCount` | 0=16×, 1=32×, 2=64×, 3=128× |
| 1 | [1:0] | `listenBw` | 0=25k (wide), 1=12.5k, 2=6.25k (narrower) |
| 1 | [2] | `modulationType` | 0=FM, 1=AM (extended modes via BK4819) |
| 1 | [5:3] | `bw` | Scan passband filter (same enum as listenBw) |
| 2–5 | [31:0] | `frequencyChangeStep` | uint32, little-endian, Hz×10 units |
| 6–7 | [15:0] | `rssiTriggerLevel` | uint16, little-endian, raw RSSI units |
| 8–9 | [15:0] | `dbMin` | int16, little-endian, dBm floor for display |
| 10–11 | [15:0] | `dbMax` | int16, little-endian, dBm ceiling for display |
| 12–13 | [15:0] | `scanDelay` | uint16, little-endian, µs dwell per step |
| 14 | [0] | `backlightState` | 1=on, 0=off |
| 15 | [0] | `useTicksGrid` | 1=grid background, 0=tick marks |
| 16 | [7:0] | `listenTScan` | uint8, RX dwell ticks after scan peak |
| 17 | [7:0] | `listenTStill` | uint8, RX dwell ticks in still/quiet mode |
| 18 | — | reserved | always 0x00 |
| 19 | — | checksum | simple byte sum of bytes 0–18 |

**Autosave behavior:** Any setting change arms an 80-tick countdown (≈800 ms). On expiry the block is written. Exiting spectrum via `EXIT` triggers an immediate write regardless of the countdown state.

---

## Revision History

- April 3, 2026
  - Manual rewritten for repository-accurate ApeX behavior.
  - Added complete current spectrum behavior model and persistence description.
  - Removed outdated peak-hold-overlay and contest-marker guidance.
  - Aligned modulation, bandwidth, and autosave behavior documentation with current implementation.
  - Added Appendix A (key reference) and Appendix B (EEPROM storage map) derived from source.
  - Updated for peak hold re-introduction: decaying dotted overlay, KEY_MENU toggle, PH status indicator.
  - Corrected frequency shift step cap (±2500.00k max, ±1000.00k first-boot default).
  - Updated Removed Features section: peak hold is no longer removed.

- March 2026 and earlier
  - Prior drafts included legacy visualization and transitional feature notes.


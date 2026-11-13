# Release Notes: ApeXLabs v7.6.13

## 📦 Release Overview

**Tag:** `v7.6.13-ApeXLabs`
**Edition:** ApeXLabs (combined Labs + ApeX)
**Target:** UV-K1 / UV-K5 V3 (PY32F071)
**Firmware binary:** `n7six.ApeXLabs-k1.v7.6.13.bin`
**CHIRP driver:** `n7six.ApeXLabs-k1.v7.6.13.py`

ApeXLabs is a combined edition that merges **Labs' experimental overlay-apps platform** with **ApeX's N7SIX waterfall feature and branding** into a single firmware build.

---

## ✨ What's New in ApeXLabs

### From Labs Edition
- **Overlay-apps platform** — Load apps from external Flash into dedicated 4 KiB RAM overlays at runtime
- **External Flash R/W** — Read and write external flash storage
- **System Info overlay** — On-radio diagnostics and firmware information
- **AirCopy** — Wireless channel transfer between radios
- **Rescue Ops** — First-responder controls

### From ApeX Edition
- **N7SIX Waterfall** — Real-time spectrum waterfall display on the LCD
- **MDC-1200 roger variants** — MENU > Roger gains three real MDC-1200 hardware-FFSK bursts on top of the standard `MDC`: **MDC-EP** (one long frame, ANI), **MDC-2P** (two standard frames, Call Alert twice) and **MDC-EL** (three standard frames, Emergency repeated 3×). Each is sent as one continuous TX-enable. Transmit only; there is no MDC receive handler.
- **N7SIX branding** — Custom boot emblem, Profile QR target, N7SIX author string
- **Branded binary** — `n7six.ApeXLabs-k1.v7.6.13.bin`

### 🔋 Battery voltage & indicator fixes
- **Audio-capture apps no longer corrupt the battery reading.** Apps that capture RX audio (APRS RX, AIS RX, ACARS RX, ATIS RX, ATIS DSC RX, POCSAG RX, SSTV, EPIRB 406, Sonde M10/M20/RS41, AF Scope, AF Spectro, Sig Finder, APRS DIGI) leave ADC1 on PA4. `BOARD_ADC_GetBatteryInfo()` now switches back to the battery channel for each conversion, then restores PA4. Previously the audio signal was read as battery voltage, causing wrong levels and false low/over-voltage warnings while these apps ran.
- **Broadcast FM and FlashMap now keep the battery icon live.** They draw the icon but never sampled the battery, so the level stayed frozen at launch. The loader now refreshes a reading older than 1 s when the icon is drawn. This is skipped during audio capture and while the PA is keyed, so TX load never reads as a low battery. Apps that already sample are not sampled twice.
- Builds without overlay apps (for example ApeX) are unchanged.
- A small voltage drop while an app runs is normal: the radio chip, CPU and backlight draw more current than on the main screen. It recovers when you exit the app.

### 🆕 Other changes since v7.6.12
- Added the ApeXLabs combined preset (Labs overlay apps + ApeX waterfall) and its CHIRP driver.
- Fixed the Labs overlay firmware build.
- Fixed N7SIX QR code branding for the ApeXLabs edition.
- Synced `App/apps/` with the F4HWN `armel/uv-k1-k5v3-firmware-custom` main branch, adding 25 new overlay apps (43 in total). Built `.app` blobs are archived under `archive/apps/v7.6.0/`.

---

## 📋 Supported Overlay Apps (Labs platform)

| Category | Apps |
|----------|------|
| Radio tools | Broadcast FM, Fox Hunt, Beacon, Beam, Spectrum3D, Sig Finder, ToneCall, VFO Test |
| Digital & signal | APRS RX, APRS TX, APRS DIGI, APRS Edit, SSTV, EPIRB 406, AIS RX, ACARS RX, ATIS RX, ATIS DSC RX, POCSAG RX, Sonde M10, Sonde M20, Sonde RS41 |
| CW | CW Decode, CW Keyer, CW USB |
| Audio analysis | AF Scope, AF Spectro |
| Diagnostics & utilities | System Info, FlashMap, Resistor |
| Games & demos | Breakout, Cube3D, Hell Maze, Minesweeper, Plasma, Rapid Roll, Sokoban, Snake, Space Impact, Tetris |
| Developer / test | Handoff A, Handoff B |

> ℹ️ The external Flash has 48 app slots (8 KiB each). Use app files supplied with the same firmware release so their ABI and required capabilities match.
>
> ⚠️ **KISS TNC** is in the app collection but is not usable on this firmware yet: it needs the USB serial overlay service (`APP_CAP_SERIAL`), which this build does not enable, so the loader refuses it.

---

## 🔧 Technical Specifications

| Parameter | Value |
|-----------|-------|
| MCU | PY32F071 |
| Radio | Quansheng UV-K1 / UV-K5 V3 |
| Edition string | `ApeXLabs` |
| Firmware version | `v7.6.13` |
| Flash usage | 118,968 B / 118 KB (98.46%) |
| RAM usage | 13,888 B / 16 KB (84.77%) |
| Build toolchain | Arm GNU Toolchain 13.3.Rel1 |

---

## 📥 Assets

| Asset | Description |
|-------|-------------|
| `n7six.ApeXLabs-k1.v7.6.13.bin` | Combined ApeXLabs firmware binary |
| `n7six.ApeXLabs-k1.v7.6.13.py` | CHIRP programming driver for ApeXLabs |

---

## 📡 CHIRP Driver

The included CHIRP driver (`n7six.ApeXLabs-k1.v7.6.13.py`) is required to program channels and settings into ApeXLabs firmware.

**Driver version:** Quansheng UV-K1 / UV-K5 V3 driver ver: 2026/10/10 (c) N7SIX v7.6.13  
**Firmware version match:** `ApeXLabs v7.6.13`

Install the driver in CHIRP's `drivers/` directory or load it directly from CHIRP's radio driver selection.

---

## 🚀 Build & Flash

### Build from source
```bash
./compile-firmware.sh ApeXLabs
```

Or build all presets including ApeXLabs:
```bash
./compile-firmware.sh All
```

The output binary will be at:
```
build/ApeXLabs/n7six.ApeXLabs-k1.v7.6.13.bin
```

### Flash the radio
Use [UV Studio](https://n7six.github.io/UVStudio/#flash) or any compatible DFU flasher to write the `.bin` file to the radio in DFU mode.

---

## ⚠️ Important Warnings

1. **Experimental software** — Labs features (overlay apps, external Flash R/W) are the newest, least-settled work. Expect rough edges. Use at your own risk.
2. **Not a strict superset** — Features may be exchanged between editions to preserve stability and memory headroom. ApeXLabs operates near the flash limit (98.46%).
3. **Calibration data** — Back up your calibration data with UV Studio immediately after flashing. Restore it before installing another firmware or returning to stock.
4. **Transmit regulations** — Transmitting apps (APRS TX, Beacon, etc.) must only be used on frequencies, power levels, and modes permitted by your license and local regulations.
5. **Overlay-app ABI** — Use overlay app files supplied with the same firmware release (`v7.6.13`) to ensure capability and ABI compatibility.

---

## 🔗 Related Editions

| Edition | Description | Binary |
|---------|-------------|--------|
| **ApeX** | N7SIX dedicated edition (Fusion + waterfall) | `n7six.ApeX-k1.v7.6.12.bin` |
| **Labs** | Experimental overlay-apps platform | `f4hwn.labs.bin` |
| **ApeXLabs** | Labs + ApeX combined (this release) | `n7six.ApeXLabs-k1.v7.6.13.bin` |

---

## 🙏 Credits

- [@N7SIX](https://github.com/N7SIX) — ApeXLabs edition, N7SIX branding, waterfall feature
- [@armel](https://github.com/armel) (Armel, **F4HWN**) — F4HWN Fusion firmware this edition is built on, the Labs overlay-apps platform and the overlay apps, and [UV Studio](https://armel.github.io/uvstudio/)
- [@muzkr](https://github.com/muzkr) & [@armel](https://github.com/armel) — UV-K1/UV-K5 V3 port
- [@DualTachyon](https://github.com/DualTachyon) — Original UV-K5 open-source firmware
- [CHIRP project](https://chirp.danplanet.com/) — Radio programming software

---

## 📄 License

Apache License 2.0 — See [LICENSE](https://github.com/N7SIX/UV-K1Series_ApeX-Edition_v7.6.0/blob/main/LICENSE) for details.
# Release Notes: ApeXLabs v7.6.13

## 📦 Release Overview

**Tag:** `v7.6.13-ApeXLabs`
**Edition:** ApeXLabs (combined Labs + ApeX)
**Target:** UV-K1 / UV-K5 V3 (PY32F071)
**Firmware binary:** `n7six.ApeXLabs-k1.v7.6.13.bin`
**CHIRP driver:** `n7six.ApeXLabs-k1.v7.6.13.py`

ApeXLabs is a combined edition that merges **Labs' experimental overlay-apps platform** with **ApeX's N7XIX waterfall feature and branding** into a single firmware build.

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
- **N7SIX branding** — Custom boot emblem, Profile QR target, N7SIX author string
- **Branded binary** — `n7six.ApeXLabs-k1.v7.6.13.bin`

---

## 📋 Supported Overlay Apps (Labs platform)

| Category | Apps |
|----------|------|
| Radio tools | Broadcast FM, Fox Hunt, Beacon, Beam, Spectrum3D |
| Digital & signal | APRS RX, APRS TX, SSTV, EPIRB 406 |
| Diagnostics | System Info |
| Games & demos | Breakout, Cube3D, Minesweeper, Plasma, Rapid Roll, Snake, Space Impact, Tetris |

> ℹ️ Up to 16 apps from the collection of 18 can be stored in external Flash. Use app files supplied with the same firmware release so their ABI and required capabilities match.

---

## 🔧 Technical Specifications

| Parameter | Value |
|-----------|-------|
| MCU | PY32F071 |
| Radio | Quansheng UV-K1 / UV-K5 V3 |
| Edition string | `ApeXLabs` |
| Firmware version | `v7.6.13` |
| Flash usage | ~115,976 B / 118 KB (95.98%) |
| RAM usage | ~13,600 B / 16 KB (83.01%) |
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
Use [UV Studio](https://armel.github.io/uvstudio/#flash) or any compatible DFU flasher to write the `.bin` file to the radio in DFU mode.

---

## ⚠️ Important Warnings

1. **Experimental software** — Labs features (overlay apps, external Flash R/W) are the newest, least-settled work. Expect rough edges. Use at your own risk.
2. **Not a strict superset** — Features may be exchanged between editions to preserve stability and memory headroom. ApeXLabs operates near the flash limit (95.98%).
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
- [@muzkr](https://github.com/muzkr) & [@armel](https://github.com/armel) — UV-K1/UV-K5 V3 port
- [@DualTachyon](https://github.com/DualTachyon) — Original UV-K5 open-source firmware
- [CHIRP project](https://chirp.danplanet.com/) — Radio programming software

---

## 📄 License

Apache License 2.0 — See [LICENSE](LICENSE) for details.
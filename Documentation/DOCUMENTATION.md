<!--
=====================================================================================
UV-K1/K5 ApeX Edition Documentation
Author: N7SIX, Sean
Version: v7.6.0 (ApeX Edition)
License: Apache License, Version 2.0
=====================================================================================
-->

# UV-K1/K5 ApeX Edition Documentation

Complete professional owner's manual and technical reference for the UV-K1 Series and UV-K5 V3 custom firmware editions.

---

## 📚 DOCUMENTATION ROADMAP

### For First-Time Users → [Owner_Manual_ApeX_Edition.md](Owner_Manual_ApeX_Edition.md)

Start here if you're new to the radio or this firmware. Covers:
- ✅ Safety and regulatory information
- ✅ Front panel controls and buttons
- ✅ Getting started with frequency tuning
- ✅ Operating modes (VFO, Memory, Scan)
- ✅ Complete menu system reference
- ✅ Professional spectrum analyzer guide
- ✅ Troubleshooting common issues
- ✅ Technical specifications

**Expected reading time**: 30-45 minutes
**Key sections for first use**: Getting Started, Front Panel Controls, Spectrum Analyzer

---

### For Quick Lookups → [QUICK_REFERENCE_CARD.md](QUICK_REFERENCE_CARD.md)

Fast reference for common tasks and key information. Includes:
- ✅ Essential button controls at a glance
- ✅ Frequency quick entry methods
- ✅ Spectrum analyzer quick start
- ✅ TX power selection
- ✅ Modulation and squelch quick access
- ✅ S-meter interpretation chart
- ✅ Emergency frequency reference
- ✅ Common troubleshooting fixes
- ✅ Keypad reference map
- ✅ Battery status interpretation

**Expected use**: 2-5 minute lookups
**Perfect for**: Pocket-size reference when operating

---

### For Technical Deep Dives → [TECHNICAL_APPENDIX.md](TECHNICAL_APPENDIX.md)

Advanced technical reference for spectrum analyzer implementation and signal processing. Contains:
- ✅ Signal acquisition pipeline (BK4819 → display)
- ✅ RSSI measurement and conversion
- ✅ Spectrum display architecture (128-bin frequency resolution)
- ✅ Waterfall rendering algorithm (circular buffer, Bayer dithering)
- ✅ Peak hold exponential decay mathematics
- ✅ Noise floor characteristics and sources
- ✅ Advanced configuration options
- ✅ Performance tuning strategies
- ✅ Detailed troubleshooting (root causes)
- ✅ Measurement techniques for professionals
- ✅ Calibration and accuracy information

**Expected reading time**: 45-60 minutes  
**Audience**: Developers, advanced users, radio technicians  
**Key sections**: Signal Acquisition Pipeline, Waterfall Rendering, Noise Floor Characteristics

---

## 🎯 QUICK START (5 MINUTES)

1. **Power on**: Press [PWR]
2. **Tune frequency**: Use [UP]/[DOWN] arrow keys
3. **Select modulation**: Press [0] to cycle (FM/AM/USB/LSB/CW)
4. **Adjust squelch**: [F] + [UP]/[DOWN] to find signal
5. **Transmit**: Press [PTT]

See [QUICK_REFERENCE_CARD.md](QUICK_REFERENCE_CARD.md#essential-controls-at-a-glance) for all key shortcuts.

---

## 📊 SPECTRUM ANALYZER QUICK START

1. **Activate**: Press [F] + [5]
2. **Start scanning**: Press [* SCAN]
3. **Watch the display**:
   - Blue trace = real-time signal
   - Waterfall rows = signal history (top=newest)
   - Dashed line = peak hold (maximum memory)
4. **Lock on signal**: Radio auto-tunes when detecting (Listen mode)
5. **Resume scanning**: Press [EXIT] when done

See [Owner_Manual_ApeX_Edition.md#professional-spectrum-analyzer](Owner_Manual_ApeX_Edition.md#professional-spectrum-analyzer) for detailed explanation of all display elements.

---

## 📋 DOCUMENT SELECTION MATRIX

| I need to... | Go to | Section |
|--------------|-------|---------|
| Learn how to use the radio | 📖 Owner's Manual | Getting Started |
| Understand all menu options | 📖 Owner's Manual | Menu System Reference |
| Use the spectrum analyzer | 📖 Owner's Manual | Professional Spectrum Analyzer |
| Quickly find a control/function | 🗂️ Quick Reference | Essential Controls / Contact & Support |
| Fix a problem | 🗂️ Quick Reference | Common Issues & Solutions |
| Understand signal chain/pipeline | 🔧 Technical Appendix | Signal Acquisition Pipeline |
| Optimize spectrum display | 🔧 Technical Appendix | Advanced Configuration |
| Troubleshoot deep issues | 🔧 Technical Appendix | Troubleshooting Deep Dive |
| Understand why grass slows | 🔧 Technical Appendix | Noise Floor Characteristics |
| Measure signals professionally | 🔧 Technical Appendix | Advanced Measurement Techniques |

---

## ❓ FREQUENTLY ASKED QUESTIONS

### Q: What's the difference between these three documents?

**Owner's Manual** = User guide (how to operate the radio)
**Quick Reference** = Cheat sheet (fast lookups)  
**Technical Appendix** = Engineer's guide (how it works internally)

### Q: I'm lost, where do I start?

→ Start with [Owner_Manual_ApeX_Edition.md](Owner_Manual_ApeX_Edition.md#getting-started) "Getting Started" section.

### Q: My spectrum analyzer shows flat lines after 2-3 seconds, is it broken?

→ **No, this is normal.** See [TECHNICAL_APPENDIX.md#problem-spectrum-grass-slowssdies-after-2-3-seconds](TECHNICAL_APPENDIX.md#problem-spectrum-grass-slowssdies-after-2-3-seconds) for detailed explanation.

### Q: How do I tune to a specific frequency?

→ Use [UP]/[DOWN] arrow keys, or enter digits directly ([0-9] keys). See [QUICK_REFERENCE_CARD.md#frequency-quick-entry](QUICK_REFERENCE_CARD.md#frequency-quick-entry).

### Q: What's the S-meter showing me?

→ Signal strength in standard IARU scale (S0-S9+20). See [QUICK_REFERENCE_CARD.md#s-meter-interpretation](QUICK_REFERENCE_CARD.md#s-meter-interpretation).

### Q: How do I activate the spectrum analyzer?

→ Press [F] + [5]. See [Owner_Manual_ApeX_Edition.md#accessing-the-spectrum-analyzer](Owner_Manual_ApeX_Edition.md#accessing-the-spectrum-analyzer).

### Q: Can I increase the spectrum update rate?

→ Yes, but it costs battery. See [TECHNICAL_APPENDIX.md#optimization-strategies](TECHNICAL_APPENDIX.md#optimization-strategies).

### Q: Why doesn't my peak hold match the current trace?

→ Peak hold shows maximum history and fades exponentially. See [TECHNICAL_APPENDIX.md#peak-hold-algorithm](TECHNICAL_APPENDIX.md#peak-hold-algorithm).

---

## 🔧 WHERE TO FIND COMMON TOPICS

### Battery & Power
- Battery specifications → [Owner's Manual](Owner_Manual_ApeX_Edition.md#battery-management)
- Battery troubleshooting → [Quick Reference](QUICK_REFERENCE_CARD.md#battery-troubleshooting)

### Spectrum Analyzer
- User guide → [Owner's Manual](Owner_Manual_ApeX_Edition.md#professional-spectrum-analyzer)
- Quick start → [Quick Reference](QUICK_REFERENCE_CARD.md#spectrum-analyzer-quick-start)
- Technical details → [Technical Appendix](TECHNICAL_APPENDIX.md#signal-acquisition-pipeline)
- Advanced analysis → [Technical Appendix](TECHNICAL_APPENDIX.md#advanced-measurement-techniques)

### Menu System
- Complete reference → [Owner's Manual](Owner_Manual_ApeX_Edition.md#menu-system-reference)
- Quick shortcuts → [Quick Reference](QUICK_REFERENCE_CARD.md#common-menu-shortcuts)

### Troubleshooting
- Common issues → [Quick Reference](QUICK_REFERENCE_CARD.md#common-issues--solutions)
- Deep troubleshooting → [Technical Appendix](TECHNICAL_APPENDIX.md#troubleshooting-deep-dive)

### Emergency & Safety
- Safety warnings → [Owner's Manual](Owner_Manual_ApeX_Edition.md#safety--regulatory-information)
- Emergency procedures → [Quick Reference](QUICK_REFERENCE_CARD.md#emergency-procedures)
- Emergency frequencies → [Quick Reference](QUICK_REFERENCE_CARD.md#emergency-frequency-chart)

---

## 📚 RELATED RESOURCES

**In this repository:**
- [README.md](README.md) - Project overview and build instructions
- [IMPLEMENTATION_GUIDE.md](Documentation/IMPLEMENTATION_GUIDE.md) - Software architecture (developers)
- [CMakePresets.json](CMakePresets.json) - Firmware configuration options

**External resources:**
- **BK4819 Receiver IC**: Datasheet available from manufacturer (RF frontend documentation)
- **PY32F071 MCU**: ARM Cortex-M0+, datasheet from Puya Semiconductor
- **IARU Region 1 Rec. R.1**: S-meter standardization (technical standard)

---

## 🎓 LEARNING PATH

**Beginner (Casual user)**:
1. Read Owner's Manual "Getting Started"
2. Practice with Quick Reference shortcuts
3. Explore spectrum analyzer via Owner's Manual guide

**Intermediate (Operator)**:
1. Complete Owner's Manual
2. Master Menu System Reference
3. Learn spectrum analysis techniques (see Owner's Manual)

**Advanced (Technician/Developer)**:
1. Read technical specifications (Owner's Manual)
2. Study Technical Appendix signal chain
3. Understand performance tuning and calibration
4. Review actual source code in `App/app/spectrum.c`

**Expert (Firmware developer)**:
1. Read IMPLEMENTATION_GUIDE.md in Documentation/
2. Study full codebase under App/
3. Review spectrum.c (2876 lines with detailed signal processing)
4. Build modified firmware using CMake presets

---

## 📞 SUPPORT

**For user-related questions:**
- Check [QUICK_REFERENCE_CARD.md](QUICK_REFERENCE_CARD.md#contact--support)
- Review [Owner_Manual_ApeX_Edition.md](Owner_Manual_ApeX_Edition.md#troubleshooting)

**For technical issues:**
- See [TECHNICAL_APPENDIX.md](TECHNICAL_APPENDIX.md#troubleshooting-deep-dive)
- Report on GitHub with firmware version and detailed reproduction steps

**For firmware development:**
- See [IMPLEMENTATION_GUIDE.md](Documentation/IMPLEMENTATION_GUIDE.md) in Documentation/

---

## 📄 DOCUMENT VERSIONS

| Document | Version | Date | Firmware |
|----------|---------|------|----------|
| Owner's Manual | 1.0 | Feb 2026 | 7.6.0+ |
| Quick Reference | 1.0 | Feb 2026 | 7.6.0+ |
| Technical Appendix | 1.0 | Feb 2026 | 7.6.0+ |

---

## ✅ CHECKLIST FOR FIRST USE

- [ ] Read "Safety & Regulatory Information" in Owner's Manual
- [ ] Verify correct frequency bands for your region
- [ ] Back up calibration data using uvtools2
- [ ] Learn front panel controls (Quick Reference)
- [ ] Practice frequency tuning ([UP]/[DOWN])
- [ ] Activate spectrum analyzer ([F] + [5])
- [ ] Practice TX using [PTT] in safe band
- [ ] Explore menu system (Menu → navigate with [UP]/[DOWN])
- [ ] Bookmark Quick Reference Card for operational use
- [ ] Save Technical Appendix for future reference

---

**Questions? Refer to the appropriate document above for detailed answers.**

---

*Documentation prepared for UV-K1 Series / UV-K5 V3 ApeX Edition Firmware*  
*Licensed under Apache 2.0 — See LICENSE file*  
*Copyright © N7SIX, F4HWN, Egzumer, DualTachyon*

# UV-K1/K5 ApeX Edition - QUICK REFERENCE CARD

---

## ESSENTIAL CONTROLS AT A GLANCE

```
POWER:           Press [PWR]
TUNE:            Use [UP]/[DOWN] arrow keys
SCAN:            Press [* SCAN]
TRANSMIT:        Press [PTT], release to end
MENU:            Press [MENU]
EXIT:            Press [EXIT]
SPECTRUM:        Press [F] + [5]
VOLUME:          [UP]/[DOWN] (when holding no key)
```

---

## FREQUENCY QUICK ENTRY

| Key Sequence | Action |
|--------------|--------|
| [0-9] digits | Direct frequency input (e.g., [145][0][0][MENU]) |
| [UP]/[DOWN] | Step by configured increment |
| Long [UP]/[DOWN] | Rapid frequency sweep |
| [F] + [UP]/[DOWN] | Change step size |
| [F] + [*] | Favorite/memory quick access |

---

## SPECTRUM ANALYZER QUICK START

```
Step 1: Press [F] + [5]           → Bandscope opens
Step 2: Press [* SCAN]             → Scanning starts
Step 3: Watch spectrum trace       → Real-time signal display
Step 4: Monitor waterfall          → Historical signal pattern
Step 5: Press [EXIT]               → Exit spectrum mode
```

**During Scan:**
- | **Signals detected**: Radio auto-locks to frequency (Listen mode)
- | **Press [UP]/[DOWN]**: Manual frequency step within scan
- | **Press [SIDE1]**: Blacklist current frequency
- | **Press [*]**: Resume from pause

---

## MODULATION SELECTOR

```
Press [0] for:
 FM  (Narrow: 12.5kHz, Wide: 25kHz)
 AM  (Amplitude Modulation)
 USB (Upper Sideband, HF/SSB)
 LSB (Lower Sideband, HF/SSB)
 CW  (Morse Code, Carrier Wave)
```

---

## TX POWER QUICK SELECT

Press **[F] + [2]** to cycle through:
- **&lt;20mW** (beacon mode, minimal power drain)
- **125mW** (short-range, battery-efficient)
- **500mW** (medium range)
- **5W** (maximum)

---

## SQUELCH ADJUSTMENT (NO MENU)

```
Press [F] while in VFO mode, then:
[UP]/[DOWN]   → Increase/decrease squelch
[F] + [UP]    → Maximum squelch
[F] + [DOWN]  → No squelch (always listen)
```

---

## BATTERY STATUS INDICATOR

| Display | Meaning | Action |
|---------|---------|--------|
| **100%** | Full battery | Ready for deployment |
| **75%-50%** | Good condition | No action needed |
| **50%-25%** | Low battery | Plan charging soon |
| **&lt;25%** | Critical | Charge immediately |
| **BUZZER + ICON FLASH** | Battery depleted | Replace/charge now |

---

## S-METER INTERPRETATION

```
S0    No signal
S1    Barely detectable
S2-S3 Weak signal
S4-S5 Moderate signal (typical contact)
S6-S7 Good signal strength
S8-S9 Strong signal, excellent contact
+10+20 Overload (signal too strong)
```

---

## SPECTRUM DISPLAY LEGEND

```
┌─ Spectrum Trace (blue solid line)
│  = Real-time RF amplitude per frequency
│
├─ Peak Hold (dashed line above)
│  = Maximum signal history, gradually fades
│
├─ Noise Floor (baseline, bottom area)
│  = Ambient RF background (-120 to -100 dBm typical)
│
└─ Waterfall (16 rows, grayscale)
   = Signal history over time (top=newest)
   = Dark=weak, Light=strong
```

---

## EMERGENCY PROCEDURES

### Radio Won't Power On
1. Check battery connection
2. Verify battery voltage (should be 7.4V DC)
3. Try different battery if available
4. Hold [PWR] for 3 seconds (cold start)

### Cannot Transmit
1. Check [PTT] button is fully pressed
2. Verify TX power is not set to <20mW (change via [F]+[2])
3. Check frequency lock is disabled (Menu → FLck)
4. Ensure modulation is valid for band

### Recovered from Crash (Frozen Display)
1. Press [PWR] to power off
2. Wait 5 seconds
3. Press [PWR] again to restart
4. Use [MENU] → Factory reset if problem persists

---

## COMMON MENU SHORTCUTS

| Function | Path | Quick Key |
|----------|------|-----------|
| Set TX Power | Menu → 02 TxPow | [F] + [2] |
| Change Bandwidth | Menu → 05 BandWdth | [0] + [UP]/[DOWN] |
| Squelch Control | Menu → 06 Squelch | [F] + [UP]/[DOWN] |
| Step Size | Menu → 11 Step | [F] + [RIGHT ARROW] |
| Backlight On/Off | Menu → 18 BLMod | [F] + [F3] |
| Battery Info | Menu → 13 BatSave | (display only) |

---

## FREQUENCY BAND QUICK REFERENCE

| Band | VHF Coverage | UHF Coverage | Notes |
|------|--------------|--------------|-------|
| **16 CH** | — | 400-480 MHz | Default factory |
| **VHF ONLY** | 136-174 MHz | — | Amateur 2m band |
| **UHF ONLY** | — | 400-480 MHz | Amateur 70cm band |
| **EXTENDED** | 108-174 MHz | 400-520 MHz | Broadcast + extended |
| **FULL RANGE** | 50-940 MHz* | *varies | Firmware-dependent |

*Check firmware F-Lock setting (Menu → 01 FLck)*

---

## TONE/CTCSS PROGRAMMING QUICK START

1. **Find tone/code needed**: Ask local repeater coordinator
2. **Go to target frequency/channel**
3. **Press [MENU]**
4. **Navigate to 08 CTCSS or 09 DCS**
5. **Use [UP]/[DOWN] to select** (e.g., 123.0 Hz for CTCSS)
6. **Press [EXIT]** to save

**Common CTCSS Tones**:
- **67.0 Hz** - Standard amateur (most common)
- **110.9 Hz** - Heavy usage US repeaters
- **162.2 Hz** - GMRS standard

---

## EMERGENCY FREQUENCY CHART

| Region | Frequency | Mode | Notes |
|--------|-----------|------|-------|
| **N. America** | 146.52 MHz | FM | Simplex calling frequency |
| **Europe** | 145.50 MHz | FM | International simplex |
| **UK** | 145.50 MHz | FM | License-required |
| **Australia** | 146.50 MHz | FM | Primary simplex |

**IMPORTANT**: Verify your license covers these frequencies before transmitting.

---

## BATTERY TROUBLESHOOTING

| Symptom | Likely Cause | Fix |
|---------|--------------|-----|
| Battery fully charged but displays 0% | Calibration error | Menu → 13 BatSave → [F] |
| Radio powers off suddenly | Voltage droops below 2.8V | Charge/replace battery |
| Slow spectrum updates with battery low | CPU throttling | Normal; wait for charge |
| Battery icon flashing | Critical voltage <2.5V | Stop all TX immediately |

---

## TECHNICAL NOTES FOR ADVANCED USERS

### Spectrum Analyzer Behavior

- **Waterfall updates every frame** (60Hz display refresh)
- **Spectrum trace based on** 128 frequency bins (configurable step)
- **Peak hold automatically resets** when new scan starts
- **Noise floor fluctuates** ±3-5 dB depending on RF environment
- **Multipath fading** may show multiple peaks at same frequency (normal in urban areas)

### CPU/Battery Optimization

- **High-capacity waterfall** uses ~5% additional CPU
- **Peak hold rendering** is computationally light
- **Spectrum updates throttled** to 6-tick heavy cycle (conserves CPU)
- **Expect: normal 2-3% CPU load for bandscope**

### Known Behaviors (NOT Bugs)

1. **Spectrum grass slows after 2-3 seconds**: EMA filter converges (mathematical norm)
   - **Workaround**: Restart scan to reset
2. **Waterfall shows only 3 lines during RX**: Known firmware limitation
   - **Workaround**: Use listening offset frequency
3. **Peak hold fades gradually**: Exponential decay (designed behavior)
   - **Normal**: Gradual fade over ~30 seconds

---

## KEYPAD REFERENCE MAP

```
         [PWR]
       [EXIT] [MENU]
   [UP]         [DOWN]
   [F]      [PTT]
   
[1] [2] [3]
[4] [5] [6]
[7] [8] [9]
[*] [0] [#]

[SIDE1]     [SIDE2]
(left btn)  (right btn)

Left display    Right display
```

---

## CONTACT & SUPPORT

- **GitHub Repository**: https://github.com/QS-UV-K1Series
- **Issue Tracker**: Report bugs with radio model + firmware version
- **Community Wiki**: FAQs and advanced usage

---

**Document Version**: 1.1 (Firmware 7.6.4br3+, v7.6.0+)  
**Last Updated**: March 2026  
**For**: UV-K1 Series & UV-K5 V3 ApeX Edition

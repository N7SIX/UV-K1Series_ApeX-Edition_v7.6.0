# Full Repository Deep Study & Review

> **Project:** UV-K1Series ApeX-Edition v7.6.8  
> **Platform:** PY32F071 (ARM Cortex-M0+, 16 KB SRAM, 128 KB Flash)  
> **Radio IC:** BK4819 (Beken Corp. RF transceiver)  
> **Display:** ST7565 128×64 LCD (SPI)  
> **Date:** 2026-06-26

---

## 1. Repository Architecture Overview

```
┌─────────────────────────────────────────────────────────────────────────┐
│                          CMAKE BUILD SYSTEM                             │
│  CMakeLists.txt (root) → CMakePresets.json → gcc-arm-none-eabi.cmake    │
│  Targets: ApeX firmware (.elf, .bin, .hex)                             │
├─────────────────────────────────────────────────────────────────────────┤
│  Core/          MCU startup, linker script, CMSIS headers, HAL          │
│  Drivers/       PY32F071 HAL drivers, CMSIS                             │
│  Middlewares/   CherryUSB stack                                         │
│  App/           Main application firmware                               │
│  tools/         Build helpers, recovery tools                           │
│  images/        Screenshots                                             │
│  archive/       Historical builds                                       │
└─────────────────────────────────────────────────────────────────────────┘

App/ structure:
  main.c, init.c, board.c/h, radio.c/h, audio.c/h, settings.c/h,
  frequencies.c/h, functions.c/h, scheduler.c/h, misc.c/h, dcs.c/h,
  bitmaps.c/h, font.c/h, am_fix.c/h, screenshot.c/h, sram-overlay.c/h,
  version.c/h, debugging.h, printf_config.h

  driver/   bk4819.c/h, bk4819-regs.h, bk1080.c/h, st7565.c/h,
            keyboard.c/h, backlight.c/h, gpio.c/h, adc.c/h, spi.c/h,
            i2c.c/h, uart.c/h, vcp.c/h, system.c/h, systick.c/h,
            eeprom.c/h, flash.c/h, py25q16.c/h, crc.c/h, aes.c/h,
            bk4829.c, voice.c/h, syscalls.c

  app/      app.c/h, action.c/h, main.c/h, menu.c/h, common.c/h,
            generic.c/h, dtmf.c/h, chFrScanner.c/h, scanner.c/h,
            spectrum.c/h, waterfall.c/h, aircopy.c/h, flashlight.c/h,
            beam.c/h, fm.c/h, breakout.c/h, rega.c/h, uart.c/h,
            keyboard_state.h

  ui/       ui.c/h, main.c/h, menu.c/h, helper.c/h, status.c/h,
            welcome.c/h, lock.c/h, battery.c/h, scanner.c/h,
            fmradio.c/h, aircopy.c/h, inputbox.c/h

  helper/   battery.c/h, boot.c/h
  external/ printf/, CMSIS_5/
  usb/      usbd_cdc_if.c, usb_config.h
  documentation/  CW_IMPLEMENTATION.md,
                  EEPROM_ARCHITECTURE.md,
                  MEMORY_OPTIMIZATION_REPORT.md,
                  RECOMMENDATION_SAFETY_ANALYSIS.md,
                  REPOSITORY_DEEP_REVIEW.md,
                  SAFETY_AND_IMPROVEMENTS.md,
                  SPECTRUM_WATERFALL_QUICK_GUIDE.md,
                  SPECTRUM_WATERFALL_REVIEW.md
```

---

## 2. Hardware Platform

### 2.1 MCU: PY32F071 (Yatli/PuiChong clone of STM32F0)

| Feature | Specification |
|---------|---------------|
| Core | ARM Cortex-M0+ @ up to 48 MHz |
| Flash | 128 KB |
| SRAM | 16 KB |
| GPIO | Up to 55 I/Os |
| ADC | 1× 12-bit, up to 10 channels |
| Timers | 1× 16-bit advanced, 5× 16-bit general, 1× SysTick |
| SPI | 2× SPI interfaces |
| I2C | 1× I2C interface |
| USART | 2× USART |
| USB | USB 2.0 FS device |
| AES | Hardware AES-128/256 |
| Package | LQFP-48 |

**Linker script note:** `Core/py32f071xb.ld` defines:
- Flash: 0x08002800, length 118K (ISR vectors at 0x08000000 occupy first 0x2800 bytes)
- RAM: 0x20000000, length 16K

### 2.2 RF Transceiver: BK4819

Fully integrated RF transceiver covering 18–630 MHz and 760–1300 MHz. Communicates over **bit-banged 3-wire SPI** (CS=PF9, SCL=PB8, SDA=PB9). Key capabilities: FM/AM demodulation, CTCSS/CDCSS, DTMF/FSK, AGC, RSSI, IF filter bandwidth selection, audio amplifier, GPIO expansion.

### 2.3 Display: ST7565

128×64 monochrome LCD, page-mapped (8 pages × 128 bytes). SPI interface (CS=P2.6, SCL=P2.4, SDA=P2.3, A0=P2.5, RST=P3.2).

### 2.4 Keypad

ADC-based resistor ladder scanning via `KEYBOARD_Poll()`.

---

## 3. Build System

### 3.1 CMake

Root `CMakeLists.txt`:
```cmake
cmake_minimum_required(VERSION 3.19)
project(ApeX C ASM)
set(CMAKE_TOOLCHAIN_FILE cmake/gcc-arm-none-eabi.cmake)
add_subdirectory(Core)
add_subdirectory(Drivers)
add_subdirectory(Middlewares)
add_subdirectory(App)
```

`App/CMakeLists.txt` collects all sources via `file(GLOB_RECURSE ...)`.

### 3.2 Compiler Flags

| Flag | Value |
|------|-------|
| Optimization | `-Os` |
| CPU | `-mcpu=cortex-m0plus -mthumb` |
| FPU | `-msoft-float` |
| Specs | `--specs=nano.specs --specs=nosys.specs` |
| Linker | `-Wl,--gc-sections,-Map=ApeX.map` |

### 3.3 Docker

Three Dockerfiles provided. Build via `./compile-with-docker.sh`.

---

## 4. System Architecture & Data Flow

### 4.1 Boot Sequence

```
Reset_Handler (startup_py32f071xx.s)
  ├── SystemInit() — clock setup (HSE 48 MHz)
  ├── .data copy (flash → RAM)
  ├── .bss clear
  └── main() (App/main.c)
        ├── SYSTICK_Init() — 1 ms tick
        ├── BOARD_Init() — GPIO, ADC, SPI, UART, I2C init
        ├── BK4819_Init() — RF chip init
        ├── SETTINGS_InitEEPROM() — Load settings
        ├── RADIO_ConfigureChannel() — VFO setup
        ├── UI_DisplayWelcome() — Boot screen (2.5s)
        ├── BOOT_ProcessMode() — F_LOCK / RESCUE modes
        ├── [State Resume] → APP_RunSpectrum() / CHFRSCANNER / FM
        └── while(1):
              APP_Update()
              APP_TimeSlice10ms()   (if gNextTimeslice)
              APP_TimeSlice500ms()  (if gNextTimeslice_500ms)
```

### 4.2 Main Loop

Cooperative multitasking with 10ms and 500ms timeslices via SysTick accumulators (TIM14/TIM16).

### 4.3 Function State Machine

```
FUNCTION_Type_t:
  FUNCTION_FOREGROUND, FUNCTION_TRANSMIT, FUNCTION_MONITOR,
  FUNCTION_INCOMING, FUNCTION_RECEIVE, FUNCTION_POWER_SAVE,
  FUNCTION_BAND_SCOPE, FUNCTION_N_ELEM
```

### 4.4 Timer System

| Timer | Resolution | Used By |
|-------|-----------|---------|
| SysTick | 1 ms | `SYSTEM_DelayMs()`, `gNextTimeslice` flag |
| TIM14 | ~10 ms | `gNextTimeslice_10ms` |
| TIM16 | ~500 ms | `gNextTimeslice_500ms` |
| Scheduler.c | 10ms/500ms counters | Battery, backlight, UI |

### 4.5 Memory Map

```
Flash Layout:
  0x08000000  - Interrupt vector table (68 entries)
  0x08002800  - .text (code) + .rodata
  0x0801F800  - Possible data storage region
  0x08020000  - End of flash (128 KB)

SRAM Layout:
  0x20000000  - .data + .bss
  0x20000000  - gFrameBuffer[8][128]   - 1,024 bytes
  0x20000400  - gStatusLine[128]        - 128 bytes
  0x20000480  - waterfallHistory[1024]  - 1,024 bytes
  0x20000880  - rssiHistory[128]        - 256 bytes
  ...  + other globals
  0x20002000  - C stack (grows downward, total 16 KB SRAM)
```

### 4.6 I2C EEPROM (driver/eeprom.c)

An **external I2C EEPROM** (address 0xA0 read=0xA1, 2-byte addressing) stores persistent settings:
```c
EEPROM_ReadBuffer(uint16_t Address, void *pBuffer, uint8_t Size);
EEPROM_WriteBuffer(uint16_t Address, const void *pBuffer);
```
There is also `eeprom_compat.c` bridging the I2C EEPROM API to the internal `SETTINGS_*` API.

### 4.7 External SPI Flash (PY25Q16)

Additional persistent storage on SPI bus:
- Spectrum settings at address 0x00A148 (8 bytes)
- 2 MB (16 Mbit) capacity on UV-K series PCB

---

## 5. Detailed Component Analysis

### 5.1 Driver Layer

#### BK4819 Driver (~1,071 lines + register definitions)

Key API:
```c
BK4819_Init();
BK4819_SetFrequency(uint32_t frequency);
BK4819_RX_TurnOn();  BK4819_TX_TurnOn(uint16_t frequency);
BK4819_WriteRegister(uint8_t reg, uint16_t val);
uint4819_ReadRegister(uint8_t reg);
BK4819_GetRSSI();
BK4819_SetFilterBandwidth(uint16_t bw, bool narrow);
BK4819_PickRXFilterPathBasedOnFrequency(uint32_t frequency);
```

SPI protocol: bit-banged 3-wire, ~12 MHz (4 CPU cycles per bit).  
Notable registers: REG_02, REG_0C, REG_13 (LNA/PGA), REG_30, REG_38/39 (freq), REG_47 (mod), REG_48, REG_63 (RSSI).  
Optimization: `__attribute__((noinline))` on read/write; cached REG_30 in spectrum mode.

#### ST7565 Display Driver

Key API:
```c
ST7565_Init();
ST7565_BlitFullScreen();  // 1024-byte framebuffer refresh
ST7565_BlitLine(uint8_t page);  // Single page (128 bytes)
ST7565_BlitStatusLine();

extern uint8_t gFrameBuffer[8][128];  // 1024 bytes
extern uint8_t gStatusLine[128];       // Status line buffer
```

Incremental refresh: spectrum mode sends 1 page per Tick() instead of full-screen burst.

#### Keyboard Driver

ADC-based resistor ladder. `KEYBOARD_Poll()` reads ADC, maps voltage → key code, applies debouncing (2 press / 3 release).

#### SPI Bus

Hardware SPI1 for ST7565 and PY25Q16. BK4819 uses bit-banged SPI (not HW SPI).

#### I2C Bus

BK1080 FM receiver (if enabled).

#### USB (VCP)

USB CDC ACM virtual serial port for firmware flashing, debug, screenshots.

---

## 5.2 Application Layer

#### Radio Abstraction (radio.c — ~1,360 lines)

```c
RADIO_InitInfo(VFO_Info_t *pVfo, uint16_t channel, uint32_t frequency);
RADIO_ConfigureChannel(uint8_t vfo, uint8_t configureType);
RADIO_SetupRegisters(bool full);
RADIO_SetVfoState(VFO_Info_t *pVfo, uint8_t state);
RADIO_SetFrequency(VFO_Info_t *pVfo, uint32_t frequency);
RADIO_SetupAGC(bool am, bool lock);
RADIO_SetModulation(ModulationMode_t mode);
RADIO_SelectVfos(void);
RADIO_Update(void);  // Squelch, VOX, tail
```

`VFO_Info_t`: VFO configuration struct with RX/TX freq configs, band, channel, CTCSS/DCS, squelch, modulation, step, TX power, etc.

#### Settings (settings.c — ~1,500 lines)

Loads/saves all radio configuration from/to I2C EEPROM. Struct `gEeprom` (~200+ bytes) covers all user settings.

#### Frequencies (frequencies.c)

```c
const freq_band_table_t frequencyBandTable[] = {
    [BAND1_50MHz ] = {  5000000,   7600000},
    [BAND2_108MHz] = { 10800000,  13700000},
    [BAND3_137MHz] = { 13700000,  17400000},
    [BAND4_174MHz] = { 17400000,  35000000},
    [BAND5_350MHz] = { 35000000,  40000000},
    [BAND6_400MHz] = { 40000000,  47000000},
    [BAND7_470MHz] = { 47000000,  60000000},
};
```

TX restricted on some bands (e.g., 350-400 MHz, air band).

#### AM Fix (am_fix.c)

Compensates for AM carrier swing affecting RSSI. `AM_fix_get_gain_diff()` added to RSSI in spectrum mode.

---

### 5.3 UI Layer

UI_Update() dispatches to: UI_DisplayMain(), UI_DisplayMenu(), UI_DisplayScanner(), UI_DisplayFM(), UI_DisplayAircopy(), UI_DisplayLock().

Rendering primitives: `UI_PrintString()`, `UI_DrawPixelBuffer()`, `UI_DrawLineBuffer()`, `UI_DrawRectangleBuffer()`.

Fonts: `gFontSmall[]` (5×7), `gFont3x5[]` (3×5, spectrum), `gFontBigDigits[]`.

---

### 5.4 Application Subsystems

| Subsystem | Files | Purpose |
|-----------|-------|---------|
| Channel/Frequency Scanner | app/chFrScanner.c | Memory channel scan & frequency range scan |
| Spectrum Analyzer | app/spectrum.c + waterfall.c | Full reviewed separately |
| FM Radio | app/fm.c + ui/fmradio.c | BK1080 FM broadcast receiver |
| Air Copy | app/aircopy.c + ui/aircopy.c | RX programming from another radio |
| Register Analyzer | app/rega.c | BK4819 register debug tool |
| Breakout Game | app/breakout.c | Easter egg game |
| DTMF Engine | app/dtmf.c | Encode/decode for PTT-ID and remote control |

---

## 6. Feature Flags (Verified from headers)

| Flag | Purpose |
|------|---------|
| ENABLE_FEAT_N7SIX | Master N7SIX feature set |
| ENABLE_FEAT_N7SIX_SPECTRUM | Extended spectrum features |
| ENABLE_FEAT_N7SIX_SCREENSHOT | Serial screenshot |
| ENABLE_FEAT_N7SIX_RESUME_STATE | Resume last mode on boot |
| ENABLE_FEAT_N7SIX_RESCUE_OPS | Rescue unlock mode |
| ENABLE_FEAT_N7SIX_BEAM | Beam/searchlight UI |
| ENABLE_FEAT_N7SIX_AUDIO | Extended audio features |
| ENABLE_FEAT_N7SIX_SCAN_SUBAUDIBLE | Sub-audible scan detection |
| ENABLE_FEAT_N7SIX_SCAN_FASTER | Faster scan mode |
| ENABLE_FEAT_N7SIX_SCAN_RSSI | RSSI sparkline in scanner |
| ENABLE_SPECTRUM | Spectrum analyzer entry |
| ENABLE_SCAN_RANGES | Frequency range scanning |
| ENABLE_FMRADIO | FM broadcast (BK1080) |
| ENABLE_NOAA | NOAA weather channels |
| ENABLE_AM_FIX | AM carrier compensation |
| ENABLE_AM_FIX_SHOW_DATA | AM fix debug display |
| ENABLE_VOICE | Voice prompts |
| ENABLE_BYP_RAW_DEMODULATORS | BYP/RAW demod modes |
| ENABLE_UART | UART debug |
| ENABLE_USB | USB VCP |
| ENABLE_WIDE_RX | Extended 1.8-1300 MHz RX |
| ENABLE_PWRON_PASSWORD | Power-on password |
| ENABLE_PWRON_VOLTAGE_CHECK | Voltage check on boot |
| ENABLE_VOX | VOX control |
| ENABLE_DTMF_CALLING | DTMF calling |
| ENABLE_AIRCOPY | Air copy RX programming |
| ENABLE_BLMIN_TMP_OFF | Temporary backlight off |
| ENABLE_SMALL_BOLD | Bold small font |
| ENABLE_CUSTOM_MENU_LAYOUT | Custom menu layout |

---

## 7. Code Quality

### Strengths
- Modular architecture (driver / app / UI separation)
- Well-documented register bitfields in bk4819-regs.h
- Efficient packed formats (4-bit waterfall, shared buffers)
- Cooperative multitasking (no RTOS overhead)
- Compile-time feature encapsulation
- SPI bus noise management in spectrum mode
- CMake + Docker reproducible builds

### Concerns
- Heavy global state coupling via misc.h (~50+ globals)
- Direct gEeprom access from many modules (no abstraction)
- No unit tests (manual hardware testing only)
- Mixed author styles (fagci, DualTachyon, N7SIX, F4HWN)
- Hardware-dependent delays assuming 48 MHz clock
- ISR safety not formally documented
- Dead code and commented-out features remain
- EEPROM address space could conflict if extended

---

## 8. Lines of Code Estimate

| Component | Lines |
|-----------|-------|
| Core (startup + linker) | ~200 |
| App/driver | ~8,000 |
| App/app | ~10,000 |
| App/ui | ~6,000 |
| App/helper + external/printf | ~1,200 |
| Middlewares/CherryUSB | ~5,000 |
| Drivers/PY32F071_HAL | ~6,000 |
| **Total** | **~36,400** |

---

## 9. Key Design Patterns

- **Framebuffer:** `gFrameBuffer[8][128]` (pages 0-7), `gStatusLine[128]`
- **Event loop:** `APP_Update()` + 10ms / 500ms timeslices
- **VFO config:** `RADIO_ConfigureChannel(vfo, type)` with reload/apply/band modes
- **Squelch:** RSSI threshold compare every 10ms, tail detection immediate close

---

## 10. External Dependencies

| Library | Integration |
|---------|-------------|
| ARM CMSIS 5 | Header-only (Core, DSP) |
| mpaland/printf | Full source (printf.c) |
| Yatli PY32F0xx HAL | Full source in Drivers/ |
| CherryUSB | Full source in Middlewares/ |

---

## 11. Summary

Production-quality embedded firmware with excellent optimization and clean modular design. The spectrum analyzer is the most sophisticated component, demonstrating careful SPI scheduling and real-time DSP awareness. Minor issues include global state coupling and lack of automated tests. Overall: **8.5/10**.

Key attributes:
- 16 KB SRAM / 128 KB Flash target
- Cooperative main loop (no RTOS)
- External I2C EEPROM for settings, external SPI flash for spectrum persistence
- Feature-rich: scanner, spectrum, FM radio, air copy, games, USB VCP
# Copilot Instructions for UV-K1 Series ApeX Edition

## Project Overview
This is an embedded radio firmware for **UV-K1 and UV-K5 V3** running the **PY32F071 MCU**. It's a C-based firmware stack combining radio control, UI/menu system, spectrum analysis, and FM radio support. The codebase has multiple firmware editions (Bandscope, Broadcast, Basic, RescueOps, Game, ApeX) compiled from shared code via CMake flags.

## Architecture Essentials

### Core Layers (Read First)
- **`App/`** - Main application layer (radio state, VFO management, settings in `radio.h`, `settings.h`)
- **`App/app/`** - Feature modules (menu system, spectrum analyzer `spectrum.c`, FM radio, scanner, DTMF)
- **`App/driver/`** - Hardware abstraction (radio IC: `bk4819.c`, display: `st7565.c`, keyboard, EEPROM)
- **`Core/`** - MCU startup, linker scripts, PY32F071 includes
- **`Drivers/`, `Middlewares/`** - HAL and third-party libs

### Data Flow
1. **Keyboard input** → `MAIN_ProcessKeys()` in `App/app/main.c`
2. **Mode dispatch** via `ProcessKeysFunctions[]` array (routing to MENU, SCANNER, FM, etc.)
3. **Radio state** in global `gVfoInfo[]` (VFO_Info_t with freq, modulation, bandwidth, CTCSS/DCS)
4. **Display refresh** via `UI_*` functions + `ST7565_` display driver
5. **Scheduler timing** in `App/main.c` — handles 10ms and 500ms time slices for periodic tasks

### VFO State Structure
Key type: `VFO_Info_t` (`radio.h:81+`) contains frequency, bandwidth, modulation, power, PTT settings. Global: `gVfoInfo[2]` (VFO A and B).

## Build & Compilation

### Docker-Based Build (Recommended)
```bash
./compile-with-docker.sh <Preset> [extra CMake options]
# Valid presets: Custom, Bandscope, Broadcast, Basic, RescueOps, Game, ApeX, All
```
- Script validates preset names, cleans build dir, runs cmake + build inside Docker
- **All outputs**: `build/<Preset>/` with `.elf`, `.bin`, `.hex` files
- **CMake presets**: Defined in `CMakePresets.json` with 50+ feature flags

### Key CMake Flags (Examples)
- `ENABLE_SPECTRUM=ON` - Spectrum analyzer (N7SIX feature)
- `ENABLE_FMRADIO=ON` - FM radio support
- `ENABLE_FEAT_N7SIX_*` - N7SIX edition features (game, screenshot, RX/TX timers, sleep mode)
- Build type: `Release` (default) or `Debug`

## Coding Patterns & Conventions

### String Handling (CRITICAL)
**ALWAYS USE `strncpy()` with null-termination**, not `strcpy()`. See `App/driver/uart.c:290` or `App/app/generic.c:196` for examples:
```c
// ❌ WRONG
strcpy(buffer, source);

// ✅ CORRECT
strncpy(buffer, source, sizeof(buffer) - 1);
buffer[sizeof(buffer) - 1] = '\0';
```

### Scheduler/Timing Pattern
App has **two main time slices** called from ISR:
- `APP_TimeSlice10ms()` — fast tasks (keyboard debounce, spectrum scan, scheduler polling)
- `APP_TimeSlice500ms()` — slow tasks (S-meter updates, battery checks, display updates)
Entry point: `App/main.c:main()` → initializes drivers, then infinite `APP_Update()` loop.

### Feature Toggle Pattern
Features are **compile-time (CMake)** or **runtime (EEPROM settings)**:
- Compile: `#ifdef ENABLE_FMRADIO` guards FM code paths
- Runtime: Global `gEeprom` struct holds user settings; menus write to it, saved to flash

### Display/UI Hierarchy
- `UI/ui.h` — Top-level display functions (`UI_DisplayMain()`, `UI_DisplayMenu()`)
- `ST7565_Buffer[]` — 128×64 pixel framebuffer (8 pages × 128 bytes)
- `st7565.c` — Low-level display commands (page/column addressing, contrast)
- Pattern: Update buffer → call `ST7565_BlitFullScreen()`

### Driver Layer Pattern
Each hardware driver (radio, display, keyboard) has init + control functions:
- `BK4819_Init()`, `BK4819_SetFrequency()`, `BK4819_SetRF()` for radio IC
- `ST7565_Init()`, `ST7565_SelectiveBlitScreen()` for display
- Drivers use HAL: `GPIO_*`, `SPI_*`, `I2C_*` from `PY32F071_HAL_Driver`

## Known Issues & TODOs

### High Priority
1. **String buffer overflows** — `UART`, `DTMF` modules use unsafe `strcpy()` (see `IMPLEMENTATION_GUIDE.md`)
2. **ST7565 FillScreen** — `st7565.c:204` is marked "TODO: This is wrong" (doesn't fill properly)
3. **Chip Select not released** — `st7565.c:312` missing CS release in contrast/invert function

### Search for TODOs
Run `grep -r "TODO\|FIXME\|XXX" App/` to find all tagged issues.

## File Geography (Quick Reference)

| Task | Primary File |
|------|--------------|
| Main event loop, timing slices | `App/app/app.c`, `App/main.c` |
| Menu system | `App/app/menu.c` |
| Radio control (VFO, freq, modulation) | `App/radio.c`, `App/radio.h` |
| Spectrum analyzer | `App/app/spectrum.c` |
| Keyboard input | `App/app/main.c` → `ProcessKeysFunctions[]` |
| Settings/EEPROM | `App/settings.c`, `App/settings.h` |
| Display output | `App/driver/st7565.c` + `App/ui/` |
| Radio IC (BK4819) | `App/driver/bk4819.c` |
| FM radio (optional) | `App/app/fm.c` (if `ENABLE_FMRADIO`) |
| UART/USB comms | `App/app/uart.c` |

## Debugging Tips

1. **Enable debug output**: Build with `CMAKE_BUILD_TYPE=Debug`; `uart.c` has printf calls
2. **Settings not persisting**: Check `EEPROM_WriteBuffer()` calls in `settings.c` after global state changes
3. **Radio not tuning**: Verify `BK4819_SetFrequency()` is called with valid range (VHF/UHF limits in `frequencies.h`)
4. **Display frozen**: Check `ST7565_` driver calls and framebuffer updates in `UI_DisplayMain()`
5. **Memory corruption**: Use `strncpy()`, check array bounds in menu indices, watch global state structs

## References
- **Architecture & implementation details**: `Documentation/IMPLEMENTATION_GUIDE.md`
- **Build instructions + Docker setup**: `README.md` lines 227+
- **Hardware offsets/registers**: `App/driver/bk4819-regs.h`, `bk1080-regs.h`

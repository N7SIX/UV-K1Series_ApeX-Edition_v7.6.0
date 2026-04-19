# Feature Flags and EEPROM Schema Documentation

## Feature Flags (CMake and Compile-Time)

The firmware uses CMake feature flags to enable/disable features at compile time. These are set in `CMakePresets.json` and referenced in code with `#ifdef ENABLE_*`.

### Common Feature Flags
- `ENABLE_SPECTRUM` — Enables spectrum analyzer
- `ENABLE_FMRADIO` — Enables FM radio support
- `ENABLE_FEAT_N7SIX_GAME` — Enables N7SIX game feature
- `ENABLE_FEAT_N7SIX_SCREENSHOT` — Enables screenshot feature
- `ENABLE_FEAT_N7SIX_RXTX_TIMERS` — Enables RX/TX timers
- `ENABLE_FEAT_N7SIX_SLEEP` — Enables sleep mode
- ... (see CMakePresets.json for full list)

### How to Add/Use Feature Flags
- Add to `CMakePresets.json` under the desired preset.
- Reference in code with `#ifdef ENABLE_*`.

## EEPROM Schema

EEPROM is used to persist user settings, VFO state, and other runtime configuration. The schema is defined in `App/settings.h` and `App/settings.c`.

### Main EEPROM Structure
- `gEeprom` (type: `EEPROM_Settings_t`)
  - Stores user settings, menu options, calibration, etc.
  - Versioned and migrated via `settings_migration.c`.

### Key Fields
- `uint8_t version` — EEPROM schema version
- `uint8_t menu_settings[...]` — Array of menu settings
- `uint16_t vfo_freq[2]` — VFO A/B frequencies
- `uint8_t calibration[...]` — Calibration data
- ... (see settings.h for full structure)

### Migration
- Handled in `App/settings_migration.c`.
- On version change, data is migrated and defaults are set for new fields.

### Data Integrity
- (Recommended) Add CRC/checksum to the structure for corruption detection.

---

For a full list of feature flags, see `CMakePresets.json`.
For EEPROM structure details, see `App/settings.h` and `App/settings.c`.

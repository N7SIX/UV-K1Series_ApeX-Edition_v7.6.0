# RSSI Deep Study (ApeX v7.6.0)

Date: 2026-04-04
Scope: End-to-end RSSI path review in spectrum and UI flows.

## Files Reviewed
- App/app/spectrum.c
- App/app/spectrum.h
- App/driver/bk4819.c
- App/ui/main.c

## Executive Summary
The RSSI pipeline is generally robust and already includes important protections:
- stale-bin reset on sweep restart,
- signed-clamped still-mode offset correction,
- per-bin smoothing and display trace stabilization,
- mode-agnostic trigger marker bounds.

However, there are two high-value improvements and several medium-priority consistency fixes that would improve reliability and user trust in RSSI behavior.

Update (Implemented on 2026-04-04):
- `GetRssi()` now uses bounded wait retries before reading RSSI.
- AM correction in `GetRssi()` now uses signed intermediate math with clamp.
- EEPROM load validation for `dbMin`/`dbMax` now matches runtime-allowed range and includes a `dbMin <= dbMax` guard.

## Current RSSI Data Path
1. Acquisition:
   - `GetRssi()` in spectrum path reads BK4819 RSSI via `BK4819_GetRSSI()`.
2. Conditioning:
   - AM fix gain delta may be applied in scan/listen path.
   - Still mode applies additional frequency-based calibration offset.
3. Statistics:
   - `scanInfo.rssiMin/rssiMax` and peak data update per sweep.
4. Triggering:
   - `AutoTriggerLevel()` adjusts threshold from scan peak with hysteresis-style behavior.
   - Manual trigger changes are pixel-space stepped and mapped back through `RssiFromY()`.
5. Rendering:
   - `Rssi2DBm()` converts raw RSSI to dBm using per-band correction table.
   - `Rssi2PX()` and `Rssi2Y()` map dBm to on-screen geometry.

## Findings (Ordered by Severity)

### 1) High: Possible unsigned underflow/overflow in AM correction inside scan path
Location:
- `App/app/spectrum.c` `GetRssi()` (~line 670+)

Observation:
- RSSI is stored as `uint16_t`.
- AM correction does `rssi += AM_fix_get_gain_diff() * 2;` where `AM_fix_get_gain_diff()` is `int8_t`.
- Negative correction can underflow and wrap to a large positive number.

Impact:
- False high RSSI spikes in scan/listen path.
- Incorrect trigger events and peak locking.
- Distorted waterfall/spectrum rendering under AM fix scenarios.

Recommendation:
- Perform correction in signed intermediate type (`int32_t` or `int16_t`) and clamp before cast back to `uint16_t`.

---

### 2) High: Potential blocking loop in `GetRssi()` with no timeout guard
Location:
- `App/app/spectrum.c` `GetRssi()` (~line 670+)

Observation:
- Code loops while register 0x63 low byte remains >= 255:
  - `while ((BK4819_ReadRegister(0x63) & 0b11111111) >= 255) SYSTICK_DelayUs(100);`
- No max retry count or timeout fallback.

Impact:
- If RF/AGC state sticks or register reads become pathological, scanner task can stall.
- UI responsiveness and scan progression can degrade.

Recommendation:
- Add bounded retries (for example 10-30 iterations).
- On timeout, proceed with best-effort read and optionally count telemetry/debug occurrences.

---

### 3) Medium: dBm max runtime range conflicts with persisted validation range
Locations:
- `App/app/spectrum.c` `UpdateDBMax()` (~line 1284+) allows up to `10` dBm.
- `App/app/spectrum.c` `SPECTRUM_LoadSettings()` (~line 926+) only accepts `dbMax <= DISPLAY_DBM_MAX` where `DISPLAY_DBM_MAX = -50`.

Observation:
- Runtime permits values that load-time validation rejects.

Impact:
- Non-persistent behavior across reboot (user-set dbMax can silently revert).
- Inconsistent UX and hard-to-understand display scale jumps.

Recommendation:
- Align one policy:
  - either clamp runtime edits to `[DISPLAY_DBM_MIN, DISPLAY_DBM_MAX]`,
  - or broaden load-time validation to match runtime policy.

---

### 4) Medium: Real hardware RSSI width appears 9-bit but many comments/state use 16-bit framing
Locations:
- `App/driver/bk4819.c` `BK4819_GetRSSI()` masks `0x01FF`.
- Several comments in spectrum/docs describe 16-bit dynamic range and use `RSSI_MAX_VALUE = 65535`.

Observation:
- Effective raw RSSI appears to be 0..511 from hardware read.

Impact:
- Confusing assumptions in future maintenance.
- Harder to tune trigger defaults and UI mapping scientifically.

Recommendation:
- Keep sentinel approach if desired, but document effective RSSI measurement range explicitly.
- Consider defining both:
  - `RSSI_HW_MAX` (effective data range), and
  - `RSSI_SENTINEL` (special control marker).

---

### 5) Medium: Trigger adaptation constants are static and not tied to measured noise statistics
Location:
- `App/app/spectrum.c` `AutoTriggerLevel()` (~line 1017+)

Observation:
- Trigger update uses fixed deltas and deadband (e.g., +3 base, >5 threshold, +/-1/3 slew).

Impact:
- Works reasonably but may be suboptimal across very quiet vs very busy bands.

Recommendation:
- Optionally derive adaptive margins from recent floor spread (for example percentile-based floor + margin).
- Keep current logic as fallback for deterministic behavior.

## Validation Checks Performed in This Study
- Verified scan->listen->still RSSI flow and key update functions.
- Verified trigger marker and trigger value path are mode-agnostic in Y-space.
- Verified still-mode calibration path already uses signed intermediate with clamp.

## Suggested Improvement Order
1. (Done) Fix AM correction arithmetic in `GetRssi()`.
2. (Done) Add timeout guard in `GetRssi()` pre-read loop.
3. (Done) Unify dBm range policy between runtime editing and EEPROM load.
4. Cleanly separate hardware max vs sentinel semantics.
5. (Optional) Add adaptive trigger margins using floor statistics.

## Minimal Test Plan for RSSI Changes
- AM mode + AM fix enabled:
  - confirm no abnormal spikes and no wraparound-like jumps.
- Stress idle/noisy RF band:
  - verify scan loop never stalls and UI remains responsive.
- dbMin/dbMax persistence:
  - set max/min, reboot, confirm values reload consistently.
- Trigger stability:
  - verify no oscillation during low-SNR noise and quick recovery after strong transient.
- Cross-mode consistency:
  - compare trigger behavior and dBm display in SPECTRUM vs STILL.

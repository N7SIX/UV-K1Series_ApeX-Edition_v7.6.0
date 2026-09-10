# BatCal — Battery Calibration Guide

This guide combines the current implementation status from
[BATCAL_DEEP_AUDIT.md](BATCAL_DEEP_AUDIT.md) with a practical, step-by-step
procedure for calibrating the battery voltage reading on a UV-K1 Series radio
running the ApeX firmware (v7.6.10D).

## 1. What BatCal does

BatCal is a 2-point linear calibration between two reference voltages:

| Point | Reference | Menu label |
|---|---|---|
| High | 8.40 V | `Hi` |
| Low | 6.00 V | `Lo` |

The firmware converts the raw ADC reading to a calibrated voltage by
interpolating between these two points (`BATTERY_CalibrateRaw()` in
`App/helper/battery_calibration.c`). This calibrated voltage is then used for:

- The main battery percentage and icon.
- The spectrum-mode status bar.
- Low-battery and critical-battery warnings (per Battery Type, see
  [BATCAL_DEEP_AUDIT.md §7](BATCAL_DEEP_AUDIT.md#7-battery-type-integration)).

See [BATCAL_DEEP_AUDIT.md](BATCAL_DEEP_AUDIT.md) for the full technical audit,
implemented fixes, and remaining hardware-validation items.

## 2. Before you start

You will need:

- A stable, adjustable DC power supply, **or** a fully charged known-good
  battery that reads a true **8.40 V** at rest (measured off-radio, right off
  the charger, with no load), **or** two known-good reference batteries.
- A trusted multimeter to confirm the actual voltage at the radio's battery
  terminals (not the supply's front-panel readout, and not the charger's
  claimed "full" indicator).
- The radio powered from the reference source with stable, settled voltage
  (allow a few seconds for the ADC average to catch up after any change).
  A battery's voltage sags slightly under load, so re-check with the
  multimeter while the radio is powered on, not just before inserting it.

**Recommended order:** calibrate **Hi first, then Lo**, because the `Auto-Cal`
option for Lo derives its value from the stored Hi point.

## 3. Step-by-step calibration

1. Power the radio from either:
   - a power supply set to **8.40 V**, confirmed with a multimeter at the
     battery terminals, **or**
   - a fully charged battery independently confirmed at **8.40 V** with a
     multimeter while installed and powering the radio.
2. Enter the hidden menu (`PTT` + side-key at power-on, per your build), then
   navigate to `BatCal`.
3. Press `MENU` to enter. The picker opens on **`Hi`** first.
4. Press `MENU` again to edit the Hi value.
5. Watch the `Live` row until the displayed voltage settles.
6. Type the 4-digit raw ADC value directly, **or** use the current displayed
   value only if it already matches expectations — normally you adjust with
   the arrow keys while watching `Live` approach `8.40V`.
7. Press `MENU` to accept. Invalid values (outside 1650–3900, or not exceeding
   the stored Lo point) are rejected with a beep and not saved.
8. Power the radio from a source set to **6.00 V**, confirmed with a
   multimeter.
9. Return to `BatCal`. Press `DOWN` to select **`Lo`**, then `MENU`.
10. Choose:
    - **`Auto-Cal`** — applies a derived preset from the Hi point and exits
      immediately. Use this if you do not have a precise 6.00 V reference.
    - **`Custom`** — opens numeric editing so you can match the displayed
      `Live` voltage to your 6.00 V reference.
11. If editing `Custom`, adjust until `Live` reads `6.00V`, then press `MENU`
    to accept. Values outside 1000–4000, or not below the stored Hi point,
    are rejected.
12. Exit the menu. The calibration is saved immediately on each accept — no
    separate "save" step is required.

## 4. Verifying the result

- Return to the main screen and confirm the battery percentage looks
  reasonable for the reference voltage in use.
- Re-enter `BatCal` and confirm the Hi/Lo picker shows `AUTO` or `CUST` next
  to Lo, matching what you selected.
- If available, check the spectrum-mode status bar reads the same voltage as
  the main screen (both use the same calibration helper).

## 5. Troubleshooting

| Symptom | Likely cause | Action |
|---|---|---|
| `MENU` beeps and does not accept a value | Value outside the valid range, or Lo/Hi would overlap | Re-check the reference voltage and retype all 4 digits |
| Displayed voltage does not match the reference | ADC not settled, or wrong reference point selected | Wait a few seconds; confirm you are editing Hi vs. Lo correctly |
| Battery percentage looks wrong on a known-good battery | Battery Type mismatch, not a BatCal issue | Check `BatTyp` — it selects the discharge curve, independent of BatCal |
| Values revert to old numbers after re-entering BatCal | Entry was not completed (fewer than 4 digits) or was rejected | Repeat entry with the full 4-digit value within limits |

## 6. Reference limits

| Field | Valid range | Notes |
|---|---|---|
| Hi (raw ADC @ 8.40 V) | 1650–3900 | Must exceed the stored Lo value |
| Lo (raw ADC @ 6.00 V) | 1000–4000 | Must be below the stored Hi value |

These limits and the calibration math are implemented in
`App/helper/battery_calibration.c` and validated by `tests/test_batcal_math.c`
(`batcal_math_tests` CMake/CTest target).

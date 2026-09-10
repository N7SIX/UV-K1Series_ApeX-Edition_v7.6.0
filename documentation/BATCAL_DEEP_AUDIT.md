# BatCal — Full Deep Review Audit (2‑Point Battery Calibration)

**Scope:** the pending v7.6.10D 2‑point battery-calibration feature (`BatCal`) as it exists in the working tree on branch `main` (base `e09bd43`), covering:
`App/helper/battery.c/h` · `App/settings.c` · `App/app/menu.c/h` · `App/ui/menu.c/h` · `App/app/spectrum.c` · `App/app/app.c`

**Method:** full code trace of the nested state machine + display path, host‑side numeric validation of the math (mirror of a known‑good copy of the formulas), and ARM‑toolchain `-fsyntax-only` compilation of every modified file with the exact ApeX preset defines.

**Result summary:** the code **compiles clean** (all 5 modified C files pass the ARM syntax check with the ApeX define set) and the **calibration arithmetic is internally correct** (migration, factory preset, interpolation, clamps — see tests/test_batcal_math.c). However the feature has **one crash path, one wrong‑readout path, and several UI/UX defects** that need fixing before it can be considered "fully working".

**Current result summary:** the pending v7.6.10D implementation compiles clean with the ApeX ARM preset, uses one shared production calibration helper across battery, spectrum, and UI paths, validates the EEPROM record and Lo/Hi relationship, and passes the production-helper regression tests. The remaining release gates are hardware validation of ADC/reference behavior and power-loss recovery during the first migration write.

> Historical findings D-1 through D-12 below document the defects found during development. The remediation section and the pending v7.6.10D changelog are authoritative for the current source state.
---

## 1. What the implementation does (as built)

| Aspect | Where | Verdict |
|---|---|---|
| 2‑point interpolation (slot0=6.0 V raw, slot3=8.4 V raw) with clamp to [6.00V,8.40V] | `battery.c BATTERY_GetReadings()` | ✅ math verified |
| Single‑point through‑origin fallback when slot0 absent (*no* 6.0 V floor) | `battery.c` else‑branch | ✅ runs, but see D‑5 |
| V1→V2 migration `slot3 ×= 840/760`, marker `BATCAL_FORMAT_V2` in slot2 | `settings.c SETTINGS_LoadCalibration()` | ✅ verified (2192→2422, blank→2210, idempotent) |
| Nested editor state machine `gBatCalStage` 0→1→2 / `gBatCalTarget` 0/1 | `app/menu.c` (EXIT/MENU/UP/DOWN/0‑9) | ✅ structurally complete |
| Picker UI (`Cal Lo / Cal Hi`, Factory/Custom, value edit) | `ui/menu.c UI_DisplayMenu()` | ⚠ see D‑4/D‑5 |
| Fill‑in numeric template (`2___`→`2000`) incl. 4‑digit BatCal | `ui/menu.c` + `MENU_Key_0_to_9()` | ✅ works |
| Immediate persist on accept / one‑time migration write | `SETTINGS_SaveBatteryCalibration()` | ✅ |
| Spectrum status‑bar battery % | `spectrum.c DrawStatus()` | ❌ **D‑2** |

---

## 2. Defects

### D‑1 · HIGH — menu/list preview and Cal‑Hi preview can divide by zero (`ui/menu.c:1581`, `:1634`)
```c
const uint16_t vol = (uint32_t)gBatteryVoltageAverage * gBatteryCalibration[3] / gSubMenuSelection;
```
- List preview (`!gIsInSubMenu`, `:1581`): `gSubMenuSelection` was just loaded as `gBatteryCalibration[3]` by `MENU_ShowCurrentSetting()`. **If slot3 == 0 the preview computes `x * 0 / 0`** → AArch32 `__aeabi_uidiv` divide‑by‑zero (returns garbage; can wedge on some libgcc builds — this is the same class of bug audit F‑1 already fixed for the key path, but the *display* path was never guarded).
- How can slot3 be 0? The one‑time migration repairs slot3 only when the V2 marker is **absent**. A marker set with `slot3 == 0` (or a block that is otherwise corrupt but marker‑guarded) is not repaired — the corruption check only handles `slot0 >= 5000`.
- Stage‑2 Cal‑Hi preview (`:1634`) divides by the candidate `gSubMenuSelection`, which the key handler clamps to min (1650) — safe there, but the guard belongs in both sites.

**Fix:** guard both sites, e.g.
```c
const uint16_t vol = (gSubMenuSelection && gBatteryCalibration[3])
                     ? (uint32_t)gBatteryVoltageAverage * gBatteryCalibration[3] / gSubMenuSelection
                     : gBatteryVoltageAverage;
```
and harden `SETTINGS_LoadCalibration()` to also repair `slot3 == 0` on marker‑present blocks.

### D‑2 · HIGH — spectrum status‑bar battery % never uses the 2‑point model (`spectrum.c:1580-1587`)
```c
uint16_t voltage = (gBatteryVoltages[0] + ... ) / 4 * 840 / gBatteryCalibration[3];
unsigned perc   = BATTERY_VoltsToPercent(voltage);
```
- Only the constant changed (760→840). The reader still uses the **single‑point through‑origin** line and **ignores `gBatteryCalibration[0]`** — so once a user calibrates Cal Lo, the status bar disagrees with the main battery icon/percentage for the same physical battery (the CHANGELOG claims this path was "likewise" re‑anchored — only the constant was).
- It also divides by `gBatteryCalibration[3]` unguarded (D‑1 family) and lacks the 2‑point `[600,840]` clamp.

**Fix:** route the status readout through the same math as `BATTERY_GetReadings()` (ideally call it, or extract a `BATTERY_Calibrate(raw_avg)` helper used by both), including the zero guard.

### D‑3 · MEDIUM — the Cal‑Hi "what will this read" preview is only valid in fallback mode (`ui/menu.c:1632-1635`)
```c
const uint16_t vol = (uint32_t)gBatteryVoltageAverage * gBatteryCalibration[3] / gSubMenuSelection;
```
`gBatteryVoltageAverage` is a **calibrated centivolt figure**, not a raw ADC count. The identity `avg × cal3 == raw × 840` holds **only** in the single‑point fallback. Once slot0 is calibrated (2‑point), scaling a *voltage* by a *raw* ratio produces a meaningless candidate preview (e.g. editing Cal Hi with slot0 active shows the wrong resulting voltage), and it will disagree with the value the radio actually reports after commit.

**Fix:** compute the candidate preview by inverting the model, i.e. for a candidate `cal` use the current raw average and the same `600 + (raw−lo)·240/(hi−lo)` formula with `hi := cal` (and guard `cal > cal_lo`), or drop the fake‑voltage line and show the raw value only. Same for the menu‑list preview formula, which is currently correct **only by coincidence** (selection == slot3 ⇒ result ≡ avg).

### D‑4 · MEDIUM — no validity check for Lo ≥ Hi; accepting silently corrupts the model
- Cal Lo range is 1000–4000, Cal Hi 1650–3900 — nothing stops `slot0 ≥ slot3`.
- The interpolator then silently falls back to single‑point (`cal_lo > 0 && cal_hi > cal_lo` fails), so the user's Lo calibration is quietly ignored, and the F/C picker label still shows the preset value.
- No on‑screen warning, no clamp.

**Fix:** either clamp the committed pair (`slot0 ≤ slot3 − 1` when saving either slot) or validate in `MENU_AcceptSetting()` and refuse/beep. Tighten the arrow‑key numeric ranges to depend on the other slot.

### D‑5 · MEDIUM — no 6.0 V floor in the default (fallback) mode → inconsistent low‑battery behaviour
After migration slot0 == 0, so the model is through‑origin (no clamp). The stage‑2 Lo label says `…@6V` and the picker shows a 6.0 V reference, but the reported voltage can go significantly below 6.0 V on a low cell, which changes when the critical‑battery path triggers *before* the user stores a Lo calibration. Either document this in the UI or clamp the fallback to 600 cV as well (making the fallback the 2‑point model with a virtual slot0 = the factory preset).

### D‑6 · LOW — `gBatCalStage`/`gBatCalTarget` are global and left set after stage‑0 EXIT / factory accept
They are re‑initialized on every MENU entry (good), but **not** reset when leaving the sub‑menu; any future code that inspects these globals outside the BatCal flow will see stale state. `gBatCalStage` also remains 1 or 2 after an accept until the next re‑entry.

### D‑7 · LOW — stage‑1 (Factory/Custom) shows neither the current stored Lo value nor the F/C state
`line2/line3` ("Custom / Edit value") do not display the current `slot0` value, so the user cannot see what they would be editing, and nothing indicates whether the present value is Factory or Custom at this stage (the F/C letter only exists on the Lo/Hi picker).

### D‑8 · LOW — no factory reset for Cal Hi, and no per‑item "set to factory" anywhere
Only Cal Lo has a "Factory" option. Cal Hi always enters direct numeric edit; a user who wants the factory high point must remember the value (2210 post‑migration default) and type it.

### D‑9 · LOW — partial typed entry commits the clamped minimum
Typing `2` then MENU in Cal Hi stores `1650` (min), not `2`. The fill‑in template makes the digit‑count expectation visible, but the accept path still silently promotes short input to the range minimum, which is surprising for a calibration menu.

### D‑10 · INFO — 1 Hz blinking `>` marker
`arrowSel = blinkOn ? '>' : ' '` toggles the active‑item arrow on/off at 1 Hz (driven by `gFlashLightBlinkCounter`, which ticks every 10 ms in `app.c` regardless of flashlight state — verified). Common convention on a 1‑bit display, but it means the selected item briefly shows *no* indicator; consider a steady `>` with blink feedback only on the secondary line.

### D‑11 · INFO — live voltage previews refresh slowly
`gBatteryVoltageAverage` updates every ~500 ms (ADC sampling cadence) and `UI_DisplayMenu` does not force a refresh for BatCal (battery.c's display poke is limited to `MENU_VOL`). The stage‑2 preview can visibly lag the value being typed.

### D‑12 · INFO — state ordering with the hidden menu
`MENU_BATCAL_LOW` was injected before `MENU_BATCAL`, shifting every later enum value. Verified symbol‑only usage across the tree (`MenuList`, `UI_MENU_GetMenuIdx`, `FIRST_HIDDEN_MENU_ITEM`, `menu_id − MENU_F1SHRT` indexing), so no persisted or hard‑coded ordinal breaks — but any future EEPROM or config that stores menu ids must not rely on numeric stability.

---

## 3. Verified‑good / by‑design

- **Migration math** — tested: 2192→2422, blank flash→2210, marker idempotence, corrupt slot0 handling.
- **Factory preset math** — `(600·slot3)/840` = `slot3·5/7` (e.g. 2422→1730) verified; correct through‑origin derivation.
- **Interpolation** — exact at anchors (6.00 V / 8.40 V), linear mid‑curve, clamps outside the window, safe degenerate handling for `cal_hi==0` and `cal_lo≥cal_hi` (falls back, no div‑by‑zero in `battery.c`).
- **Nested navigation** — EXIT walks back one stage; MENU walks forward; arrows span 0..1 in pickers and the numeric range in stage 2; `MENU_GetLimits` reference‑specific bounds for target 0 are consistently applied in accept, arrows, and the 4‑digit input‑box width.
- **Fill‑in template** — widths adapt per menu; BatCal correctly gets 4 digits.
- **Accept path** — always saves the calibration block immediately (`SETTINGS_SaveBatteryCalibration`), key‑path Min‑clamp (F‑1/F‑4) prevents the old 0→division‑by‑zero on commit.

---

## 4. Test coverage added
`tests/test_batcal_math.c` — production-helper host test covering migration, preset,
interpolation, fallback, limits, and Battery Type thresholds. Run through the
registered CMake target:
```
cmake -S tests -B build/tests -G Ninja
cmake --build build/tests --target batcal_math_tests
build\tests\batcal_math_tests.exe        # -> ALL PASS
```

## 5. Recommended fix order
1. D‑1 guards (crash class) + slot3/0 repair in `SETTINGS_LoadCalibration()`.
2. D‑2 share one calibrate helper with `spectrum.c`.
3. D‑4 Lo/Hi cross‑validation (silent model corruption).
4. D‑3 correct the Cal‑Hi candidate preview.
5. D‑5, D‑7, D‑8, D‑9 UX polish.

---

## 6. Remediation applied

The following hardening changes are now implemented:

- Shared production calibration model in `App/helper/battery_calibration.c` and `.h`.
- Normal battery readings, spectrum status, and the Cal Hi preview use the same helper.
- Calibration records are range-checked on load, including marker-present records.
- Invalid Lo/Hi pairs are rejected at the save boundary and editing limits are cross-constrained.
- All calibration-dependent display paths avoid division by the user-editable value.
- The 3.5 Ah critical threshold includes the 6.00 V clamp boundary.
- `tests/test_batcal_math.c` now exercises the production helper and is registered as the
    `batcal_math_tests` CMake/CTest target.

The clean ApeX firmware build and standalone BatCal CMake test build pass. Hardware
validation remains recommended for EEPROM power-loss behavior, ADC noise near both
calibration anchors, and the user-visible low-battery transition on each battery type.

## 7. Battery Type integration

`Battery Type` remains intentionally separate from BatCal. BatCal converts raw ADC
counts to calibrated voltage; Battery Type selects the discharge curve used for
percentage and critical-battery behavior.

The critical-voltage policy is now centralized for every supported type:

| Battery Type | Critical threshold |
|---|---:|
| 1600 mAh | <= 6.30 V |
| 2200 mAh | <= 6.30 V |
| 3500 mAh | <= 6.00 V |
| 1500 mAh | <= 6.30 V |
| 2500 mAh | <= 6.23 V |

These thresholds operate on the calibrated voltage produced by BatCal and do not
alter the calibration curve itself.
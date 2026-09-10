# MENU SYSTEM — FULL DEEP AUDIT (v7.6.x)

**Scope:** every item in `MenuList[]` (`App/ui/menu.c`) cross-checked against its four
implementation pillars in `App/app/menu.c` + `App/ui/menu.c`:

1. `MENU_GetLimits()` — min/max bounds
2. `MENU_ShowCurrentSetting()` — value loaded from EEPROM / VFO on entry
3. `MENU_AcceptSetting()` — value written back + save flags (`gRequestSaveSettings` / `gRequestSaveChannel`)
4. `UI_DisplayMenu()` — rendering, string tables, gauges, badges

**Audit result legend:** ✅ verified correct · ⚠ finding (see ID) · 🔍 info / by-design

---

## 1. Architecture verification (all PASS)

| Aspect | Verdict |
|---|---|
| `MenuList[]` names ≤ 6 chars fit `name[7]` | ✅ |
| `gMenuListCount` stops at `FIRST_HIDDEN_MENU_ITEM == MENU_F_LOCK`; hidden items (F Lock, 350 En, FrCali, BatCal, BatTyp, SetNav, Reset) only reachable via PTT+side-key at power-on | ✅ |
| Value refresh on cursor move & submenu entry via `gFlagRefreshSetting` → `MENU_ShowCurrentSetting()` in main loop (`app.c:2805`) | ✅ |
| `MENU_AcceptSetting()` clamps selection to `[Min,Max]` before writing | ✅ |
| Save-flag discipline: per-channel items use `gRequestSaveChannel` + `return`; global items `break` → `gRequestSaveSettings`; action-only items (`D_LIST`, `RESET`, `F_CALI`, `BATCAL`, `STEP`) handle their own persistence | ✅ |
| Display buffer `String[64]` sufficient for all composed multi-line strings (longest ≈ 24 chars) | ✅ |
| Sub-menu string array sizes all match their limits (see per-menu table) | ✅ |
| `UI_MENU_GetCurrentMenuId()` OOB-safe fallback | ✅ |
| EEPROM read-back validation on boot (e.g. `gSetting_F_LOCK < F_LOCK_LEN`, `DTMF_ValidateCodes`, power/typ nibbles) | ✅ |


---

## 2. Per-menu audit table (N7SIX build feature set)

| Menu | Limits (min..max) | Show | Accept | Display | Notes |
|---|---|---|---|---|---|
| Sql | 0..9 | ✅ SQUELCH_LEVEL | ✅ + `gSquelchLevelOriginal` restore | ✅ | |
| Step | 0..`STEP_N_ELEM-1` (sorted) | ✅ `GetSortedIdxFromStepIdx` | ✅ maps back, saves channel if VFO | ✅ kHz | |
| W/N | 0..1 | ✅ | ✅ saves channel | ✅ | |
| Power (TXP) | 0..7 = `OUTPUT_POWER` enum (USER,LOW1..5,MID,HIGH) | ✅ | ✅ saves channel | ⚠ F-2 | `USER` delegates to `SetPwr`; `radio.c` maps correctly |
| BatSav | 0..5 | ✅ | ✅ | ✅ `1:%u` | |
| VOX | 0..10 | ✅ `VOX_SWITCH ? LEVEL+1 : 0` | ✅ + `LoadCalibration` + reconfig | ✅ | |
| RxMode (TDR) | 0..3 | ✅ bit-composed from DUAL_WATCH/CROSS_BAND | ✅ inverse mapping + `gDW/gCB` state | ✅ 4 strings | |
| Beep | 0..1 | ✅ | ✅ | ✅ | |
| KeyLck | 0..40 (×15 s) | ✅ | ✅ `gKeyLockCountdown = ×1500` (10 ms units, 16-bit) | ✅ m:ss | overflow/timing fixed |
| Mode (AM) | 0..`MODULATION_UKNOWN-1` | ✅ | ✅ saves channel | ✅ | |
| RxDCS/TxDCS | 0..208 | ✅ (norm 1..104, inv 105..208) | ✅ correct `CodeType/Code`, keeps CTCSS when set to 0 | ✅ `D%03oN/I` | 104-entry `DCS_Options` exact |
| RxCTCS/TxCTCS | 0..50 | ✅ | ✅ symmetric to DCS | ✅ Hz | 50-entry `CTCSS_Options`, `pMax=50` → last index 49 ✔ |
| TxODir | 0..2 | ✅ | ✅ | ✅ | |
| TxOffs | 0..99,999,990 Hz | ✅ | ✅ | ✅ 6-digit input + `RoundToStep`, UP/DOWN wrap | |
| TxTOut | 5..179 | ✅ | ✅ | ✅ `(sel+1)*5 s` | ⚠ F-4 (numeric Min) |
| BusyCL | 0..1 | ✅ | ✅ | ✅ | |
| UPCode/DWCode | editor (no numeric limits) | ✅ | ✅ `gRequestSaveSettings`; validated by `DTMF_ValidateCodes` at EEPROM save | ✅ 2-line | 15-char cap, A–D cycling, EXIT backspace + dirty-flag persist, auto-save at 15 |
| MDC ID | 0..0xFFFF | ✅ | ✅ commits on 4th nibble (digit **and** A–F letter path), immediate save | ✅ `0x____` live editor | |
| PTT ID | 0..6 | ✅ | ✅ saves channel | ✅ 7 strings incl. MDC-1200 | |
| Roger | 0..2 | ✅ | ✅ | ✅ | |
| TXLock | 0..1 | ✅ | ✅ per-channel | ✅ shows "Inside F Lock Plan" when applicable | |
| ChSave | 0..1023 | ✅ MrChannel[TX_VFO] | ✅ + VFO reload | ✅ CH-####/freq/name | |
| ChDele | 0..1023 | ✅ next valid channel | ✅ `SETTINGS_UpdateChannel(delete)` | ✅ + SURE?/WAIT! confirm | |
| ChName | 0..1023 | ✅ | ✅ trims trailing spaces, saves only on change | ✅ 10-char editor, ABC/abc, cursor | EXIT at pos 0 discards (by design) |
| ChDisp | 0..3 | ✅ | ✅ | ✅ 4 strings | |
| ChList | 0..25 | ✅ | ✅ `SETTINGS_UpdateChannel` | ✅ OFF/01..24/ALL | `gListName[24][4]` bounds safe (`%.3s`) |
| ScnRev | 0..104 | ✅ | ✅ | ✅ STOP / CARRIER s:ms / TIMEOUT m:s + gauge | 🔍 F-11 extended semantics verified in scanner |
| STE, RP STE | 0..1 / 0..10 | ✅ | ✅ | ✅ | |
| Mic | 0..8 | ✅ | ✅ + `LoadCalibration` + reconfig | ✅ dB from `gMicGain_dB2[9]` | |
| MicBar, Compnd, 1 Call | — | ✅ | ✅ | ✅ | |
| ScList | 1..25 | ✅ | ✅ | ✅ | |
| ScPri | 0..1 | ✅ SCAN_LIST_ENABLED | ✅ | ✅ | |
| PriCh1/PriCh2 | 0..1024 ("None") | ✅ | ✅ stores `SCANLIST_PRIORITY_CH[]` uint16 | ✅ "None" sentinel | ⚠ F-6, F-8 |
| D ST | 0..1 | ✅ | ✅ | ✅ | |
| D Prel | 3..99 (×10 ms) | ✅ (`/10`) | ✅ (`×10`, uint16 field) | ✅ | ⚠ F-4 |
| D Live | 0..1 | ✅ | ✅ disables DTMF + reconfig | ✅ | |
| BLTime | 0..61 (61 = ON) | ✅ | ✅ | ✅ m:ss + gauge | app.c countdown guard `<61` consistent; ABR/BLMin/BLMax exempt from backlight timeout ✔ |
| BLMin / BLMax | 0..9 / 1..10 | ✅ | ✅ cross-clamp MIN<MAX both directions | ✅ live brightness preview | |
| BLTxRx | 0..3 | ✅ | ✅ | ✅ | |
| POnMsg | `ARRAY_SIZE-1` | ✅ | ✅ | ✅ adapts to LOGO flag | |
| BatTxt | 0..2 | ✅ | ✅ | ✅ status bar 2-bit field | |
| F1Shrt..MLong | 0..sidefunctions-1 | ⚠ F-9 | ✅ indexed write to 5 EEPROM actions | ✅ multi-line names | |
| SetPwr | 0..6 | ✅ | ⚠ F-7 | ✅ `<20m..5W` | |
| SetPTT | 0..1 | ✅ session copy | ✅ | ✅ | |
| SetTOT/SetEOT | 0..3 | ✅ | ✅ | ✅ | app.c TOT alert logic matches |
| SetCtr | 1..15 | ✅ | ✅ | 🔍 F-10 live preview | |
| SetInv | 0..1 | ✅ | ✅ | 🔍 F-10 | |
| SetLck | 0..3 | ✅ | ✅ | ✅ | |
| SetMet/SetGUI | 0..1 | ✅ | ✅ | ✅ | |
| SetRxA (AUD) | FM 0..4 / AM 0..2 / USB 0 | ✅ | ✅ per-modulation global + `RADIO_SetModulation` | ✅ FM/AM/USB badge | |
| SetTmr | 0..1 | ✅ | ✅ | ✅ | |
| SetOff | 0..120 | ✅ | ✅ | ✅ h:mm + gauge | |
| SetNFM | 0..1 | ✅ | ✅ + `SetTxParameters`+`SetupRegisters` | ✅ | |
| SetVol | 0..63 | ✅ VOLUME_GAIN | ✅ | 🔍 F-10 gain preview | |
| SetKey | 0..4 | ✅ | ✅ | ✅ 5 strings | |
| SetScn | 0..1 | ✅ | ✅ | ✅ | |
| SetSav | 0..`SET_SAV_LEN-1` | ✅ | ✅ | ✅ | |
| SysInf (VOL) | pages computed from feature flags | ✅ always page 0 | n/a (read-only) | ✅ identity/build/battery/mem/QR | page count == limit ✔ |
| F Lock | 0..`F_LOCK_LEN-1` | ✅ | 🔍 F-14 unlock counter | ✅ | enum & array kept in sync via `F_LOCK_LEN` |
| 350 En | 0..1 | ✅ | ✅ + VFO reload | ✅ | |
| FrCali | −50..+50 | ✅ XTAL_LOW | ✅ `writeXtalFreqCal(true)` | ✅ live xtal MHz | |
| BatCal | Lo 1000..4000 / Hi 1650..3900 | ✅ nested Hi/Lo state | ✅ validates and saves calibration | ✅ | Hi-first, Auto-Cal/CUST UI |
| BatTyp | 0..4 | ✅ | ✅ | ✅ 5 strings | |
| SetNav | 0..1 | ✅ | ✅ | ✅ | 🔍 F-13 UP/DOWN inversion by design |
| Reset | 0..1 | ✅ =0 | ✅ `SETTINGS_FactoryReset` | ✅ SURE?/WAIT! | 🔍 F-12 |

---

## 3. Findings

### F-1 · MEDIUM — `MENU_BATCAL`: partial/zero numeric entry reaches the divider — ✅ FIXED
`MENU_Key_0_to_9()` generic path commits as soon as `Value <= Max`, with **no Min check and no
"all digits entered" gating**. In the historical implementation, `BatCal` used limits 1500..3500:
- typing `0` → `gSubMenuSelection = 0` → `UI_DisplayMenu()` computes
  `gBatteryVoltageAverage * gBatteryCalibration[3] / gSubMenuSelection` → **division by zero**
  (software `__aeabi_uidiv`; result 0/garbage, possible hang on some libgcc builds).
- typing `1`..`9` → selection set to 1..9 → display shows truncated-garbage voltage.

Accept still clamps to 1500, so nothing corrupt is saved, but the display can misbehave.
**Fix:** only commit when `Offset == gInputBoxIndex` (full digit count), and/or clamp
`Value = MAX(Value, Min)` before assigning.

### F-2 · MEDIUM (latent build break) — `MENU_TXP` display uses N7SIX-only array unguarded — ✅ FIXED
`App/ui/menu.c:973` (`sprintf(String, "%s\n%sW", gSubMenu_TXP[...], gSubMenu_SET_PWR[sel-1])`)
has no `#ifdef ENABLE_FEAT_N7SIX`, but `gSubMenu_SET_PWR[]` is only compiled under that flag
(`App/ui/menu.c:590`). Any build with `ENABLE_FEAT_N7SIX=OFF` fails to compile.
**Fix:** guard the sprintf (fall back to plain `gSubMenu_TXP[]`), or move the array out of the guard.

### F-3 · MEDIUM (latent build break) — `MenuList[].voice_id` field does not exist — ✅ FIXED
`App/app/menu.c:1940`: `gAnotherVoiceID = MenuList[gMenuCursor].voice_id;` — but
`t_menu_item` is `{name[7], menu_id}` only. Dormant because it sits inside
`#ifdef ENABLE_VOICE`; enabling voice prompts breaks the build.
**Fix:** add `voice_id` to `t_menu_item` + populate `MenuList`, or drop the line.

### F-4 · LOW — numeric entry ignores `Min` (several menus) — ✅ FIXED
`MENU_Key_0_to_9()` checks only `Value <= Max`. For `TxTOut` (min 5), `D Hold` (min 5),
`D Prel` (min 3), `BatCal` (min 1500), `BLMax` (min 1), a below-min value is accepted into
`gSubMenuSelection` and displayed; it is silently clamped only at `MENU_AcceptSetting()`.
Cosmetic + confusing; combined with F-1 it is the root cause there.

### F-5 · LOW — `MENU_D_LIST` display can read an unterminated string — ✅ FIXED
`memcpy(String, Contact, 8)` (`App/ui/menu.c:1355`) leaves `String` without a NUL when the
contact name occupies all 8 bytes → later `strlen(String)` reads uninitialized stack.
DTMF-calling builds only. **Fix:** `String[8] = '\0';` after the memcpy (the ID path already
handles termination via `Contact[11]=0`).

### F-6 · LOW — `S_PRI_CH_1/2` limit allows an invalid sentinel — ✅ FIXED
`*pMax = MR_CHANNEL_LAST + 2` (1025) while the only meaningful "None" sentinel is
`MR_CHANNELS_MAX` (1024). Not reachable through the current UI paths (wrap logic caps at 1024),
but the bound should be `MR_CHANNELS_MAX` for defense in depth.

### F-7 · LOW — `MENU_SET_PWR` accept triggers a per-channel save — ✅ FIXED
`gRequestSaveChannel = 1` for a global setting causes an unnecessary VFO/channel flash write
(wear + needless work). The subsequent `gRequestSaveSettings` is sufficient.

### F-8 · LOW — static `last` shared between PriCh1/PriCh2 — ✅ FIXED
The `static int16_t last` in `MENU_Key_UP_DOWN()` persists across the two priority-channel
menus, so the wrap-around heuristic can misfire immediately after switching menus. Reset it
when `gMenuCursor` changes or make it per-menu.

### F-9 · LOW — key-action Show can leave a stale selection — ✅ FIXED
`MENU_ShowCurrentSetting()` for `F1SHRT..MLONG` leaves `gSubMenuSelection` untouched when the
stored action id is not present in `gSubMenu_SIDEFUNCTIONS[]` (e.g. action of a feature built
out). Accepting the menu then writes whatever stale value is held. Initialize to `0` (NONE)
when the lookup fails.

### F-10 · INFO — hardware side effects inside the display function
`SetCtr`/`SetInv` call `ST7565_ContrastAndInv()` and `SetVol` calls `BK4819_SetRxAudioGain()`
from `UI_DisplayMenu()`. Works (gives live preview), but side effects belong in an
apply/accept path; a pure redraw elsewhere could re-apply them unexpectedly.

### F-11 · INFO — `ScnRev` extended semantics verified
0 = STOP, 1..80 = carrier delay ×250 ms, 81..104 = timeout ×5 s; matches
`app.c:632-642` and `chFrScanner.c:885-889`, and fits `uint8_t SCAN_RESUME_MODE`.

### F-12 · INFO — Reset flow is sound
Two-step confirm (SURE? → WAIT!), `MENU_AcceptSetting()` then
`PY25Q16_FlushPendingWrite()` **before** `NVIC_SystemReset()` — good power-loss safety.

### F-13 · INFO — UP/DOWN inversion in submenus
`if (!gEeprom.SET_NAV && gIsInSubMenu) Direction = -Direction;` intentionally adapts arrow
direction to UV-K1 (left/right) vs UV-K5 (up/down) keypad layouts.

### F-14 · INFO — F-Lock unlock-all
Requires selecting `UNLOCK ALL` 3× (N7SIX) / 10× (stock); UI shows "READ MANUAL" during the
count; `F_LOCK_ALL` additionally calls `SETTINGS_ResetTxLock()`. Enum and string array stay
in sync through `F_LOCK_LEN`.

### F-15 · INFO — USER power line intentionally minimal — DEFERRED (2026-09-09)
In the per-channel `Power` menu, selection `USER` (index 0) displays only `USER` — no wattage —
because a USER channel's actual TX power comes from the **global** `SetPwr` setting
(`gSetting_set_pwr`, 0..6 → `<20m/125m/250m/500m/1W/2W/5W`), not from the channel itself.
This is correct and consistent with how USER works; the README already documents "User (see SetPwr)".
The queried change — rendering e.g. `USER\n125mW` on that line for immediate visibility — is a
genuine small clarity improvement, but was reviewed and intentionally **not applied** for this
release. Revisit if per-channel power clarity becomes a user-reported friction point.

---

## 4. Feature-flag portability

`ENABLE_NOAA`, `ENABLE_VOICE`, `ENABLE_ALARM`, `ENABLE_DTMF_CALLING`, `ENABLE_AM_FIX` are
**OFF** in the shipped build; all their menu items, limits, show/accept cases and display
strings are consistently wrapped — no dangling references found **except F-2 and F-3** above.

## 5. Recommended fix order

> **STATUS: F-1 through F-9 have all been applied** to `App/app/menu.c` and
> `App/ui/menu.c` (search for the `F-1` … `F-9` tags in the code). Remaining
> report items are INFO-only (F-10…F-14, verified by-design). Flash the rebuilt
> firmware and spot-check numeric entry in `BatCal`, `TxTOut` and `D Prel`,
> plus `PriCh1`/`PriCh2` arrow navigation across the wrap.
>
> **Follow-up (hardware-verified finding):** BatCal's numeric entry also had a
> 3-digit input buffer for a 4-digit value (`Offset` heuristic had no
> `Max >= 1000` tier) — the 4th keystroke reset the entry and landed below Min.
> Fixed in the same `Offset` line (`MENU_Key_0_to_9()`); BatCal is the only
> menu in the generic numeric path with Max >= 1000.
>
> **UX enhancement (follow-up):** numeric menus now show a fill-in template
> (`2___` → `20__` → `200_` → `2000`) while digits are being typed, instead of
> the clamped interim value. Implemented in `UI_DisplayMenu()` via
> `UI_MENU_IsNumericEntry()` (`App/ui/menu.c`); covers Sql, Mic, BatSav, VOX,
> TxTOut, KeyLck, ScnRev, RP STE, BLTime/Min/Max, BatCal, D Prel/D Hold/D List,
> SetOff, SetVol and SetCtr.
>
> **Battery calibration (2-point, pending v7.6.10D):** `BATTERY_GetReadings()`
> now uses one shared production helper across battery, spectrum, and UI paths,
> interpolating between `gBatteryCalibration[0]` (~6.0 V) and
> `gBatteryCalibration[3]` (~8.4 V) with a safe single-point fallback.
> `BatCal` is a **nested editor**: entering it shows a **Cal Lo / Cal Hi**
> picker; Hi is presented first for 8.4 V numeric editing (1650–3900), while
> Lo offers **Auto-Cal** (one-press apply of the preset
> `(600 · slot-3) / 840` = `slot-3 × 5/7`) or **Custom** (numeric edit, 1000–4000).
> Entry requires four digits within the active ADC limits. Navigation is
> driven by `gBatCalStage`/`gBatCalTarget`, with fixed left-aligned small-font
> rows, and EXIT walks back one level. The
> earlier standalone `Cal Lo` top-level entry was replaced by this in-menu
> picker per the design review; `MENU_BATCAL_LOW` remains only as an inert
> placeholder.
>
> **V1 → V2 calibration-format migration (hardware-verified):** the legacy
> single-point firmware stored `slot-3` as the raw ADC at **7.6 V**
> (`raw × 760 / slot-3`), while the 2-point model needs the raw ADC at
> **8.4 V**. Interpreting the legacy value directly over-reported every
> reading by `840/760` (+10.5 %) and clamped used batteries at a false
> 8.40 V (e.g. a legacy 2192 is really raw@7.6 V = raw@8.4 V × 2422/2192).
> `SETTINGS_LoadCalibration()` now performs a one-time, marker-guarded
> migration (`BATCAL_FORMAT_V2 = 0xB5F2` in slot-2, which legacy code never
> used): `slot-3 ×= 840/760`, `slot-0` cleared, block re-persisted. The
> single-point fallback and the spectrum status-bar readout are re-anchored
> to **840** accordingly. With the 2-point model a full 8.4 V battery
> correctly displays **8.40 V** (the old `×760` formula capped the display at
> 7.60 V), and a used ~8.0 V battery reads **8.00 V** — verified numerically
> for a 2192 legacy calibration (migrates to 2422; Auto-Cal Lo preset 1730).



1. F-1 (user-visible misbehavior / potential hang)
2. F-3, F-2 (latent build breaks behind flags)
3. F-5, F-9 (robustness)
4. F-4, F-6, F-7, F-8 (polish)

## 6. Pending v7.6.10D implementation status

- **KeyLck:** `gKeyLockCountdown` is now a 16-bit 10-ms counter using
  `AUTO_KEYPAD_LOCK × 1500`, fixing the previous overflow and incorrect timing.
- **ANI ID:** when DTMF calling is enabled, the menu uses the bounded DTMF editor
  and persists the 7-character identifier through the normal settings path.
- **Battery Type:** critical-voltage decisions are centralized on calibrated
  voltage for all five supported battery types; BatCal remains independent and
  continues to provide the ADC-to-voltage conversion.
- **Release state:** these changes are source-pending for v7.6.10D and require
  inclusion in the final release build and hardware menu validation.

*Host unit tests (`tests/`) cover frequencies/dcs/crc/mdc1200 only; there is no menu logic
test yet. A host test for `MENU_GetLimits` vs string-table sizes would catch the F-2/F-6
class of bugs and is recommended.*


<!--
=====================================================================================
Release Notes Template - ApeX Edition
Author: N7SIX, Sean and contributors
License: Apache License, Version 2.0
=====================================================================================
-->

# Release Notes - <version> ApeX Edition

Release date: <Month Year>
Build target: UV-K1 Series / UV-K5 V3 (<MCU scope>)
Build identifier: <artifact_name_if_available>
Edition: ApeX (all-in-one)

## Release Summary
One short paragraph (2-3 lines) describing the release focus and impact.

## Compatibility
- Supported: UV-K1 and UV-K5 V3 hardware with PY32F071 MCU
- Not supported: UV-K5 V1 or non-PY32F071 variants
- Notes: <optional compatibility caveat>

## Key Updates

### 1. <Primary Theme>
- <change 1>
- <change 2>
- <change 3>

### 2. <Secondary Theme>
- <change 1>
- <change 2>

### 3. <Optional Theme>
- <change 1>
- <change 2>

## Validation Status
- Build command: <command>
- Build result: <success/failure>
- Memory snapshot (if available):
  - RAM: <used> / <total> (<percent>)
  - FLASH: <used> / <total> (<percent>)
- Verification notes: <tests or checks performed>

## Upgrade Notes
- Back up calibration before flashing.
- Verify battery calibration after flashing.
- Verify key workflows after first boot (VFO, channel mode, scan, TX safety behavior).

## Outcome
- <user-facing improvement 1>
- <reliability or safety improvement 2>

---

## Writing Standard (Professional)
- Keep total length concise and scannable.
- Use clear, factual language with no hype wording.
- Prefer measurable statements (thresholds, limits, addresses, build IDs).
- Keep section order unchanged for consistency.
- Do not duplicate old release history in a version file.
- Put only version-specific content in each file.

## File Naming Standard
Use lowercase and this format:
- `v<version>-apex.md`
- Examples:
  - `v7.6.6-apex.md`
  - `v7.6.5br2-apex.md`

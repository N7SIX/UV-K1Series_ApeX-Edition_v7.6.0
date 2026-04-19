<!--
=====================================================================================
UV-K1 Series / UV-K5 V3 ApeX Edition Release Notes Index
Author: N7SIX, Sean and contributors
License: Apache License, Version 2.0
=====================================================================================
-->

# UV-K1 SERIES / UV-K5 V3 APEX EDITION
## Release Notes Index (Versioned)

This repository now stores release notes as separate version files for cleaner maintenance and archival clarity.

## Current Recommended Notes
- [N7SIX v7.6.5br5 ApeX](release-notes/v7.6.5br5-apex.md)
- [N7SIX v7.6.5br4 Patch ApeX](release-notes/v7.6.5br4-patch-apex.md)
- [N7SIX v7.6.5br3 Patch ApeX](release-notes/v7.6.5br3-patch-apex.md)
- [N7SIX v7.6.5br2 ApeX](release-notes/v7.6.5br2-apex.md)

## Versioned Release Notes
- [v7.6.5br5 ApeX](release-notes/v7.6.5br5-apex.md)
- [v7.6.5br4 Patch ApeX](release-notes/v7.6.5br4-patch-apex.md)
- [v7.6.6 ApeX](release-notes/v7.6.6-apex.md)
- [v7.6.5 ApeX](release-notes/v7.6.5-apex.md)
- [v7.6.4br5 ApeX](release-notes/v7.6.4br5-apex.md)
- [v7.6.4br4 ApeX](release-notes/v7.6.4br4-apex.md)
- [v7.6.0 ApeX](release-notes/v7.6.0-apex.md)

## Archived Material
- [Legacy monolithic release notes](release-notes/archive/RELEASE_NOTES_legacy-monolith.md)

## Template
- [Release note style template](release-notes/RELEASE_NOTE_TEMPLATE.md)

## Related Standards

## Notes

## Migration/Upgrade Process for v7.6.5br5

To ensure a clean and reliable upgrade to v7.6.5br5, follow this process:

1. **Restore Stock Firmware**
	- Use QPS-K Series Software to flash the official stock firmware.
2. **Factory Reset**
	- On the radio, select RESET ALL from the menu.
3. **Install ApeX Edition (Base)**
	- Flash N7SIX v7.6.5 ApeX Edition firmware. Do not configure settings yet.
4. **Apply ApeX Edition (Revision)**
	- Flash N7SIX v7.6.5br5 ApeX Edition update immediately after the base install.
5. **Restore User Data**
	- Use QPS-K Series Software to restore your saved channel backup and settings.

**Important:**
- For restoring stock firmware, always use the official QPS-K Series Software for flashing.
- For flashing N7SIX ApeX Edition (steps 3 and 4), use Multi-UVTools or UVTools for best compatibility and reliability.
- Do not skip the factory reset step.
- Do not configure the radio between steps 3 and 4.
- Use QPS-K Series Software for backup/restore of user data.

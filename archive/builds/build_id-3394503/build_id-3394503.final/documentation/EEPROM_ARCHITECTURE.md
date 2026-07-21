# EEPROM Architecture Deep Dive

> **Firmware:** UV-K1Series ApeX-Edition v7.6.8  
> **Date:** 2026-06-26

---

## Two EEPROM Systems

This firmware has **two distinct persistent storage systems** that are often confused:

### 1. I2C EEPROM (`driver/eeprom.c`)

```c
EEPROM_ReadBuffer(uint16_t Address, void *pBuffer, uint8_t Size);
EEPROM_WriteBuffer(uint16_t Address, const void *pBuffer);
```

- **Hardware:** External I2C EEPROM chip (address 0xA0 write / 0xA1 read)
- **Capacity:** Unknown — likely AT24C02 (256 bytes) or similar
- **Purpose:** Unknown — may be used for quick-access settings or boot-critical data
- **NOT the same as PY25Q16 settings storage**

### 2. PY25Q16 SPI Flash (`driver/eeprom_compat.c`)

```c
EEPROM_ReadBuffer(uint16_t Address, void *pBuffer, uint8_t Size);
EEPROM_WriteBuffer(uint16_t Address, const void *pBuffer);
```

- **Hardware:** 2 MB SPI flash on the UV-K series PCB
- **Virtual EEPROM:** 64 KB address space (0x00000-0x0FFFF) mapped to physical PY25Q16 sectors
- **Purpose:** ALL user-facing settings, memory channels, VFO configs, calibration

---

## PY25Q16 Memory Map (from `ADDR_MAPPINGS[]`)

```
Physical PY25Q16    Virtual EEPROM    Content                    Size
─────────────────────────────────────────────────────────────────────
0x000000-0x004000  0x000000-0x004000  Memory channel frequencies  4 KB per bank × 4 banks = 16 KB
0x004000-0x008000  0x004000-0x008000  Memory channel names        4 KB per bank × 4 banks = 16 KB
0x008000-0x009000  0x008800-0x00886E  Channel data + VFO attr     3.5 KB
                          0x00886E-0x009000  (gap / padding)
0x009000-0x0090D6  0x009000-0x0090D6  VFO configurations (14×16)  214 bytes
0x00A000-0x00A170  0x00A000-0x00A170  ALL SETTINGS                368 bytes total:
  0x00A000-0x00A010  General settings                              16 bytes
  0x00A010-0x00A020  (duplicate/mirror?)                          16 bytes
  0x00A020-0x00A028  FM settings                                   8 bytes
  0x00A028-0x00A0A8  FM memory channels                            128 bytes
  0x00A0A8-0x00A0F8  Extended settings                             80 bytes
  0x00A0F8-0x00A130  More extended settings                        56 bytes
  0x00A130-0x00A138  Scan list settings                            8 bytes
  0x00A138-0x00A148  AES settings                                  16 bytes
  0x00A148-0x00A150  **Spectrum settings**                         8 bytes
  0x00A150-0x00A158  (more settings)                              8 bytes
  0x00A158-0x00A160  N7SIX custom settings                         8 bytes
  0x00A160-0x00A170  Version/build info                            16 bytes
0x010000-0x010200  0x00B000-0x00B200  Calibration data            512 bytes
0x011000-0x012000  (unmapped)          Boot logo (128×64 bitmap)   1 KB
```

---

## How `eeprom_compat.c` Works

### AddrTranslate() — Virtual to Physical Mapping

```c
bool AddrTranslate(uint16_t EEPROM_Addr, uint16_t Size,
                   uint32_t *PY25Q16_Addr_out, uint16_t *Size_out, bool *End_out);
```

- Searches `ADDR_MAPPINGS[]` for the mapping containing `EEPROM_Addr`
- If found: returns PY25Q16 physical address = `p->PY25Q16_Addr + (EEPROM_Addr - p->EEPROM_Addr)`
- If NOT found: returns `HOLE_ADDR` (0x1000000) — a sentinel value
- Writes that land on `HOLE_ADDR` are **silently dropped** (never written to flash)

### Write Path

```c
void EEPROM_WriteBuffer(uint16_t Address, const void *pBuffer)
{
    uint16_t Size = 8;  // ALWAYS writes 8 bytes, regardless of parameter
    while (Size)
    {
        AddrTranslate(Address, Size, &PY_Addr, &PY_Size, &AppendFlag);
        if (PY_Addr < HOLE_ADDR)  // Skip unmapped addresses
            PY25Q16_WriteBuffer(PY_Addr, pBuffer, PY_Size, AppendFlag);
        Address += PY_Size;
        pBuffer += PY_Size;
        Size -= PY_Size;
    }
}
```

**Critical detail:** `Size` is **hardcoded to 8**. The function signature takes `uint16_t Address, const void *pBuffer` with no Size parameter visible. It always writes exactly 8 bytes, starting at the mapped address.

### Read Path

```c
void EEPROM_ReadBuffer(uint16_t Address, void *pBuffer, uint8_t Size)
{
    while (Size)
    {
        AddrTranslate(Address, Size, &PY_Addr, &PY_Size, NULL);
        if (PY_Addr >= HOLE_ADDR)
            memset(pBuffer, 0xff, PY_Size);  // Unmapped → return 0xFF
        else
            PY25Q16_ReadBuffer(PY_Addr, pBuffer, PY_Size);
        Address += PY_Size;
        pBuffer += PY_Size;
        Size -= PY_Size;
    }
}
```

Reads do take a `Size` parameter and handle cross-boundary reads correctly.

---

## Write-Verify Safety Analysis

### Where to add verify

**NOT in `driver/eeprom.c`** (I2C layer) — that's a separate chip of unknown purpose.

**IN `driver/eeprom_compat.c`** — this is where all settings writes actually go.

### How it would work

Since `eeprom_compat.c` already writes in fixed 8-byte chunks, verify is straightforward:

```c
static uint8_t verify_buf[8];  // Static — no stack usage

void EEPROM_WriteBuffer(uint16_t Address, const void *pBuffer)
{
    uint16_t Size = 8;
    // ... existing write logic ...
    
    // Verify read-back (after all chunks written)
    EEPROM_ReadBuffer(Address, verify_buf, 8);
    if (memcmp(verify_buf, pBuffer, 8) != 0)
    {
        // One retry
        PY25Q16_WriteBuffer(PY_Addr, pBuffer, 8, AppendFlag);
        EEPROM_ReadBuffer(Address, verify_buf, 8);
        if (memcmp(verify_buf, pBuffer, 8) != 0)
            gEepromWriteErrorCount++;  // Persistent error
    }
}
```

### Does this affect EEPROM mapping?

**No.** The mapping in `ADDR_MAPPINGS[]` is read-only and used only for address translation. Write-verify:
- Uses the SAME `AddrTranslate()` call
- Reads from the SAME physical PY25Q16 address
- Does not change any mapping entries
- Does not change the `HOLE_ADDR` behavior

### Does this affect EEPROM wear?

- Each verify attempt adds 1 extra READ (no flash wear — reads are free)
- A failed verify adds 1 extra WRITE (8 bytes = 1/512 of a 4KB sector)
- EEPROM/flash endurance: ~100,000 writes per sector
- Even with daily settings saves, a single 8-byte sector would last ~270 years
- The retry-on-failure pattern adds at most 1 extra write per save event
- **Wear impact: negligible**

### Edge cases to handle

| Case | Behavior |
|------|----------|
| Write to unmapped address (`HOLE_ADDR`) | `AddrTranslate` returns `HOLE_ADDR`, write is skipped. Verify should also skip. |
| Cross-sector write (8 bytes spans two 4KB sectors) | `AddrTranslate` clamps `PY_Size` to remaining bytes in first sector. The while-loop handles the rest. Verify must read back all chunks and compare each. |
| Power loss during verify read | PY25Q16 read is atomic at 8-byte level. If power is lost mid-read, partial data is returned — but this is the same risk as any normal read. |
| `AppendFlag` semantics | The `PY25Q16_WriteBuffer()` `AppendFlag` parameter suggests wear-leveling. Verify read must use the PHYSICAL address (with offset), not the virtual address. `EEPROM_ReadBuffer()` handles this correctly. |

---

## Conclusion

**Write-verify in `eeprom_compat.c` is SAFE and does NOT affect EEPROM mapping.**

The mapping is a read-only translation table. Write-verify reads back the same physical location that was just written. The only behavioral change is:
1. Slightly longer write time (one extra 8-byte read per save)
2. Potential retry write on mismatch (adds at most 1 extra write per save)
3. Persistent error counter on double-failure

**Recommendation update:** The P1 recommendation should specify `eeprom_compat.c` as the target, not `eeprom.c`. The I2C EEPROM layer is a separate system of unknown purpose and should not be modified without first determining its actual role in the firmware.
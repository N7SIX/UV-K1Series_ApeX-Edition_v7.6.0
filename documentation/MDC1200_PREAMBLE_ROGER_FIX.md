# MDC-1200 Roger Preamble Fix — Genuine Motorola Sound

**Version**: v7.6.0 post-audit (2026)
**Status**: ✅ Implemented (+ host unit tests for the protocol change)

---

## Problem

The `MDC-1200` / `MDC-1200L` Roger preamble sounded "off" (or was not decoded at all)
by a genuine Motorola MDC-1200 radio. Two defects in the BK4819/BK4829 FSK TX path
caused this:

### 1. `REG_5B = 0x55AA` — 180° phase reversal in the middle of the preamble (PRIMARY)

The FSK sync word is a 4-byte pattern programmed into `REG_5A`/`REG_5B`:
- `REG_5A = 0x5555` → bytes `55 55` (alternating 0101... = continuous 1200 Hz tone)
- `REG_5B = 0x55AA` → bytes `55 AA`

The `0xAA` byte is the **bit-inversion** of `0x55`. Its LSB-first bitstream
`0 1 0 1 0 1 0 1` (= 0x55) suddenly becomes `1 0 1 0 1 0 1 0` (= 0xAA), which is a
180° phase flip of the FSK carrier **mid-tone**.

A genuine Motorola preamble is a phase-continuous `0x55` run (`01010101...`). The
embedded `0xAA` looks like end-of-preamble / a corrupted header to real MDC-1200
decoders, so the leader that follows is often missed or the packet is rejected → the
Roger sounds "off" / broken.

**Fix:** `REG_5B` is now `0x5555` everywhere (sync word = `55 55 55 55`, a continuous,
phase-coherent 1200 Hz tone).

### 2. Double preamble — the frame's own preamble was reloaded into the FIFO (SECONDARY)

The BK4819/4829 hardware **automatically generates** a `0x55` preamble and a 4-byte
sync word immediately before the data clocked out of the TX FIFO. The TX path was
also loading the entire encoded frame — including its own 7-byte (standard) or 27-byte
(composite) preamble — into the FIFO:

- Standard: HW `7 + 4` + FIFO `7` = **18 bytes** of `0x55` (genuine Motorola: **7**)
- Long:     HW `7 + 4` + FIFO `27` = **38 bytes** of `0x55` (genuine: **27**)

That made the preamble far longer than the protocol spec and, combined with the
`0xAA` glitch, unlike anything a Motorola radio expects.

**Fix:** a new helper strips the frame's preamble before it reaches the FIFO and
appends a single `0x55` pad byte for 16-bit alignment:

- Standard: FIFO = `leader(5) + payload(14) + pad(1)` = 20 bytes → on-air `11 + 20 = 31`
- Long:     FIFO = `pretime(20) + leader(5) + payload(14) + pad(1)` = 40 bytes → on-air `11 + 40 = 51`

The hardware preamble + sync (all `0x55`) now leads straight into the Barker leader,
producing a clean, phase-coherent, genuine-sounding Roger.

---

## Changes

| File | Change |
|------|--------|
| `App/mdc1200.h` | Documented + declared `MDC1200_BuildTxFifoWords()` |
| `App/mdc1200.c` | Added `MDC1200_BuildTxFifoWords()` (preamble-strip + pad + FIFO words) |
| `App/driver/bk4819.c` | `BK4819_PlayRogerMDC()` & `BK4819_TransmitMDC1200Frame()`: `REG_5B = 0x5555`, use `MDC1200_BuildTxFifoWords()`, updated `REG_5D` (0x1400 / 0x2800) & TX timing |
| `App/driver/bk4829.c` | Same fixes as `bk4819.c`; corrected stale `555555aa` comment |
| `tests/test_mdc1200.c` | New TX-FIFO-word tests (structure, no-`0xAA`, error paths) |
| `documentation/CHANGELOG.md` | Entry for this fix |

---

## Resulting On-Air Preamble (TX FIFO contents)

Standard (`0x01,0x23,0x4567`):

```
HW (auto):  55 55 55 55 55 55 55 | sync 55 55 55 55
FIFO:       07 09 2A 44 6F | 76 76 2C A6 1C B8 68 19 10 31 18 E6 08 60 | 55(pad)
```

- No `0xAA` anywhere → continuous tone, no phase reversal.
- Preamble length matches genuine Motorola's spec far more closely.

---

## MDC-1200 vs MDC-1200L — Distinction Preserved

This fix deliberately **does not** collapse the two modes into one. The whole point
of the **"L"** (Long) variant is a longer `0x55` preamble for weak-signal reach, and
that is retained:

| Mode | HW preamble+sync | FIFO preamble (TX) | Trailing pad | **On-air 0x55 preamble** |
|------|:---:|:---:|:---:|:---:|
| `MDC-1200` (standard) | `7 + 4` = 11 | none (strips its 7B) | 1 | **12 bytes** |
| `MDC-1200L` (long)    | `7 + 4` = 11 | 20-byte pretime kept | 1 | **32 bytes** |

- `MDC1200_BuildTxFifoWords()` strips **only the trailing 7-byte sync preamble**
  from the long frame and keeps the 20-byte extended pretime, so the long burst
  leads with exactly `MDC1200_PRETIME_LENGTH` (20) more `0x55` bytes than the
  standard burst — a **20-byte (~133 ms at 1200 baud)** preamble advantage.
- The leaders and 14-byte payloads are byte-identical between the two modes; only
  the `0x55` run differs (7 vs 27 bytes in the frame, 12 vs 32 bytes on air).
- The test suite pins this with explicit assertions so a future refactor cannot
  silently make `MDC-1200L` identical to `MDC-1200`.

---

## Verification Plan

1. **Host unit tests** (`tests/`): the existing MDC-1200 suite now also validates
   `MDC1200_BuildTxFifoWords` output (word count, leader/payload placement, trailing
   `0x55` pad, no `0xAA`, and invalid-input error codes).
2. **On-air / bench:** confirm with a genuine Motorola or an SDR decode of the
   preamble that (a) the tone is phase-continuous and (b) the leader is decoded.
3. **Receiver compatibility:** verify a second radio's MDC-1200 decode still works
   with the new (correct) preamble length.

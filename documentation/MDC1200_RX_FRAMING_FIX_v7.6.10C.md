# MDC-1200 RX Framing Fix - Why the MDC-ID Was Never Displayed

**Date**: 2026-09-05
**Status**: FIXED (all five passes complete) - on-air validation pending
**Files touched**: `App/driver/bk4829.c`, `App/app/app.c`, `App/radio.c`, `App/driver/bk4819.h`, `App/mdc1200.h`, `tests/test_mdc1200.c`

---

## Executive Summary

The MDC-1200/MDC1200L UI chain (decode -> dispatch -> `UI_DisplayMDCAlert()`) was intact.
The failure was in the **BK4829 hardware framing layer**: the FSK receiver was armed
with a byte count that standard MDC-1200 frames can never satisfy, so `FSK_RX_FINISHED`
never fired and `APP_HandleMDC1200Receive()` was never called. Three further defects
blocked every remaining path.

**Second-pass finding:** the first-pass fix (v7.6.10C) introduced a three-way mismatch
between TX FIFO loading, RX FIFO arming, and the RX handler. The TX was changed to
preamble-stripped (20-byte FIFO) while REG_5D was programmed for full-frame reception
(22 bytes) and the handler expected 11 words + 4-byte preamble re-insertion. None of
the three agreed; FSK_RX_FINISHED could not fire reliably and even when it did, the
frame was corrupted.

**Resolution:** switch all three layers to full-frame framing - the on-air-proven
approach. The TX FIFO now carries the complete MDC frame (preamble included), REG_5D
is armed with the full 26/46-byte count, and the handler reads the FIFO words directly
with no preamble re-insertion.

## Root Causes (Corrected Second-Pass Analysis)

### RC1 (primary) - `REG_5D = 0x2A00` (42 bytes) in `BK4819_EnableMDC1200RX()`
Aircopy (the only proven FSK RX in this codebase) sets `REG_5D` to the exact number of
RX bytes expected after the 4-byte HW sync (0x4700 = 72). The MDC RX config reused the
MDC1200L figure (46 - 4 = 42) for **both** modes, so a standard 26-byte frame delivers
only 22 bytes after sync -> `FSK_RX_FINISHED` never asserts -> handler never runs -> no ID.
**Fix**: `REG_5D = 0x1A00` (26 B) for standard, `0x2E00` (46 B) for MDC1200L - full frame
byte count matching the full-frame TX FIFO loading.

### RC2 - RX handler hardwired to 26-byte frames
`APP_HandleMDC1200Receive()` always read 11 words and decoded a 26-byte frame, so an
MDC1200L burst (21 words / 42 bytes after sync, all-0x55 preamble prefix) could never
decode. **Fix**: the handler is length-aware (`MDC1200L_RX_FIFO_WORD_COUNT = 23`) and
reads 13 or 23 words directly from the FIFO with no preamble re-insertion.

### RC3 - TX/RX framing asymmetry (first-pass regression)
The first-pass fix changed TX to preamble-stripped (20-byte FIFO) without updating
REG_5D and the handler to match - created a three-way mismatch. **Fix**: TX now loads
the full frame (preamble included) into the FIFO, matching the on-air-proven convention.
Sync words are `5555 5555` on both TX and RX, `REG_5D = 0x1A00/0x2E00`, burst =
HW(11)+FIFO(26/46) = 37/57 bytes.

### RC4 - wrong RX-FIFO clear strobe
`APP_HandleMDC1200Receive()` re-armed with `REG_59 = 0x8068` (bit 15 = **TX** FIFO
clear). Stale words corrupted every subsequent frame. **Fix**: `0x4068` (bit 14 = RX
FIFO clear), matching the proven `BK4819_PrepareFSKReceive()` sequence.

### RC5 - inconsistent `REG_5C`
RX used the aircopy value `0x5665` (enables the HW CRC checker, which MDC frames -
carrying no HW CRC bytes - can fail). **Fix**: RX now uses the TX value `0xAA30`.

## Verification

- Host unit tests: 17,241 checks, 0 failures - including new RX FIFO-reconstruction
  vectors (standard 13-word and MDC1200L 23-word direct reads) and TX FIFO length contracts.
- Firmware builds: `n7six.ApeX-k1.v7.6.10C` - FLASH 114,300 B (94.59 %), RAM 14,528 B (88.67 %).

## On-Air Test Matrix (pending hardware validation)

| # | Test | Expected |
|---|------|----------|
| 1 | ApeX -> ApeX, PTT_ID_MDC1200, both radios | `MDC: <opcode>` / `Unit: 0xXXXX` center-line alert |
| 2 | ApeX -> ApeX, PTT_ID_MDC1200L | same, 46-byte capture path |
| 3 | Genuine Motorola -> ApeX (if available) | standard-frame decode |
| 4 | Second frame after a first reception | still decodes (RC4 fix) |
| 5 | Receiving channel with PTT ID = OFF | nothing (by design: RX is gated on the RX VFO PTT-ID mode) |
## Third-Pass: Documentation Audit & Bug Fixes

### Finding

The second-pass fix (full-frame TX) was functionally correct, but five documentation
bugs created maintenance hazards that could mislead future developers.

### Bugs Fixed

| # | File | Fix |
|---|------|-----|
| 1 | `App/mdc1200.h:74-83` | Updated `MDC1200_TX_FIFO_LENGTH` macros from preamble-stripped (20/40) to full-frame (26/46) |
| 2 | `App/mdc1200.h:114-116` | Fixed `MDC1200_BuildFrame()` doc: 26-byte frame (was incorrectly 46-byte) |
| 3 | `App/mdc1200.h:141-145` | Fixed `MDC1200_BuildFifoWords()` doc: handles both 26/46-byte frames |
| 4 | `App/mdc1200.h:165-166` | Fixed `MDC1200_DecodeFrame()` doc: handles both 26/46-byte frames |
| 5 | `App/driver/bk4829.c:1943` | Fixed REG_5D comment: `0x1A00/0x2E00` (was `0x1400/0x2800`) |

### Verification

- All code functionally correct � no code changes needed
- All documentation now accurately reflects the implementation

## Fourth-Pass: Simulation Tests

### Tests Added (13 new tests)

| # | Test | What It Verifies |
|---|------|------------------|
| 1 | Standard TX?RX round-trip | BuildFrame ? BuildFifoWords ? DecodeFrameWords |
| 2 | MDC-1200L TX?RX round-trip | Same for 46-byte long frame |
| 3 | All opcodes (0x00-0x07) | Every opcode round-trips correctly |
| 4 | Boundary unit IDs | 0x0000 (broadcast) and 0xFFFF (max) |
| 5 | All arguments (0x00-0x0F) | Every argument value round-trips |
| 6 | Viterbi ECC - 1-bit error | Single-bit leader error corrected |
| 7 | Viterbi ECC - 2-bit error | Two-bit leader error corrected |
| 8 | Heavy corruption (4-bit) | Graceful handling of uncorrectable frames |
| 9 | Sliding-window sync search | Leader found at canonical offset |
| 10 | Standard preamble verification | First 7 bytes are 0x55, byte 7 is 0x07 |
| 11 | MDC-1200L preamble verification | First 27 bytes are 0x55, byte 27 is 0x07 |
| 12 | On-air burst length | Standard=37 bytes, Long=57 bytes |
| 13 | Timing verification | Standard=246ms, Long=380ms |

### Verification

- Host unit tests: **17,462 checks, 0 failures**
- MDC fuzz test: **0 miscorrections** (beyond dfree/2)

## Fifth-Pass: Motorola XOR Differential Encoding

### Background

Motorola's real MDC-1200 implementation uses XOR differential encoding at the physical layer (per Batlabs reverse-engineering of US Patents 4,457,005 / 4,517,561 / 4,590,473 / 4,517,669). Successive data bits are exclusive-ored just before they go to the modulator. The modulated tones carry only the information 'this bit differs from the previous bit' or 'this bit equals the previous bit'.

The alternating-bit preamble (0x55 = 01010101) encodes to 0x7F/0xFF (almost a constant tone), which the receiver can easily detect and synchronize to.

### Implementation

The BK4819/BK4829 hardware has NO register bit for XOR differential encoding. The 'scramble' feature (REG_31 bit 1) is a different frequency-offset mechanism. This must be done in software.

**Forward encoding:** diff[n] = data[n] XOR data[n-1] (data[-1] assumed 0)
**Reverse decoding:** data[n] = diff[n] XOR data[n-1]

Applied to the ENTIRE frame (preamble + leader + payload) as a continuous bit stream, MSB-first within each byte.

### Files Changed

- App/mdc1200.c: Added MDC1200_DiffEncodeFrame() and MDC1200_DiffDecodeFrame()
- App/mdc1200.h: Added function declarations
- App/driver/bk4829.c: TX path encodes a local copy of the frame before loading FIFO
- App/app/app.c: RX path decodes the received bytes before passing to decoder
- tests/test_mdc1200.c: Added 5 new tests (Tests 14-18)

### Verification

- Host unit tests: 17,556 checks, 0 failures
- Firmware builds: n7six.ApeX-k1.v7.6.10C - FLASH 114,468 B (94.73 %), RAM 14,528 B (88.67 %)

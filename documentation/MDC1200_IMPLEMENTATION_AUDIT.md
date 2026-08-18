# MDC-1200 Implementation Deep Audit Report

**Date:** 2026-08-16  
**Version Audited:** v7.6.10A  
**File:** `App/mdc1200.c`  
**Status:** ⚠️ CRITICAL ISSUES FOUND

---

## Executive Summary

The MDC-1200 implementation contains **3 CRITICAL BUGS** that will cause frame decode failures, CRC verification errors, and inability to receive valid MDC signals. The bugs are in the CRC byte ordering and preamble validation logic.

---

## Issues Found

### 🔴 CRITICAL: Issue #1 - CRC Byte Order Reversal in MDC1200_DecodeFrame()

**Location:** `MDC1200_DecodeFrame()` at line 256-257  
**Severity:** CRITICAL - Causes all CRC validation to fail  

**Code:**
```c
crc_in = ((uint16_t)payload[4] | ((uint16_t)payload[5] << 8u));
crc_calc = mdc1200_crc16(payload, 4u);
*valid_out = (crc_in == crc_calc);
```

**Problem:**
In `mdc1200_encode_str()` (lines 114-116), the CRC is stored as:
```c
data[4] = (uint8_t)(crc & 0x00FFu);           // LOW byte at [4]
data[5] = (uint8_t)((crc >> 8) & 0x00FFu);   // HIGH byte at [5]
```

But in `MDC1200_DecodeFrame()`, it's read back with **reversed byte order**:
```c
crc_in = payload[4] | (payload[5] << 8)   // Treats [4] as low, [5] as high ✗
```

This is **backwards**. It should be:
```c
crc_in = ((uint16_t)payload[5] << 8u) | (uint16_t)payload[4];
```

**Impact:**
- All received MDC frames will fail CRC validation
- The `valid_out` flag will always be false
- MDC decode will never work on RX side

---

### 🔴 CRITICAL: Issue #2 - Same CRC Byte Order Bug in MDC1200_VerifyCRC()

**Location:** `MDC1200_VerifyCRC()` at line 295  
**Severity:** CRITICAL - Same issue as #1  

**Code:**
```c
crc_in = ((uint16_t)payload[4] | ((uint16_t)payload[5] << 8u));
```

**Problem:** Identical to Issue #1 - reversed byte order.

**Impact:**
- CRC verification will always fail
- Any validation of received frames fails

---

### 🔴 CRITICAL: Issue #3 - Incomplete Preamble Validation in MDC1200_DecodeFrame()

**Location:** `MDC1200_DecodeFrame()` at line 238-241  
**Severity:** CRITICAL - Allows corrupted frames to pass validation  

**Code:**
```c
for (n = 0u; n < 7u; ++n) {
    if (frame[n] != 0x55u)
        break;
}
```

**Problem:**
This loop checks if the first 7 bytes are 0x55 but **does nothing with the result**. If any byte is not 0x55, the loop breaks, but execution continues normally and the frame is still "decoded" as if valid.

**Expected Behavior:**
Should validate and reject frames with invalid preamble:
```c
for (n = 0u; n < 7u; ++n) {
    if (frame[n] != 0x55u) {
        *valid_out = false;
        return MDC1200_ERROR_NONE;  // or error code
    }
}
```

**Impact:**
- Corrupted frames with invalid preambles pass validation
- Noise and garbage can be decoded as valid MDC signals
- False positive detections in RX mode

---

## Secondary Issues

### ⚠️ MEDIUM: Issue #4 - Missing Sync Pattern Validation

**Location:** `MDC1200_DecodeFrame()` lines 12-15 of payload  
**Severity:** MEDIUM - No validation of sync bytes  

The MDC frame header contains fixed sync bytes at positions 7-11:
```
frame[7] = 0x07;
frame[8] = 0x09;
frame[9] = 0x2A;
frame[10] = 0x44;
frame[11] = 0x6F;
```

**Current Code:** Does NOT validate these bytes during decode.  

**Recommendation:** Add validation:
```c
if (frame[7] != 0x07 || frame[8] != 0x09 || frame[9] != 0x2A ||
    frame[10] != 0x44 || frame[11] != 0x6F) {
    *valid_out = false;
    return MDC1200_ERROR_NONE;
}
```

**Impact:** Moderate - increases false positive rate but is not strictly required if CRC is working correctly.

---

### ⚠️ MINOR: Issue #5 - Missing ECC Validation

**Location:** `MDC1200_DecodeFrame()`  
**Severity:** MINOR - ECC data not validated  

The 7 ECC bytes (`payload[7]` through `payload[13]`) are never validated against the convolutional encoder. While CRC provides error detection, ECC could provide error correction.

**Current Status:** ECC bytes are extracted but never used for validation or correction.

**Note:** This is acceptable for a minimal implementation if you only want CRC validation.

---

## Testing Recommendations

### Test Case 1: CRC Round-Trip
```c
// Build a frame with known values
uint8_t frame[26];
size_t len_out;
MDC1200_BuildFrame(0x01, 0x23, 0x4567, frame, sizeof(frame), &len_out);

// Immediately decode it
uint8_t op, arg;
uint16_t unit_id;
bool valid;
MDC1200_DecodeFrame(frame, len_out, &op, &arg, &unit_id, &valid);

// MUST pass - should always be true
ASSERT_TRUE(valid);
ASSERT_EQ(op, 0x01);
ASSERT_EQ(arg, 0x23);
ASSERT_EQ(unit_id, 0x4567);
```

**Current Status:** This test WILL FAIL due to Issue #1 and #2.

---

## Fix Summary

| Issue | File | Line(s) | Fix Type | Priority |
|-------|------|---------|----------|----------|
| #1 - CRC byte order | mdc1200.c | 256-257 | Swap byte order in read | CRITICAL |
| #2 - CRC byte order | mdc1200.c | 295 | Swap byte order in read | CRITICAL |
| #3 - No preamble validation | mdc1200.c | 238-241 | Add validation check | CRITICAL |
| #4 - No sync validation | mdc1200.c | ~248 | Add sync byte checks | MEDIUM |
| #5 - No ECC validation | mdc1200.c | N/A | Optional enhancement | MINOR |

---

## Recommended Actions

1. **Immediate:** Fix Issues #1, #2, #3 before any production use
2. **High Priority:** Add Issue #4 (sync validation)
3. **Optional:** Implement Issue #5 (ECC validation) for error correction capability
4. **Post-Fix:** Run comprehensive round-trip tests with multiple payload variations
5. **Testing:** Compare output against reference implementations (fsync-mdc1200-decode)

---

## Code Quality Observations

**Positive:**
- Good documentation and comments
- Clear error return codes
- Well-structured interleave/deinterleave logic
- Proper CRC-16 implementation for encoding path

**Negative:**
- Inconsistent validation (preamble checked but not enforced)
- Missing sync pattern validation
- ECC not validated
- No comprehensive test suite attached

---

## Conclusion

The MDC-1200 protocol layer has **correct encoding logic** but **broken decoding validation**. The implementation cannot receive valid MDC frames due to CRC byte order reversal and incomplete preamble checking.

**Status: NOT PRODUCTION READY - Requires immediate fixes**

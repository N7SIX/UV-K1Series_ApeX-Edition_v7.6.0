/* Copyright 2026 Sean, N7SIX
 * https://github.com/N7SIX
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 *
 * MDC-1200 Full Implementation: v7.6.10A
 *
 * Complete protocol-layer implementation providing:
 * - MDC1200_BuildFrame(): Encode raw parameters into 26-byte frames
 * - MDC1200_BuildFifoWords(): Convert frames to 16-bit FIFO words
 * - MDC1200_DecodeFrame(): Reverse frame encoding for validation/testing
 * - MDC1200_VerifyCRC(): Validate CRC and payload integrity
 * - MDC1200_Transmit(): Public API combining frame building and RF transmission
 *
 * Protocol Compliance (all features v7.6.10A):
 * - CRC-16 (polynomial 0x1021, XOR finalization 0xFFFF)
 * - 7-bit convolutional LFSR ECC
 * - Canonical 16×7 bit interleaving (fsync-mdc1200-decode standard)
 * - MSB-first bit ordering
 * - Motorola-compatible frame structure
 *
 * Hardware integration: Calls BK4819_TransmitMDC1200Frame() from driver layer
 * to maintain clean separation between protocol and RF-level concerns.
 */

#include "mdc1200.h"

static uint16_t mdc1200_flip_crc16(uint16_t crc, int bitnum)
{
    uint16_t crcout = 0;
    uint16_t i;
    uint16_t j;

    j = 1u;
    for (i = (uint16_t)(1u << (bitnum - 1)); i != 0u; i >>= 1u) {
        if (crc & i)
            crcout |= j;
        j <<= 1u;
    }
    return crcout;
}

static uint16_t mdc1200_crc16(const uint8_t *p, size_t len)
{
    size_t i;
    int j;
    uint16_t c;
    int bit;
    uint16_t crc = 0x0000u;

    if (p == NULL && len != 0u) {
        return 0u;
    }

    for (i = 0u; i < len; ++i) {
        c = (uint16_t)p[i];
        c = mdc1200_flip_crc16(c, 8);

        for (j = 0x80; j; j >>= 1) {
            bit = (int)(crc & 0x8000u);
            crc <<= 1u;
            if (c & (uint16_t)j)
                bit ^= 0x8000;
            if (bit)
                crc ^= 0x1021u;
        }
    }

    crc = mdc1200_flip_crc16(crc, 16);
    crc ^= 0xffffu;
    crc &= 0xFFFFu;
    return crc;
}

/* Authentic MDC-1200 16x7 bit interleaver.
 *
 * The 112 source bits (14 bytes: 4 data + 2 CRC + 1 pad + 7 ECC) are spread
 * across a 16-column by 7-row matrix. Source bit n is placed at output bit
 * position (n % 7) * 16 + (n / 7). This is the exact interleave used by the
 * canonical MDC-1200 reference (fsync-mdc1200-decode / mdc-encode-decode) and
 * is the inverse of the de-interleaver used on the receive side.
 *
 * The previous implementation used a broken "step by 16 with wrap" loop that
 * wrote one element past the end of the 112-bit buffer (lbits[112]..lbits[125])
 * on every 8th bit, silently losing 14 source bits and emitting 14 uninitialized
 * bits. That produced frames that were NOT valid MDC-1200 and would not decode
 * on genuine receivers, nor round-trip through MDC1200_DecodeFrame(). This
 * canonical permutation fixes the encoder so it is bit-exact with the standard.
 */
static uint8_t mdc1200_interleave_pos(unsigned int n)
{
    return (uint8_t)(((n % 7u) * 16u) + (n / 7u));
}

static void mdc1200_encode_str(uint8_t *data)
{
    uint8_t csr[7] = {0};
    uint8_t src[14] = {0};
    uint8_t lbits[112] = {0};
    int i;
    int j;
    int k;
    int b;
    uint16_t crc;

    if (data == NULL) {
        return;
    }

    crc = mdc1200_crc16(data, 4u);
    data[4] = (uint8_t)(crc & 0x00FFu);
    data[5] = (uint8_t)((crc >> 8) & 0x00FFu);
    data[6] = 0;

    /* Convolutional ECC: one parity byte per data byte over a K=7 shift
     * register with generator taps at positions 0, 2, 5 and 6. */
    for (i = 0; i < 7; ++i) {
        data[i + 7] = 0;
        for (j = 0; j <= 7; ++j) {
            for (k = 6; k > 0; --k)
                csr[k] = csr[k - 1];
            csr[0] = (data[i] >> j) & 0x01u;
            b = csr[0] + csr[2] + csr[5] + csr[6];
            data[i + 7] |= (uint8_t)((b & 0x01u) << j);
        }
    }

    /* Snapshot the 14 source bytes (4 data + 2 CRC + 1 pad + 7 ECC) before
     * overwriting, then extract their 112 bits MSB-first (bit 7 of each byte
     * is the first bit), interleave with the canonical 16x7 permutation, and
     * repack MSB-first. The decoder uses the exact inverse convention, so a
     * built frame round-trips bit-for-bit. */
    for (i = 0; i < 14; ++i) {
        src[i] = data[i];
    }

    for (i = 0; i < 112; ++i) {
        k = (int)mdc1200_interleave_pos((unsigned int)i);
        lbits[k] = (uint8_t)((src[i / 8] >> (7 - (i % 8))) & 0x01u);
    }

    for (i = 0; i < 14; ++i) {
        data[i] = 0;
        for (j = 0; j < 8; ++j) {
            if (lbits[(i * 8) + j])
                data[i] |= (uint8_t)(1u << (7 - j));
        }
    }
}

MDC1200_Error_t MDC1200_BuildFrame(uint8_t op,
                                    uint8_t arg,
                                    uint16_t unit_id,
                                    uint8_t *frame,
                                    size_t frame_size,
                                    size_t *frame_len_out)
{
    uint8_t *dp;

    if (frame == NULL || frame_len_out == NULL)
        return MDC1200_ERROR_INVALID_PARAMS;

    if (frame_size < MDC1200_FRAME_LENGTH)
        return MDC1200_ERROR_INVALID_PARAMS;

    frame[0] = 0x55;
    frame[1] = 0x55;
    frame[2] = 0x55;
    frame[3] = 0x55;
    frame[4] = 0x55;
    frame[5] = 0x55;
    frame[6] = 0x55;

    frame[7] = 0x07;
    frame[8] = 0x09;
    frame[9] = 0x2A;
    frame[10] = 0x44;
    frame[11] = 0x6F;

    dp = &frame[12];
    dp[0] = op;
    dp[1] = arg;
    dp[2] = (uint8_t)((unit_id >> 8) & 0xFFu);
    dp[3] = (uint8_t)(unit_id & 0xFFu);

    mdc1200_encode_str(dp);

    *frame_len_out = MDC1200_FRAME_LENGTH;
    return MDC1200_ERROR_NONE;
}

MDC1200_Error_t MDC1200_BuildFifoWords(const uint8_t *frame,
                                       size_t frame_len,
                                       uint16_t *fifo_words,
                                       size_t fifo_word_capacity,
                                       size_t *fifo_word_count_out)
{
    size_t i;
    const size_t bytes_per_word = 2u;

    if (frame == NULL || fifo_words == NULL || fifo_word_count_out == NULL)
        return MDC1200_ERROR_INVALID_PARAMS;

    if (frame_len != MDC1200_FRAME_LENGTH)
        return MDC1200_ERROR_FRAME_BUILD_FAILED;

    if ((frame_len % bytes_per_word) != 0u)
        return MDC1200_ERROR_FRAME_BUILD_FAILED;

    if (fifo_word_capacity < MDC1200_FIFO_WORD_COUNT)
        return MDC1200_ERROR_FIFO_WRITE_FAILED;

    for (i = 0u; i < frame_len; i += bytes_per_word) {
        fifo_words[i / 2u] = ((uint16_t)frame[i] << 8u) | (uint16_t)frame[i + 1u];
    }

    *fifo_word_count_out = MDC1200_FIFO_WORD_COUNT;
    return MDC1200_ERROR_NONE;
}

/* Inverse of mdc1200_interleave_pos().
 *
 * Forward: k = (n % 7) * 16 + (n / 7)   =>  col = n % 7, row = n / 7
 * Inverse: n = (k % 16) * 7 + (k / 16)  =>  row = k % 16, col = k / 16
 *
 * Given a frame bit index k (0..111) that holds source bit n, recover n. */
static unsigned int mdc1200_deinterleave_pos(unsigned int k)
{
    return (k % 16u) * 7u + (k / 16u);
}

/* Reverse the canonical 16x7 bit interleave. on-air/frame bit at index k holds
 * source bit mdc1200_deinterleave_pos(k). */
static void mdc1200_deinterleave(const uint8_t *bits, uint8_t *out)
{
    unsigned int k;

    for (k = 0u; k < 112u; ++k) {
        out[mdc1200_deinterleave_pos(k)] = bits[k];
    }
}

MDC1200_Error_t MDC1200_DecodeFrame(const uint8_t *frame,
                                   size_t frame_len,
                                   uint8_t *op_out,
                                   uint8_t *arg_out,
                                   uint16_t *unit_id_out,
                                   bool *valid_out)
{
    uint8_t payload[14u] = {0};
    uint8_t bits[112u] = {0};
    uint8_t srcbits[112u] = {0};
    size_t n;
    size_t byte_index;
    size_t bit_index;
    uint16_t crc_in;
    uint16_t crc_calc;

    if (frame == NULL || op_out == NULL || arg_out == NULL || unit_id_out == NULL || valid_out == NULL)
        return MDC1200_ERROR_INVALID_PARAMS;

    if (frame_len != MDC1200_FRAME_LENGTH)
        return MDC1200_ERROR_FRAME_BUILD_FAILED;

    for (n = 0u; n < 7u; ++n) {
        if (frame[n] != 0x55u) {
            *valid_out = false;
            return MDC1200_ERROR_NONE;
        }
    }

    if (frame[7] != 0x07u || frame[8] != 0x09u || frame[9] != 0x2Au ||
        frame[10] != 0x44u || frame[11] != 0x6Fu) {
        *valid_out = false;
        return MDC1200_ERROR_NONE;
    }

    for (n = 0u; n < 112u; ++n) {
        byte_index = (n / 8u);
        bit_index = (n % 8u);
        bits[n] = (uint8_t)((frame[12u + byte_index] >> (7u - bit_index)) & 0x01u);
    }

    mdc1200_deinterleave(bits, srcbits);

    for (n = 0u; n < 112u; ++n) {
        byte_index = (n / 8u);
        bit_index = (n % 8u);
        payload[byte_index] = (uint8_t)(payload[byte_index] | ((srcbits[n] & 0x01u) << (7u - bit_index)));
    }

    *op_out = payload[0];
    *arg_out = payload[1];
    *unit_id_out = ((uint16_t)payload[2] << 8u) | (uint16_t)payload[3];

    crc_in = ((uint16_t)payload[5] << 8u) | (uint16_t)payload[4];
    crc_calc = mdc1200_crc16(payload, 4u);
    *valid_out = (crc_in == crc_calc);

    return MDC1200_ERROR_NONE;
}

MDC1200_Error_t MDC1200_DecodeFrameWords(const uint16_t *fifo_words,
                                        size_t fifo_word_count,
                                        uint8_t *op_out,
                                        uint8_t *arg_out,
                                        uint16_t *unit_id_out,
                                        bool *valid_out)
{
    uint8_t frame[MDC1200_FRAME_LENGTH] = {0};
    size_t i;

    if (fifo_words == NULL || op_out == NULL || arg_out == NULL || unit_id_out == NULL || valid_out == NULL)
        return MDC1200_ERROR_INVALID_PARAMS;

    if (fifo_word_count != MDC1200_FIFO_WORD_COUNT)
        return MDC1200_ERROR_FRAME_BUILD_FAILED;

    for (i = 0u; i < MDC1200_FIFO_WORD_COUNT; ++i) {
        frame[i * 2u] = (uint8_t)((fifo_words[i] >> 8u) & 0xFFu);
        frame[i * 2u + 1u] = (uint8_t)(fifo_words[i] & 0xFFu);
    }

    return MDC1200_DecodeFrame(frame, sizeof(frame), op_out, arg_out, unit_id_out, valid_out);
}

MDC1200_Error_t MDC1200_VerifyCRC(const uint8_t *frame,
                                 size_t frame_len,
                                 bool *valid_out)
{
    uint8_t payload[14u] = {0};
    uint8_t bits[112u] = {0};
    uint8_t srcbits[112u] = {0};
    uint16_t crc_in;
    uint16_t crc_calc;
    size_t n;
    size_t byte_index;
    size_t bit_index;

    if (frame == NULL || valid_out == NULL)
        return MDC1200_ERROR_INVALID_PARAMS;

    if (frame_len != MDC1200_FRAME_LENGTH)
        return MDC1200_ERROR_FRAME_BUILD_FAILED;

    for (n = 0u; n < 7u; ++n) {
        if (frame[n] != 0x55u) {
            *valid_out = false;
            return MDC1200_ERROR_NONE;
        }
    }

    if (frame[7] != 0x07u || frame[8] != 0x09u || frame[9] != 0x2Au ||
        frame[10] != 0x44u || frame[11] != 0x6Fu) {
        *valid_out = false;
        return MDC1200_ERROR_NONE;
    }

    for (n = 0u; n < 112u; ++n) {
        byte_index = (n / 8u);
        bit_index = (n % 8u);
        bits[n] = (uint8_t)((frame[12u + byte_index] >> (7u - bit_index)) & 0x01u);
    }

    mdc1200_deinterleave(bits, srcbits);

    for (n = 0u; n < 112u; ++n) {
        byte_index = (n / 8u);
        bit_index = (n % 8u);
        payload[byte_index] = (uint8_t)(payload[byte_index] | ((srcbits[n] & 0x01u) << (7u - bit_index)));
    }

    crc_in = ((uint16_t)payload[5] << 8u) | (uint16_t)payload[4];
    crc_calc = mdc1200_crc16(payload, 4u);
    *valid_out = (crc_in == crc_calc);

    return MDC1200_ERROR_NONE;
}

/* External RF transmission function provided by the driver layer (bk4819.c/bk4829.c).
 * This function handles low-level register configuration and RF transmission. */
extern int BK4819_TransmitMDC1200Frame(const uint8_t *frame, size_t frame_len)
    __attribute__((weak));

int BK4819_TransmitMDC1200Frame(const uint8_t *frame, size_t frame_len)
{
    (void)frame;
    (void)frame_len;
    return MDC1200_ERROR_TX_NOT_READY;
}

/**
 * MDC1200_Transmit: Public API for parameterized MDC-1200 transmission
 *
 * This is the primary public interface for MDC-1200 transmission. It combines
 * frame building and RF transmission in one complete operation.
 *
 * Protocol Features (all v7.6.10A):
 * - CRC-16 with polynomial 0x1021 and XOR finalization (0xFFFF)
 * - 7-bit convolutional LFSR error correction code (ECC)
 * - Canonical 16×7 bit interleaving (matches fsync-mdc1200-decode standard)
 * - MSB-first bit extraction and repacking
 * - 7-byte 0x55 preamble for synchronization
 * - 5-byte leader: 0x07 0x09 0x2A 0x44 0x6F
 * - 14-byte encoded payload (4 data + 2 CRC + 1 pad + 7 ECC bytes)
 * - Total frame: 26 bytes (13 × 16-bit FIFO words)
 *
 * Single-burst MDC-1200 is the supported transmission mode. Legacy long-mode
 * settings are normalized to single-burst for protocol compliance.
 *
 * @param params - Pointer to MDC1200_Params_t with:
 *                 - unit_id: destination Unit ID (0x0000 = broadcast)
 *                 - op: opcode (e.g., 0x00 for status)
 *                 - arg: opcode-dependent argument
 *
 * @return MDC1200_ERROR_NONE (0) on success, or negative MDC1200_Error_t code:
 *         - MDC1200_ERROR_INVALID_PARAMS: params is NULL
 *         - MDC1200_ERROR_FRAME_BUILD_FAILED: Frame encoding error
 *         - MDC1200_ERROR_FIFO_WRITE_FAILED: FIFO conversion error
 *         - MDC1200_ERROR_TX_NOT_READY: Transceiver not ready (optional driver check)
 *
 * v7.6.10A: Full protocol-layer implementation with comprehensive error handling.
 */
MDC1200_Error_t MDC1200_Transmit(const MDC1200_Params_t *params)
{
    uint8_t frame[26];
    size_t frame_len = 0;
    int status = 0;

    /* Validate input parameters */
    if (params == NULL) {
        return MDC1200_ERROR_INVALID_PARAMS;
    }

    /* Build complete MDC-1200 frame with protocol-mandated encoding:
     * - CRC-16 calculation over 4 data bytes
     * - 7-byte convolutional ECC generation
     * - Bit interleaving and repacking
     * - Preamble and leader injection */
    status = (int)MDC1200_BuildFrame(params->op, params->arg, params->unit_id,
                                     frame, sizeof(frame), &frame_len);
    if (status != 0) {
        return MDC1200_ERROR_FRAME_BUILD_FAILED;
    }

    /* Transmit frame via RF driver. The driver layer (BK4819_TransmitMDC1200Frame)
     * handles register configuration, FIFO loading, and RF timing for the
     * supported single-burst transmission mode. */
    return (MDC1200_Error_t)BK4819_TransmitMDC1200Frame(frame, frame_len);
}
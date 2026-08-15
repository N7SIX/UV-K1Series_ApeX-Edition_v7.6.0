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
 * MDC-1200 Implementation: v7.6.10A
 * Pure logic layer for protocol-compliant MDC-1200 frame encoding.
 * Hardware-specific transmission handled in driver layer (bk4819.c, bk4829.c).
 * 
 * Note: MDC1200_Transmit() is declared here but implemented in the driver layer
 * to maintain separation of concerns and avoid circular dependencies.
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

static void mdc1200_encode_str(uint8_t *data)
{
    uint8_t csr[7] = {0};
    int i;
    int j;
    int k;
    int m;
    int b;
    uint8_t lbits[112];
    uint16_t crc;

    if (data == NULL) {
        return;
    }

    crc = mdc1200_crc16(data, 4u);
    data[4] = (uint8_t)(crc & 0x00FFu);
    data[5] = (uint8_t)((crc >> 8) & 0x00FFu);
    data[6] = 0;

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

    k = 0;
    m = 0;
    for (i = 0; i < 14; ++i) {
        for (j = 0; j <= 7; ++j) {
            b = 0x01 & (data[i] >> j);
            lbits[k] = b;
            k += 16;
            if (k > 111)
                k = ++m;
        }
    }

    k = 0;
    for (i = 0; i < 14; ++i) {
        data[i] = 0;
        for (j = 7; j >= 0; --j) {
            if (lbits[k])
                data[i] |= (uint8_t)(1u << j);
            ++k;
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

MDC1200_Error_t MDC1200_DecodeFrame(const uint8_t *frame,
                                   size_t frame_len,
                                   uint8_t *op_out,
                                   uint8_t *arg_out,
                                   uint16_t *unit_id_out,
                                   bool *valid_out)
{
    uint8_t payload[14u] = {0};
    uint8_t bits[112u] = {0};
    size_t n;
    size_t pos;
    size_t byte_index;
    size_t bit_index;
    uint16_t crc_in;
    uint16_t crc_calc;

    if (frame == NULL || op_out == NULL || arg_out == NULL || unit_id_out == NULL || valid_out == NULL)
        return MDC1200_ERROR_INVALID_PARAMS;

    if (frame_len != MDC1200_FRAME_LENGTH)
        return MDC1200_ERROR_FRAME_BUILD_FAILED;

    for (n = 0u; n < 7u; ++n) {
        if (frame[n] != 0x55u)
            break;
    }

    for (n = 0u; n < 112u; ++n) {
        byte_index = (n / 8u);
        bit_index = (n % 8u);
        bits[n] = (uint8_t)((frame[12u + byte_index] >> (7u - bit_index)) & 0x01u);
    }

    for (n = 0u; n < 112u; ++n) {
        pos = ((n % 7u) * 16u) + (n / 7u);
        byte_index = (pos / 8u);
        bit_index = (pos % 8u);
        payload[byte_index] = (uint8_t)(payload[byte_index] | ((bits[n] & 0x01u) << (7u - bit_index)));
    }

    *op_out = payload[0];
    *arg_out = payload[1];
    *unit_id_out = ((uint16_t)payload[2] << 8u) | (uint16_t)payload[3];

    crc_in = ((uint16_t)payload[4] | ((uint16_t)payload[5] << 8u));
    crc_calc = mdc1200_crc16(payload, 4u);
    *valid_out = (crc_in == crc_calc);

    return MDC1200_ERROR_NONE;
}

MDC1200_Error_t MDC1200_VerifyCRC(const uint8_t *frame,
                                 size_t frame_len,
                                 bool *valid_out)
{
    uint8_t payload[14u] = {0};
    uint8_t bits[112u] = {0};
    uint16_t crc_in;
    uint16_t crc_calc;
    size_t n;
    size_t pos;
    size_t byte_index;
    size_t bit_index;

    if (frame == NULL || valid_out == NULL)
        return MDC1200_ERROR_INVALID_PARAMS;

    if (frame_len != MDC1200_FRAME_LENGTH)
        return MDC1200_ERROR_FRAME_BUILD_FAILED;

    for (n = 0u; n < 112u; ++n) {
        byte_index = (n / 8u);
        bit_index = (n % 8u);
        bits[n] = (uint8_t)((frame[12u + byte_index] >> (7u - bit_index)) & 0x01u);
    }

    for (n = 0u; n < 112u; ++n) {
        pos = ((n % 7u) * 16u) + (n / 7u);
        byte_index = (pos / 8u);
        bit_index = (pos % 8u);
        payload[byte_index] = (uint8_t)(payload[byte_index] | ((bits[n] & 0x01u) << (7u - bit_index)));
    }

    crc_in = ((uint16_t)payload[4] | ((uint16_t)payload[5] << 8u));
    crc_calc = mdc1200_crc16(payload, 4u);
    *valid_out = (crc_in == crc_calc);

    return MDC1200_ERROR_NONE;
}

/* Copyright 2026 Sean, N7SIX
 * https://github.com/N7SIX
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * MDC-1200 Full Implementation: v7.6.10A
 *
 * Protocol-layer implementation supporting two transmission profiles:
 * - MDC1200_BuildFrame():  26-byte standard frame (7-byte preamble)        -> PTT_ID_MDC1200
 * - MDC1200_BuildFrameLong(): 46-byte long frame (27-byte composite preamble)-> PTT_ID_MDC1200L
 * - MDC1200_DecodeFrame(): bit-exact decode + CRC + Viterbi ECC for both lengths
 *
 * The encoded payload (leader + 14 payload bytes + CRC + ECC + interleaving)
 * is identical for both; only the leading 0x55 run length differs.
 */

#include "mdc1200.h"
#include <string.h>

/* Nibble-table CRC-16 (poly 0x1021, reflected in/out, final XOR 0xFFFF).
 *
 * The original bit-serial implementation reflected each input byte and ran
 * an MSB-first 0x1021 LFSR, then reflected the register and XORed 0xFFFF.
 * That is equivalent to a standard LSB-first CRC with the reflected
 * polynomial 0x8408, processed nibble-wise:
 *   crc = (crc >> 4) ^ T[(crc ^ byte) & 0x0F];        // low nibble
 *   crc = (crc >> 4) ^ T[(crc ^ (byte >> 4)) & 0x0F]; // high nibble
 * which is bit-for-bit identical to the bit-loop (verified exhaustively by
 * tests/crc_equiv_check.c), ~4x faster than the bit-loop, and uses only a
 * 16-entry table (32 B) instead of a 256-entry table (512 B) -- a 480 B RAM
 * saving on this small-MCU firmware. The table lives in .bss and is built
 * once on first use; the single-threaded context makes lazy init safe.
 */
static uint16_t mdc1200_crc_nibble[16u];
static bool mdc1200_crc_table_ready = false;

static void mdc1200_crc_table_init(void)
{
    unsigned int b;
    int j;
    uint16_t crc;

    if (mdc1200_crc_table_ready)
        return;

    for (b = 0u; b < 16u; ++b) {
        crc = (uint16_t)b;
        for (j = 0; j < 4; ++j)
            crc = (uint16_t)((crc >> 1) ^ ((crc & 1u) ? 0x8408u : 0x0000u));
        mdc1200_crc_nibble[b] = crc;
    }

    mdc1200_crc_table_ready = true;
}

static uint16_t mdc1200_crc16(const uint8_t *p, size_t len)
{
    uint16_t crc = 0x0000u;

    if (p == NULL && len != 0u) {
        return 0u;
    }

    mdc1200_crc_table_init();

    while (len-- != 0u) {
        crc = (uint16_t)((uint16_t)(crc >> 4) ^
                         mdc1200_crc_nibble[(uint8_t)((crc ^ *p) & 0x0Fu)]);
        crc = (uint16_t)((uint16_t)(crc >> 4) ^
                         mdc1200_crc_nibble[(uint8_t)((crc ^ (*p >> 4)) & 0x0Fu)]);
        ++p;
    }

    return (uint16_t)(crc ^ 0xffffu);
}

/* Canonical MDC-1200 16x7 bit interleaver.
 *
 * The 112 source bits (14 bytes: 4 data + 2 CRC + 1 pad + 7 ECC) are spread
 * across a 16-column by 7-row matrix. Source bit n is placed at output bit
 * position (n % 7) * 16 + (n / 7). This is the exact interleave used by the
 * canonical MDC-1200 reference (fsync-mdc1200-decode / mdc-encode-decode) and
 * is the inverse of the de-interleaver used on the receive side.
 *
 * The previous implementation used a broken "step by 16 with wrap" loop that
 * wrote one element past the end of the 112-bit buffer (lbits[112]..lbits[125])
 * on every 8th bit, silently losing 14 source bits and emitting 14
 * uninitialized bits. That produced frames that were NOT valid MDC-1200 and
 * would not decode on genuine receivers, nor round-trip through
 * MDC1200_DecodeFrame(). This canonical permutation fixes the encoder so it
 * is bit-exact with the standard.
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

/*
 * Shared frame builder. The on-air payload (leader + 14 payload bytes) is
 * identical for both profiles; only the leading 0x55 preamble run differs:
 *   long_preamble == false :  7-byte standard preamble  -> 26-byte frame
 *   long_preamble == true  :  27-byte composite preamble -> 46-byte frame
 */
static MDC1200_Error_t MDC1200_BuildFrameImpl(uint8_t op,
                                              uint8_t arg,
                                              uint16_t unit_id,
                                              uint8_t *frame,
                                              size_t frame_size,
                                              size_t *frame_len_out,
                                              bool long_preamble)
{
    const size_t preamble_len = long_preamble
        ? MDC1200_COMPOSITE_PREAMBLE_LENGTH : MDC1200_PREAMBLE_LENGTH;
    const size_t total_len   = long_preamble
        ? MDC1200L_FRAME_LENGTH : MDC1200_FRAME_LENGTH;
    uint8_t *dp;
    size_t i;

    if (frame == NULL || frame_len_out == NULL)
        return MDC1200_ERROR_INVALID_PARAMS;

    if (frame_size < total_len)
        return MDC1200_ERROR_INVALID_PARAMS;

    /* Preamble: 0x55 alternating-bit run. Standard MDC-1200 uses 7 bytes;
     * MDC-1200L prepends 20 bytes of extended pretime (20 + 7 = 27 bytes,
     * ~180 ms at 1200 baud) for distant/weak-signal receivers. The BK4819
     * hardware sync detector locks on the first four 0x55 bytes, and the RX
     * sliding-window sync search tolerates bit slips around the leader. */
    for (i = 0u; i < preamble_len; ++i)
        frame[i] = (uint8_t)MDC1200_PREAMBLE_BYTE;

    frame[preamble_len + 0] = 0x07;
    frame[preamble_len + 1] = 0x09;
    frame[preamble_len + 2] = 0x2A;
    frame[preamble_len + 3] = 0x44;
    frame[preamble_len + 4] = 0x6F;

    dp = &frame[preamble_len + MDC1200_LEADER_LENGTH];
    dp[0] = op;
    dp[1] = arg;
    dp[2] = (uint8_t)((unit_id >> 8) & 0xFFu);
    dp[3] = (uint8_t)(unit_id & 0xFFu);

    mdc1200_encode_str(dp);

    *frame_len_out = total_len;
    return MDC1200_ERROR_NONE;
}

/* Standard 26-byte MDC-1200 frame: 7-byte preamble + 5-byte leader + 14-byte
 * encoded payload. Protocol-compliant short burst. */
MDC1200_Error_t MDC1200_BuildFrame(uint8_t op,
                                    uint8_t arg,
                                    uint16_t unit_id,
                                    uint8_t *frame,
                                    size_t frame_size,
                                    size_t *frame_len_out)
{
    return MDC1200_BuildFrameImpl(op, arg, unit_id, frame, frame_size,
                                  frame_len_out, false);
}

/* MDC-1200L 46-byte frame: 27-byte composite preamble + 5-byte leader +
 * 14-byte encoded payload. Extended pretime for weak-signal reach. */
MDC1200_Error_t MDC1200_BuildFrameLong(uint8_t op,
                                       uint8_t arg,
                                       uint16_t unit_id,
                                       uint8_t *frame,
                                       size_t frame_size,
                                       size_t *frame_len_out)
{
    return MDC1200_BuildFrameImpl(op, arg, unit_id, frame, frame_size,
                                  frame_len_out, true);
}

MDC1200_Error_t MDC1200_BuildFifoWords(const uint8_t *frame,
                                       size_t frame_len,
                                       uint16_t *fifo_words,
                                       size_t fifo_word_capacity,
                                       size_t *fifo_word_count_out)
{
    size_t i;
    const size_t bytes_per_word = 2u;
    size_t words;

    if (frame == NULL || fifo_words == NULL || fifo_word_count_out == NULL)
        return MDC1200_ERROR_INVALID_PARAMS;

    if (frame_len != MDC1200_FRAME_LENGTH && frame_len != MDC1200L_FRAME_LENGTH)
        return MDC1200_ERROR_FRAME_BUILD_FAILED;

    if ((frame_len % bytes_per_word) != 0u)
        return MDC1200_ERROR_FRAME_BUILD_FAILED;

    words = frame_len / bytes_per_word;
    if (fifo_word_capacity < words)
        return MDC1200_ERROR_FIFO_WRITE_FAILED;

    for (i = 0u; i < frame_len; i += bytes_per_word) {
        fifo_words[i / 2u] = ((uint16_t)frame[i] << 8u) | (uint16_t)frame[i + 1u];
    }

    *fifo_word_count_out = words;
    return MDC1200_ERROR_NONE;
}

MDC1200_Error_t MDC1200_BuildTxFifoWords(const uint8_t *frame,
                                          size_t frame_len,
                                          uint16_t *fifo_words,
                                          size_t fifo_word_capacity,
                                          size_t *fifo_word_count_out)
{
    size_t i;
    size_t data_len;
    size_t word_count;
    uint8_t tx_buf[MDC1200L_FRAME_LENGTH];  /* holds stripped frame + pad */

    if (frame == NULL || fifo_words == NULL || fifo_word_count_out == NULL)
        return MDC1200_ERROR_INVALID_PARAMS;

    if (frame_len != MDC1200_FRAME_LENGTH && frame_len != MDC1200L_FRAME_LENGTH)
        return MDC1200_ERROR_FRAME_BUILD_FAILED;

    if (frame_len == MDC1200_FRAME_LENGTH) {
        /*
         * Standard frame: [7-byte preamble][5-byte leader][14-byte payload]
         * The BK4819 generates its own 7-byte preamble + 4-byte sync (all 0x55)
         * before the FIFO data, so we strip the frame's 7-byte preamble to
         * avoid a double preamble and match genuine Motorola's 7-byte run.
         *
         * Stripped: 5 + 14 = 19 bytes (odd) → pad 1×0x55 → 20 bytes → 10 words
         */
        memcpy(tx_buf,
               &frame[MDC1200_PREAMBLE_LENGTH],
               MDC1200_LEADER_LENGTH + MDC1200_PAYLOAD_LENGTH);
        data_len = MDC1200_LEADER_LENGTH + MDC1200_PAYLOAD_LENGTH;  /* 19 */
        tx_buf[data_len] = MDC1200_PREAMBLE_BYTE;  /* pad for word alignment */
        data_len++;                                    /* 20 */
    } else {
        /*
         * Long frame: [20-byte pretime][7-byte sync preamble][5-byte leader][14-byte payload]
         * Strip the 7-byte sync preamble only (keep the 20-byte pretime for weak-signal
         * reach).  The hardware adds 11 bytes (7 preamble + 4 sync, all 0x55), giving a
         * total preamble of 11 + 20 + 1(pad) = 32 bytes vs genuine Motorola's 27.
         *
         * Non-contiguous copy: pretime (bytes 0-19) + leader+payload (bytes 27-45)
         * Stripped: 20 + 5 + 14 = 39 bytes (odd) → pad 1×0x55 → 40 bytes → 20 words
         */
        memcpy(tx_buf, frame, MDC1200_PRETIME_LENGTH);
        memcpy(tx_buf + MDC1200_PRETIME_LENGTH,
               &frame[MDC1200_COMPOSITE_PREAMBLE_LENGTH],
               MDC1200_LEADER_LENGTH + MDC1200_PAYLOAD_LENGTH);
        data_len = MDC1200_PRETIME_LENGTH +
                   MDC1200_LEADER_LENGTH +
                   MDC1200_PAYLOAD_LENGTH;  /* 39 */
        tx_buf[data_len] = MDC1200_PREAMBLE_BYTE;  /* pad for word alignment */
        data_len++;                                    /* 40 */
    }

    /* Convert bytes to 16-bit FIFO words (MSB first, as BK4819 expects) */
    word_count = data_len / 2u;
    if (word_count > fifo_word_capacity)
        return MDC1200_ERROR_FIFO_WRITE_FAILED;

    for (i = 0u; i < word_count; i++) {
        fifo_words[i] = ((uint16_t)tx_buf[i * 2u] << 8u) | (uint16_t)tx_buf[i * 2u + 1u];
    }

    *fifo_word_count_out = word_count;
    return MDC1200_ERROR_NONE;
}

/* Inverse of mdc1200_interleave_pos().
 *
 * Forward: k = (n % 7) * 16 + (n / 7)   =>  col = n % 7, row = n / 7
 * Inverse: n = (k % 16) * 7 + (k /  16) =>  row = k % 16, col = k / 16
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

/* Hamming distance between a candidate 5-byte leader and the MDC-1200 sync
 * word 07 09 2A 44 6F. Used by the RX sliding-window sync search. */
static unsigned int mdc1200_popcount8(unsigned int v)
{
    unsigned int count = 0u;
    while (v != 0u) {
        v &= v - 1u;
        ++count;
    }
    return count;
}

static bool mdc1200_leader_match(const uint8_t *p)
{
    static const uint8_t leader[5u] = { 0x07u, 0x09u, 0x2Au, 0x44u, 0x6Fu };
    unsigned int dist = 0u;
    unsigned int i;

    for (i = 0u; i < 5u; ++i)
        dist += mdc1200_popcount8((unsigned int)(p[i] ^ leader[i]));

    /* Tolerate up to 2 bit errors across the 40-bit sync word: enough to
     * ride through squelch-tail bit slips, far below the distance at which
     * random noise could plausibly masquerade as sync. */
    return dist <= 2u;
}

/* Extract the 14-byte interleaved payload following the leader found at offset
 * `off` within a captured frame of `frame_len` bytes. Returns true if a
 * matching leader was found and the full payload fits inside frame_len. */
static bool mdc1200_frame_to_payload(const uint8_t *frame, size_t frame_len,
                                     size_t *off_out, uint8_t *payload)
{
    size_t n;
    size_t byte_index;
    size_t bit_index;
    size_t off;
    bool found = false;
    uint8_t bits[112u] = {0};
    uint8_t srcbits[112u] = {0};

    /* Sliding-window sync search.
     *
     * The leader sits immediately after the preamble (offset 7 for standard
     * 26-byte frames, offset 27 for 46-byte long frames), but on-air bit
     * slips can shift the captured frame by a few byte positions. Scan every
     * legal leader offset (leader + 14-byte payload must still fit) and
     * accept the first offset whose leader matches within 2 bit errors; the
     * canonical offset is tried first so clean frames take the fast path. */
    for (off = 0u; off + 5u + 14u <= frame_len; ++off) {
        if (mdc1200_leader_match(&frame[off])) {
            found = true;
            break;
        }
    }

    if (!found)
        return false;

    for (n = 0u; n < 112u; ++n) {
        byte_index = (n / 8u);
        bit_index = (n % 8u);
        bits[n] = (uint8_t)((frame[off + 5u + byte_index] >> (7u - bit_index)) & 0x01u);
    }

    mdc1200_deinterleave(bits, srcbits);

    for (n = 0u; n < 112u; ++n) {
        byte_index = (n / 8u);
        bit_index = (n % 8u);
        payload[byte_index] = (uint8_t)(payload[byte_index] | ((srcbits[n] & 0x01u) << (7u - bit_index)));
    }

    if (off_out != NULL)
        *off_out = off;

    return true;
}

static bool mdc1200_payload_crc_ok(const uint8_t *payload)
{
    uint16_t crc_in = (uint16_t)(((uint16_t)payload[5] << 8u) | (uint16_t)payload[4]);
    return (crc_in == mdc1200_crc16(payload, 4u));
}

/* ============================================================================
 * Hard-decision Viterbi error correction for the rate-1/2 K=7 code
 *
 * The encoder emits one systematic bit plus one parity bit per data bit,
 * processed byte-by-byte, LSB-first within each byte:
 *   parity(d_t) = d_t ^ d_{t-2} ^ d_{t-5} ^ d_{t-6}
 * State v packs the previous six inputs: v&1 = d_{t-1} ... (v>>5)&1 = d_{t-6}.
 * The 56 systematic bits live in payload[0..6] and the 56 parity bits in
 * payload[7..13], using the same byte/bit ordering as the encoder.
 * ============================================================================ */

#define MDC1200_VIT_STEPS  56u
#define MDC1200_VIT_STATES 64u

/* Scratch buffers are file-scope instead of on-stack: ~600 B of stack is a
 * real overflow risk on this small-MCU firmware. The RX decode path runs in
 * a single thread, so shared static scratch is safe.
 *
 * Metrics are uint8: the worst-case path metric is 56 steps x 2 bit errors
 * = 112, which fits in a uint8 (max 255). Using uint8 instead of uint16
 * halves the metric buffer RAM (64 states x 2 arrays x 1 B saved = 128 B). */
static uint8_t mdc1200_vit_metric[MDC1200_VIT_STATES];
static uint8_t mdc1200_vit_nmetric[MDC1200_VIT_STATES];
static uint64_t mdc1200_vit_decisions[MDC1200_VIT_STEPS];

static void mdc1200_viterbi_correct(const uint8_t *payload, uint8_t *out7)
{
    uint8_t *metric = mdc1200_vit_metric;
    uint8_t *nmetric = mdc1200_vit_nmetric;
    uint64_t *decisions = mdc1200_vit_decisions;
    unsigned int t;
    unsigned int s;
    unsigned int best;
    uint8_t best_metric;
    unsigned int st;

    memset(out7, 0, 7u);

    for (s = 0u; s < MDC1200_VIT_STATES; ++s) {
        metric[s] = 0xFFu;
        nmetric[s] = 0xFFu;
    }
    metric[0] = 0u;    /* start state from the all-zero state */

    for (t = 0u; t < MDC1200_VIT_STEPS; ++t) {
        const unsigned int byte_i = t >> 3;
        const unsigned int bit_j = t & 7u;
        const unsigned int rsys = (unsigned int)((payload[byte_i] >> bit_j) & 1u);
        const unsigned int rpar = (unsigned int)((payload[7u + byte_i] >> bit_j) & 1u);
        uint64_t d = 0;

        for (s = 0u; s < MDC1200_VIT_STATES; ++s) {
            unsigned int b;
            if (metric[s] == 0xFFu)
                continue;
            for (b = 0u; b < 2u; ++b) {
                const unsigned int ns = ((s << 1) | b) & (MDC1200_VIT_STATES - 1u);
                const unsigned int exp_par = b ^ ((s >> 1) & 1u) ^ ((s >> 4) & 1u) ^ ((s >> 5) & 1u);
                const unsigned int m = (unsigned int)metric[s]
                                     + (rsys != b ? 1u : 0u)
                                     + (rpar != exp_par ? 1u : 0u);
                if (m < nmetric[ns]) {
                    nmetric[ns] = (uint8_t)m;
                    /* Record which predecessor won: for ns = (s<<1)|b the
                     * deciding bit is whether s had its bit 5 set. */
                    if ((s & 32u) != 0u)
                        d |= (uint64_t)1u << ns;
                }
            }
        }

        decisions[t] = d;
        for (s = 0u; s < MDC1200_VIT_STATES; ++s) {
            metric[s] = nmetric[s];
            nmetric[s] = 0xFFu;
        }
    }

    best = 0u;
    best_metric = metric[0];
    for (s = 1u; s < MDC1200_VIT_STATES; ++s) {
        if (metric[s] < best_metric) {
            best_metric = metric[s];
            best = s;
        }
    }

    st = best;
    for (t = MDC1200_VIT_STEPS; t-- > 0u;) {
        /* The input bit at time t is the LSB of the current state. */
        out7[t >> 3] |= (uint8_t)((st & 1u) << (t & 7u));
        /* Step back to the predecessor, restoring its bit 5 from the
         * recorded survivor decision. */
        st = (st >> 1) | (unsigned int)(((decisions[t] >> st) & 1u) << 5);
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
    size_t leader_off;

    if (frame == NULL || op_out == NULL || arg_out == NULL || unit_id_out == NULL || valid_out == NULL)
        return MDC1200_ERROR_INVALID_PARAMS;

    if (frame_len != MDC1200_FRAME_LENGTH && frame_len != MDC1200L_FRAME_LENGTH)
        return MDC1200_ERROR_INVALID_LENGTH;

    if (!mdc1200_frame_to_payload(frame, frame_len, &leader_off, payload)) {
        *valid_out = false;
        return MDC1200_ERROR_NONE;
    }

    if (!mdc1200_payload_crc_ok(payload)) {
        /* CRC failed: attempt hard-decision Viterbi ECC correction over the
         * 7 systematic bytes (payload bytes 0..6 carry the 4 data bytes plus
         * the 2 CRC bytes and 1 pad byte that the conv encoder consumed in
         * register order). */
        uint8_t fixed[7u] = {0};
        mdc1200_viterbi_correct(payload, fixed);
        memcpy(payload, fixed, sizeof(fixed));
    }

    *valid_out = mdc1200_payload_crc_ok(payload);

    if (*valid_out) {
        *op_out = payload[0];
        *arg_out = payload[1];
        *unit_id_out = ((uint16_t)payload[2] << 8u) | (uint16_t)payload[3];
    }

    return MDC1200_ERROR_NONE;
}

MDC1200_Error_t MDC1200_DecodeFrameWords(const uint16_t *fifo_words,
                                        size_t fifo_word_count,
                                        uint8_t *op_out,
                                        uint8_t *arg_out,
                                        uint16_t *unit_id_out,
                                        bool *valid_out)
{
    uint8_t frame[MDC1200L_FRAME_LENGTH] = {0};
    size_t i;
    size_t frame_len;

    if (fifo_words == NULL || op_out == NULL || arg_out == NULL || unit_id_out == NULL || valid_out == NULL)
        return MDC1200_ERROR_INVALID_PARAMS;

    if (fifo_word_count != MDC1200_FIFO_WORD_COUNT &&
        fifo_word_count != MDC1200L_FIFO_WORD_COUNT)
        return MDC1200_ERROR_INVALID_LENGTH;

    frame_len = fifo_word_count * 2u;

    for (i = 0u; i < fifo_word_count; ++i) {
        frame[i * 2u]     = (uint8_t)((fifo_words[i] >> 8u) & 0xFFu);
        frame[i * 2u + 1u] = (uint8_t)(fifo_words[i] & 0xFFu);
    }

    return MDC1200_DecodeFrame(frame, frame_len, op_out, arg_out, unit_id_out, valid_out);
}

MDC1200_Error_t MDC1200_VerifyCRC(const uint8_t *frame,
                                 size_t frame_len,
                                 bool *valid_out)
{
    uint8_t payload[14u] = {0};

    if (frame == NULL || valid_out == NULL)
        return MDC1200_ERROR_INVALID_PARAMS;

    if (frame_len != MDC1200_FRAME_LENGTH && frame_len != MDC1200L_FRAME_LENGTH)
        return MDC1200_ERROR_INVALID_LENGTH;

    if (!mdc1200_frame_to_payload(frame, frame_len, NULL, payload)) {
        *valid_out = false;
        return MDC1200_ERROR_NONE;
    }

    if (!mdc1200_payload_crc_ok(payload)) {
        uint8_t fixed[7u] = {0};
        mdc1200_viterbi_correct(payload, fixed);
        memcpy(payload, fixed, sizeof(fixed));
    }

    *valid_out = mdc1200_payload_crc_ok(payload);

    return MDC1200_ERROR_NONE;
}

/* External RF transmission function provided by the driver layer
 * (bk4819.c/bk4829.c). Handles low-level register configuration and RF
 * transmission. Recognizes both 26-byte (standard) and 46-byte (long) frames. */
extern int BK4819_TransmitMDC1200Frame(const uint8_t *frame, size_t frame_len)
    __attribute__((weak));

int BK4819_TransmitMDC1200Frame(const uint8_t *frame, size_t frame_len)
{
    (void)frame;
    (void)frame_len;
    return MDC1200_ERROR_TX_NOT_READY;
}

/**
 * MDC1200_Transmit: Public API for standard (short) MDC-1200 transmission.
 *
 * Builds a 26-byte frame (7-byte preamble) and transmits a single burst.
 *
 * Protocol Features (v7.6.10A):
 * - CRC-16 polynomial 0x1021, final XOR 0xFFFF
 * - 7-bit convolutional LFSR ECC
 * - Canonical 16x7 bit interleaving
 * - 7-byte 0x55 preamble + 5-byte leader 07 09 2A 44 6F + 14-byte payload
 * - Total frame: 26 bytes (13 x 16-bit FIFO words)
 *
 * @param params - MDC1200_Params_t (unit_id, op, arg)
 * @return MDC1200_ERROR_NONE on success, negative error code otherwise.
 */
MDC1200_Error_t MDC1200_Transmit(const MDC1200_Params_t *params)
{
    uint8_t frame[MDC1200_FRAME_LENGTH];
    size_t frame_len = 0;
    int status;

    if (params == NULL)
        return MDC1200_ERROR_INVALID_PARAMS;

    status = (int)MDC1200_BuildFrame(params->op, params->arg, params->unit_id,
                                     frame, sizeof(frame), &frame_len);
    if (status != 0)
        return MDC1200_ERROR_FRAME_BUILD_FAILED;

    return (MDC1200_Error_t)BK4819_TransmitMDC1200Frame(frame, frame_len);
}

/**
 * MDC1200_TransmitLong: Public API for MDC-1200L (extended pretime) TX.
 *
 * Identical to MDC1200_Transmit() except it emits the 27-byte composite
 * preamble (20-byte extended pretime + 7-byte standard sync) as a 46-byte
 * frame. This is the longer, weak-signal-friendly burst exposed by the
 * PTT_ID_MDC1200L menu option.
 *
 * @param params - MDC1200_Params_t (unit_id, op, arg)
 * @return MDC1200_ERROR_NONE on success, negative error code otherwise.
 */
MDC1200_Error_t MDC1200_TransmitLong(const MDC1200_Params_t *params)
{
    uint8_t frame[MDC1200L_FRAME_LENGTH];
    size_t frame_len = 0;
    int status;

    if (params == NULL)
        return MDC1200_ERROR_INVALID_PARAMS;

    status = (int)MDC1200_BuildFrameLong(params->op, params->arg, params->unit_id,
                                         frame, sizeof(frame), &frame_len);
    if (status != 0)
        return MDC1200_ERROR_FRAME_BUILD_FAILED;

    return (MDC1200_Error_t)BK4819_TransmitMDC1200Frame(frame, frame_len);
}

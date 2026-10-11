/* Copyright 2026 N7SIX
 * https://github.com/n7six
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * ApeX-only MDC-1200 roger variants (MENU > Roger: MDC-EP / MDC-2P / MDC-EL).
 *
 * This module ports the proven v7.6.10A reference MDC-1200 stack
 * (App/mdc1200.c frame builder + BK4819_TransmitMDC1200Frame) so each
 * variant is a REAL MDC-1200 burst on air instead of a home-made run-in:
 *
 *  - Frame layout: 0x55 run-in (7 B standard / 27 B MDC-1200L composite
 *    pretime) + 5 B leader (07 09 2A 44 6F) + 14 B payload built from
 *    op/arg/unit-id via CRC-16, K=7 convolutional ECC and the canonical
 *    16x7 bit interleave.
 *  - Motorola XOR differential encoding of each frame's bit stream in
 *    software (the BK4819/BK4829 has NO register for it): the 0x55 run-in
 *    comes out on air as the steady preamble tone and the payload as the
 *    characteristic warble - the classic MDC rasp.
 *  - Reference register programming: REG_58 0x37C3, REG_72 0x3065,
 *    REG_70 0x00E0 (Tone-2 gain 96 - same as the stock BK4819 roger path;
 *    the old loud-gain 112 deviation is dropped for reference fidelity),
 *    REG_5A/5B 0x5555, REG_5C 0xAA30 and REG_5D = raw TX FIFO byte count
 *    (0x1A00 / 0x2E00 for 26 / 46 B frames; 11-bit field, low 8 bits in
 *    <15:8>, high 3 in <7:5>). The preamble-stripped 0x1400/0x2800 variant
 *    produced no audible burst on real hardware and must not return.
 *
 * Each variant is ONE continuous TX-enable - a single FIFO load with no
 * inter-frame gap - so the receiving squelch never closes mid-burst (a
 * prior multi-frame version with gaps was cut off by fast squelch). The
 * chip's HW preamble+sync is prepended once at TX start. Variants:
 *
 *   EP = 1 x MDC-1200L long frame   (27 B pretime + ANI 0x01)
 *        on air 11 HW B + 46 B ~= 380 ms   -> ~415 ms armed
 *   2P = 2 x standard frame         (Call Alert 0x30 twice - real MDC
 *        sends every message as two packets)
 *        on air 11 + 52 B ~= 420 ms        -> ~455 ms armed
 *   EL = 3 x standard frame         (Emergency 0x81 three times)
 *        on air 11 + 78 B ~= 593 ms        -> ~628 ms armed
 *   (armed = (bytes + 11) * 8 / 1200 ms + 35 ms guard; the TX FIFO holds
 *    128 words so the 78 B EL stream fits in one load)
 *
 * Frames are differential-encoded individually - prev-bit resets at each
 * frame start, exactly like the reference single-frame path - so every
 * packet in the stream stays independently decodable. There is no MDC unit
 * ID setting in this firmware: 0x0000 is sent (opcode/arg still make the
 * variants acoustically distinct).
 *
 * TX discipline mirrors the stock roger: program the modem, load the FIFO,
 * arm FSK TX, blind-delay the on-air time, stop TX. NO TX-done polling -
 * REG_0C<0> reads "done" immediately on this path and truncates every
 * burst to a short squawk.
 *
 * Preconditions (same as the stock roger path): call only with the TX
 * link up at end-of-transmission. No bandwidth switching - the stock MDC
 * burst runs in the channel's current bandwidth. The FSK registers are
 * left as the stock roger leaves them; the RX path re-initialises them via
 * BK4819_ResetFSK() on return to receive.
 *
 * Gated by N7SIX_APEX (ApeX edition only).
 */

#include "driver/mdc.h"

#ifdef N7SIX_APEX

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "misc.h"
#include "settings.h"

#include "driver/system.h"


/* ============================================================================
 * MDC-1200 frame layout (reference App/mdc1200.h v7.6.10A)
 * ==========================================================================*/

#define MDC_PREAMBLE_BYTE   0x55u
#define MDC_PREAMBLE_STD    7u     /* standard 7-byte run-in                  */
#define MDC_PREAMBLE_LONG   27u    /* MDC-1200L: 20 B pretime + 7 B run-in    */
#define MDC_LEADER_LEN      5u     /* 07 09 2A 44 6F                          */
#define MDC_PAYLOAD_LEN     14u    /* CRC + ECC + interleave output           */

#define MDC_FRAME_STD       (MDC_PREAMBLE_STD  + MDC_LEADER_LEN + MDC_PAYLOAD_LEN)  /* 26 */
#define MDC_FRAME_LONG      (MDC_PREAMBLE_LONG + MDC_LEADER_LEN + MDC_PAYLOAD_LEN)  /* 46 */
#define MDC_LEN_2P          (MDC_FRAME_STD * 2u)   /* 52 B: two packets       */
#define MDC_LEN_EL          (MDC_FRAME_STD * 3u)   /* 78 B: three packets     */

/* Motorola MDC-1200 opcodes (reference mdc_handler.h): 0x01 = PTT ID / ANI
 * (the standard identification burst), 0x30 = Call Alert (MDC's SelCall
 * equivalent), 0x81 = Emergency ANI. This firmware has no MDC RX handler,
 * so the opcodes exist for third-party MDC decoders and acoustics. */
#define MDC_OP_PTT_ID       0x01u
#define MDC_OP_CALL_ALERT   0x30u
#define MDC_OP_EMERGENCY    0x81u

#define MDC_ARG             0x00u
#define MDC_UNIT_ID         0x0000u   /* no unit-ID setting in this firmware */

/* Shared frame buffer: built, diff-encoded and transmitted serially from
 * the TX context only. Largest variant = 78 B (EL). */
static uint8_t MDC_FrameBuf[MDC_LEN_EL];

/* ============================================================================
 * CRC-16 (poly 0x1021, reflected in/out, final XOR 0xFFFF), nibble table.
 * Ported verbatim from the reference: bit-for-bit equal to the original
 * bit-serial implementation (verified there by tests/crc_equiv_check.c),
 * 16-entry table (32 B in .bss, built once on first use - single-threaded
 * TX context makes lazy init safe).
 * ==========================================================================*/

static uint16_t mdc_crc_nibble[16u];
static bool mdc_crc_table_ready = false;

static void mdc_crc_table_init(void)
{
    unsigned int b;
    int j;
    uint16_t crc;

    if (mdc_crc_table_ready)
        return;

    for (b = 0u; b < 16u; ++b) {
        crc = (uint16_t)b;
        for (j = 0; j < 4; ++j)
            crc = (uint16_t)((crc >> 1) ^ ((crc & 1u) ? 0x8408u : 0x0000u));
        mdc_crc_nibble[b] = crc;
    }

    mdc_crc_table_ready = true;
}

static uint16_t mdc_crc16(const uint8_t *p, size_t len)
{
    uint16_t crc = 0x0000u;

    if (p == NULL && len != 0u)
        return 0u;

    mdc_crc_table_init();

    while (len-- != 0u) {
        crc = (uint16_t)((uint16_t)(crc >> 4) ^
                         mdc_crc_nibble[(uint8_t)((crc ^ *p) & 0x0Fu)]);
        crc = (uint16_t)((uint16_t)(crc >> 4) ^
                         mdc_crc_nibble[(uint8_t)((crc ^ (*p >> 4)) & 0x0Fu)]);
        ++p;
    }

    return (uint16_t)(crc ^ 0xffffu);
}

/* Payload encoder: 4 data bytes -> CRC-16 -> pad -> K=7 convolutional ECC
 * (generator taps 0, 2, 5, 6) -> canonical 16x7 bit interleave, in place.
 * Source bit n lands at output bit (n % 7) * 16 + (n / 7). Ported verbatim
 * from the reference mdc1200_encode_str(). */
static void mdc_encode_str(uint8_t *data)
{
    uint8_t csr[7] = {0};
    uint8_t src[14] = {0};
    uint8_t lbits[112] = {0};
    int i;
    int j;
    int k;
    int b;
    uint16_t crc;

    if (data == NULL)
        return;

    crc = mdc_crc16(data, 4u);
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

    for (i = 0; i < 14; ++i)
        src[i] = data[i];

    for (i = 0; i < 112; ++i) {
        const unsigned int n = (unsigned int)i;
        k = (int)(((n % 7u) * 16u) + (n / 7u));
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

/* Build one raw (not yet diff-encoded) frame at `out`:
 *   0x55 x preamble_len + leader (07 09 2A 44 6F) + encoded payload.
 * Returns the frame length (26 or 46 B). Ported from the reference
 * MDC1200_BuildFrameImpl(). */
static size_t MDC_BuildFrameRaw(uint8_t *out,
                                size_t preamble_len,
                                uint8_t op,
                                uint8_t arg,
                                uint16_t unit_id)
{
    uint8_t *dp;
    size_t i;

    for (i = 0u; i < preamble_len; ++i)
        out[i] = (uint8_t)MDC_PREAMBLE_BYTE;

    out[preamble_len + 0u] = 0x07u;
    out[preamble_len + 1u] = 0x09u;
    out[preamble_len + 2u] = 0x2Au;
    out[preamble_len + 3u] = 0x44u;
    out[preamble_len + 4u] = 0x6Fu;

    dp = &out[preamble_len + MDC_LEADER_LEN];
    dp[0] = op;
    dp[1] = arg;
    dp[2] = (uint8_t)((unit_id >> 8) & 0xFFu);
    dp[3] = (uint8_t)(unit_id & 0xFFu);

    mdc_encode_str(dp);

    return preamble_len + MDC_LEADER_LEN + MDC_PAYLOAD_LEN;
}

/* Motorola XOR differential encoding (US Patents 4,457,005 / 4,517,561 /
 * 4,590,473 / 4,517,669): diff[n] = data[n] XOR data[n-1], data[-1] = 0,
 * MSB-first within each byte, applied to the whole frame as one bit stream.
 * The alternating 0x55 run-in becomes a constant tone on air. NOT the
 * BK4819 "scramble" feature (REG_31) - software only. Ported verbatim from
 * the reference MDC1200_DiffEncodeFrame(). Called once per frame with the
 * prev-bit starting at 0, matching the reference decode convention, so
 * every packet in a multi-packet stream is independently decodable. */
static void MDC_DiffEncodeFrame(uint8_t *frame, size_t frame_len)
{
    size_t i;
    uint8_t prev_bit = 0;

    if (frame == NULL || frame_len == 0u)
        return;

    for (i = 0u; i < frame_len; ++i) {
        uint8_t curr = frame[i];
        uint8_t encoded = 0;
        unsigned int bit;

        for (bit = 0u; bit < 8u; ++bit) {
            uint8_t data_bit = (uint8_t)((curr >> (7u - bit)) & 1u);
            uint8_t diff_bit = (uint8_t)(data_bit ^ prev_bit);
            encoded |= (uint8_t)(diff_bit << (7u - bit));
            prev_bit = data_bit;
        }

        frame[i] = encoded;
    }
}

/* Append one complete, individually diff-encoded packet at `out + offset`
 * and return the new offset (offset + frame length). */
static size_t MDC_AppendPacket(size_t offset,
                               size_t preamble_len,
                               uint8_t op,
                               uint8_t arg,
                               uint16_t unit_id)
{
    const size_t len = MDC_BuildFrameRaw(&MDC_FrameBuf[offset],
                                         preamble_len, op, arg, unit_id);
    MDC_DiffEncodeFrame(&MDC_FrameBuf[offset], len);
    return offset + len;
}


/* ============================================================================
 * Transmit core - a frame-length generalisation of the reference
 * BK4819_TransmitMDC1200Frame(), matching the stock BK4819_PlayRogerMDC()
 * register values word for word. The input frames in MDC_FrameBuf must
 * already be diff-encoded; the buffer is consumed in place.
 * ==========================================================================*/

static void MDC_Transmit(size_t frame_len)
{
    struct reg_value {
        BK4819_REGISTER_t reg;
        uint16_t value;
    };

    /* REG_5D = raw TX FIFO byte count (reference: 0x1A00 for 26 B,
     * 0x2E00 for 46 B), packed as an 11-bit field: low 8 bits in <15:8>,
     * high 3 in <7:5> (aircopy Reg5D layout). The reference's own notes
     * record that the bytes-1 / preamble-stripped 0x1400/0x2800 encoding
     * produced NO audible burst on real hardware - do not "correct" it. */
    const uint16_t reg5d = (uint16_t)(((frame_len & 0x00FFu) << 8) |
                                      (((frame_len >> 8) & 0x07u) << 5));

    /* Blind-delay for the on-air time: the chip prepends 11 B (6 B HW
     * preamble + 4 B sync + guard) before the FIFO bytes, all at 1200 bps,
     * plus a 35 ms guard so the last byte always completes. Proven
     * reference numbers: 26 B -> 280 ms, 46 B -> 400 ms; this formula
     * yields 281 / 415 ms. NO TX-done polling: REG_0C<0> reads "done"
     * immediately on this path and truncates the burst to a squawk. */
    const unsigned int tx_ms =
        (unsigned int)(((frame_len + 11u) * 20u) / 3u + 35u);

    unsigned int i;

    const struct reg_value MDC_Configuration[] = {
        { BK4819_REG_58, 0x37C3 },  // FSK enable, RX BW FFSK 1200/1800,
                                    // 0xAA/0x55 preamble, TX FFSK 1200/1800
        { BK4819_REG_72, 0x3065 },  // Tone-2 frequency = 1200 Hz
        { BK4819_REG_70, 0x00E0 },  // Enable Tone-2, Tone-2 gain 96
                                    // (reference value; stock BK4819 roger
                                    // uses the same 0x00E0)
        { BK4819_REG_59, 0x8068 },  // 4 byte sync length, 6 byte preamble,
                                    // clear TX FIFO
        { BK4819_REG_59, 0x0068 },  // Clear TX FIFO bit unset (clearing done)
        { BK4819_REG_5A, 0x5555 },  // Sync word 1 (matches the reference RX
                                    // sync detector so the radio can decode
                                    // its own bursts)
        { BK4819_REG_5B, 0x5555 },  // Sync word 2 - no 0xAA phase reversal
        { BK4819_REG_5C, 0xAA30 },  // FIFO/sync config, HW CRC disabled
                                    // (MDC frames carry no HW CRC bytes)
    };

    BK4819_SetAF(BK4819_AF_MUTE);

    for (i = 0; i < ARRAY_SIZE(MDC_Configuration); i++) {
        BK4819_WriteRegister(MDC_Configuration[i].reg,
                             MDC_Configuration[i].value);
    }

    /* Push the full frame(s) into the TX FIFO as big-endian byte pairs -
     * preamble included: this is the on-air-proven full-frame framing.
     * The TX FIFO holds 128 words, so the 78 B EL stream fits. */
    for (i = 0; i + 1u < frame_len; i += 2u) {
        BK4819_WriteRegister(BK4819_REG_5F,
            (uint16_t)(((uint16_t)MDC_FrameBuf[i] << 8) |
                       (uint16_t)MDC_FrameBuf[i + 1u]));
    }

    BK4819_WriteRegister(BK4819_REG_5D, reg5d);

    SYSTEM_DelayMs(20);

    // 4 sync bytes, 6 byte preamble, Enable FSK TX
    BK4819_WriteRegister(BK4819_REG_59, 0x0868);

    SYSTEM_DelayMs(tx_ms);

    // Stop FSK TX, reset Tone-2, disable FSK (same teardown as stock)
    BK4819_WriteRegister(BK4819_REG_59, 0x0068);
    BK4819_WriteRegister(BK4819_REG_70, 0x0000);
    BK4819_WriteRegister(BK4819_REG_58, 0x0000);
}


/**
 * @brief Extended run-in MDC-1200 roger burst (hardware FFSK).
 * @details ONE MDC-1200L long frame: 27-byte composite pretime (20 B
 *          extended pretime + 7 B run-in) + leader + ANI payload, a single
 *          TX-enable. On air: [6 B HW pre][4 B sync][27 B 0x55 tone]
 *          [leader + payload warble] ~= 380 ms -> ~415 ms armed. The long
 *          pretime is the classic audible MDC preamble rasp.
 */
static void MDC_PlayExtendedPreamble(const BK4819_FilterBandwidth_t Bandwidth)
{
    (void)Bandwidth;

    const size_t len = MDC_AppendPacket(0u, MDC_PREAMBLE_LONG,
                                        MDC_OP_PTT_ID, MDC_ARG, MDC_UNIT_ID);
    MDC_Transmit(len);
}

/**
 * @brief SelCall / Call Alert double-packet roger burst (hardware FFSK).
 * @details TWO standard frames - Call Alert sent twice, the way real MDC
 *          doubles every message - in ONE FIFO load and a single TX-enable
 *          (no inter-frame gap, so the squelch never closes). On air:
 *          [HW pre+sync][frame1][frame2] ~= 420 ms -> ~455 ms armed: two
 *          back-to-back MDC warbles separated by each frame's run-in tone.
 */
static void MDC_PlaySelCall2Packet(const BK4819_FilterBandwidth_t Bandwidth)
{
    (void)Bandwidth;

    size_t len = MDC_AppendPacket(0u, MDC_PREAMBLE_STD,
                                  MDC_OP_CALL_ALERT, MDC_ARG, MDC_UNIT_ID);
    len = MDC_AppendPacket(len, MDC_PREAMBLE_STD,
                           MDC_OP_CALL_ALERT, MDC_ARG, MDC_UNIT_ID);
    MDC_Transmit(len);
}

/**
 * @brief Emergency retry-sequence roger burst (hardware FFSK).
 * @details THREE standard frames - Emergency sent three times, matching
 *          the reference feature's "Emergency 3x repeated burst" - in ONE
 *          FIFO load and a single TX-enable. On air: [HW pre+sync]
 *          [frame]x3 ~= 593 ms -> ~628 ms armed: the emergency retry
 *          sequence fused into one unbroken burst.
 */
static void MDC_PlayEmergencyLoop(const BK4819_FilterBandwidth_t Bandwidth)
{
    (void)Bandwidth;

    size_t len = 0u;
    for (unsigned int rep = 0u; rep < 3u; rep++) {
        len = MDC_AppendPacket(len, MDC_PREAMBLE_STD,
                               MDC_OP_EMERGENCY, MDC_ARG, MDC_UNIT_ID);
    }
    MDC_Transmit(len);
}

/**
 * @brief Dispatch the ApeX-only MDC roger variants.
 * @details Called from the N7SIX_APEX hook in BK4819_PlayRoger(). The OFF /
 *          ROGER / stock-MDC modes are handled by the caller; the EEPROM
 *          clamp in settings.c guarantees only the three ApeX variants
 *          below can reach here, so no fallback branch is needed (any other
 *          value is unreachable by construction).
 */
void MDC_PlayApexRoger(const BK4819_FilterBandwidth_t Bandwidth)
{
    if (gEeprom.ROGER == ROGER_MODE_MDC_EP) {
        MDC_PlayExtendedPreamble(Bandwidth);
    } else if (gEeprom.ROGER == ROGER_MODE_MDC_2P) {
        MDC_PlaySelCall2Packet(Bandwidth);
    } else if (gEeprom.ROGER == ROGER_MODE_MDC_EL) {
        MDC_PlayEmergencyLoop(Bandwidth);
    }
}

#endif /* N7SIX_APEX */


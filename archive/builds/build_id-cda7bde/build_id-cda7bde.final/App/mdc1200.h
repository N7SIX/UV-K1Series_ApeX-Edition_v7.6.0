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
 * MDC-1200 Implementation: v7.6.10A
 * This module provides protocol-compliant MDC-1200 (Short) and MDC-1200L (Long)
 * frame encoding for transmission over BK4819/BK4829 RF transceivers.
 */

#ifndef MDC1200_H
#define MDC1200_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MDC1200_FRAME_LENGTH 26u
#define MDC1200_FIFO_WORD_COUNT (MDC1200_FRAME_LENGTH / 2u)

/* Error codes for MDC-1200 operations */
typedef enum {
    MDC1200_ERROR_NONE = 0,               /*!< Success */
    MDC1200_ERROR_INVALID_PARAMS = -1,   /*!< Invalid parameters (NULL pointer) */
    MDC1200_ERROR_FRAME_BUILD_FAILED = -2,   /*!< Frame encoding failed */
    MDC1200_ERROR_FIFO_WRITE_FAILED = -3,    /*!< FIFO word conversion failed */
    MDC1200_ERROR_TX_NOT_READY = -4      /*!< Transceiver not ready for TX */
} MDC1200_Error_t;

/* MDC-1200 transmission parameters (v7.6.10A) */
typedef struct {
    uint16_t unit_id;   /*!< Destination Unit ID (0x0000 = broadcast) */
    uint8_t op;         /*!< Opcode (0x00=Status, 0x01=Acknowledge, etc.) */
    uint8_t arg;        /*!< Argument (opcode-dependent) */
} MDC1200_Params_t;

/**
 * Build a raw MDC-1200 frame with specified parameters.
 * 
 * Frame structure: 7-byte preamble (0x55) + 5-byte leader + 14-byte encoded payload = 26 bytes
 *
 * @param op            - Opcode (e.g., 0x00 for status)
 * @param arg           - Argument (opcode-dependent meaning)
 * @param unit_id       - Destination Unit ID (0x0000 = broadcast)
 * @param frame         - Output buffer for 26-byte frame
 * @param frame_size    - Size of output buffer (must be >= 26)
 * @param frame_len_out - Output: actual frame length (always 26 on success)
 *
 * @return MDC1200_ERROR_NONE (0) on success, negative MDC1200_Error_t code on failure
 *
 * Protocol Compliance:
 * - CRC-16 with polynomial 0x1021, XOR finalization 0xFFFF
 * - 7-bit convolutional LFSR ECC encoding
 * - Bit interleaving across 16-column layout
 * - Reference: fsync-mdc1200-decode, mdc-encode-decode GitHub repositories
 */
MDC1200_Error_t MDC1200_BuildFrame(uint8_t op,
                                    uint8_t arg,
                                    uint16_t unit_id,
                                    uint8_t *frame,
                                    size_t frame_size,
                                    size_t *frame_len_out);

/**
 * Convert MDC-1200 frame bytes to 16-bit FIFO words for BK4819/BK4829 transmission.
 *
 * @param frame               - 26-byte MDC frame (from MDC1200_BuildFrame)
 * @param frame_len           - Frame length (must be 26)
 * @param fifo_words          - Output buffer for 16-bit FIFO words (13 words for 26 bytes)
 * @param fifo_word_capacity  - Capacity of fifo_words buffer
 * @param fifo_word_count_out - Output: actual word count (13 for standard frame)
 *
 * @return MDC1200_ERROR_NONE (0) on success, negative MDC1200_Error_t code on failure
 *
 * FIFO Format: Each 16-bit word contains two bytes in big-endian order
 * Example: frame[0]=0x55, frame[1]=0x55 → fifo_words[0]=0x5555
 */
MDC1200_Error_t MDC1200_BuildFifoWords(const uint8_t *frame,
                                       size_t frame_len,
                                       uint16_t *fifo_words,
                                       size_t fifo_word_capacity,
                                       size_t *fifo_word_count_out);

/**
 * Decode a raw MDC-1200 frame back into its logical fields.
 *
 * This is a reference-side validation helper: it reverses the payload bit
 * interleaving and ECC encoding, then verifies the embedded CRC.
 *
 * @param frame         - Raw 26-byte MDC frame including 7-byte preamble.
 * @param frame_len     - Frame length (must be 26)
 * @param op_out        - Output: opcode field
 * @param arg_out       - Output: argument field
 * @param unit_id_out   - Output: decoded unit ID
 * @param valid_out     - Output: true when CRC and frame structure validate
 *
 * @return MDC1200_ERROR_NONE on success, otherwise negative MDC1200_Error_t.
 */
MDC1200_Error_t MDC1200_DecodeFrame(const uint8_t *frame,
                                   size_t frame_len,
                                   uint8_t *op_out,
                                   uint8_t *arg_out,
                                   uint16_t *unit_id_out,
                                   bool *valid_out);

/**
 * Decode a raw MDC-1200 transmission represented as 16-bit FIFO words.
 *
 * This is the companion helper to MDC1200_BuildFifoWords(), and is intended
 * for validation paths that operate directly on RX FIFO data instead of a raw
 * byte buffer.
 *
 * @param fifo_words       - Buffer of 13 FIFO words (26 bytes total)
 * @param fifo_word_count  - Number of words in fifo_words
 * @param op_out           - Output: opcode field
 * @param arg_out          - Output: argument field
 * @param unit_id_out      - Output: decoded unit ID
 * @param valid_out        - Output: true when the frame validates
 */
MDC1200_Error_t MDC1200_DecodeFrameWords(const uint16_t *fifo_words,
                                        size_t fifo_word_count,
                                        uint8_t *op_out,
                                        uint8_t *arg_out,
                                        uint16_t *unit_id_out,
                                        bool *valid_out);

/**
 * Validate that a raw MDC-1200 frame has a correct CRC and recoverable payload.
 *
 * @param frame     - Raw 26-byte MDC frame
 * @param frame_len - Frame length (must be 26)
 * @param valid_out - Output: true when CRC matches and fields are consistent
 */
MDC1200_Error_t MDC1200_VerifyCRC(const uint8_t *frame,
                                 size_t frame_len,
                                 bool *valid_out);

/**
 * Transmit a single MDC-1200 frame with caller-specified parameters.
 *
 * This is the primary public API for MDC-1200 transmission. It combines
 * frame building and RF transmission in one call.
 *
 * @param params   - Pointer to MDC1200_Params_t containing unit_id, op, arg
 *
 * @return MDC1200_ERROR_NONE (0) on success, negative MDC1200_Error_t code on failure:
 *         - MDC1200_ERROR_INVALID_PARAMS: params is NULL
 *         - MDC1200_ERROR_FRAME_BUILD_FAILED: Frame encoding error
 *         - MDC1200_ERROR_FIFO_WRITE_FAILED: FIFO conversion error
 *         - MDC1200_ERROR_TX_NOT_READY: Transceiver not ready (optional validation)
 *
 * A single MDC-1200 burst is the supported protocol mode. Legacy long-mode
 * settings are treated as standard MDC-1200 for compatibility and are not
 * exposed in the user menu.
 *
 * v7.6.10A: Public API for parameterized transmission
 */
MDC1200_Error_t MDC1200_Transmit(const MDC1200_Params_t *params);

#ifdef __cplusplus
}
#endif

#endif /* MDC1200_H */
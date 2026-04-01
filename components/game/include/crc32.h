/**
 * crc32.h — FiestaQuest CRC32 hash calculator.
 *
 * Implements IEEE 802.3 CRC-32 using the reflected (LSB-first) polynomial
 * 0xEDB88320. Uses a pre-computed 256-entry static lookup table for speed.
 *
 * This module is used to:
 *   1. Verify combat log parity across BLE-connected devices.
 *   2. Validate save file integrity.
 *   3. Pin the PRNG output stream in determinism tests.
 *
 * Constitution Priority 0: No floating point. Pure function — no global
 * mutable state. Result is purely a function of (data, len).
 *
 * Check vector: fq_crc32("123456789", 9) == 0xCBF43926
 */

#ifndef FIESTAQUEST_CRC32_H
#define FIESTAQUEST_CRC32_H

#include <stdint.h>
#include <stddef.h>

/**
 * fq_crc32() — Compute the CRC32 of a byte buffer.
 *
 * Algorithm: init=0xFFFFFFFF, per-byte XOR through the 256-entry lookup
 * table, finalize by XOR with 0xFFFFFFFF.
 *
 * @param data  Pointer to the data buffer. If NULL, returns 0 immediately
 *              (no dereference).
 * @param len   Number of bytes to hash. If 0, returns 0x00000000.
 * @return      CRC32 of the input data, or 0 for NULL/empty inputs.
 */
uint32_t fq_crc32(const uint8_t *data, size_t len);

#endif /* FIESTAQUEST_CRC32_H */

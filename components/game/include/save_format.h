/**
 * save_format.h — FiestaQuest LittleFS Save File Serializer / Deserializer
 *
 * Provides field-by-field binary serialization of fq_character_t and
 * fq_inventory_t to/from a flat byte buffer suitable for LittleFS storage.
 *
 * Wire format:
 *   [0]      version   : uint8_t  — FQ_SAVE_VERSION_CURRENT
 *   [1..N-5] payload   : serialized character + inventory (little-endian)
 *   [N-4..N-1] crc32   : uint32_t, little-endian — CRC32 over bytes [0..N-5]
 *
 * The CRC covers the version byte AND all payload bytes. It does NOT cover
 * itself (the last 4 bytes). This is enforced in the implementation and
 * verified by the CRC range tests.
 *
 * Endianness: all multi-byte fields are encoded little-endian using explicit
 * bit shifts. Struct-casting to byte arrays is FORBIDDEN (endian-unsafe).
 *
 * Constitution Priority 0: No floating point. Pure functions — no global
 * mutable state.
 *
 * Architecture constraint: this module belongs to components/game/ and MUST
 * NOT depend on hal/ or presentation/ headers.
 */

#ifndef FIESTAQUEST_SAVE_FORMAT_H
#define FIESTAQUEST_SAVE_FORMAT_H

#include <stdint.h>
#include <stddef.h>

#include "types.h"

/* ---------------------------------------------------------------------------
 * Constants
 * ---------------------------------------------------------------------------*/

/** Current save file schema version. Increment when the wire format changes. */
#define FQ_SAVE_VERSION_CURRENT  1u

/**
 * Maximum buffer size for a serialized save record.
 *
 * Estimated layout:
 *   1   version byte
 *   156 fq_character_t fields (field-by-field, no struct padding in wire format)
 *   66  fq_inventory_t fields
 *   4   CRC32
 * Total estimated: ~227 bytes. 512 provides a comfortable margin for future
 * schema additions without a version bump.
 */
#define FQ_SAVE_MAX_SIZE         512u

/**
 * Exact byte length of a version-1 serialized save record.
 * Layout: 1 (version) + 146 (character fields) + 65 (inventory fields)
 *         + 4 (CRC32) = 216 bytes.
 * Tests pin this constant to catch accidental wire-format changes.
 */
#define FQ_SAVE_SERIALIZED_SIZE_V1  216u

/* ---------------------------------------------------------------------------
 * Error codes
 * ---------------------------------------------------------------------------*/
typedef enum {
    FQ_SAVE_OK                  = 0, /**< Success. */
    FQ_SAVE_ERR_NULL_PTR        = 1, /**< A required pointer argument was NULL. */
    FQ_SAVE_ERR_BUFFER_TOO_SMALL = 2, /**< Buffer too small to hold the payload. */
    FQ_SAVE_ERR_CRC             = 3, /**< CRC32 mismatch — data is corrupted. */
    FQ_SAVE_ERR_VERSION_TOO_NEW = 4, /**< Save version newer than this firmware. */
    FQ_SAVE_ERR_CORRUPT         = 5  /**< Valid CRC but invalid field values. */
} fq_save_err_t;

/* ---------------------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------------------*/

/**
 * fq_save_serialize() — Serialize a character and inventory into a byte buffer.
 *
 * Encodes all fields of @p ch and @p inv into @p buf using little-endian
 * bit shifts. Prepends the save version byte. Appends a CRC32 of all
 * preceding bytes (version + payload).
 *
 * On any failure (NULL pointer, buffer too small) this function returns 0
 * and leaves @p buf completely unmodified. There are no partial writes.
 *
 * @param ch        Pointer to the character to serialize. Must not be NULL.
 * @param inv       Pointer to the inventory to serialize. Must not be NULL.
 * @param buf       Output buffer. Must not be NULL.
 * @param buf_size  Size of @p buf in bytes.
 * @return          Number of bytes written on success; 0 on failure.
 */
size_t fq_save_serialize(const fq_character_t *ch, const fq_inventory_t *inv,
                         uint8_t *buf, size_t buf_size);

/**
 * fq_save_deserialize() — Deserialize a byte buffer into a character and inventory.
 *
 * Checks the version byte first. If the version is greater than
 * FQ_SAVE_VERSION_CURRENT, returns FQ_SAVE_ERR_VERSION_TOO_NEW immediately
 * (before checking the CRC). If the version is 0 or unknown, returns
 * FQ_SAVE_ERR_CORRUPT.
 *
 * After version check: verifies the CRC32 over bytes [0..N-5]. Returns
 * FQ_SAVE_ERR_CRC on mismatch.
 *
 * After CRC check: validates field ranges (enum values, count bounds). Returns
 * FQ_SAVE_ERR_CORRUPT on any out-of-range value.
 *
 * On success, writes decoded data to @p ch and @p inv and returns FQ_SAVE_OK.
 * Always null-terminates ch->name[11] on success.
 *
 * @param buf       Input buffer. Must not be NULL.
 * @param buf_size  Exact number of valid bytes in @p buf.
 * @param ch        Output character. Must not be NULL.
 * @param inv       Output inventory. Must not be NULL.
 * @return          FQ_SAVE_OK on success, error code on failure.
 */
fq_save_err_t fq_save_deserialize(const uint8_t *buf, size_t buf_size,
                                   fq_character_t *ch, fq_inventory_t *inv);

#endif /* FIESTAQUEST_SAVE_FORMAT_H */

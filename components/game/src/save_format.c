/**
 * save_format.c — FiestaQuest LittleFS Save File Serializer / Deserializer
 *
 * All multi-byte values are encoded little-endian using explicit bit shifts.
 * Struct-casting to/from byte arrays is strictly forbidden (endian-unsafe).
 *
 * Wire format:
 *   buf[0]         : uint8_t  — FQ_SAVE_VERSION_CURRENT
 *   buf[1..N-5]    : payload  — character + inventory, field-by-field LE
 *   buf[N-4..N-1]  : uint32_t — CRC32(buf[0..N-5]), little-endian
 *
 * Serialized payload byte count (version 1):
 *   character fields (field-by-field, no struct padding on wire):
 *     id(4) + xp(4) + legacy_tree(4)                        = 12
 *     hp_max(2) + wins(2) + losses(2) + equipped[5](10)     = 16
 *     name[12](12)                                          = 12
 *     save_version(1)+class_id(1)+level(1)+strength(1)
 *       +speed(1)+precision(1)+intelligence(1)
 *       +rebirth_count(1)+legacy_points(1)+is_dead(1)
 *       +sprite_base(1)                                     = 11
 *     cosmetic_slots[4](4)                                  =  4
 *     title(1)+equipped_count(1)+wildcard_passive(1)        =  3
 *     rival_log[8] * (opp_id(4)+last_fight_ts(4)
 *                     +encounters(1)+wins(1)+is_nemesis(1)) = 88
 *   character subtotal: 12+16+12+11+4+3+88                 = 146 bytes
 *
 *   inventory fields:
 *     items[32](64) + count(1)                              =  65 bytes
 *
 *   version prefix: 1 byte
 *   CRC suffix:     4 bytes
 *   TOTAL: 1 + 146 + 65 + 4                                = 216 bytes
 *
 * Note on rival_log: the _pad field is NOT written to the wire. The format
 * is therefore independent of in-memory struct layout.
 *
 * Constitution Priority 0: No floating point. No global mutable state.
 */

#include "save_format.h"
#include "crc32.h"

#include <string.h>

/* ---------------------------------------------------------------------------
 * Internal payload constants
 *
 * Verified by running the serializer and checking pos after the last field:
 *   version(1) + char_fields(146) + inv_fields(65) + crc(4) = 216
 * ---------------------------------------------------------------------------*/
#define SAVE_CHAR_BYTES_V1  ((size_t)146u)
#define SAVE_INV_BYTES_V1   ((size_t)65u)
#define SAVE_OVERHEAD       ((size_t)(1u + 4u))  /* version + crc */
#define SAVE_TOTAL_V1       ((size_t)(SAVE_OVERHEAD + SAVE_CHAR_BYTES_V1 + SAVE_INV_BYTES_V1))

/* SAVE_TOTAL_V1 = 216 */

/* ---------------------------------------------------------------------------
 * Encoding helpers — write little-endian values into a buffer.
 * Each helper increments *pos by the number of bytes written.
 * ---------------------------------------------------------------------------*/

static void write_u8(uint8_t *buf, size_t *pos, uint8_t val)
{
    buf[(*pos)++] = val;
}

static void write_u16_le(uint8_t *buf, size_t *pos, uint16_t val)
{
    buf[(*pos)++] = (uint8_t)(val         & 0xFFu);
    buf[(*pos)++] = (uint8_t)((val >> 8u) & 0xFFu);
}

static void write_u32_le(uint8_t *buf, size_t *pos, uint32_t val)
{
    buf[(*pos)++] = (uint8_t)(val          & 0xFFu);
    buf[(*pos)++] = (uint8_t)((val >> 8u)  & 0xFFu);
    buf[(*pos)++] = (uint8_t)((val >> 16u) & 0xFFu);
    buf[(*pos)++] = (uint8_t)((val >> 24u) & 0xFFu);
}

/* ---------------------------------------------------------------------------
 * Decoding helpers — read little-endian values from a buffer.
 * ---------------------------------------------------------------------------*/

static uint8_t read_u8(const uint8_t *buf, size_t *pos)
{
    return buf[(*pos)++];
}

static uint16_t read_u16_le(const uint8_t *buf, size_t *pos)
{
    uint16_t lo = (uint16_t)buf[(*pos)++];
    uint16_t hi = (uint16_t)buf[(*pos)++];
    return (uint16_t)(lo | (uint16_t)(hi << 8u));
}

static uint32_t read_u32_le(const uint8_t *buf, size_t *pos)
{
    uint32_t b0 = (uint32_t)buf[(*pos)++];
    uint32_t b1 = (uint32_t)buf[(*pos)++];
    uint32_t b2 = (uint32_t)buf[(*pos)++];
    uint32_t b3 = (uint32_t)buf[(*pos)++];
    return b0 | (b1 << 8u) | (b2 << 16u) | (b3 << 24u);
}

/* ---------------------------------------------------------------------------
 * fq_save_serialize()
 * ---------------------------------------------------------------------------*/
size_t fq_save_serialize(const fq_character_t *ch, const fq_inventory_t *inv,
                         uint8_t *buf, size_t buf_size)
{
    int i;

    /* Guard: null pointers — return 0, buffer untouched. */
    if (ch == NULL || inv == NULL || buf == NULL) {
        return 0u;
    }

    /* Guard: buffer too small — return 0, buffer untouched. */
    if (buf_size < SAVE_TOTAL_V1) {
        return 0u;
    }

    size_t pos = 0u;

    /* --- Version byte --- */
    write_u8(buf, &pos, FQ_SAVE_VERSION_CURRENT);

    /* --- Character fields (field-by-field, little-endian) --- */

    /* 32-bit fields (12 bytes) */
    write_u32_le(buf, &pos, ch->id);
    write_u32_le(buf, &pos, ch->xp);
    write_u32_le(buf, &pos, ch->legacy_tree);

    /* 16-bit fields (16 bytes) */
    write_u16_le(buf, &pos, ch->hp_max);
    write_u16_le(buf, &pos, ch->wins);
    write_u16_le(buf, &pos, ch->losses);

    for (i = 0; i < 5; i++) {
        write_u16_le(buf, &pos, ch->equipped[i]);
    }

    /* name[12] — write all 12 bytes including null padding (12 bytes) */
    for (i = 0; i < 12; i++) {
        write_u8(buf, &pos, (uint8_t)ch->name[i]);
    }

    /* 8-bit fields (11 bytes) */
    write_u8(buf, &pos, ch->save_version);
    write_u8(buf, &pos, ch->class_id);
    write_u8(buf, &pos, ch->level);
    write_u8(buf, &pos, ch->strength);
    write_u8(buf, &pos, ch->speed);
    write_u8(buf, &pos, ch->precision);
    write_u8(buf, &pos, ch->intelligence);
    write_u8(buf, &pos, ch->rebirth_count);
    write_u8(buf, &pos, ch->legacy_points);
    write_u8(buf, &pos, ch->is_dead);
    write_u8(buf, &pos, ch->sprite_base);

    /* cosmetic_slots[4] (4 bytes) */
    for (i = 0; i < 4; i++) {
        write_u8(buf, &pos, ch->cosmetic_slots[i]);
    }

    /* title, equipped_count, wildcard_passive (3 bytes) */
    write_u8(buf, &pos, ch->title);
    write_u8(buf, &pos, ch->equipped_count);
    write_u8(buf, &pos, ch->wildcard_passive);

    /* rival_log[8] — 11 wire bytes per entry (no _pad field on wire); 88 bytes total */
    for (i = 0; i < 8; i++) {
        write_u32_le(buf, &pos, ch->rival_log[i].opponent_id);
        write_u32_le(buf, &pos, ch->rival_log[i].last_fight_ts);
        write_u8(buf, &pos, ch->rival_log[i].encounters);
        write_u8(buf, &pos, ch->rival_log[i].wins);
        write_u8(buf, &pos, ch->rival_log[i].is_nemesis);
    }

    /* pos == 1 + 146 == 147 at this point */

    /* --- Inventory fields (65 bytes) --- */
    for (i = 0; i < 32; i++) {
        write_u16_le(buf, &pos, inv->items[i]);
    }
    write_u8(buf, &pos, inv->count);

    /* pos == 147 + 65 == 212 at this point */

    /* --- CRC32 over version + payload (all bytes written so far) --- */
    uint32_t crc = fq_crc32(buf, pos);
    write_u32_le(buf, &pos, crc);

    /* pos == 212 + 4 == 216 == SAVE_TOTAL_V1 */
    return pos;
}

/* ---------------------------------------------------------------------------
 * fq_save_deserialize()
 * ---------------------------------------------------------------------------*/
fq_save_err_t fq_save_deserialize(const uint8_t *buf, size_t buf_size,
                                   fq_character_t *ch, fq_inventory_t *inv)
{
    int i;

    /* Guard: null pointers */
    if (buf == NULL || ch == NULL || inv == NULL) {
        return FQ_SAVE_ERR_NULL_PTR;
    }

    /* Guard: minimum buffer size */
    if (buf_size < SAVE_TOTAL_V1) {
        return FQ_SAVE_ERR_BUFFER_TOO_SMALL;
    }

    /* --- Step 1: Version check (BEFORE CRC — fast-reject future formats) --- */
    uint8_t version = buf[0];

    if (version == 0u) {
        /* Version 0 is invalid — never a valid save version. */
        return FQ_SAVE_ERR_CORRUPT;
    }

    if (version > FQ_SAVE_VERSION_CURRENT) {
        /* This firmware is too old to read this save file. */
        return FQ_SAVE_ERR_VERSION_TOO_NEW;
    }

    /* --- Step 2: CRC verification ---
     *
     * CRC covers buf[0..buf_size-5] (version + payload bytes).
     * The last 4 bytes of buf are the stored CRC and are NOT included.
     */
    size_t   crc_payload_len = buf_size - 4u;
    size_t   crc_read_pos    = buf_size - 4u;
    uint32_t stored_crc      = read_u32_le(buf, &crc_read_pos);
    uint32_t computed_crc    = fq_crc32(buf, crc_payload_len);

    if (computed_crc != stored_crc) {
        return FQ_SAVE_ERR_CRC;
    }

    /* --- Step 3: Deserialize fields --- */
    size_t pos = 1u; /* skip version byte (already validated above) */

    /* 32-bit fields */
    ch->id          = read_u32_le(buf, &pos);
    ch->xp          = read_u32_le(buf, &pos);
    ch->legacy_tree = read_u32_le(buf, &pos);

    /* 16-bit fields */
    ch->hp_max  = read_u16_le(buf, &pos);
    ch->wins    = read_u16_le(buf, &pos);
    ch->losses  = read_u16_le(buf, &pos);

    for (i = 0; i < 5; i++) {
        ch->equipped[i] = read_u16_le(buf, &pos);
    }

    /* name[12] */
    for (i = 0; i < 12; i++) {
        ch->name[i] = (char)read_u8(buf, &pos);
    }
    /* Always enforce null-termination at name[11] regardless of wire content */
    ch->name[11] = '\0';

    /* 8-bit fields */
    ch->save_version  = read_u8(buf, &pos);
    ch->class_id      = read_u8(buf, &pos);
    ch->level         = read_u8(buf, &pos);
    ch->strength      = read_u8(buf, &pos);
    ch->speed         = read_u8(buf, &pos);
    ch->precision     = read_u8(buf, &pos);
    ch->intelligence  = read_u8(buf, &pos);
    ch->rebirth_count = read_u8(buf, &pos);
    ch->legacy_points = read_u8(buf, &pos);
    ch->is_dead       = read_u8(buf, &pos);
    ch->sprite_base   = read_u8(buf, &pos);

    for (i = 0; i < 4; i++) {
        ch->cosmetic_slots[i] = read_u8(buf, &pos);
    }

    ch->title            = read_u8(buf, &pos);
    ch->equipped_count   = read_u8(buf, &pos);
    ch->wildcard_passive = read_u8(buf, &pos);

    /* rival_log[8] */
    for (i = 0; i < 8; i++) {
        ch->rival_log[i].opponent_id   = read_u32_le(buf, &pos);
        ch->rival_log[i].last_fight_ts = read_u32_le(buf, &pos);
        ch->rival_log[i].encounters    = read_u8(buf, &pos);
        ch->rival_log[i].wins          = read_u8(buf, &pos);
        ch->rival_log[i].is_nemesis    = read_u8(buf, &pos);
        ch->rival_log[i]._pad          = 0u; /* always zero in-memory */
    }

    /* Inventory */
    for (i = 0; i < 32; i++) {
        inv->items[i] = read_u16_le(buf, &pos);
    }
    inv->count = read_u8(buf, &pos);
    inv->_pad  = 0u;

    /* --- Step 4: Semantic field validation (after CRC passes) --- */

    if (ch->class_id >= (uint8_t)FQ_CLASS_COUNT) {
        return FQ_SAVE_ERR_CORRUPT;
    }

    if (ch->equipped_count > 5u) {
        return FQ_SAVE_ERR_CORRUPT;
    }

    if (inv->count > 32u) {
        return FQ_SAVE_ERR_CORRUPT;
    }

    /* Zero in-memory explicit padding fields */
    ch->_pad[0] = 0u;
    ch->_pad[1] = 0u;

    return FQ_SAVE_OK;
}

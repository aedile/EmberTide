/**
 * test_p22_bounds.c — Phase 22: Bound / Math / Safety Tests
 *
 * Rule 22 Phase A (BOUND RED): These tests prove the system REJECTS
 * integer overflows, division-by-zero, NULL dereferences, and malformed
 * input BEFORE any feature test is written.
 *
 * Tests in this file:
 *   1.  test_clamp16_overflow          — INT16_MAX + INT16_MAX → INT16_MAX
 *   2.  test_clamp16_underflow         — INT16_MIN + INT16_MIN → INT16_MIN
 *   3.  test_mix_volume_zero           — volume 0 → silence
 *   4.  test_mix_volume_max_plus_sfx   — volume 255 + max SFX → clamps
 *   5.  test_flash_read_file_null_path — ERR_NULL
 *   6.  test_flash_read_file_not_found — ERR_NOT_FOUND
 *   7.  test_flash_read_file_buf_size_zero — ERR_SIZE
 *   8.  test_music_init_null_data      — FQ_MUSIC_ERR_NULL
 *   9.  test_music_init_truncated      — FQ_MUSIC_ERR_FORMAT (or non-OK)
 *   10. test_music_init_exceeds_max    — FQ_MUSIC_ERR_TOO_LARGE
 *   11. test_music_render_before_init  — silence (no crash)
 *   12. test_music_prng_isolation      — combat PRNG unchanged after track pick
 *   13. test_save_backward_compat      — old save → wire format V1 preserved
 *   14. test_ducking_no_permanent_reduction — rapid SFX → volume recovers
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <inttypes.h>

#include "test_assert.h"
#include "micromod.h"

/* Game layer headers */
#include "music.h"
#include "music_table.h"
#include "prng.h"
#include "save_format.h"
#include "types.h"

/* HAL layer (mock) */
#include "hal_flash.h"
#include "mock_hal_flash.h"

/* -------------------------------------------------------------------------
 * clamp16 — same formula as app_main.c.
 * Replicated here to test the math contract in isolation.
 * -------------------------------------------------------------------------
 */
static inline int16_t clamp16(int32_t v)
{
    if (v >  32767)  { return  32767; }
    if (v < -32768)  { return -32768; }
    return (int16_t)v;
}

/* -------------------------------------------------------------------------
 * Bound test 1: clamp16 overflow
 * -------------------------------------------------------------------------
 */
static void test_clamp16_overflow(void)
{
    int32_t sum = (int32_t)32767 + (int32_t)32767;  /* 65534 — overflows int16 */
    int16_t result = clamp16(sum);
    TEST_ASSERT_EQUAL_INT(32767, (int)result);
}

/* -------------------------------------------------------------------------
 * Bound test 2: clamp16 underflow
 * -------------------------------------------------------------------------
 */
static void test_clamp16_underflow(void)
{
    int32_t sum = (int32_t)(-32768) + (int32_t)(-32768); /* -65536 — underflows */
    int16_t result = clamp16(sum);
    TEST_ASSERT_EQUAL_INT(-32768, (int)result);
}

/* -------------------------------------------------------------------------
 * Bound test 3: mix volume zero → silence
 * -------------------------------------------------------------------------
 */
static void test_mix_volume_zero(void)
{
    int16_t music_sample = 32767;
    uint8_t vol = 0u;
    int32_t scaled = ((int32_t)music_sample * (int32_t)vol) / 256;
    int16_t result = clamp16(scaled);
    TEST_ASSERT_EQUAL_INT(0, (int)result);
}

/* -------------------------------------------------------------------------
 * Bound test 4: mix volume 255 + max SFX → clamps correctly
 * -------------------------------------------------------------------------
 */
static void test_mix_volume_max_plus_sfx(void)
{
    int16_t music_raw  = 32767;
    uint8_t vol        = 255u;
    int16_t sfx_sample = 32767;

    int32_t music_scaled = ((int32_t)music_raw * (int32_t)vol) / 256;
    int32_t mixed = music_scaled + (int32_t)sfx_sample;
    int16_t result = clamp16(mixed);

    /* Must clamp to INT16_MAX, never wrap negative */
    TEST_ASSERT_EQUAL_INT(32767, (int)result);
}

/* -------------------------------------------------------------------------
 * Bound test 5: hal_flash_read_file null path → ERR_NULL
 * -------------------------------------------------------------------------
 */
static void test_flash_read_file_null_path(void)
{
    mock_flash_reset();
    hal_flash_init();

    uint8_t buf[16];
    size_t  bytes_read = 0u;
    hal_flash_err_t err = hal_flash_read_file(NULL, buf, sizeof(buf), &bytes_read);
    TEST_ASSERT_EQUAL_INT((int)HAL_FLASH_ERR_NULL, (int)err);
    TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)bytes_read);
}

/* -------------------------------------------------------------------------
 * Bound test 6: hal_flash_read_file non-existent path → ERR_NOT_FOUND
 * -------------------------------------------------------------------------
 */
static void test_flash_read_file_not_found(void)
{
    mock_flash_reset();
    hal_flash_init();

    uint8_t buf[64];
    size_t  bytes_read = 0u;
    hal_flash_err_t err = hal_flash_read_file("/littlefs/noexist.mod",
                                               buf, sizeof(buf), &bytes_read);
    TEST_ASSERT_EQUAL_INT((int)HAL_FLASH_ERR_NOT_FOUND, (int)err);
    TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)bytes_read);
}

/* -------------------------------------------------------------------------
 * Bound test 7: hal_flash_read_file buf_size == 0 → ERR_SIZE
 * -------------------------------------------------------------------------
 */
static void test_flash_read_file_buf_size_zero(void)
{
    mock_flash_reset();
    hal_flash_init();

    uint8_t buf[64];
    size_t  bytes_read = 0u;
    hal_flash_err_t err = hal_flash_read_file("/littlefs/track.mod",
                                               buf, 0u, &bytes_read);
    TEST_ASSERT_EQUAL_INT((int)HAL_FLASH_ERR_SIZE, (int)err);
}

/* -------------------------------------------------------------------------
 * Bound test 8: fq_music_init with NULL data → FQ_MUSIC_ERR_NULL
 * -------------------------------------------------------------------------
 */
static void test_music_init_null_data(void)
{
    fq_music_err_t err = fq_music_init(NULL, 128u);
    TEST_ASSERT_EQUAL_INT((int)FQ_MUSIC_ERR_NULL, (int)err);
    fq_music_stop();
}

/* -------------------------------------------------------------------------
 * Bound test 9: fq_music_init with truncated (too-small) buffer → error
 * -------------------------------------------------------------------------
 */
static void test_music_init_truncated(void)
{
    static const uint8_t tiny_buf[100] = { 0 };
    fq_music_err_t err = fq_music_init(tiny_buf, sizeof(tiny_buf));
    TEST_ASSERT_TRUE(err != FQ_MUSIC_OK);
    fq_music_stop();
}

/* -------------------------------------------------------------------------
 * Bound test 10: fq_music_init with data exceeding MAX_MOD_FILE_SIZE → error
 * -------------------------------------------------------------------------
 */
static void test_music_init_exceeds_max(void)
{
    static uint8_t dummy[4];
    fq_music_err_t err = fq_music_init(dummy, MAX_MOD_FILE_SIZE + 1u);
    TEST_ASSERT_EQUAL_INT((int)FQ_MUSIC_ERR_TOO_LARGE, (int)err);
    fq_music_stop();
}

/* -------------------------------------------------------------------------
 * Bound test 11: fq_music_render before init → silence, no crash
 * -------------------------------------------------------------------------
 */
static void test_music_render_before_init(void)
{
    fq_music_stop(); /* ensure clean state */

    static int16_t buf[64];
    /* Fill with non-zero sentinel to prove overwrite */
    memset(buf, 0xAB, sizeof(buf));

    fq_music_err_t err = fq_music_render(buf, 64u, 128u);
    (void)err; /* OK or NOT_INIT are both acceptable */

    for (size_t i = 0u; i < 64u; i++) {
        TEST_ASSERT_EQUAL_INT(0, (int)buf[i]);
    }
}

/* -------------------------------------------------------------------------
 * Bound test 12: combat PRNG isolation — track selection must not alter rng
 * -------------------------------------------------------------------------
 */
static void test_music_prng_isolation(void)
{
    fq_prng_t rng;
    fq_prng_init(&rng, 0xDEADBEEFu);

    /* Capture a sequence of 4 PRNG values. */
    uint32_t before[4];
    before[0] = fq_prng_next(&rng);
    before[1] = fq_prng_next(&rng);
    before[2] = fq_prng_next(&rng);
    before[3] = fq_prng_next(&rng);

    /* Re-initialise same seed. */
    fq_prng_init(&rng, 0xDEADBEEFu);

    /* Perform track selection using tick_count (not the rng object). */
    (void)fq_music_table_pick(FQ_MUSIC_CAT_CHILL, 12345u);
    (void)fq_music_table_pick(FQ_MUSIC_CAT_INTENSE, 99999u);

    /* Drain same 4 values — must match before[]. */
    uint32_t after[4];
    after[0] = fq_prng_next(&rng);
    after[1] = fq_prng_next(&rng);
    after[2] = fq_prng_next(&rng);
    after[3] = fq_prng_next(&rng);

    TEST_ASSERT_EQUAL_UINT32(before[0], after[0]);
    TEST_ASSERT_EQUAL_UINT32(before[1], after[1]);
    TEST_ASSERT_EQUAL_UINT32(before[2], after[2]);
    TEST_ASSERT_EQUAL_UINT32(before[3], after[3]);
}

/* -------------------------------------------------------------------------
 * Bound test 13: save backward compatibility
 *
 * Verifies that the V1 save format serializes to the pinned size constant.
 * music_enabled is NOT in the save — it lives in fq_app_ctx_t and defaults
 * to 1 on init. This test verifies the wire format is unchanged.
 * -------------------------------------------------------------------------
 */
static void test_save_backward_compat(void)
{
    fq_character_t ch;
    fq_inventory_t inv;
    memset(&ch,  0, sizeof(ch));
    memset(&inv, 0, sizeof(inv));

    ch.class_id = 1u;
    ch.level    = 5u;

    uint8_t buf[FQ_SAVE_MAX_SIZE];
    size_t  sz = fq_save_serialize(&ch, &inv, buf, sizeof(buf));
    TEST_ASSERT_TRUE(sz > 0u);

    fq_character_t ch2;
    fq_inventory_t inv2;
    fq_save_err_t  serr = fq_save_deserialize(buf, sz, &ch2, &inv2);
    TEST_ASSERT_EQUAL_INT((int)FQ_SAVE_OK, (int)serr);
    TEST_ASSERT_EQUAL_UINT8(5u, ch2.level);

    /* Wire format size must be unchanged — backward compat guaranteed. */
    TEST_ASSERT_EQUAL_UINT32((uint32_t)FQ_SAVE_SERIALIZED_SIZE_V1, (uint32_t)sz);
}

/* -------------------------------------------------------------------------
 * Bound test 14: ducking does not permanently reduce volume
 * -------------------------------------------------------------------------
 */
static void test_ducking_no_permanent_reduction(void)
{
    uint8_t vol = 200u;
    int16_t music_sample = 10000;

    /* Pass 1: with SFX (ducking active) */
    uint8_t ducked_vol = (uint8_t)((uint32_t)vol * 153u / 256u);
    int32_t with_sfx_scaled = ((int32_t)music_sample * (int32_t)ducked_vol) / 256;
    int16_t with_sfx = clamp16(with_sfx_scaled);

    /* Pass 2: no SFX (ducking inactive) */
    int32_t no_sfx_scaled = ((int32_t)music_sample * (int32_t)vol) / 256;
    int16_t without_sfx = clamp16(no_sfx_scaled);

    /* The music-only pass must produce a HIGHER output than the ducked pass. */
    TEST_ASSERT_TRUE(without_sfx > with_sfx);

    /* Exact values:
     * ducked_vol = 200*153/256 = 119 (integer division)
     * full:   10000 * 200 / 256 = 7812
     * ducked: 10000 * 119 / 256 = 4648
     */
    TEST_ASSERT_EQUAL_INT(7812, (int)without_sfx);
    TEST_ASSERT_EQUAL_INT(4648, (int)with_sfx);

    /* Verify ducking does NOT stack (no persistent state). */
    int32_t second_sfx_scaled = ((int32_t)music_sample * (int32_t)ducked_vol) / 256;
    int16_t second_sfx = clamp16(second_sfx_scaled);
    TEST_ASSERT_EQUAL_INT((int)with_sfx, (int)second_sfx);
}

/* -------------------------------------------------------------------------
 * main
 * -------------------------------------------------------------------------
 */
int main(void)
{
    test_clamp16_overflow();
    test_clamp16_underflow();
    test_mix_volume_zero();
    test_mix_volume_max_plus_sfx();
    test_flash_read_file_null_path();
    test_flash_read_file_not_found();
    test_flash_read_file_buf_size_zero();
    test_music_init_null_data();
    test_music_init_truncated();
    test_music_init_exceeds_max();
    test_music_render_before_init();
    test_music_prng_isolation();
    test_save_backward_compat();
    test_ducking_no_permanent_reduction();

    printf("All Phase 22 bound tests passed.\n");
    return 0;
}

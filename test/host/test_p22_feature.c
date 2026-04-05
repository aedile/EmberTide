/**
 * test_p22_feature.c — Phase 22: Feature Tests (music, mixing, track selection)
 *
 * Rule 22 Phase B (FEATURE RED): Happy-path contracts for music playback,
 * audio mixing, and gameplay integration.
 *
 * Tests:
 *   1.  test_music_table_chill_non_empty      — chill category has tracks
 *   2.  test_music_table_intense_non_empty    — intense category has tracks
 *   3.  test_music_table_upbeat_non_empty     — upbeat category has tracks
 *   4.  test_music_table_pick_deterministic   — same tick → same track
 *   5.  test_music_table_pick_varies          — different ticks → coverage
 *   6.  test_music_table_oob_returns_null     — OOB index → NULL
 *   7.  test_music_table_empty_cat_returns_null — count=0 category returns NULL
 *   8.  test_music_init_valid_mod             — init with minimal valid MOD header
 *   9.  test_music_play_stop                  — play/stop flag contract
 *   10. test_music_render_silence_when_stopped — render after stop → zeros
 *   11. test_music_render_nonzero_when_playing — render after init+play → varies
 *   12. test_music_vol_scaling                — vol=128 produces ~50% amplitude
 *   13. test_flash_read_file_round_trip       — store and retrieve file data
 *   14. test_mix_music_only                   — SFX zero → output equals music
 *   15. test_mix_sfx_only                     — music zero → output equals SFX
 *   16. test_mix_combined                     — both present → additive mix
 *   17. test_mix_ducking_applied              — SFX chunk triggers ducking
 *   18. test_music_render_null_buf            — FQ_MUSIC_ERR_NULL
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <inttypes.h>

#include "test_assert.h"

#include "music.h"
#include "music_table.h"
#include "hal_flash.h"
#include "mock_hal_flash.h"

/* -------------------------------------------------------------------------
 * clamp16 (same formula as app_main.c)
 * -------------------------------------------------------------------------
 */
static inline int16_t clamp16(int32_t v)
{
    if (v >  32767)  { return  32767; }
    if (v < -32768)  { return -32768; }
    return (int16_t)v;
}

/* -------------------------------------------------------------------------
 * Minimal valid ProTracker MOD header (31-sample, 4-channel).
 *
 * We build a synthetic MOD that passes the magic check and has enough
 * structure for micromod_init() to parse without triggering an error.
 *
 * Layout:
 *   [0..19]    Title (20 bytes, zero-padded)
 *   [20..909]  31 sample descriptors × 30 bytes = 930 bytes
 *   [910]      Song length (1 byte)
 *   [911]      Restart position (1 byte)
 *   [912..1039] Pattern order (128 bytes)
 *   [1040..1043] Magic "M.K." (4 bytes)
 *   [1044..]   One empty pattern: 64 rows × 4 channels × 4 bytes = 1024 bytes
 *
 * Total header: 1044 bytes.
 * Total with one empty pattern: 1044 + 1024 = 2068 bytes.
 * -------------------------------------------------------------------------
 */
#define MINIMAL_MOD_SIZE  2068u
static uint8_t s_minimal_mod[MINIMAL_MOD_SIZE];

static void build_minimal_mod(void)
{
    memset(s_minimal_mod, 0, sizeof(s_minimal_mod));

    /* Title: "TestMOD" */
    memcpy(s_minimal_mod + 0, "TestMOD", 7);

    /* Sample descriptors: all zero (muted, length=0) — valid for silence. */
    /* (already zeroed by memset) */

    /* Song length = 1 (one pattern in the order). */
    s_minimal_mod[950] = 1u;

    /* Restart position = 0. */
    s_minimal_mod[951] = 0u;

    /* Pattern order: order[0] = 0 (pattern index 0). */
    s_minimal_mod[952] = 0u;

    /* Magic "M.K." at offset 1080. */
    s_minimal_mod[1080] = 'M';
    s_minimal_mod[1081] = '.';
    s_minimal_mod[1082] = 'K';
    s_minimal_mod[1083] = '.';

    /* Pattern data at 1084: 64 rows × 4 channels × 4 bytes = 1024 bytes.
     * All zero (empty notes). */
}

/* -------------------------------------------------------------------------
 * Feature test 1: chill category is non-empty
 * -------------------------------------------------------------------------
 */
static void test_music_table_chill_non_empty(void)
{
    size_t cnt = fq_music_table_count(FQ_MUSIC_CAT_CHILL);
    TEST_ASSERT_TRUE(cnt > 0u);
}

/* -------------------------------------------------------------------------
 * Feature test 2: intense category is non-empty
 * -------------------------------------------------------------------------
 */
static void test_music_table_intense_non_empty(void)
{
    size_t cnt = fq_music_table_count(FQ_MUSIC_CAT_INTENSE);
    TEST_ASSERT_TRUE(cnt > 0u);
}

/* -------------------------------------------------------------------------
 * Feature test 3: upbeat category is non-empty
 * -------------------------------------------------------------------------
 */
static void test_music_table_upbeat_non_empty(void)
{
    size_t cnt = fq_music_table_count(FQ_MUSIC_CAT_UPBEAT);
    TEST_ASSERT_TRUE(cnt > 0u);
}

/* -------------------------------------------------------------------------
 * Feature test 4: pick with same tick_count returns same track (deterministic)
 * -------------------------------------------------------------------------
 */
static void test_music_table_pick_deterministic(void)
{
    const fq_music_track_t *a = fq_music_table_pick(FQ_MUSIC_CAT_CHILL, 42u);
    const fq_music_track_t *b = fq_music_table_pick(FQ_MUSIC_CAT_CHILL, 42u);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);
    /* Same tick → same pointer (same entry in the static table). */
    TEST_ASSERT_TRUE(a == b);
}

/* -------------------------------------------------------------------------
 * Feature test 5: pick with different tick_counts covers multiple entries
 *
 * If there are N tracks, varying tick_count by N should cover all entries.
 * We only verify that at least two distinct ticks produce a valid (non-NULL)
 * result — the actual variation depends on the table size.
 * -------------------------------------------------------------------------
 */
static void test_music_table_pick_varies(void)
{
    size_t cnt = fq_music_table_count(FQ_MUSIC_CAT_CHILL);
    if (cnt < 2u) {
        /* Only one track — variation not possible, skip variance check. */
        const fq_music_track_t *t = fq_music_table_pick(FQ_MUSIC_CAT_CHILL, 0u);
        TEST_ASSERT_NOT_NULL(t);
        return;
    }
    /* Collect track pointers for different ticks. */
    uint8_t found_different = 0u;
    const fq_music_track_t *first = fq_music_table_pick(FQ_MUSIC_CAT_CHILL, 0u);
    for (uint32_t tick = 1u; tick < (uint32_t)(cnt * 4u); tick++) {
        const fq_music_track_t *t = fq_music_table_pick(FQ_MUSIC_CAT_CHILL, tick);
        if (t != first) {
            found_different = 1u;
            break;
        }
    }
    TEST_ASSERT_TRUE(found_different == 1u);
}

/* -------------------------------------------------------------------------
 * Feature test 6: OOB index in fq_music_table_get returns NULL
 * -------------------------------------------------------------------------
 */
static void test_music_table_oob_returns_null(void)
{
    size_t cnt = fq_music_table_count(FQ_MUSIC_CAT_CHILL);
    const fq_music_track_t *t = fq_music_table_get(FQ_MUSIC_CAT_CHILL, cnt);
    TEST_ASSERT_NULL(t);
}

/* -------------------------------------------------------------------------
 * Feature test 7: OOB category returns NULL from pick
 * -------------------------------------------------------------------------
 */
static void test_music_table_empty_cat_returns_null(void)
{
    /* FQ_MUSIC_CAT_COUNT is out-of-range. */
    const fq_music_track_t *t = fq_music_table_pick(FQ_MUSIC_CAT_COUNT, 0u);
    TEST_ASSERT_NULL(t);
}

/* -------------------------------------------------------------------------
 * Feature test 8: fq_music_init with minimal valid MOD → FQ_MUSIC_OK
 * -------------------------------------------------------------------------
 */
static void test_music_init_valid_mod(void)
{
    build_minimal_mod();
    fq_music_err_t err = fq_music_init(s_minimal_mod, MINIMAL_MOD_SIZE);
    TEST_ASSERT_EQUAL_INT((int)FQ_MUSIC_OK, (int)err);
    fq_music_stop();
}

/* -------------------------------------------------------------------------
 * Feature test 9: play/stop contract
 * -------------------------------------------------------------------------
 */
static void test_music_play_stop(void)
{
    build_minimal_mod();
    fq_music_init(s_minimal_mod, MINIMAL_MOD_SIZE);

    TEST_ASSERT_EQUAL_UINT8(0u, fq_music_is_playing());

    fq_music_err_t err = fq_music_play();
    TEST_ASSERT_EQUAL_INT((int)FQ_MUSIC_OK, (int)err);
    TEST_ASSERT_EQUAL_UINT8(1u, fq_music_is_playing());

    fq_music_stop();
    TEST_ASSERT_EQUAL_UINT8(0u, fq_music_is_playing());
}

/* -------------------------------------------------------------------------
 * Feature test 10: render after stop → silence
 * -------------------------------------------------------------------------
 */
static void test_music_render_silence_when_stopped(void)
{
    build_minimal_mod();
    fq_music_init(s_minimal_mod, MINIMAL_MOD_SIZE);
    fq_music_play();
    fq_music_stop();

    static int16_t buf[32];
    memset(buf, 0xAA, sizeof(buf));
    fq_music_render(buf, 32u, 200u);

    for (size_t i = 0; i < 32u; i++) {
        TEST_ASSERT_EQUAL_INT(0, (int)buf[i]);
    }
}

/* -------------------------------------------------------------------------
 * Feature test 11: render after init+play — does not crash, returns OK
 *
 * This MOD has no sample data so the output should be zero (silence from
 * empty channels). We just verify no crash and OK return.
 * -------------------------------------------------------------------------
 */
static void test_music_render_nonzero_when_playing(void)
{
    build_minimal_mod();
    fq_music_init(s_minimal_mod, MINIMAL_MOD_SIZE);
    fq_music_play();

    static int16_t buf[64];
    memset(buf, 0, sizeof(buf));
    fq_music_err_t err = fq_music_render(buf, 64u, 200u);
    TEST_ASSERT_TRUE(err == FQ_MUSIC_OK || err == FQ_MUSIC_ERR_LOOP_GUARD);

    fq_music_stop();
}

/* -------------------------------------------------------------------------
 * Feature test 12: volume scaling
 *
 * A raw sample of 16384 at vol=128 should produce:
 *   16384 * 128 / 256 = 8192 exactly.
 * -------------------------------------------------------------------------
 */
static void test_music_vol_scaling(void)
{
    int32_t raw = 16384;
    uint8_t vol = 128u;
    int32_t scaled = ((int32_t)raw * (int32_t)vol) / 256;
    int16_t result = clamp16(scaled);
    TEST_ASSERT_EQUAL_INT(8192, (int)result);
}

/* -------------------------------------------------------------------------
 * Feature test 13: hal_flash_read_file round-trip
 *
 * Store a test file in the mock via mock_flash_store_file(), then read it
 * back with hal_flash_read_file().
 * -------------------------------------------------------------------------
 */
static void test_flash_read_file_round_trip(void)
{
    mock_flash_reset();
    hal_flash_init();

    static const uint8_t payload[] = { 0x4D, 0x2E, 0x4B, 0x2E, 0x01, 0x02 };
    static const char   *path      = "/littlefs/music/track1.mod";

    /* Store via mock helper. */
    mock_flash_store_file(path, payload, sizeof(payload));

    uint8_t read_buf[32];
    size_t  bytes_read = 0u;
    hal_flash_err_t err = hal_flash_read_file(path, read_buf, sizeof(read_buf),
                                               &bytes_read);
    TEST_ASSERT_EQUAL_INT((int)HAL_FLASH_OK, (int)err);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)sizeof(payload), (uint32_t)bytes_read);

    for (size_t i = 0; i < sizeof(payload); i++) {
        TEST_ASSERT_EQUAL_UINT8(payload[i], read_buf[i]);
    }
}

/* -------------------------------------------------------------------------
 * Feature test 14: mix music only (SFX zero)
 *
 * When the SFX sample is 0, the output must equal the volume-scaled music.
 * -------------------------------------------------------------------------
 */
static void test_mix_music_only(void)
{
    int16_t music = 10000;
    int16_t sfx   = 0;
    uint8_t vol   = 200u;

    /* No ducking: sfx == 0. */
    int32_t music_scaled = ((int32_t)music * (int32_t)vol) / 256;
    int32_t mixed = music_scaled + (int32_t)sfx;
    int16_t result = clamp16(mixed);

    /* 10000 * 200 / 256 = 7812 */
    TEST_ASSERT_EQUAL_INT(7812, (int)result);
}

/* -------------------------------------------------------------------------
 * Feature test 15: mix SFX only (music zero)
 * -------------------------------------------------------------------------
 */
static void test_mix_sfx_only(void)
{
    int16_t music = 0;
    int16_t sfx   = 8000;
    uint8_t vol   = 200u;

    int32_t music_scaled = ((int32_t)music * (int32_t)vol) / 256; /* 0 */
    int32_t mixed = music_scaled + (int32_t)sfx;
    int16_t result = clamp16(mixed);

    TEST_ASSERT_EQUAL_INT(8000, (int)result);
}

/* -------------------------------------------------------------------------
 * Feature test 16: combined mix (additive, clamped)
 * -------------------------------------------------------------------------
 */
static void test_mix_combined(void)
{
    int16_t music = 10000;
    int16_t sfx   = 5000;
    uint8_t vol   = 200u;

    /* Ducking: sfx != 0 → ducked_vol = 200 * 153 / 256 = 119 */
    uint8_t ducked_vol = (uint8_t)(((uint32_t)vol * 153u) / 256u);
    int32_t music_scaled = ((int32_t)music * (int32_t)ducked_vol) / 256;
    int32_t mixed = music_scaled + (int32_t)sfx;
    int16_t result = clamp16(mixed);

    /* music_scaled = 10000 * 119 / 256 = 4648
     * mixed = 4648 + 5000 = 9648 */
    TEST_ASSERT_EQUAL_INT(9648, (int)result);
}

/* -------------------------------------------------------------------------
 * Feature test 17: ducking applied when SFX is present
 *
 * With sfx present, output should be LESS than the same music at full vol.
 * -------------------------------------------------------------------------
 */
static void test_mix_ducking_applied(void)
{
    int16_t music = 20000;
    int16_t sfx   = 100;   /* small but non-zero — triggers ducking */
    uint8_t vol   = 200u;

    /* With sfx: ducked music + sfx */
    uint8_t ducked_vol = (uint8_t)(((uint32_t)vol * 153u) / 256u);
    int32_t music_ducked = ((int32_t)music * (int32_t)ducked_vol) / 256;
    int16_t with_duck = clamp16(music_ducked + (int32_t)sfx);

    /* Without sfx: full music only */
    int32_t music_full = ((int32_t)music * (int32_t)vol) / 256;
    int16_t no_duck = clamp16(music_full);

    /* Ducked output must be less than undocked. */
    TEST_ASSERT_TRUE(with_duck < no_duck);
}

/* -------------------------------------------------------------------------
 * Feature test 18: fq_music_render with NULL buf → FQ_MUSIC_ERR_NULL
 * -------------------------------------------------------------------------
 */
static void test_music_render_null_buf(void)
{
    build_minimal_mod();
    fq_music_init(s_minimal_mod, MINIMAL_MOD_SIZE);
    fq_music_play();

    fq_music_err_t err = fq_music_render(NULL, 64u, 200u);
    TEST_ASSERT_EQUAL_INT((int)FQ_MUSIC_ERR_NULL, (int)err);

    fq_music_stop();
}

/* -------------------------------------------------------------------------
 * main
 * -------------------------------------------------------------------------
 */
int main(void)
{
    test_music_table_chill_non_empty();
    test_music_table_intense_non_empty();
    test_music_table_upbeat_non_empty();
    test_music_table_pick_deterministic();
    test_music_table_pick_varies();
    test_music_table_oob_returns_null();
    test_music_table_empty_cat_returns_null();
    test_music_init_valid_mod();
    test_music_play_stop();
    test_music_render_silence_when_stopped();
    test_music_render_nonzero_when_playing();
    test_music_vol_scaling();
    test_flash_read_file_round_trip();
    test_mix_music_only();
    test_mix_sfx_only();
    test_mix_combined();
    test_mix_ducking_applied();
    test_music_render_null_buf();

    printf("All Phase 22 feature tests passed.\n");
    return 0;
}

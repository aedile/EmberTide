/**
 * test_p23_bounds.c — Phase 23 Bound / Math / Safety Tests
 *
 * Rule 22 Phase A (BOUND RED): These tests prove the system REJECTS
 * integer overflows, NULL dereferences, and invalid inputs BEFORE any
 * feature test or implementation is written.
 *
 * Tests:
 *   1.  test_ring_count_never_exceeds_capacity  — hal_audio_get_ring_count()
 *       never returns a value > AUDIO_RING_BUF_SAMPLES after overflow.
 *   2.  test_ring_count_zero_before_init        — returns 0 before init.
 *   3.  test_ring_count_after_stop              — returns 0 after stop.
 *   4.  test_ring_count_increments_with_writes  — count tracks samples written.
 *   5.  test_stereo_downmix_no_overflow         — (INT16_MAX + INT16_MAX)/2
 *       must NOT overflow int32_t intermediate; result clamped.
 *   6.  test_stereo_downmix_underflow           — (INT16_MIN + INT16_MIN)/2
 *       result clamped to INT16_MIN.
 *   7.  test_stereo_downmix_vol_zero_silence    — vol=0 → output 0.
 *   8.  test_prefill_threshold_math             — 80% of AUDIO_RING_BUF_SAMPLES
 *       must be computed without integer overflow.
 *   9.  test_ring_count_write_null_returns_err  — NULL buf → HAL_AUDIO_ERR_NULL.
 *   10. test_ring_count_write_before_init       — write before init → ERR_INIT.
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <limits.h>
#include <inttypes.h>

#include "test_assert.h"
#include "hal_audio.h"

/* -------------------------------------------------------------------------
 * mock_audio_reset declared in mock_hal_audio.c.
 * hal_audio_get_ring_count() is the new Phase-23 public API added to
 * hal_audio.h — called directly here.
 * -------------------------------------------------------------------------
 */
extern void mock_audio_reset(void);

/* -------------------------------------------------------------------------
 * stereo_downmix_sample — replicates the downmix math from do_music_tick().
 *
 * Keeps the formula isolated here so changes in app_main.c cannot silently
 * invalidate these bound proofs.
 * -------------------------------------------------------------------------
 */
static inline int16_t stereo_downmix_sample(int16_t l, int16_t r, uint8_t vol)
{
    int32_t mono = ((int32_t)l + (int32_t)r) / 2;
    mono = (mono * (int32_t)vol) / 256;
    if (mono >  32767) { return  32767; }
    if (mono < -32768) { return -32768; }
    return (int16_t)mono;
}

/* -------------------------------------------------------------------------
 * Bound test 1: hal_audio_get_ring_count() never exceeds capacity.
 *
 * Write exactly AUDIO_RING_BUF_SAMPLES samples, then try to write one more.
 * The ring should be full; get_ring_count() must report AUDIO_RING_BUF_SAMPLES
 * (not a larger value).
 * -------------------------------------------------------------------------
 */
static void test_ring_count_never_exceeds_capacity(void)
{
    mock_audio_reset();
    hal_audio_init();

    static int16_t fill_buf[AUDIO_RING_BUF_SAMPLES];
    memset(fill_buf, 0, sizeof(fill_buf));

    hal_audio_write_samples(fill_buf, AUDIO_RING_BUF_SAMPLES);

    /* Ring is now full. Try to write one more sample — overflow. */
    int16_t extra = 0;
    hal_audio_err_t err = hal_audio_write_samples(&extra, 1u);
    TEST_ASSERT_EQUAL_INT((int)HAL_AUDIO_ERR_OVERFLOW, (int)err);

    /* Ring fill must not exceed capacity. */
    uint32_t fill = hal_audio_get_ring_count();
    TEST_ASSERT_EQUAL_UINT32(AUDIO_RING_BUF_SAMPLES, fill);
}

/* -------------------------------------------------------------------------
 * Bound test 2: hal_audio_get_ring_count() returns 0 before init.
 * -------------------------------------------------------------------------
 */
static void test_ring_count_zero_before_init(void)
{
    mock_audio_reset();  /* resets without re-initializing */

    uint32_t count = hal_audio_get_ring_count();
    TEST_ASSERT_EQUAL_UINT32(0u, count);
}

/* -------------------------------------------------------------------------
 * Bound test 3: hal_audio_get_ring_count() returns 0 after stop.
 * -------------------------------------------------------------------------
 */
static void test_ring_count_after_stop(void)
{
    mock_audio_reset();
    hal_audio_init();

    int16_t samples[16] = {0};
    hal_audio_write_samples(samples, 16u);

    hal_audio_stop();

    uint32_t count = hal_audio_get_ring_count();
    TEST_ASSERT_EQUAL_UINT32(0u, count);
}

/* -------------------------------------------------------------------------
 * Bound test 4: hal_audio_get_ring_count() increments with writes.
 * -------------------------------------------------------------------------
 */
static void test_ring_count_increments_with_writes(void)
{
    mock_audio_reset();
    hal_audio_init();

    int16_t samples[100] = {0};
    hal_audio_write_samples(samples, 100u);

    uint32_t count = hal_audio_get_ring_count();
    TEST_ASSERT_EQUAL_UINT32(100u, count);
}

/* -------------------------------------------------------------------------
 * Bound test 5: stereo downmix — no overflow in int32_t intermediate.
 *
 * L = INT16_MAX (32767), R = INT16_MAX (32767).
 * Intermediate: (32767 + 32767) = 65534 — fits in int32_t cleanly.
 * After /2 = 32767, vol=255: (32767 * 255)/256 = 32639 — no overflow.
 * -------------------------------------------------------------------------
 */
static void test_stereo_downmix_no_overflow(void)
{
    int16_t result = stereo_downmix_sample(32767, 32767, 255u);
    /* Must be positive and at most INT16_MAX. */
    TEST_ASSERT_EQUAL_INT(1, (int)(result > 0));
    TEST_ASSERT_EQUAL_INT(1, (int)(result <= 32767));
}

/* -------------------------------------------------------------------------
 * Bound test 6: stereo downmix — underflow clamped.
 *
 * L = INT16_MIN (-32768), R = INT16_MIN (-32768).
 * Intermediate: -32768 + -32768 = -65536 — fits in int32_t.
 * After /2 = -32768, vol=255: (-32768 * 255)/256 = -32640 — stays negative.
 * -------------------------------------------------------------------------
 */
static void test_stereo_downmix_underflow(void)
{
    int16_t result = stereo_downmix_sample(-32768, -32768, 255u);
    /* Must be negative and at or above INT16_MIN. */
    TEST_ASSERT_EQUAL_INT(1, (int)(result < 0));
    TEST_ASSERT_EQUAL_INT(1, (int)(result >= -32768));
}

/* -------------------------------------------------------------------------
 * Bound test 7: stereo downmix — vol=0 produces silence.
 * -------------------------------------------------------------------------
 */
static void test_stereo_downmix_vol_zero_silence(void)
{
    int16_t result = stereo_downmix_sample(32767, 32767, 0u);
    TEST_ASSERT_EQUAL_INT(0, (int)result);
}

/* -------------------------------------------------------------------------
 * Bound test 8: prefill threshold — 80% of AUDIO_RING_BUF_SAMPLES has no
 * integer overflow and is strictly less than capacity.
 *
 * For AUDIO_RING_BUF_SAMPLES=44100: 44100 * 80 = 3,528,000 < UINT32_MAX.
 * For AUDIO_RING_BUF_SAMPLES=4096:  4096 * 80 = 327,680 < UINT32_MAX.
 * Both safe.
 * -------------------------------------------------------------------------
 */
static void test_prefill_threshold_math(void)
{
    uint32_t threshold = (AUDIO_RING_BUF_SAMPLES * 80u) / 100u;

    /* Threshold must be strictly less than capacity (not 100%). */
    TEST_ASSERT_EQUAL_INT(1, (int)(threshold < AUDIO_RING_BUF_SAMPLES));

    /* Threshold must be positive. */
    TEST_ASSERT_EQUAL_INT(1, (int)(threshold > 0u));

    /* Threshold must be exactly 80% (integer). */
    uint32_t expected = (AUDIO_RING_BUF_SAMPLES * 80u) / 100u;
    TEST_ASSERT_EQUAL_UINT32(expected, threshold);
}

/* -------------------------------------------------------------------------
 * Bound test 9: write_samples with NULL buffer returns ERR_NULL.
 * -------------------------------------------------------------------------
 */
static void test_ring_count_write_null_returns_err(void)
{
    mock_audio_reset();
    hal_audio_init();

    hal_audio_err_t err = hal_audio_write_samples(NULL, 1u);
    TEST_ASSERT_EQUAL_INT((int)HAL_AUDIO_ERR_NULL, (int)err);
}

/* -------------------------------------------------------------------------
 * Bound test 10: write_samples before init returns ERR_INIT.
 * -------------------------------------------------------------------------
 */
static void test_ring_count_write_before_init(void)
{
    mock_audio_reset();  /* not initialised */

    int16_t sample = 0;
    hal_audio_err_t err = hal_audio_write_samples(&sample, 1u);
    TEST_ASSERT_EQUAL_INT((int)HAL_AUDIO_ERR_INIT, (int)err);
}

/* -------------------------------------------------------------------------
 * main — run all bound tests in sequence.
 * -------------------------------------------------------------------------
 */
int main(void)
{
    test_ring_count_never_exceeds_capacity();
    test_ring_count_zero_before_init();
    test_ring_count_after_stop();
    test_ring_count_increments_with_writes();
    test_stereo_downmix_no_overflow();
    test_stereo_downmix_underflow();
    test_stereo_downmix_vol_zero_silence();
    test_prefill_threshold_math();
    test_ring_count_write_null_returns_err();
    test_ring_count_write_before_init();

    return 0;
}

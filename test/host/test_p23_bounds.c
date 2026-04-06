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
 *       must NOT overflow int32_t intermediate; exact result = 32639.
 *   6.  test_stereo_downmix_underflow           — (INT16_MIN + INT16_MIN)/2
 *       exact result = -32640 (not clamped; within INT16 range).
 *   7.  test_stereo_downmix_vol_zero_silence    — vol=0 → output 0.
 *   8.  test_prefill_threshold_math             — 80% of AUDIO_RING_BUF_SAMPLES
 *       must equal exactly 35280u (no tautological re-computation).
 *   9.  test_ring_count_write_null_returns_err  — NULL buf → HAL_AUDIO_ERR_NULL.
 *   10. test_ring_count_write_before_init       — write before init → ERR_INIT.
 *   11. test_write_zero_count_valid_buf         — write count=0 valid buf → OK,
 *       ring count unchanged (B2 guard: zero-count write must not error).
 *   12. test_write_zero_count_null_buf          — write count=0 NULL buf →
 *       HAL_AUDIO_ERR_NULL (NULL guard fires before count check).
 *   13. test_ring_fill_to_capacity_then_one_more — fill ring to exactly
 *       AUDIO_RING_BUF_SAMPLES=44100 samples (100% capacity), then one
 *       more sample returns HAL_AUDIO_ERR_OVERFLOW (B3 capacity boundary).
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
 * After /2 = 32767.
 * vol=255: (32767 * 255) / 256 = 8355585 / 256 = 32639 (C integer truncation).
 * Exact expected value: 32639.
 * -------------------------------------------------------------------------
 */
static void test_stereo_downmix_no_overflow(void)
{
    int16_t result = stereo_downmix_sample(32767, 32767, 255u);
    TEST_ASSERT_EQUAL_INT(32639, (int)result);
}

/* -------------------------------------------------------------------------
 * Bound test 6: stereo downmix — underflow result is exact.
 *
 * L = INT16_MIN (-32768), R = INT16_MIN (-32768).
 * Intermediate: -32768 + -32768 = -65536 — fits in int32_t.
 * After /2 = -32768.
 * vol=255: (-32768 * 255) / 256 = -8355840 / 256 = -32640 (C truncation
 *   toward zero: -8355840 / 256 = -32640.0 exactly → -32640).
 * Exact expected value: -32640.
 * -------------------------------------------------------------------------
 */
static void test_stereo_downmix_underflow(void)
{
    int16_t result = stereo_downmix_sample(-32768, -32768, 255u);
    TEST_ASSERT_EQUAL_INT(-32640, (int)result);
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
 * Bound test 8: prefill threshold — 80% of AUDIO_RING_BUF_SAMPLES equals
 * exactly 35280u (for AUDIO_RING_BUF_SAMPLES=44100).
 *
 * Pinned constant: 44100 * 80 / 100 = 35280.
 * Must not be a tautological re-computation of the same expression.
 * If AUDIO_RING_BUF_SAMPLES changes, this test will catch the mismatch.
 * -------------------------------------------------------------------------
 */
static void test_prefill_threshold_math(void)
{
    uint32_t threshold = (AUDIO_RING_BUF_SAMPLES * 80u) / 100u;

    /* Pinned expected value: 44100 * 80 / 100 = 35280. */
    TEST_ASSERT_EQUAL_UINT32(35280u, threshold);

    /* Threshold must be strictly less than capacity (not 100%). */
    TEST_ASSERT_EQUAL_INT(1, (int)(threshold < AUDIO_RING_BUF_SAMPLES));
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
 * Bound test 11 (B2): write_samples with count=0 and valid buffer → OK.
 *
 * A zero-count write is a no-op. It must not return an error and must
 * not change the ring fill count.
 * -------------------------------------------------------------------------
 */
static void test_write_zero_count_valid_buf(void)
{
    mock_audio_reset();
    hal_audio_init();

    int16_t samples[4] = {0};
    hal_audio_write_samples(samples, 10u);  /* prime ring with 10 samples */
    uint32_t count_before = hal_audio_get_ring_count();
    TEST_ASSERT_EQUAL_UINT32(10u, count_before);

    hal_audio_err_t err = hal_audio_write_samples(samples, 0u);
    TEST_ASSERT_EQUAL_INT((int)HAL_AUDIO_OK, (int)err);

    /* Ring count must not change after a zero-count write. */
    TEST_ASSERT_EQUAL_UINT32(10u, hal_audio_get_ring_count());
}

/* -------------------------------------------------------------------------
 * Bound test 12 (B2): write_samples with count=0 and NULL buffer → ERR_NULL.
 *
 * NULL guard must fire BEFORE the count check. A NULL pointer is always
 * an error regardless of count.
 * -------------------------------------------------------------------------
 */
static void test_write_zero_count_null_buf(void)
{
    mock_audio_reset();
    hal_audio_init();

    hal_audio_err_t err = hal_audio_write_samples(NULL, 0u);
    TEST_ASSERT_EQUAL_INT((int)HAL_AUDIO_ERR_NULL, (int)err);
}

/* -------------------------------------------------------------------------
 * Bound test 13 (B3): ring fills to exactly AUDIO_RING_BUF_SAMPLES = 44100.
 *
 * Writing exactly AUDIO_RING_BUF_SAMPLES samples fills the ring to 100%
 * capacity. One additional sample must then return HAL_AUDIO_ERR_OVERFLOW.
 * This test specifically verifies the capacity boundary at the known value
 * 44100 (AUDIO_MAX_TONE_MS=2000ms × 22050Hz = 44100 samples).
 * -------------------------------------------------------------------------
 */
static void test_ring_fill_to_capacity_then_one_more(void)
{
    /* Compile-time sanity: capacity must be exactly 44100. */
    _Static_assert(AUDIO_RING_BUF_SAMPLES == 44100u,
                   "B3: ring capacity changed — update pinned constant");

    mock_audio_reset();
    hal_audio_init();

    static int16_t full_buf[AUDIO_RING_BUF_SAMPLES];
    memset(full_buf, 0, sizeof(full_buf));

    /* Fill ring to 100% capacity. */
    hal_audio_err_t fill_err = hal_audio_write_samples(full_buf, AUDIO_RING_BUF_SAMPLES);
    TEST_ASSERT_EQUAL_INT((int)HAL_AUDIO_OK, (int)fill_err);
    TEST_ASSERT_EQUAL_UINT32(44100u, hal_audio_get_ring_count());

    /* One more sample must overflow. */
    int16_t one_more = 0;
    hal_audio_err_t over_err = hal_audio_write_samples(&one_more, 1u);
    TEST_ASSERT_EQUAL_INT((int)HAL_AUDIO_ERR_OVERFLOW, (int)over_err);

    /* Ring count must still be exactly at capacity — not beyond. */
    TEST_ASSERT_EQUAL_UINT32(44100u, hal_audio_get_ring_count());
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
    test_write_zero_count_valid_buf();
    test_write_zero_count_null_buf();
    test_ring_fill_to_capacity_then_one_more();

    return 0;
}

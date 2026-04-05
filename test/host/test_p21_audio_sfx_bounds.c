/**
 * test_p21_audio_sfx_bounds.c — Phase 21 Bound tests: Audio HAL, sfxr, SFX.
 *
 * Rule 22: Written BEFORE feature tests and BEFORE implementation (BOUND RED).
 * These tests prove the system REJECTS:
 *
 * 1. test_audio_err_abi_pinned        — HAL_AUDIO_ERR_INIT==1, ERR_INVALID_FREQ==2
 *                                       ERR_NULL==3, ERR_OVERFLOW==4 (new Phase 21 values)
 * 2. test_audio_write_before_init     — hal_audio_write_samples() before init
 *                                       returns HAL_AUDIO_ERR_INIT
 * 3. test_audio_write_null_buffer     — hal_audio_write_samples(NULL, N) after init
 *                                       returns HAL_AUDIO_ERR_NULL
 * 4. test_audio_play_freq_zero        — hal_audio_play(0, 100) returns
 *                                       HAL_AUDIO_ERR_INVALID_FREQ
 * 5. test_audio_play_duration_clamp   — hal_audio_play(440, 65535) clamped to 2000ms
 *                                       (mock captures clamped value)
 * 6. test_audio_ring_overflow         — fill ring buffer, then write more — no crash
 *                                       and overflow_flag is set
 * 7. test_sfx_invalid_id              — fq_sfx_play(SFX_COUNT, buf, len) returns
 *                                       GAME_ERR_INVALID
 * 8. test_sfx_no_hal_include          — verified at CMake configure time via
 *                                       assert_compile_fails boundary check; this
 *                                       test asserts the static fact that
 *                                       AUDIO_MAX_TONE_MS is defined in hal_audio.h
 *                                       (not game headers) — confirms boundary
 * 9. test_sfx_sample_count_bounded    — fq_sfx_play for each preset generates
 *                                       <= AUDIO_RING_BUF_SAMPLES samples
 * 10. test_sfx_play_returns_game_err  — return type is game_err_t (GAME_OK == 0)
 *
 * B1-B4 additions (reviewer blockers):
 * 11. test_sfx_play_null_buf          — fq_sfx_play with NULL buf returns GAME_ERR_NULL_PTR
 * 12. test_sfx_play_null_samples_out  — fq_sfx_play with NULL samples_out returns
 *                                       GAME_ERR_NULL_PTR
 * 13. test_audio_play_before_init     — hal_audio_play() before init returns
 *                                       HAL_AUDIO_ERR_INIT
 * 14. test_audio_nyquist_frequency    — hal_audio_play(22050, 100) returns HAL_AUDIO_OK
 *                                       (aliased but valid at Nyquist boundary)
 * 15. test_audio_deinit_while_playing — init, write_samples, deinit, write_samples again
 *                                       returns HAL_AUDIO_ERR_INIT
 */

#include "hal_audio.h"
#include "sfx.h"
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>

/* Forward-declare mock accessors (defined in mock_hal_audio.c). */
void     mock_audio_reset(void);
uint32_t mock_audio_get_samples_written(void);
uint32_t mock_audio_get_overflow_flag(void);
uint16_t mock_audio_get_last_duration_ms(void);

#define ASSERT_EQ(label, expected, actual)                                  \
    do {                                                                    \
        if ((int)(expected) != (int)(actual)) {                             \
            printf("FAIL [%s]: expected %d got %d\n",                       \
                   (label), (int)(expected), (int)(actual));                \
            return 1;                                                        \
        }                                                                   \
        printf("PASS [%s]\n", (label));                                     \
    } while (0)

#define ASSERT_TRUE(label, cond)                                            \
    do {                                                                    \
        if (!(cond)) {                                                      \
            printf("FAIL [%s]: condition was false\n", (label));            \
            return 1;                                                        \
        }                                                                   \
        printf("PASS [%s]\n", (label));                                     \
    } while (0)

int main(void)
{
    mock_audio_reset();

    /* -----------------------------------------------------------------------
     * 1. Error code ABI pinned via _Static_assert in hal_audio.h.
     *    Test confirms values at runtime too for belt-and-suspenders.
     * ----------------------------------------------------------------------- */
    ASSERT_EQ("audio_ok_is_zero",              0, (int)HAL_AUDIO_OK);
    ASSERT_EQ("audio_err_init_is_one",         1, (int)HAL_AUDIO_ERR_INIT);
    ASSERT_EQ("audio_err_invalid_freq_is_two", 2, (int)HAL_AUDIO_ERR_INVALID_FREQ);
    ASSERT_EQ("audio_err_null_is_three",       3, (int)HAL_AUDIO_ERR_NULL);
    ASSERT_EQ("audio_err_overflow_is_four",    4, (int)HAL_AUDIO_ERR_OVERFLOW);

    /* -----------------------------------------------------------------------
     * 2. hal_audio_write_samples() before init returns ERR_INIT.
     * ----------------------------------------------------------------------- */
    {
        static int16_t buf[16];
        ASSERT_EQ("write_before_init",
                  HAL_AUDIO_ERR_INIT,
                  hal_audio_write_samples(buf, 16u));
    }

    /* -----------------------------------------------------------------------
     * 3. hal_audio_write_samples(NULL, N) after init returns ERR_NULL.
     * ----------------------------------------------------------------------- */
    hal_audio_init();
    ASSERT_EQ("write_null_buf",
              HAL_AUDIO_ERR_NULL,
              hal_audio_write_samples(NULL, 16u));

    /* -----------------------------------------------------------------------
     * 4. hal_audio_play(0, 100) returns ERR_INVALID_FREQ.
     * ----------------------------------------------------------------------- */
    ASSERT_EQ("play_freq_zero_rejected",
              HAL_AUDIO_ERR_INVALID_FREQ,
              hal_audio_play(0u, 100u));

    /* -----------------------------------------------------------------------
     * 5. Duration clamp: hal_audio_play(440, 65535) must clamp duration to
     *    AUDIO_MAX_TONE_MS (2000). The mock captures the clamped value.
     * ----------------------------------------------------------------------- */
    ASSERT_EQ("play_duration_clamp",
              HAL_AUDIO_OK,
              hal_audio_play(440u, 65535u));
    ASSERT_EQ("duration_clamped_to_2000",
              (uint32_t)AUDIO_MAX_TONE_MS,
              (uint32_t)mock_audio_get_last_duration_ms());

    /* -----------------------------------------------------------------------
     * 6. Ring buffer overflow: fill ring, write more — no crash.
     *    AUDIO_RING_BUF_SAMPLES is the ring capacity.
     *    Write exactly RING+1 samples in two passes; verify overflow_flag is set.
     * ----------------------------------------------------------------------- */
    {
        /* Static buffer of ring capacity + 1 to trigger exactly one overflow. */
        static int16_t big_buf[AUDIO_RING_BUF_SAMPLES + 1u];
        /* Flush existing samples so we start with an empty ring. */
        mock_audio_reset();
        hal_audio_init();

        /* Fill the ring exactly. */
        hal_audio_write_samples(big_buf, AUDIO_RING_BUF_SAMPLES);
        ASSERT_EQ("ring_not_overflowed_after_exact_fill",
                  0u,
                  mock_audio_get_overflow_flag());

        /* Write one more sample — must set overflow flag and not crash. */
        hal_audio_write_samples(big_buf, 1u);
        ASSERT_EQ("ring_overflow_flag_set",
                  1u,
                  mock_audio_get_overflow_flag());
    }

    /* -----------------------------------------------------------------------
     * 7. fq_sfx_play(SFX_COUNT, ...) returns GAME_ERR_INVALID.
     * ----------------------------------------------------------------------- */
    {
        static int16_t sfx_buf[AUDIO_RING_BUF_SAMPLES];
        size_t         samples_out = 0u;
        ASSERT_EQ("sfx_invalid_id",
                  GAME_ERR_INVALID,
                  (int)fq_sfx_play(SFX_COUNT, sfx_buf, AUDIO_RING_BUF_SAMPLES,
                                   &samples_out));
    }

    /* -----------------------------------------------------------------------
     * 8. AUDIO_MAX_TONE_MS is defined in hal_audio.h (not game headers).
     *    This is a static/compile-time fact; the assertion below just proves
     *    the constant exists and has the expected value.
     * ----------------------------------------------------------------------- */
    ASSERT_EQ("audio_max_tone_ms_is_2000",
              2000u,
              (uint32_t)AUDIO_MAX_TONE_MS);

    /* -----------------------------------------------------------------------
     * 9. Each SFX preset generates <= AUDIO_RING_BUF_SAMPLES samples.
     * ----------------------------------------------------------------------- */
    {
        static int16_t sfx_buf[AUDIO_RING_BUF_SAMPLES];
        size_t         samples_out = 0u;

        for (int id = 0; id < (int)SFX_COUNT; id++) {
            samples_out = 0u;
            game_err_t err = fq_sfx_play((fq_sfx_id_t)id,
                                          sfx_buf,
                                          AUDIO_RING_BUF_SAMPLES,
                                          &samples_out);
            if (err != GAME_OK) {
                printf("FAIL [sfx_sample_count_bounded]: sfx id %d returned err %d\n",
                       id, (int)err);
                return 1;
            }
            if (samples_out > (size_t)AUDIO_RING_BUF_SAMPLES) {
                printf("FAIL [sfx_sample_count_bounded]: id %d generated %u > %u\n",
                       id, (unsigned)samples_out, (unsigned)AUDIO_RING_BUF_SAMPLES);
                return 1;
            }
        }
        printf("PASS [sfx_sample_count_bounded]\n");
    }

    /* -----------------------------------------------------------------------
     * 10. fq_sfx_play() returns game_err_t. Verify GAME_OK == 0 for a valid call.
     * ----------------------------------------------------------------------- */
    {
        static int16_t sfx_buf[AUDIO_RING_BUF_SAMPLES];
        size_t         samples_out = 0u;
        game_err_t     ret = fq_sfx_play(SFX_BTN_PRESS,
                                          sfx_buf,
                                          AUDIO_RING_BUF_SAMPLES,
                                          &samples_out);
        ASSERT_EQ("sfx_play_returns_game_err_ok", (int)GAME_OK, (int)ret);
    }

    /* -----------------------------------------------------------------------
     * B1. fq_sfx_play with NULL buf returns GAME_ERR_NULL_PTR.
     * ----------------------------------------------------------------------- */
    {
        size_t samples_out = 0u;
        ASSERT_EQ("sfx_play_null_buf",
                  GAME_ERR_NULL_PTR,
                  (int)fq_sfx_play(SFX_BTN_PRESS, NULL, AUDIO_RING_BUF_SAMPLES,
                                   &samples_out));
    }

    /* -----------------------------------------------------------------------
     * B2. fq_sfx_play with NULL samples_out returns GAME_ERR_NULL_PTR.
     * ----------------------------------------------------------------------- */
    {
        static int16_t sfx_buf[AUDIO_RING_BUF_SAMPLES];
        ASSERT_EQ("sfx_play_null_samples_out",
                  GAME_ERR_NULL_PTR,
                  (int)fq_sfx_play(SFX_BTN_PRESS, sfx_buf, AUDIO_RING_BUF_SAMPLES,
                                   NULL));
    }

    /* -----------------------------------------------------------------------
     * B3. hal_audio_play() before init returns HAL_AUDIO_ERR_INIT.
     *     mock_audio_reset() clears the initialised flag without re-initing.
     * ----------------------------------------------------------------------- */
    mock_audio_reset();
    ASSERT_EQ("audio_play_before_init",
              HAL_AUDIO_ERR_INIT,
              hal_audio_play(440u, 100u));

    /* -----------------------------------------------------------------------
     * B4. Nyquist boundary: hal_audio_play(22050, 100) returns HAL_AUDIO_OK.
     *     22050 Hz is the sample rate itself — aliased but syntactically valid.
     *     The HAL must not reject it (only freq_hz == 0 is invalid).
     * ----------------------------------------------------------------------- */
    hal_audio_init();
    ASSERT_EQ("audio_nyquist_frequency_ok",
              HAL_AUDIO_OK,
              hal_audio_play(22050u, 100u));

    /* -----------------------------------------------------------------------
     * B5. Deinit while playing: init, write_samples, deinit, write_samples
     *     again — must return HAL_AUDIO_ERR_INIT after deinit.
     * ----------------------------------------------------------------------- */
    {
        static int16_t sfx_buf[64];
        mock_audio_reset();
        hal_audio_init();

        /* Write some samples while initialised — must succeed. */
        ASSERT_EQ("write_after_init_ok",
                  HAL_AUDIO_OK,
                  hal_audio_write_samples(sfx_buf, 64u));

        /* Deinit — teardown the driver. */
        hal_audio_deinit();

        /* Write again after deinit — must return ERR_INIT. */
        ASSERT_EQ("write_after_deinit_err_init",
                  HAL_AUDIO_ERR_INIT,
                  hal_audio_write_samples(sfx_buf, 64u));
    }

    hal_audio_deinit();
    return 0;
}

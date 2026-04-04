/**
 * test_p21_audio_sfx_feature.c — Phase 21 Feature tests: Audio HAL + sfxr SFX.
 *
 * Rule 22: Written AFTER bound tests but BEFORE implementation (FEATURE RED).
 *
 * Happy-path behaviours tested:
 *
 * 1. hal_audio_write_samples() writes samples into mock ring buffer.
 * 2. mock captures sample count written.
 * 3. hal_audio_play() still works (square wave path) with clamped duration.
 * 4. fq_sfx_play(SFX_BTN_PRESS)   — generates > 0 samples.
 * 5. fq_sfx_play(SFX_COMBAT_HIT)  — generates > 0 samples, different from BTN_PRESS.
 * 6. fq_sfx_play(SFX_LEVEL_UP)    — generates > 0 samples.
 * 7. Rapid SFX spam (10 calls)     — no crash, ring buffer handles overflow.
 * 8. All 10 SFX IDs succeed        — complete enum coverage.
 * 9. fq_sfx_play returns GAME_OK for all valid IDs.
 * 10. SFX presets are distinct      — BTN_PRESS and DEATH produce different first sample.
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
        if ((uint32_t)(expected) != (uint32_t)(actual)) {                   \
            printf("FAIL [%s]: expected %u got %u\n",                       \
                   (label), (unsigned)(expected), (unsigned)(actual));      \
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
    hal_audio_init();

    /* -----------------------------------------------------------------------
     * 1 & 2. hal_audio_write_samples() writes into mock, captures count.
     * ----------------------------------------------------------------------- */
    {
        static int16_t buf[64];
        buf[0] = 1000;
        buf[1] = -1000;
        hal_audio_err_t err = hal_audio_write_samples(buf, 64u);
        ASSERT_EQ("write_samples_ok",    (uint32_t)HAL_AUDIO_OK, (uint32_t)err);
        ASSERT_EQ("samples_written_64",  64u, mock_audio_get_samples_written());
    }

    /* -----------------------------------------------------------------------
     * 3. hal_audio_play() still works; duration clamped at AUDIO_MAX_TONE_MS.
     * ----------------------------------------------------------------------- */
    mock_audio_reset();
    hal_audio_init();
    ASSERT_EQ("play_440hz_ok",
              (uint32_t)HAL_AUDIO_OK,
              (uint32_t)hal_audio_play(440u, 100u));
    ASSERT_EQ("last_duration_100", 100u, (uint32_t)mock_audio_get_last_duration_ms());

    /* Clamp test: 3000ms must be clamped to AUDIO_MAX_TONE_MS (2000ms). */
    ASSERT_EQ("play_3000ms_ok",
              (uint32_t)HAL_AUDIO_OK,
              (uint32_t)hal_audio_play(440u, 3000u));
    ASSERT_EQ("duration_clamped",
              (uint32_t)AUDIO_MAX_TONE_MS,
              (uint32_t)mock_audio_get_last_duration_ms());

    /* -----------------------------------------------------------------------
     * 4. fq_sfx_play(SFX_BTN_PRESS) generates > 0 samples.
     * ----------------------------------------------------------------------- */
    mock_audio_reset();
    hal_audio_init();
    {
        static int16_t sfx_buf[AUDIO_RING_BUF_SAMPLES];
        size_t         samples_out = 0u;
        game_err_t     err = fq_sfx_play(SFX_BTN_PRESS,
                                          sfx_buf,
                                          AUDIO_RING_BUF_SAMPLES,
                                          &samples_out);
        ASSERT_EQ("sfx_btn_press_ok",         (uint32_t)GAME_OK, (uint32_t)err);
        ASSERT_TRUE("sfx_btn_press_nonzero",  samples_out > 0u);
    }

    /* -----------------------------------------------------------------------
     * 5. fq_sfx_play(SFX_COMBAT_HIT) generates > 0 samples.
     * ----------------------------------------------------------------------- */
    {
        static int16_t sfx_buf[AUDIO_RING_BUF_SAMPLES];
        size_t         samples_out = 0u;
        game_err_t     err = fq_sfx_play(SFX_COMBAT_HIT,
                                          sfx_buf,
                                          AUDIO_RING_BUF_SAMPLES,
                                          &samples_out);
        ASSERT_EQ("sfx_combat_hit_ok",        (uint32_t)GAME_OK, (uint32_t)err);
        ASSERT_TRUE("sfx_combat_hit_nonzero", samples_out > 0u);
    }

    /* -----------------------------------------------------------------------
     * 6. fq_sfx_play(SFX_LEVEL_UP) generates > 0 samples.
     * ----------------------------------------------------------------------- */
    {
        static int16_t sfx_buf[AUDIO_RING_BUF_SAMPLES];
        size_t         samples_out = 0u;
        game_err_t     err = fq_sfx_play(SFX_LEVEL_UP,
                                          sfx_buf,
                                          AUDIO_RING_BUF_SAMPLES,
                                          &samples_out);
        ASSERT_EQ("sfx_level_up_ok",        (uint32_t)GAME_OK, (uint32_t)err);
        ASSERT_TRUE("sfx_level_up_nonzero", samples_out > 0u);
    }

    /* -----------------------------------------------------------------------
     * 7. Rapid SFX spam — 10 rapid calls must not crash.
     *    Ring buffer overflow is handled gracefully (no crash, flag set is OK).
     * ----------------------------------------------------------------------- */
    {
        static int16_t sfx_buf[AUDIO_RING_BUF_SAMPLES];
        size_t         samples_out = 0u;
        mock_audio_reset();
        hal_audio_init();

        for (int i = 0; i < 10; i++) {
            samples_out = 0u;
            /* Write samples directly to test ring overflow resilience. */
            fq_sfx_play(SFX_BTN_PRESS, sfx_buf, AUDIO_RING_BUF_SAMPLES,
                        &samples_out);
            hal_audio_write_samples(sfx_buf, samples_out);
        }
        ASSERT_TRUE("rapid_spam_no_crash", 1);
    }

    /* -----------------------------------------------------------------------
     * 8. All 10 SFX IDs succeed without crash.
     * ----------------------------------------------------------------------- */
    {
        static int16_t sfx_buf[AUDIO_RING_BUF_SAMPLES];
        size_t         samples_out = 0u;
        int            all_ok      = 1;

        for (int id = 0; id < (int)SFX_COUNT; id++) {
            samples_out = 0u;
            game_err_t err = fq_sfx_play((fq_sfx_id_t)id,
                                          sfx_buf,
                                          AUDIO_RING_BUF_SAMPLES,
                                          &samples_out);
            if (err != GAME_OK || samples_out == 0u) {
                printf("FAIL [all_sfx_ids_succeed]: id=%d err=%d samples=%u\n",
                       id, (int)err, (unsigned)samples_out);
                all_ok = 0;
            }
        }
        ASSERT_TRUE("all_sfx_ids_succeed", all_ok);
    }

    /* -----------------------------------------------------------------------
     * 9. Return type is game_err_t — GAME_OK for all valid IDs.
     * ----------------------------------------------------------------------- */
    {
        static int16_t sfx_buf[AUDIO_RING_BUF_SAMPLES];
        size_t         samples_out = 0u;

        /* Spot-check three IDs. */
        ASSERT_EQ("sfx_rebirth_ok",
                  (uint32_t)GAME_OK,
                  (uint32_t)fq_sfx_play(SFX_REBIRTH, sfx_buf,
                                         AUDIO_RING_BUF_SAMPLES, &samples_out));
        ASSERT_EQ("sfx_death_ok",
                  (uint32_t)GAME_OK,
                  (uint32_t)fq_sfx_play(SFX_DEATH, sfx_buf,
                                         AUDIO_RING_BUF_SAMPLES, &samples_out));
        ASSERT_EQ("sfx_item_equip_ok",
                  (uint32_t)GAME_OK,
                  (uint32_t)fq_sfx_play(SFX_ITEM_EQUIP, sfx_buf,
                                         AUDIO_RING_BUF_SAMPLES, &samples_out));
    }

    /* -----------------------------------------------------------------------
     * 10. BTN_PRESS and DEATH produce different first samples (distinct presets).
     * ----------------------------------------------------------------------- */
    {
        static int16_t buf_press[AUDIO_RING_BUF_SAMPLES];
        static int16_t buf_death[AUDIO_RING_BUF_SAMPLES];
        size_t         cnt_press = 0u;
        size_t         cnt_death = 0u;

        fq_sfx_play(SFX_BTN_PRESS, buf_press, AUDIO_RING_BUF_SAMPLES, &cnt_press);
        fq_sfx_play(SFX_DEATH,     buf_death, AUDIO_RING_BUF_SAMPLES, &cnt_death);

        /* At least one of the first 4 samples must differ — presets are distinct. */
        int differs = 0;
        size_t check = (cnt_press < 4u) ? cnt_press : 4u;
        if (cnt_death < check) { check = cnt_death; }
        for (size_t i = 0; i < check; i++) {
            if (buf_press[i] != buf_death[i]) {
                differs = 1;
                break;
            }
        }
        ASSERT_TRUE("sfx_presets_distinct", differs);
    }

    hal_audio_deinit();
    return 0;
}

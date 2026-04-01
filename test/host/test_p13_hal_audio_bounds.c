/**
 * test_p13_hal_audio_bounds.c — Phase 13 Bound tests for hal_audio interface.
 *
 * Rule 22: Written BEFORE feature tests and BEFORE implementation (BOUND RED).
 * Tests prove the system REJECTS:
 *   - freq_hz == 0 at play              → HAL_AUDIO_ERR_INVALID_FREQ
 *   - play before init                  → HAL_AUDIO_ERR_INIT
 *   - stop before init                  → HAL_AUDIO_OK (stop is always safe)
 *   - stop after deinit                 → HAL_AUDIO_OK (stop is always safe)
 *   - UINT16_MAX frequency accepted     → HAL_AUDIO_OK (no upper freq bound)
 *   - Double deinit is safe             → no crash
 *   - Re-init after deinit is safe      → HAL_AUDIO_OK
 *   - Enum value contract locks (A4)    → HAL_AUDIO_OK==0, ERR_INIT==1,
 *                                          ERR_INVALID_FREQ==2
 */

#include "hal_audio.h"
#include <stdint.h>
#include <stdio.h>

/* Forward-declare mock reset accessor. */
void mock_audio_reset(void);

#define ASSERT_EQ(label, expected, actual)                              \
    do {                                                                \
        if ((int)(expected) != (int)(actual)) {                         \
            printf("FAIL [%s]: expected %d got %d\n",                  \
                   (label), (int)(expected), (int)(actual));            \
            return 1;                                                   \
        }                                                               \
        printf("PASS [%s]\n", (label));                                 \
    } while (0)

#define ASSERT_TRUE(label, cond)                                        \
    do {                                                                \
        if (!(cond)) {                                                  \
            printf("FAIL [%s]: condition was false\n", (label));        \
            return 1;                                                   \
        }                                                               \
        printf("PASS [%s]\n", (label));                                 \
    } while (0)

int main(void)
{
    mock_audio_reset();

    /* ------------------------------------------------------------------
     * A4: Enum value contract locks — numeric values must never change.
     * ------------------------------------------------------------------ */
    ASSERT_EQ("audio_ok_is_zero",            0, (int)HAL_AUDIO_OK);
    ASSERT_EQ("audio_err_init_is_one",       1, (int)HAL_AUDIO_ERR_INIT);
    ASSERT_EQ("audio_err_invalid_freq_is_2", 2, (int)HAL_AUDIO_ERR_INVALID_FREQ);

    /* ------------------------------------------------------------------
     * play before init must return ERR_INIT.
     * ------------------------------------------------------------------ */
    ASSERT_EQ("play_before_init",
              HAL_AUDIO_ERR_INIT,
              hal_audio_play(440u, 100u));

    /* ------------------------------------------------------------------
     * stop before init must return OK (stop is always a safe no-op).
     * ------------------------------------------------------------------ */
    ASSERT_EQ("stop_before_init_ok",
              HAL_AUDIO_OK,
              hal_audio_stop());

    /* ------------------------------------------------------------------
     * Init succeeds.
     * ------------------------------------------------------------------ */
    ASSERT_EQ("init_ok", HAL_AUDIO_OK, hal_audio_init());

    /* ------------------------------------------------------------------
     * freq_hz == 0 is invalid — ERR_INVALID_FREQ.
     * ------------------------------------------------------------------ */
    ASSERT_EQ("zero_freq_rejected",
              HAL_AUDIO_ERR_INVALID_FREQ,
              hal_audio_play(0u, 100u));

    /* ------------------------------------------------------------------
     * duration_ms == 0 is valid (instantaneous click / stop-on-next).
     * Any non-zero freq with zero duration returns OK.
     * ------------------------------------------------------------------ */
    ASSERT_EQ("zero_duration_ok",
              HAL_AUDIO_OK,
              hal_audio_play(440u, 0u));

    /* ------------------------------------------------------------------
     * UINT16_MAX frequency is accepted (no artificial upper cap in HAL).
     * ------------------------------------------------------------------ */
    ASSERT_EQ("max_freq_accepted",
              HAL_AUDIO_OK,
              hal_audio_play(65535u, 100u));

    /* ------------------------------------------------------------------
     * Double deinit is safe.
     * ------------------------------------------------------------------ */
    hal_audio_deinit();
    hal_audio_deinit();
    ASSERT_TRUE("double_deinit_safe", 1);

    /* ------------------------------------------------------------------
     * B1: stop after deinit must return OK (stop is unconditionally safe).
     * ------------------------------------------------------------------ */
    ASSERT_EQ("stop_after_deinit_ok",
              HAL_AUDIO_OK,
              hal_audio_stop());

    /* ------------------------------------------------------------------
     * Re-init after deinit returns OK.
     * ------------------------------------------------------------------ */
    ASSERT_EQ("reinit_after_deinit_ok", HAL_AUDIO_OK, hal_audio_init());

    hal_audio_deinit();
    return 0;
}

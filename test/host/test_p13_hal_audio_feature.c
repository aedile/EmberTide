/**
 * test_p13_hal_audio_feature.c — Phase 13 Feature tests for hal_audio.
 *
 * Rule 22: Written BEFORE implementation (FEATURE RED).
 * Tested happy-path behaviours:
 *   - init returns OK
 *   - play valid freq/duration returns OK and captures values in mock
 *   - stop returns OK
 *   - play count increments per call
 *   - overlapping tones: second play overrides first (play_count still increments)
 *   - mock_audio_reset clears all state
 *   - deinit is safe, re-init is safe
 *   - last_freq and last_duration_ms hold the most recent values
 *   - post-deinit state inspection: last_freq and last_duration_ms persist
 *     across deinit (A5)
 */

#include "hal_audio.h"
#include <stdint.h>
#include <stdio.h>

/* Test accessor declarations (defined in mock_hal_audio.c). */
void     mock_audio_reset(void);
uint16_t mock_audio_get_last_freq(void);
uint16_t mock_audio_get_last_duration_ms(void);
uint32_t mock_audio_get_play_count(void);

#define ASSERT_EQ(label, expected, actual)                              \
    do {                                                                \
        if ((uint32_t)(expected) != (uint32_t)(actual)) {              \
            printf("FAIL [%s]: expected %u got %u\n",                  \
                   (label), (unsigned)(expected), (unsigned)(actual));  \
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
     * 1. Init returns OK; play count starts at 0.
     * ------------------------------------------------------------------ */
    ASSERT_EQ("init_ok", (uint32_t)HAL_AUDIO_OK, (uint32_t)hal_audio_init());
    ASSERT_EQ("play_count_initial", 0u, mock_audio_get_play_count());

    /* ------------------------------------------------------------------
     * 2. Play valid freq 440 Hz, 250 ms — returns OK.
     * ------------------------------------------------------------------ */
    ASSERT_EQ("play_440hz_ok",
              (uint32_t)HAL_AUDIO_OK,
              (uint32_t)hal_audio_play(440u, 250u));
    ASSERT_EQ("play_count_after_one",    1u, mock_audio_get_play_count());
    ASSERT_EQ("last_freq_440",         440u, (uint32_t)mock_audio_get_last_freq());
    ASSERT_EQ("last_duration_250",     250u, (uint32_t)mock_audio_get_last_duration_ms());

    /* ------------------------------------------------------------------
     * 3. Stop returns OK.
     * ------------------------------------------------------------------ */
    ASSERT_EQ("stop_ok", (uint32_t)HAL_AUDIO_OK, (uint32_t)hal_audio_stop());

    /* ------------------------------------------------------------------
     * 4. Play another tone — play count increments, values updated.
     * ------------------------------------------------------------------ */
    ASSERT_EQ("play_880hz_ok",
              (uint32_t)HAL_AUDIO_OK,
              (uint32_t)hal_audio_play(880u, 100u));
    ASSERT_EQ("play_count_after_two",  2u,   mock_audio_get_play_count());
    ASSERT_EQ("last_freq_880",         880u, (uint32_t)mock_audio_get_last_freq());
    ASSERT_EQ("last_duration_100",     100u, (uint32_t)mock_audio_get_last_duration_ms());

    /* ------------------------------------------------------------------
     * 5. Overlapping tones: second play overrides first within mock.
     *    (On hardware this resyncs the PWM timer — mock just captures.)
     * ------------------------------------------------------------------ */
    ASSERT_EQ("overlap_play_first",
              (uint32_t)HAL_AUDIO_OK,
              (uint32_t)hal_audio_play(220u, 500u));
    ASSERT_EQ("overlap_play_second",
              (uint32_t)HAL_AUDIO_OK,
              (uint32_t)hal_audio_play(1000u, 50u));
    ASSERT_EQ("play_count_after_overlap", 4u,    mock_audio_get_play_count());
    ASSERT_EQ("last_freq_after_overlap",  1000u, (uint32_t)mock_audio_get_last_freq());
    ASSERT_EQ("last_dur_after_overlap",   50u,   (uint32_t)mock_audio_get_last_duration_ms());

    /* ------------------------------------------------------------------
     * 6. mock_audio_reset clears all state.
     * ------------------------------------------------------------------ */
    mock_audio_reset();
    ASSERT_EQ("play_count_after_reset",   0u, mock_audio_get_play_count());
    ASSERT_EQ("last_freq_after_reset",    0u, (uint32_t)mock_audio_get_last_freq());
    ASSERT_EQ("last_dur_after_reset",     0u, (uint32_t)mock_audio_get_last_duration_ms());

    /* ------------------------------------------------------------------
     * 7. Deinit is safe; re-init is safe.
     * ------------------------------------------------------------------ */
    hal_audio_deinit();
    ASSERT_EQ("reinit_ok", (uint32_t)HAL_AUDIO_OK, (uint32_t)hal_audio_init());
    ASSERT_EQ("play_after_reinit_ok",
              (uint32_t)HAL_AUDIO_OK,
              (uint32_t)hal_audio_play(330u, 75u));
    ASSERT_EQ("play_count_after_reinit",  1u,   mock_audio_get_play_count());
    ASSERT_EQ("last_freq_after_reinit",   330u, (uint32_t)mock_audio_get_last_freq());

    hal_audio_deinit();
    ASSERT_TRUE("final_deinit_safe", 1);

    /* ------------------------------------------------------------------
     * 8. A5: Post-deinit state inspection.
     *    init → play(750, 200) → deinit.
     *    The mock preserves last_freq and last_duration_ms across deinit
     *    so callers can inspect the final played tone after shutdown.
     * ------------------------------------------------------------------ */
    mock_audio_reset();
    hal_audio_init();
    ASSERT_EQ("a5_play_750hz_ok",
              (uint32_t)HAL_AUDIO_OK,
              (uint32_t)hal_audio_play(750u, 200u));
    hal_audio_deinit();
    ASSERT_EQ("a5_last_freq_persists_750",     750u,
              (uint32_t)mock_audio_get_last_freq());
    ASSERT_EQ("a5_last_duration_persists_200", 200u,
              (uint32_t)mock_audio_get_last_duration_ms());

    return 0;
}

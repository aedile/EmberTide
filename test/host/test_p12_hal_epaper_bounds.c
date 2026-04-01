/**
 * test_p12_hal_epaper_bounds.c — Phase 12 Bound tests for hal_epaper interface.
 *
 * Rule 22: Written BEFORE feature tests and BEFORE implementation (BOUND RED).
 * Tests prove the system REJECTS:
 *   - NULL pixel pointer        → HAL_EPAPER_ERR_NULL
 *   - Wrong buffer size         → HAL_EPAPER_ERR_NULL
 *   - Zero size                 → HAL_EPAPER_ERR_NULL
 *   - Flush before init         → HAL_EPAPER_ERR_INIT
 *   - Size underflow (size-1)   → HAL_EPAPER_ERR_NULL
 *   - Size overflow (size+1)    → HAL_EPAPER_ERR_NULL
 *   - UINT32_MAX size           → HAL_EPAPER_ERR_NULL
 *   - HAL_EPAPER_FB_SIZE constant equals 5000 (contract lock)
 */

#include "hal_epaper.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>

/* Minimal assertion helper — no Unity dependency in bound tests. */
#define ASSERT_EQ(label, expected, actual)                          \
    do {                                                            \
        if ((expected) != (actual)) {                               \
            printf("FAIL [%s]: expected %d got %d\n",              \
                   (label), (int)(expected), (int)(actual));        \
            return 1;                                               \
        }                                                           \
        printf("PASS [%s]\n", (label));                             \
    } while (0)

#define ASSERT_TRUE(label, cond)                                    \
    do {                                                            \
        if (!(cond)) {                                              \
            printf("FAIL [%s]: condition was false\n", (label));    \
            return 1;                                               \
        }                                                           \
        printf("PASS [%s]\n", (label));                             \
    } while (0)

int main(void)
{
    static uint8_t buf[HAL_EPAPER_FB_SIZE];

    /* ------------------------------------------------------------------ */
    /* Contract lock: HAL_EPAPER_FB_SIZE must equal 5000.                  */
    /* This is the 200*200/8 = 5000 byte packed 1-bit framebuffer size.    */
    /* ------------------------------------------------------------------ */
    ASSERT_EQ("FB_SIZE_equals_5000", 5000u, (uint32_t)HAL_EPAPER_FB_SIZE);

    /* ------------------------------------------------------------------ */
    /* Flush before init must return ERR_INIT.                             */
    /* Call deinit first to ensure the mock is in uninitialised state.     */
    /* ------------------------------------------------------------------ */
    hal_epaper_deinit();
    memset(buf, 0, sizeof(buf));
    ASSERT_EQ("flush_before_init",
              HAL_EPAPER_ERR_INIT,
              hal_epaper_flush(buf, HAL_EPAPER_FB_SIZE));

    /* Now init so remaining bound tests exercise the size/null guards. */
    ASSERT_EQ("init_returns_ok", HAL_EPAPER_OK, hal_epaper_init());

    /* ------------------------------------------------------------------ */
    /* NULL pointer guard.                                                 */
    /* ------------------------------------------------------------------ */
    ASSERT_EQ("null_ptr_rejected",
              HAL_EPAPER_ERR_NULL,
              hal_epaper_flush(NULL, HAL_EPAPER_FB_SIZE));

    /* ------------------------------------------------------------------ */
    /* Wrong size: zero.                                                   */
    /* ------------------------------------------------------------------ */
    ASSERT_EQ("zero_size_rejected",
              HAL_EPAPER_ERR_NULL,
              hal_epaper_flush(buf, 0u));

    /* ------------------------------------------------------------------ */
    /* Wrong size: size - 1 (underflow by 1 byte).                         */
    /* ------------------------------------------------------------------ */
    ASSERT_EQ("size_minus1_rejected",
              HAL_EPAPER_ERR_NULL,
              hal_epaper_flush(buf, HAL_EPAPER_FB_SIZE - 1u));

    /* ------------------------------------------------------------------ */
    /* Wrong size: size + 1 (overflow by 1 byte).                          */
    /* ------------------------------------------------------------------ */
    ASSERT_EQ("size_plus1_rejected",
              HAL_EPAPER_ERR_NULL,
              hal_epaper_flush(buf, HAL_EPAPER_FB_SIZE + 1u));

    /* ------------------------------------------------------------------ */
    /* Wrong size: UINT32_MAX.                                             */
    /* ------------------------------------------------------------------ */
    ASSERT_EQ("uint32_max_size_rejected",
              HAL_EPAPER_ERR_NULL,
              hal_epaper_flush(buf, UINT32_MAX));

    /* ------------------------------------------------------------------ */
    /* NULL + wrong size together: NULL must dominate (ERR_NULL returned). */
    /* ------------------------------------------------------------------ */
    ASSERT_EQ("null_plus_wrong_size_rejected",
              HAL_EPAPER_ERR_NULL,
              hal_epaper_flush(NULL, 0u));

    hal_epaper_deinit();
    return 0;
}

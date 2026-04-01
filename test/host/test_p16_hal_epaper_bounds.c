/**
 * test_p16_hal_epaper_bounds.c — Phase 16 bound tests: e-paper HAL.
 *
 * BOUND RED tests (Rule 22): Prove the system REJECTS invalid inputs and
 * that the mock correctly supports failure injection for SPI error simulation
 * and BUSY timeout simulation.
 *
 * Bound conditions tested:
 *   B1 : flush with NULL pixel pointer returns HAL_EPAPER_ERR_NULL.
 *   B2 : flush with wrong size returns HAL_EPAPER_ERR_SIZE.
 *   B3 : flush before init returns HAL_EPAPER_ERR_INIT (guard order).
 *   B4 : flush size of 0 returns HAL_EPAPER_ERR_SIZE (size 0 is wrong size).
 *   B5 : flush size UINT32_MAX returns HAL_EPAPER_ERR_SIZE.
 *   B6 : mock_epaper_inject_spi_error() makes next flush return HAL_EPAPER_ERR_SPI.
 *   B7 : mock_epaper_inject_busy_timeout() makes next flush return
 *        HAL_EPAPER_ERR_BUSY_TIMEOUT.
 *   B8 : After injected error fires once, subsequent flush succeeds (one-shot).
 *   B9 : NULL pointer passed to flush checks NULL BEFORE size (guard order).
 *   B10: sleep() and deinit() are safe before init (no-op / OK).
 */

#include "hal_epaper.h"
#include "mock_hal_epaper.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Minimal test framework — no Unity dependency in bounds tests. */
static int g_failures = 0;

#define ASSERT_EQ(label, expected, actual) do { \
    if ((int)(actual) != (int)(expected)) { \
        fprintf(stderr, "[FAIL] %s: expected %d, got %d\n", (label), (int)(expected), (int)(actual)); \
        g_failures++; \
    } \
} while (0)

int main(void)
{
    static uint8_t fb[HAL_EPAPER_FB_SIZE];
    hal_epaper_err_t err;

    /* -----------------------------------------------------------------------
     * B1: NULL pointer check fires before size/init checks.
     * ----------------------------------------------------------------------- */
    mock_epaper_reset();
    err = hal_epaper_flush(NULL, HAL_EPAPER_FB_SIZE);
    ASSERT_EQ("B1 NULL before init", HAL_EPAPER_ERR_NULL, err);

    /* -----------------------------------------------------------------------
     * B9: NULL check is first even when size is also wrong.
     * ----------------------------------------------------------------------- */
    mock_epaper_reset();
    err = hal_epaper_flush(NULL, 0u);
    ASSERT_EQ("B9 NULL check before size check", HAL_EPAPER_ERR_NULL, err);

    /* -----------------------------------------------------------------------
     * B2: Wrong size (HAL_EPAPER_FB_SIZE - 1).
     * ----------------------------------------------------------------------- */
    mock_epaper_reset();
    hal_epaper_init();
    err = hal_epaper_flush(fb, HAL_EPAPER_FB_SIZE - 1u);
    ASSERT_EQ("B2 size -1", HAL_EPAPER_ERR_SIZE, err);

    /* -----------------------------------------------------------------------
     * B4: Size 0 is also a wrong size.
     * ----------------------------------------------------------------------- */
    mock_epaper_reset();
    hal_epaper_init();
    err = hal_epaper_flush(fb, 0u);
    ASSERT_EQ("B4 size 0", HAL_EPAPER_ERR_SIZE, err);

    /* -----------------------------------------------------------------------
     * B5: UINT32_MAX is also a wrong size.
     * ----------------------------------------------------------------------- */
    mock_epaper_reset();
    hal_epaper_init();
    err = hal_epaper_flush(fb, 0xFFFFFFFFu);
    ASSERT_EQ("B5 UINT32_MAX size", HAL_EPAPER_ERR_SIZE, err);

    /* -----------------------------------------------------------------------
     * B3: Valid pointer + valid size but NOT initialised → ERR_INIT.
     * Guard order: NULL ok, size ok, then init check fires.
     * ----------------------------------------------------------------------- */
    mock_epaper_reset();  /* reset clears initialized flag */
    err = hal_epaper_flush(fb, HAL_EPAPER_FB_SIZE);
    ASSERT_EQ("B3 not initialised", HAL_EPAPER_ERR_INIT, err);

    /* -----------------------------------------------------------------------
     * B6: SPI error injection — flush returns ERR_SPI after injection.
     * ----------------------------------------------------------------------- */
    mock_epaper_reset();
    hal_epaper_init();
    mock_epaper_inject_spi_error();
    err = hal_epaper_flush(fb, HAL_EPAPER_FB_SIZE);
    ASSERT_EQ("B6 SPI error injection", HAL_EPAPER_ERR_SPI, err);

    /* -----------------------------------------------------------------------
     * B8: SPI error is one-shot — subsequent flush succeeds.
     * ----------------------------------------------------------------------- */
    err = hal_epaper_flush(fb, HAL_EPAPER_FB_SIZE);
    ASSERT_EQ("B8 one-shot SPI error clears", HAL_EPAPER_OK, err);

    /* -----------------------------------------------------------------------
     * B7: BUSY timeout injection.
     * ----------------------------------------------------------------------- */
    mock_epaper_reset();
    hal_epaper_init();
    mock_epaper_inject_busy_timeout();
    err = hal_epaper_flush(fb, HAL_EPAPER_FB_SIZE);
    ASSERT_EQ("B7 busy timeout injection", HAL_EPAPER_ERR_BUSY_TIMEOUT, err);

    /* -----------------------------------------------------------------------
     * B10: sleep() and deinit() are safe before init.
     * ----------------------------------------------------------------------- */
    mock_epaper_reset();
    err = hal_epaper_sleep();
    ASSERT_EQ("B10a sleep before init", HAL_EPAPER_OK, err);
    hal_epaper_deinit();   /* must not crash */

    return g_failures;
}

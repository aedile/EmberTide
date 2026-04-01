/**
 * test_p16_hal_epaper_feature.c — Phase 16 feature tests: e-paper HAL.
 *
 * FEATURE tests (Rule 22 Phase B): Happy-path contracts for the e-paper
 * HAL mock. These tests verify the full init→flush→sleep→deinit lifecycle
 * and assert specific byte values to prove correct pixel buffer capture.
 *
 * Tests:
 *   F1 : init succeeds, flush_count starts at 0.
 *   F2 : flush with valid FB_SIZE buffer returns OK.
 *   F3 : flush increments flush_count.
 *   F4 : mock_epaper_get_buffer() returns exact bytes written (pixel fidelity).
 *   F5 : second flush overwrites the captured buffer (not appended).
 *   F6 : sleep() after init returns OK.
 *   F7 : deinit() clears initialized state — subsequent flush returns ERR_INIT.
 *   F8 : reinit after deinit restores flush capability.
 *   F9 : SPI error injection: flush_count does NOT increment on injected error.
 *   F10: busy timeout injection: flush_count does NOT increment on timeout.
 */

#include "hal_epaper.h"
#include "mock_hal_epaper.h"
#include <stdint.h>
#include <string.h>

static int g_failures = 0;

#define ASSERT_EQ(actual, expected, label)               \
    do {                                                 \
        if ((uint32_t)(actual) != (uint32_t)(expected)) { \
            g_failures++;                                \
        }                                                \
    } while (0)

int main(void)
{
    static uint8_t fb[HAL_EPAPER_FB_SIZE];
    hal_epaper_err_t err;
    const uint8_t   *captured;

    /* -----------------------------------------------------------------------
     * F1: init succeeds, flush_count starts at 0.
     * ----------------------------------------------------------------------- */
    mock_epaper_reset();
    err = hal_epaper_init();
    ASSERT_EQ(err, HAL_EPAPER_OK, "F1 init OK");
    ASSERT_EQ(mock_epaper_get_flush_count(), 0u, "F1 flush_count starts 0");

    /* -----------------------------------------------------------------------
     * F2 + F3: flush returns OK and increments count.
     * ----------------------------------------------------------------------- */
    memset(fb, 0xAB, sizeof(fb));
    err = hal_epaper_flush(fb, HAL_EPAPER_FB_SIZE);
    ASSERT_EQ(err, HAL_EPAPER_OK, "F2 flush OK");
    ASSERT_EQ(mock_epaper_get_flush_count(), 1u, "F3 flush_count == 1");

    /* -----------------------------------------------------------------------
     * F4: Pixel fidelity — specific bytes preserved in capture buffer.
     * ----------------------------------------------------------------------- */
    captured = mock_epaper_get_buffer();
    ASSERT_EQ(captured[0],    0xABu, "F4 byte[0] captured");
    ASSERT_EQ(captured[4999], 0xABu, "F4 last byte captured");

    /* -----------------------------------------------------------------------
     * F5: Second flush overwrites buffer.
     * ----------------------------------------------------------------------- */
    memset(fb, 0x55, sizeof(fb));
    fb[100] = 0xCCu;
    hal_epaper_flush(fb, HAL_EPAPER_FB_SIZE);
    captured = mock_epaper_get_buffer();
    ASSERT_EQ(captured[100], 0xCCu, "F5 overwrite byte[100]");
    ASSERT_EQ(captured[0],   0x55u, "F5 overwrite byte[0]");
    ASSERT_EQ(mock_epaper_get_flush_count(), 2u, "F5 flush_count == 2");

    /* -----------------------------------------------------------------------
     * F6: sleep() returns OK.
     * ----------------------------------------------------------------------- */
    err = hal_epaper_sleep();
    ASSERT_EQ(err, HAL_EPAPER_OK, "F6 sleep OK");

    /* -----------------------------------------------------------------------
     * F7: deinit clears initialized state.
     * ----------------------------------------------------------------------- */
    hal_epaper_deinit();
    memset(fb, 0, sizeof(fb));
    err = hal_epaper_flush(fb, HAL_EPAPER_FB_SIZE);
    ASSERT_EQ(err, HAL_EPAPER_ERR_INIT, "F7 flush after deinit ERR_INIT");

    /* -----------------------------------------------------------------------
     * F8: Reinit after deinit restores capability.
     * ----------------------------------------------------------------------- */
    err = hal_epaper_init();
    ASSERT_EQ(err, HAL_EPAPER_OK, "F8 reinit OK");
    ASSERT_EQ(mock_epaper_get_flush_count(), 0u, "F8 flush_count reset on init");
    memset(fb, 0x77, sizeof(fb));
    err = hal_epaper_flush(fb, HAL_EPAPER_FB_SIZE);
    ASSERT_EQ(err, HAL_EPAPER_OK, "F8 flush after reinit OK");

    /* -----------------------------------------------------------------------
     * F9: SPI injection — flush_count does not increment.
     * ----------------------------------------------------------------------- */
    mock_epaper_reset();
    hal_epaper_init();
    mock_epaper_inject_spi_error();
    uint32_t count_before = mock_epaper_get_flush_count();
    hal_epaper_flush(fb, HAL_EPAPER_FB_SIZE);
    ASSERT_EQ(mock_epaper_get_flush_count(), count_before, "F9 SPI err no count");

    /* -----------------------------------------------------------------------
     * F10: Busy timeout — flush_count does not increment.
     * ----------------------------------------------------------------------- */
    mock_epaper_reset();
    hal_epaper_init();
    mock_epaper_inject_busy_timeout();
    count_before = mock_epaper_get_flush_count();
    hal_epaper_flush(fb, HAL_EPAPER_FB_SIZE);
    ASSERT_EQ(mock_epaper_get_flush_count(), count_before, "F10 busy timeout no count");

    return g_failures;
}

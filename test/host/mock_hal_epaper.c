/**
 * mock_hal_epaper.c — Host mock for hal_epaper.
 *
 * Linked by test/host/ targets instead of components/hal/src/hal_epaper.c.
 * Simulates display behaviour entirely in RAM:
 *   - Tracks initialised/uninitialised state.
 *   - Captures the last flushed pixel buffer for assertion.
 *   - Counts total SUCCESSFUL flush calls (errors do not increment count).
 *
 * Phase 16 additions:
 *   - mock_epaper_inject_spi_error(): arm a one-shot SPI error.
 *   - mock_epaper_inject_busy_timeout(): arm a one-shot BUSY timeout error.
 *   Both flags are consumed on the next flush() that passes all guards and
 *   do NOT increment flush_count.
 *
 * Test accessor functions (not declared in hal_epaper.h) expose internal
 * mock state for inspection in test_p12_hal_epaper_*.c and
 * test_p16_hal_epaper_*.c.
 */

#include "hal_epaper.h"
#include <string.h>

/* -------------------------------------------------------------------------
 * Internal mock state — all static, zero-initialised by C runtime.
 * -------------------------------------------------------------------------
 */
static uint8_t  g_mock_epaper_buffer[HAL_EPAPER_FB_SIZE];
static uint8_t  g_mock_epaper_initialized;  /* 0 = uninit, 1 = init */
static uint32_t g_mock_flush_count;

/* One-shot failure injection flags (Phase 16). */
static uint8_t  g_mock_inject_spi_error;    /* 1 = fire ERR_SPI next flush */
static uint8_t  g_mock_inject_busy_timeout; /* 1 = fire ERR_BUSY_TIMEOUT next flush */

/* -------------------------------------------------------------------------
 * Public hal_epaper API — mock implementations.
 * -------------------------------------------------------------------------
 */

hal_epaper_err_t hal_epaper_init(void)
{
    memset(g_mock_epaper_buffer, 0, sizeof(g_mock_epaper_buffer));
    g_mock_epaper_initialized  = 1u;
    g_mock_flush_count         = 0u;
    g_mock_inject_spi_error    = 0u;
    g_mock_inject_busy_timeout = 0u;
    return HAL_EPAPER_OK;
}

hal_epaper_err_t hal_epaper_flush(const uint8_t *fb_pixels, uint32_t size)
{
    /* Guard order per spec: NULL check first, size check second, init check third. */
    if (!fb_pixels) {
        return HAL_EPAPER_ERR_NULL;
    }
    if (size != HAL_EPAPER_FB_SIZE) {
        return HAL_EPAPER_ERR_SIZE;
    }
    if (!g_mock_epaper_initialized) {
        return HAL_EPAPER_ERR_INIT;
    }

    /* Phase 16: One-shot injected failures fire after all guards pass.
     * Busy timeout check before SPI (hardware order: wait BUSY then SPI). */
    if (g_mock_inject_busy_timeout) {
        g_mock_inject_busy_timeout = 0u;
        return HAL_EPAPER_ERR_BUSY_TIMEOUT;
    }
    if (g_mock_inject_spi_error) {
        g_mock_inject_spi_error = 0u;
        return HAL_EPAPER_ERR_SPI;
    }

    memcpy(g_mock_epaper_buffer, fb_pixels, HAL_EPAPER_FB_SIZE);
    g_mock_flush_count++;
    return HAL_EPAPER_OK;
}

hal_epaper_err_t hal_epaper_sleep(void)
{
    return HAL_EPAPER_OK;
}

void hal_epaper_deinit(void)
{
    g_mock_epaper_initialized  = 0u;
    g_mock_inject_spi_error    = 0u;
    g_mock_inject_busy_timeout = 0u;
    /* Buffer and flush_count intentionally preserved for post-deinit inspection. */
}

/* -------------------------------------------------------------------------
 * Test accessors — host-only, not declared in hal_epaper.h.
 * -------------------------------------------------------------------------
 */

/** Returns a pointer to the internal mock display buffer (read-only). */
const uint8_t *mock_epaper_get_buffer(void)
{
    return g_mock_epaper_buffer;
}

/** Returns the cumulative number of successful hal_epaper_flush() calls. */
uint32_t mock_epaper_get_flush_count(void)
{
    return g_mock_flush_count;
}

/**
 * mock_epaper_reset — Reset all mock state to power-on defaults.
 *
 * Zeroes the capture buffer, clears initialized flag, resets flush_count
 * to 0, and clears any pending injection flags.
 * Call at the start of each test main() to ensure a clean slate regardless
 * of prior static initialisation order.
 */
void mock_epaper_reset(void)
{
    memset(g_mock_epaper_buffer, 0, sizeof(g_mock_epaper_buffer));
    g_mock_epaper_initialized  = 0u;
    g_mock_flush_count         = 0u;
    g_mock_inject_spi_error    = 0u;
    g_mock_inject_busy_timeout = 0u;
}

/**
 * mock_epaper_inject_spi_error — Arm a one-shot SPI error.
 *
 * The next hal_epaper_flush() call that passes all pointer/size/init guards
 * will return HAL_EPAPER_ERR_SPI. The flag is cleared after one firing.
 * flush_count is NOT incremented on an injected error.
 */
void mock_epaper_inject_spi_error(void)
{
    g_mock_inject_spi_error = 1u;
}

/**
 * mock_epaper_inject_busy_timeout — Arm a one-shot BUSY timeout.
 *
 * The next hal_epaper_flush() call that passes all pointer/size/init guards
 * will return HAL_EPAPER_ERR_BUSY_TIMEOUT. The flag is cleared after one
 * firing. flush_count is NOT incremented on an injected error.
 */
void mock_epaper_inject_busy_timeout(void)
{
    g_mock_inject_busy_timeout = 1u;
}

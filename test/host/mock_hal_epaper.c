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
 * Phase 19.5 additions:
 *   - hal_epaper_flush_partial(): partial-refresh mock. Tracks s_partial_flush_count.
 *     Every EPD_FULL_REFRESH_INTERVAL calls triggers a full refresh (g_last_flush_was_full=1).
 *   - mock_epaper_get_partial_flush_count(): accessor for partial flush count.
 *   - mock_epaper_set_partial_flush_count(): setter to inject arbitrary counter value.
 *   - mock_epaper_get_last_flush_was_full(): was the most recent partial call actually full?
 *   - hal_epaper_sleep() now clears g_mock_epaper_initialized (Phase 19.5 spec).
 *
 * Test accessor functions (not declared in hal_epaper.h) expose internal
 * mock state for inspection in test files.
 */

#include "hal_epaper.h"
#include "mock_hal_epaper.h"
#include <string.h>

/* Compile-time guard: EPD_FULL_REFRESH_INTERVAL must be > 0. */
_Static_assert(EPD_FULL_REFRESH_INTERVAL > 0u,
               "EPD_FULL_REFRESH_INTERVAL must be > 0 — would cause division-by-zero");

/* -------------------------------------------------------------------------
 * Internal mock state — all static, zero-initialised by C runtime.
 * -------------------------------------------------------------------------
 */
static uint8_t  g_mock_epaper_buffer[HAL_EPAPER_FB_SIZE];
static uint8_t  g_mock_epaper_initialized;  /* 0 = uninit, 1 = init */
static uint32_t g_mock_flush_count;

/* Phase 19.5: partial flush state. */
static uint32_t g_mock_partial_flush_count;   /* total successful flush_partial calls */
static uint8_t  g_mock_last_flush_was_full;   /* 1 = last partial triggered full refresh */

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
    g_mock_epaper_initialized    = 1u;
    g_mock_flush_count           = 0u;
    g_mock_partial_flush_count   = 0u;
    g_mock_last_flush_was_full   = 0u;
    g_mock_inject_spi_error      = 0u;
    g_mock_inject_busy_timeout   = 0u;
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

/**
 * hal_epaper_flush_partial — Partial-refresh mock implementation.
 *
 * Guard order: NULL → size → init (matches flush() spec).
 * Every EPD_FULL_REFRESH_INTERVAL successful calls, a full refresh is
 * simulated (g_mock_last_flush_was_full = 1), otherwise partial (= 0).
 * Counter incremented ONLY on success.
 */
hal_epaper_err_t hal_epaper_flush_partial(const uint8_t *fb_pixels, uint32_t size)
{
    /* Guard order: NULL → size → init. */
    if (!fb_pixels) {
        return HAL_EPAPER_ERR_NULL;
    }
    if (size != HAL_EPAPER_FB_SIZE) {
        return HAL_EPAPER_ERR_SIZE;
    }
    if (!g_mock_epaper_initialized) {
        return HAL_EPAPER_ERR_INIT;
    }

    /* Capture the buffer. */
    memcpy(g_mock_epaper_buffer, fb_pixels, HAL_EPAPER_FB_SIZE);

    /* Increment counter BEFORE the modulo check so counter=1 on first call
     * and the full-refresh fires at counter == EPD_FULL_REFRESH_INTERVAL. */
    g_mock_partial_flush_count++;

    /* Every EPD_FULL_REFRESH_INTERVAL flushes, perform a simulated full refresh. */
    if (g_mock_partial_flush_count % EPD_FULL_REFRESH_INTERVAL == 0u) {
        g_mock_last_flush_was_full = 1u;
    } else {
        g_mock_last_flush_was_full = 0u;
    }

    return HAL_EPAPER_OK;
}

/**
 * hal_epaper_sleep — Phase 19.5: clears s_initialized so subsequent
 * flush/flush_partial calls return ERR_INIT until re-init.
 */
hal_epaper_err_t hal_epaper_sleep(void)
{
    g_mock_epaper_initialized = 0u;
    return HAL_EPAPER_OK;
}

void hal_epaper_deinit(void)
{
    g_mock_epaper_initialized    = 0u;
    g_mock_inject_spi_error      = 0u;
    g_mock_inject_busy_timeout   = 0u;
    /* Buffer and counters intentionally preserved for post-deinit inspection. */
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
 * Zeroes the capture buffer, clears initialized flag, resets all counters
 * to 0, and clears any pending injection flags.
 * Call at the start of each test main() to ensure a clean slate regardless
 * of prior static initialisation order.
 */
void mock_epaper_reset(void)
{
    memset(g_mock_epaper_buffer, 0, sizeof(g_mock_epaper_buffer));
    g_mock_epaper_initialized    = 0u;
    g_mock_flush_count           = 0u;
    g_mock_partial_flush_count   = 0u;
    g_mock_last_flush_was_full   = 0u;
    g_mock_inject_spi_error      = 0u;
    g_mock_inject_busy_timeout   = 0u;
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

/* -------------------------------------------------------------------------
 * Phase-19.5 test accessors.
 * -------------------------------------------------------------------------
 */

/** Returns cumulative successful hal_epaper_flush_partial() call count. */
uint32_t mock_epaper_get_partial_flush_count(void)
{
    return g_mock_partial_flush_count;
}

/**
 * mock_epaper_set_partial_flush_count — Inject an arbitrary counter value.
 *
 * Used by bound tests to simulate near-overflow conditions without calling
 * flush_partial UINT32_MAX times.
 */
void mock_epaper_set_partial_flush_count(uint32_t value)
{
    g_mock_partial_flush_count = value;
}

/**
 * mock_epaper_get_last_flush_was_full — Was the last flush_partial a full refresh?
 *
 * Returns 1 if the most recent hal_epaper_flush_partial() triggered a full
 * refresh (at EPD_FULL_REFRESH_INTERVAL). Returns 0 for true partial flushes.
 */
uint8_t mock_epaper_get_last_flush_was_full(void)
{
    return g_mock_last_flush_was_full;
}

/**
 * mock_hal_sleep.c — Host mock for hal_sleep deep-sleep driver.
 *
 * Linked by test/host/ targets instead of components/hal/src/hal_sleep.c.
 * Simulates deep-sleep requests entirely in RAM — the mock records that
 * hal_sleep_enter() was called without actually suspending the process.
 *   - Tracks initialised/uninitialised state.
 *   - Counts total hal_sleep_enter() requests.
 *   - Captures the timeout_sec from the most recent enter call.
 *   - mock_sleep_reset() clears all state to power-on defaults.
 *
 * On the real target hal_sleep_enter() calls esp_deep_sleep_start() and
 * never returns. The mock returns HAL_SLEEP_OK so that host tests can
 * exercise the full init/enter/deinit cycle without process termination.
 */

#include "hal_sleep.h"

/* -------------------------------------------------------------------------
 * Internal mock state.
 * -------------------------------------------------------------------------
 */
static uint8_t  s_mock_initialized;
static uint32_t s_mock_request_count;
static uint32_t s_mock_last_timeout_sec;

/* -------------------------------------------------------------------------
 * Public hal_sleep API — mock implementations.
 * -------------------------------------------------------------------------
 */

hal_sleep_err_t hal_sleep_init(void)
{
    s_mock_initialized = 1u;
    return HAL_SLEEP_OK;
}

hal_sleep_err_t hal_sleep_enter(uint32_t timeout_sec)
{
    if (!s_mock_initialized) {
        return HAL_SLEEP_ERR_INIT;
    }
    s_mock_last_timeout_sec = timeout_sec;
    s_mock_request_count++;
    /*
     * On the target: esp_deep_sleep_start() — does not return.
     * Mock: returns OK so the test can continue executing.
     */
    return HAL_SLEEP_OK;
}

void hal_sleep_deinit(void)
{
    s_mock_initialized = 0u;
    /* request_count and last_timeout preserved for post-deinit inspection. */
}

/* -------------------------------------------------------------------------
 * Test accessor functions — host-only, not declared in hal_sleep.h.
 * -------------------------------------------------------------------------
 */

/** Returns the cumulative count of hal_sleep_enter() calls since last reset. */
uint32_t mock_sleep_get_request_count(void)
{
    return s_mock_request_count;
}

/** Returns the timeout_sec from the most recent hal_sleep_enter() call. */
uint32_t mock_sleep_get_last_timeout_sec(void)
{
    return s_mock_last_timeout_sec;
}

/**
 * mock_sleep_reset — Reset all mock state to power-on defaults.
 *
 * Clears initialized flag, request count, and last timeout.
 * Call at the start of each test main() for a clean slate.
 */
void mock_sleep_reset(void)
{
    s_mock_initialized      = 0u;
    s_mock_request_count    = 0u;
    s_mock_last_timeout_sec = 0u;
}

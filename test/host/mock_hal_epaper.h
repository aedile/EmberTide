/**
 * mock_hal_epaper.h — Public interface for the hal_epaper host mock.
 *
 * Test files #include this header to access mock state inspectors and
 * failure injection APIs. Only compiled in the host test environment —
 * never on the target.
 *
 * Phase 16 additions:
 *   mock_epaper_inject_spi_error()     — Next flush returns ERR_SPI (one-shot).
 *   mock_epaper_inject_busy_timeout()  — Next flush returns ERR_BUSY_TIMEOUT (one-shot).
 *
 * Phase 19.5 additions:
 *   mock_epaper_get_partial_flush_count()     — Cumulative successful flush_partial calls.
 *   mock_epaper_set_partial_flush_count()     — Force counter to a specific value (for
 *                                               uint32_t wrap-around tests).
 *   mock_epaper_get_last_flush_was_full()     — 1 if the last flush_partial triggered a
 *                                               full refresh, 0 if it was a true partial.
 */

#ifndef FIESTAQUEST_MOCK_HAL_EPAPER_H
#define FIESTAQUEST_MOCK_HAL_EPAPER_H

#include <stdint.h>

/** Reset all mock state to power-on defaults. */
void mock_epaper_reset(void);

/** Returns pointer to the internal mock display buffer (read-only). */
const uint8_t *mock_epaper_get_buffer(void);

/** Returns the cumulative number of successful hal_epaper_flush() calls. */
uint32_t mock_epaper_get_flush_count(void);

/**
 * mock_epaper_inject_spi_error — Arm a one-shot SPI error.
 *
 * The next call to hal_epaper_flush() (after guards pass) will return
 * HAL_EPAPER_ERR_SPI instead of OK. The flag is cleared after one use.
 * Subsequent flushes succeed normally.
 */
void mock_epaper_inject_spi_error(void);

/**
 * mock_epaper_inject_busy_timeout — Arm a one-shot BUSY timeout.
 *
 * The next call to hal_epaper_flush() (after guards pass) will return
 * HAL_EPAPER_ERR_BUSY_TIMEOUT instead of OK. The flag is cleared after
 * one use. Subsequent flushes succeed normally.
 */
void mock_epaper_inject_busy_timeout(void);

/* ---------------------------------------------------------------------------
 * Phase-19.5 additions
 * ---------------------------------------------------------------------------*/

/**
 * mock_epaper_get_partial_flush_count — Number of successful flush_partial calls.
 *
 * Only incremented on HAL_EPAPER_OK returns from hal_epaper_flush_partial().
 * Resets to 0 on mock_epaper_reset() or hal_epaper_init().
 */
uint32_t mock_epaper_get_partial_flush_count(void);

/**
 * mock_epaper_set_partial_flush_count — Force the partial flush counter to a
 * specific value.
 *
 * Used by bound tests to probe behaviour at UINT32_MAX - 1 without calling
 * flush_partial UINT32_MAX times.
 */
void mock_epaper_set_partial_flush_count(uint32_t value);

/**
 * mock_epaper_get_last_flush_was_full — Indicates whether the most recent
 * hal_epaper_flush_partial() internally performed a full refresh.
 *
 * Returns 1 if the last partial call triggered a full refresh (every
 * EPD_FULL_REFRESH_INTERVAL flushes), 0 if it was a true partial.
 * Returns 0 after mock_epaper_reset().
 */
uint8_t mock_epaper_get_last_flush_was_full(void);

#endif /* FIESTAQUEST_MOCK_HAL_EPAPER_H */

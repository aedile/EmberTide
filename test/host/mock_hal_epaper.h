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

#endif /* FIESTAQUEST_MOCK_HAL_EPAPER_H */

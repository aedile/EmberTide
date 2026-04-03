/**
 * hal_epaper.h — Hardware Abstraction Layer: E-Paper Display
 *
 * Target: Waveshare 1.54" V2 200x200 1-bit E-paper over SPI.
 * ESP32-S3-PICO-1-N8R8 via ESP-IDF v5.x.
 *
 * Architecture constraint: This header is the BOTTOM layer.
 * It MUST NOT be included by game/, presentation/, or connectivity/.
 * Upper layers reach display output exclusively through presentation/
 * screen_mgr.h (device-only) which calls hal_epaper internally.
 *
 * No ESP-IDF types appear in this public API — callers pass a raw
 * packed pixel buffer (MSB-first, 1-bit per pixel, 5000 bytes for
 * 200x200 display).  This keeps the interface host-compilable so that
 * mock implementations can be linked in test/host/.
 *
 * Phase-19.5 additions:
 *   hal_epaper_flush_partial() — partial-refresh variant using partial LUT.
 *   EPD_FULL_REFRESH_INTERVAL  — force a full refresh every N partial flushes.
 */

#ifndef FIESTAQUEST_HAL_EPAPER_H
#define FIESTAQUEST_HAL_EPAPER_H

#include <stdint.h>

/** Size of the packed 1-bit framebuffer: 200 * 200 / 8 = 5000 bytes. */
#define HAL_EPAPER_FB_SIZE  5000u

/**
 * EPD_FULL_REFRESH_INTERVAL — how many flush_partial calls trigger a full
 * refresh to clear accumulated ghosting.  Default: 10.
 *
 * Must be > 0.  Enforced by _Static_assert in hal_epaper.c.
 */
#define EPD_FULL_REFRESH_INTERVAL  10u

/**
 * hal_epaper_err_t — Return codes for all hal_epaper operations.
 *
 * HAL_EPAPER_OK            — Operation completed successfully.
 * HAL_EPAPER_ERR_INIT      — Driver not initialised (call hal_epaper_init first).
 * HAL_EPAPER_ERR_BUSY_TIMEOUT — Hardware BUSY pin held high beyond timeout.
 * HAL_EPAPER_ERR_SPI       — SPI bus transaction failed.
 * HAL_EPAPER_ERR_NULL      — Caller passed a NULL pointer.
 * HAL_EPAPER_ERR_SIZE      — size does not equal HAL_EPAPER_FB_SIZE exactly.
 */
typedef enum {
    HAL_EPAPER_OK              = 0,
    HAL_EPAPER_ERR_INIT        = 1,
    HAL_EPAPER_ERR_BUSY_TIMEOUT = 2,
    HAL_EPAPER_ERR_SPI         = 3,
    HAL_EPAPER_ERR_NULL        = 4,
    HAL_EPAPER_ERR_SIZE        = 5
} hal_epaper_err_t;

/**
 * hal_epaper_init — Initialise the SPI bus and display controller.
 *
 * Must be called once before any other hal_epaper function.
 * Safe to call multiple times (idempotent reinit).
 *
 * Phase-19.5: performs a boot-time full clear (white→black→white) to
 * establish a clean baseline regardless of prior screen state.
 * The flush counter (s_flush_count) is reset to 0 AFTER the boot clear
 * completes, so the boot clear does not count toward the partial interval.
 *
 * @return HAL_EPAPER_OK on success, HAL_EPAPER_ERR_SPI on bus failure.
 */
hal_epaper_err_t hal_epaper_init(void);

/**
 * hal_epaper_flush — Push a full framebuffer to the display.
 *
 * Transmits HAL_EPAPER_FB_SIZE bytes via SPI using the RAM write
 * command (0x24) and triggers a display update sequence.
 *
 * Guard order (checked in this sequence):
 *   1. NULL pointer check   → HAL_EPAPER_ERR_NULL   (fb_pixels is NULL)
 *   2. Size check           → HAL_EPAPER_ERR_SIZE   (size != HAL_EPAPER_FB_SIZE)
 *   3. Init check           → HAL_EPAPER_ERR_INIT   (driver not initialised)
 *
 * @param fb_pixels  Pointer to a packed 1-bit pixel buffer.
 *                   Must not be NULL.  MSB of byte 0 = pixel (0,0).
 * @param size       Must equal HAL_EPAPER_FB_SIZE exactly.
 * @return HAL_EPAPER_OK           on success.
 *         HAL_EPAPER_ERR_NULL     if fb_pixels is NULL.
 *         HAL_EPAPER_ERR_SIZE     if size != HAL_EPAPER_FB_SIZE.
 *         HAL_EPAPER_ERR_INIT     if hal_epaper_init() was not called.
 *         HAL_EPAPER_ERR_BUSY_TIMEOUT if BUSY pin did not clear within timeout.
 *         HAL_EPAPER_ERR_SPI      on SPI transaction failure.
 */
hal_epaper_err_t hal_epaper_flush(const uint8_t *fb_pixels, uint32_t size);

/**
 * hal_epaper_flush_partial — Push framebuffer using partial-refresh waveform.
 *
 * Uses a faster partial-refresh LUT (k_wf_partial_1in54) that avoids the
 * full black-white-black flicker cycle. Suitable for animation updates.
 *
 * Every EPD_FULL_REFRESH_INTERVAL calls, a full refresh is performed instead
 * to clear accumulated ghosting. The caller does not need to track this — the
 * HAL handles the interval internally via s_flush_count.
 *
 * Guard order matches hal_epaper_flush():
 *   1. NULL pointer check   → HAL_EPAPER_ERR_NULL
 *   2. Size check           → HAL_EPAPER_ERR_SIZE
 *   3. Init check           → HAL_EPAPER_ERR_INIT
 *
 * s_flush_count is incremented ONLY on success.
 *
 * @param fb_pixels  Pointer to packed 1-bit pixel buffer. Must not be NULL.
 * @param size       Must equal HAL_EPAPER_FB_SIZE exactly.
 * @return HAL_EPAPER_OK on success, or error code on failure.
 */
hal_epaper_err_t hal_epaper_flush_partial(const uint8_t *fb_pixels, uint32_t size);

/**
 * hal_epaper_sleep — Put the display controller into deep-sleep mode.
 *
 * Reduces current draw to ~5µA. Wake requires a full hal_epaper_init().
 *
 * Phase-19.5: clears s_initialized so subsequent flush/flush_partial calls
 * return HAL_EPAPER_ERR_INIT until hal_epaper_init() is called again.
 *
 * @return HAL_EPAPER_OK on success.
 */
hal_epaper_err_t hal_epaper_sleep(void);

/**
 * hal_epaper_deinit — Release SPI bus resources and mark driver uninitialised.
 *
 * Safe to call without a preceding init (no-op in that case).
 */
void hal_epaper_deinit(void);

#endif /* FIESTAQUEST_HAL_EPAPER_H */

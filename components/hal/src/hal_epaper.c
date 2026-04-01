/**
 * hal_epaper.c — E-Paper Display HAL: Target Stub
 *
 * This file compiles ONLY with idf.py build (target = ESP32-S3).
 * It requires ESP-IDF spi_master.h and driver/gpio.h which are NOT
 * available on the host.  The host test suite links mock_hal_epaper.c
 * from test/host/ instead of this file.
 *
 * Phase 12 status: STUB — all functions return OK.
 * Real SPI command sequences (Waveshare 1.54" V2 init LUT, 0x24 RAM
 * write, 0x22 update control, BUSY pin polling with esp_timer timeout)
 * are deferred until hardware bring-up phase.
 *
 * Pin assignments: see components/hal/include/hal_pins.h (Phase 12+).
 */

#include "hal_epaper.h"

/* -------------------------------------------------------------------------
 * Phase 12 target stub — returns OK for all operations.
 * Real implementation:
 *   hal_epaper_init  : spi_bus_initialize() + spi_device_add_driver_obj()
 *                      + GPIO config for CS/DC/RST/BUSY + full init LUT send.
 *   hal_epaper_flush : wait BUSY, send 0x24 + 5000 bytes DMA, send 0x22 0xF7,
 *                      send 0x20 (Master Activation), wait BUSY with timeout.
 *   hal_epaper_sleep : send 0x10 0x01 (deep sleep mode 1), GPIO low.
 *   hal_epaper_deinit: spi_device_remove_driver_obj() + spi_bus_free().
 * -------------------------------------------------------------------------
 */

hal_epaper_err_t hal_epaper_init(void)
{
    return HAL_EPAPER_OK;
}

hal_epaper_err_t hal_epaper_flush(const uint8_t *fb_pixels, uint32_t size)
{
    /* Guard order: NULL check first, then size check. */
    if (!fb_pixels) {
        return HAL_EPAPER_ERR_NULL;
    }
    if (size != HAL_EPAPER_FB_SIZE) {
        return HAL_EPAPER_ERR_SIZE;
    }
    return HAL_EPAPER_OK;
}

hal_epaper_err_t hal_epaper_sleep(void)
{
    return HAL_EPAPER_OK;
}

void hal_epaper_deinit(void)
{
    /* No resources to release in stub. */
}

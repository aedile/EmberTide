/**
 * hal_epaper.c — E-Paper Display HAL: Waveshare 1.54" V2 (SSD1681)
 *
 * Target: ESP32-S3 (Waveshare ESP32-S3-ePaper-1.54 V2).
 * Compiled ONLY with idf.py build (target = ESP32-S3).
 * The host test suite links mock_hal_epaper.c instead of this file.
 *
 * Pin assignments (from example/Example/ESP-IDF/V2/11_FactoryProgram/main/user_config.h):
 *   MOSI = GPIO13, CLK = GPIO12, CS = GPIO11, DC = GPIO10
 *   RST  = GPIO9,  BUSY = GPIO8, PWR = GPIO6
 *
 * SPI: SPI2_HOST, 40 MHz, Mode 0, DMA auto.
 * Init sequence ported from epaper_driver_bsp.cpp (C++ → C).
 * Waveform LUT: WF_Full_1IN54 (159 bytes, copied verbatim from example driver).
 *
 * Architecture constraint: This file is the BOTTOM layer.
 * It MUST NOT be included by game/, presentation/, or connectivity/.
 */

#include "hal_epaper.h"

#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "hal_epaper";

/* -------------------------------------------------------------------------
 * Pin definitions (V2 hardware — matches user_config.h).
 * -------------------------------------------------------------------------
 */
#define EPD_MOSI_PIN   GPIO_NUM_13
#define EPD_CLK_PIN    GPIO_NUM_12
#define EPD_CS_PIN     GPIO_NUM_11
#define EPD_DC_PIN     GPIO_NUM_10
#define EPD_RST_PIN    GPIO_NUM_9
#define EPD_BUSY_PIN   GPIO_NUM_8
#define EPD_PWR_PIN    GPIO_NUM_6

/* -------------------------------------------------------------------------
 * Timing constants.
 * -------------------------------------------------------------------------
 */
#define EPD_SPI_CLK_HZ       (40 * 1000 * 1000)   /* 40 MHz                */
#define EPD_BUSY_TIMEOUT_US  (3000 * 1000)         /* 3 s in microseconds   */
#define EPD_RESET_DELAY_MS   50u
#define EPD_RST_LOW_MS       20u

/* -------------------------------------------------------------------------
 * Waveform LUT — WF_Full_1IN54 (159 bytes).
 * Source: epaper_driver_bsp.cpp from Waveshare example driver.
 * -------------------------------------------------------------------------
 */
static const uint8_t k_wf_full_1in54[159] = {
    0x80, 0x48, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x40, 0x48, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x80, 0x48, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x40, 0x48, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x08, 0x01, 0x00, 0x08, 0x01, 0x00, 0x02,
    0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x00, 0x00, 0x00,
    0x22, 0x17, 0x41, 0x00, 0x32, 0x20
};

/* -------------------------------------------------------------------------
 * Module state.
 * -------------------------------------------------------------------------
 */
static spi_device_handle_t s_spi;
static uint8_t             s_initialized;  /* 0 = uninit, 1 = init */

/* -------------------------------------------------------------------------
 * Low-level GPIO helpers (inline-style, static).
 * -------------------------------------------------------------------------
 */
static inline void epd_dc_high(void)  { gpio_set_level(EPD_DC_PIN,  1); }
static inline void epd_dc_low(void)   { gpio_set_level(EPD_DC_PIN,  0); }
static inline void epd_cs_high(void)  { gpio_set_level(EPD_CS_PIN,  1); }
static inline void epd_cs_low(void)   { gpio_set_level(EPD_CS_PIN,  0); }
static inline void epd_rst_high(void) { gpio_set_level(EPD_RST_PIN, 1); }
static inline void epd_rst_low(void)  { gpio_set_level(EPD_RST_PIN, 0); }

/* -------------------------------------------------------------------------
 * epd_spi_send_byte — Transmit one byte over SPI (polling).
 * -------------------------------------------------------------------------
 */
static hal_epaper_err_t epd_spi_send_byte(uint8_t byte)
{
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length    = 8u;
    t.tx_buffer = &byte;
    esp_err_t ret = spi_device_polling_transmit(s_spi, &t);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI byte tx failed: %d", ret);
        return HAL_EPAPER_ERR_SPI;
    }
    return HAL_EPAPER_OK;
}

/* -------------------------------------------------------------------------
 * epd_spi_send_buf — Transmit len bytes over SPI (polling).
 * DC is assumed already set by the caller.
 * -------------------------------------------------------------------------
 */
static hal_epaper_err_t epd_spi_send_buf(const uint8_t *buf, uint32_t len)
{
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length    = 8u * len;
    t.tx_buffer = buf;
    esp_err_t ret = spi_device_polling_transmit(s_spi, &t);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI buf tx failed: %d", ret);
        return HAL_EPAPER_ERR_SPI;
    }
    return HAL_EPAPER_OK;
}

/* -------------------------------------------------------------------------
 * epd_cmd — Send one command byte (DC=0).
 * -------------------------------------------------------------------------
 */
static hal_epaper_err_t epd_cmd(uint8_t cmd)
{
    epd_dc_low();
    epd_cs_low();
    hal_epaper_err_t err = epd_spi_send_byte(cmd);
    epd_cs_high();
    return err;
}

/* -------------------------------------------------------------------------
 * epd_data_byte — Send one data byte (DC=1).
 * -------------------------------------------------------------------------
 */
static hal_epaper_err_t epd_data_byte(uint8_t data)
{
    epd_dc_high();
    epd_cs_low();
    hal_epaper_err_t err = epd_spi_send_byte(data);
    epd_cs_high();
    return err;
}

/* -------------------------------------------------------------------------
 * epd_data_buf — Send len data bytes (DC=1).
 * -------------------------------------------------------------------------
 */
static hal_epaper_err_t epd_data_buf(const uint8_t *buf, uint32_t len)
{
    epd_dc_high();
    epd_cs_low();
    hal_epaper_err_t err = epd_spi_send_buf(buf, len);
    epd_cs_high();
    return err;
}

/* -------------------------------------------------------------------------
 * epd_wait_busy — Poll BUSY pin until low (display idle) with timeout.
 *
 * BUSY pin: HIGH = busy, LOW = idle.
 * Timeout: EPD_BUSY_TIMEOUT_US (3 seconds).
 * -------------------------------------------------------------------------
 */
static hal_epaper_err_t epd_wait_busy(void)
{
    int64_t deadline = esp_timer_get_time() + (int64_t)EPD_BUSY_TIMEOUT_US;
    while (gpio_get_level(EPD_BUSY_PIN) == 1) {
        if (esp_timer_get_time() >= deadline) {
            ESP_LOGE(TAG, "BUSY pin timeout");
            return HAL_EPAPER_ERR_BUSY_TIMEOUT;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    return HAL_EPAPER_OK;
}

/* -------------------------------------------------------------------------
 * epd_hardware_reset — Toggle RST pin for display controller reset.
 * -------------------------------------------------------------------------
 */
static void epd_hardware_reset(void)
{
    epd_rst_high();
    vTaskDelay(pdMS_TO_TICKS(EPD_RESET_DELAY_MS));
    epd_rst_low();
    vTaskDelay(pdMS_TO_TICKS(EPD_RST_LOW_MS));
    epd_rst_high();
    vTaskDelay(pdMS_TO_TICKS(EPD_RESET_DELAY_MS));
}

/* -------------------------------------------------------------------------
 * epd_set_windows — Set RAM X/Y address window.
 * -------------------------------------------------------------------------
 */
static hal_epaper_err_t epd_set_windows(uint16_t x_start, uint16_t y_start,
                                         uint16_t x_end,   uint16_t y_end)
{
    hal_epaper_err_t err;

    err = epd_cmd(0x44u);  /* SET_RAM_X_ADDRESS_START_END_POSITION */
    if (err != HAL_EPAPER_OK) return err;
    err = epd_data_byte((uint8_t)((x_start >> 3) & 0xFFu));
    if (err != HAL_EPAPER_OK) return err;
    err = epd_data_byte((uint8_t)((x_end   >> 3) & 0xFFu));
    if (err != HAL_EPAPER_OK) return err;

    err = epd_cmd(0x45u);  /* SET_RAM_Y_ADDRESS_START_END_POSITION */
    if (err != HAL_EPAPER_OK) return err;
    err = epd_data_byte((uint8_t)( y_start        & 0xFFu));
    if (err != HAL_EPAPER_OK) return err;
    err = epd_data_byte((uint8_t)((y_start >> 8)  & 0xFFu));
    if (err != HAL_EPAPER_OK) return err;
    err = epd_data_byte((uint8_t)( y_end          & 0xFFu));
    if (err != HAL_EPAPER_OK) return err;
    err = epd_data_byte((uint8_t)((y_end   >> 8)  & 0xFFu));
    return err;
}

/* -------------------------------------------------------------------------
 * epd_set_cursor — Set RAM X/Y address counter.
 * -------------------------------------------------------------------------
 */
static hal_epaper_err_t epd_set_cursor(uint16_t x, uint16_t y)
{
    hal_epaper_err_t err;

    err = epd_cmd(0x4Eu);  /* SET_RAM_X_ADDRESS_COUNTER */
    if (err != HAL_EPAPER_OK) return err;
    err = epd_data_byte((uint8_t)(x & 0xFFu));
    if (err != HAL_EPAPER_OK) return err;

    err = epd_cmd(0x4Fu);  /* SET_RAM_Y_ADDRESS_COUNTER */
    if (err != HAL_EPAPER_OK) return err;
    err = epd_data_byte((uint8_t)( y       & 0xFFu));
    if (err != HAL_EPAPER_OK) return err;
    err = epd_data_byte((uint8_t)((y >> 8) & 0xFFu));
    return err;
}

/* -------------------------------------------------------------------------
 * epd_set_lut — Load waveform LUT into display controller.
 * Uses same sequence as EPD_SetLut() in example driver.
 * -------------------------------------------------------------------------
 */
static hal_epaper_err_t epd_set_lut(const uint8_t *lut)
{
    hal_epaper_err_t err;

    err = epd_cmd(0x32u);
    if (err != HAL_EPAPER_OK) return err;
    err = epd_data_buf(lut, 153u);
    if (err != HAL_EPAPER_OK) return err;

    err = epd_wait_busy();
    if (err != HAL_EPAPER_OK) return err;

    err = epd_cmd(0x3Fu);
    if (err != HAL_EPAPER_OK) return err;
    err = epd_data_byte(lut[153]);
    if (err != HAL_EPAPER_OK) return err;

    err = epd_cmd(0x03u);
    if (err != HAL_EPAPER_OK) return err;
    err = epd_data_byte(lut[154]);
    if (err != HAL_EPAPER_OK) return err;

    err = epd_cmd(0x04u);
    if (err != HAL_EPAPER_OK) return err;
    err = epd_data_byte(lut[155]);
    if (err != HAL_EPAPER_OK) return err;
    err = epd_data_byte(lut[156]);
    if (err != HAL_EPAPER_OK) return err;
    err = epd_data_byte(lut[157]);
    if (err != HAL_EPAPER_OK) return err;

    err = epd_cmd(0x2Cu);
    if (err != HAL_EPAPER_OK) return err;
    err = epd_data_byte(lut[158]);
    return err;
}

/* -------------------------------------------------------------------------
 * Public API implementation.
 * -------------------------------------------------------------------------
 */

hal_epaper_err_t hal_epaper_init(void)
{
    esp_err_t ret;

    /* Power on the e-paper rail (active-low power enable: LOW = ON). */
    gpio_set_direction(EPD_PWR_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(EPD_PWR_PIN, 0);

    /* Configure DC, CS, RST as outputs; BUSY as input. */
    gpio_config_t io_conf = {};
    io_conf.intr_type    = GPIO_INTR_DISABLE;
    io_conf.mode         = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << EPD_DC_PIN)  |
                           (1ULL << EPD_CS_PIN)  |
                           (1ULL << EPD_RST_PIN);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en   = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    io_conf.mode         = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << EPD_BUSY_PIN);
    gpio_config(&io_conf);

    /* Initialise SPI bus (SPI2_HOST). */
    spi_bus_config_t bus_cfg = {};
    bus_cfg.mosi_io_num     = EPD_MOSI_PIN;
    bus_cfg.miso_io_num     = -1;
    bus_cfg.sclk_io_num     = EPD_CLK_PIN;
    bus_cfg.quadwp_io_num   = -1;
    bus_cfg.quadhd_io_num   = -1;
    bus_cfg.max_transfer_sz = HAL_EPAPER_FB_SIZE;

    ret = spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        /* ESP_ERR_INVALID_STATE means the bus was already initialized — OK. */
        ESP_LOGE(TAG, "spi_bus_initialize failed: %d", ret);
        return HAL_EPAPER_ERR_SPI;
    }

    /* Add the e-paper device. CS managed manually (spics_io_num = -1). */
    spi_device_interface_config_t dev_cfg = {};
    dev_cfg.clock_speed_hz = EPD_SPI_CLK_HZ;
    dev_cfg.mode           = 0;
    dev_cfg.spics_io_num   = -1;   /* CS toggled manually */
    dev_cfg.queue_size     = 7;

    ret = spi_bus_add_device(SPI2_HOST, &dev_cfg, &s_spi);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_add_device failed: %d", ret);
        return HAL_EPAPER_ERR_SPI;
    }

    /* Hardware reset sequence. */
    epd_hardware_reset();

    /* Wait for display to come out of reset. */
    hal_epaper_err_t err = epd_wait_busy();
    if (err != HAL_EPAPER_OK) return err;

    /* SWRESET (0x12) — software reset. */
    err = epd_cmd(0x12u);
    if (err != HAL_EPAPER_OK) return err;
    err = epd_wait_busy();
    if (err != HAL_EPAPER_OK) return err;

    /* Driver output control (0x01): 200 rows, gate scan up. */
    err = epd_cmd(0x01u);
    if (err != HAL_EPAPER_OK) return err;
    err = epd_data_byte(0xC7u);   /* 0xC7 = 199 (200-1) */
    if (err != HAL_EPAPER_OK) return err;
    err = epd_data_byte(0x00u);
    if (err != HAL_EPAPER_OK) return err;
    err = epd_data_byte(0x01u);
    if (err != HAL_EPAPER_OK) return err;

    /* Data entry mode (0x11): Y increment, X increment. */
    err = epd_cmd(0x11u);
    if (err != HAL_EPAPER_OK) return err;
    err = epd_data_byte(0x01u);
    if (err != HAL_EPAPER_OK) return err;

    /* Set RAM window: X [0,199], Y [199,0] (portrait, matches example). */
    err = epd_set_windows(0u, 199u, 199u, 0u);
    if (err != HAL_EPAPER_OK) return err;

    /* Border waveform (0x3C). */
    err = epd_cmd(0x3Cu);
    if (err != HAL_EPAPER_OK) return err;
    err = epd_data_byte(0x01u);
    if (err != HAL_EPAPER_OK) return err;

    /* Temperature sensor select (0x18): internal. */
    err = epd_cmd(0x18u);
    if (err != HAL_EPAPER_OK) return err;
    err = epd_data_byte(0x80u);
    if (err != HAL_EPAPER_OK) return err;

    /* Load temperature and waveform (0x22 0xB1, 0x20). */
    err = epd_cmd(0x22u);
    if (err != HAL_EPAPER_OK) return err;
    err = epd_data_byte(0xB1u);
    if (err != HAL_EPAPER_OK) return err;
    err = epd_cmd(0x20u);
    if (err != HAL_EPAPER_OK) return err;

    /* Set cursor to (0, 199) — start of Y scan. */
    err = epd_set_cursor(0u, 199u);
    if (err != HAL_EPAPER_OK) return err;

    err = epd_wait_busy();
    if (err != HAL_EPAPER_OK) return err;

    /* Load full waveform LUT. */
    err = epd_set_lut(k_wf_full_1in54);
    if (err != HAL_EPAPER_OK) return err;

    s_initialized = 1u;
    ESP_LOGI(TAG, "E-paper init OK");
    return HAL_EPAPER_OK;
}

hal_epaper_err_t hal_epaper_flush(const uint8_t *fb_pixels, uint32_t size)
{
    hal_epaper_err_t err;

    /* Guard order: NULL → size → init. */
    if (!fb_pixels) {
        return HAL_EPAPER_ERR_NULL;
    }
    if (size != HAL_EPAPER_FB_SIZE) {
        return HAL_EPAPER_ERR_SIZE;
    }
    if (!s_initialized) {
        return HAL_EPAPER_ERR_INIT;
    }

    ESP_LOGI(TAG, "flush: writing %lu bytes", (unsigned long)size);

    /* Wait for display to be ready before starting the transfer. */
    err = epd_wait_busy();
    if (err != HAL_EPAPER_OK) return err;

    /* Set RAM window and cursor. */
    err = epd_set_windows(0u, 199u, 199u, 0u);
    if (err != HAL_EPAPER_OK) return err;
    err = epd_set_cursor(0u, 199u);
    if (err != HAL_EPAPER_OK) return err;

    /* Write RAM command (0x24) + 5000 pixel bytes via DMA. */
    err = epd_cmd(0x24u);
    if (err != HAL_EPAPER_OK) return err;
    err = epd_data_buf(fb_pixels, HAL_EPAPER_FB_SIZE);
    if (err != HAL_EPAPER_OK) return err;

    ESP_LOGI(TAG, "flush: RAM write done, triggering update");

    /* Display update control 2 (0x22 0xF7) + Master activation (0x20). */
    err = epd_cmd(0x22u);
    if (err != HAL_EPAPER_OK) return err;
    err = epd_data_byte(0xF7u);
    if (err != HAL_EPAPER_OK) return err;
    err = epd_cmd(0x20u);
    if (err != HAL_EPAPER_OK) return err;

    ESP_LOGI(TAG, "flush: waiting for refresh...");

    /* Wait for refresh to complete (up to 3 s). */
    err = epd_wait_busy();
    if (err != HAL_EPAPER_OK) return err;

    ESP_LOGI(TAG, "flush: refresh complete");

    return HAL_EPAPER_OK;
}

hal_epaper_err_t hal_epaper_sleep(void)
{
    if (!s_initialized) return HAL_EPAPER_OK;

    hal_epaper_err_t err;

    /* Deep sleep mode 1: retain RAM, ~5 µA. */
    err = epd_cmd(0x10u);
    if (err != HAL_EPAPER_OK) return err;
    err = epd_data_byte(0x01u);
    if (err != HAL_EPAPER_OK) return err;

    vTaskDelay(pdMS_TO_TICKS(100));
    return HAL_EPAPER_OK;
}

void hal_epaper_deinit(void)
{
    if (!s_initialized) {
        return;
    }
    s_initialized = 0u;

    spi_bus_remove_device(s_spi);
    spi_bus_free(SPI2_HOST);

    /* Power off the e-paper rail (active-low power enable: HIGH = OFF). */
    gpio_set_level(EPD_PWR_PIN, 1);
}

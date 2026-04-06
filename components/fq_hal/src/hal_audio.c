/**
 * hal_audio.c — Hardware Abstraction Layer: Audio / I2S + ES8311 Codec
 *
 * Target: ESP32-S3-PICO-1-N8R8, ESP-IDF v5.5.1+.
 *
 * Hardware configuration (from user_config.h):
 *   I2S:  MCLK=GPIO14, BCLK=GPIO15, WS=GPIO38, DOUT=GPIO45, DIN=GPIO16
 *   I2C:  SDA=GPIO47, SCL=GPIO48  (ES8311 codec, address 0x18)
 *   PWR:  Audio_PWR_PIN=GPIO42 (audio power rail, active-LOW)
 *         PA_EN=GPIO46 (PA amplifier, managed by codec_board via pa_pin)
 *
 * Audio format: 16-bit signed mono PCM at 22050 Hz.
 * Ring buffer: static internal SRAM (DMA-safe), AUDIO_RING_BUF_SAMPLES deep.
 *   Producer: hal_audio_write_samples() / hal_audio_play().
 *   Consumer: audio_drain_task() drains to codec in 256-sample stereo chunks.
 *   The drain task duplicates each mono sample into both L and R channels.
 *
 * This file is NOT compiled on the host — mock_hal_audio.c is linked instead.
 * Real hardware wiring is gated on CONFIG_BSP_AUDIO.
 *
 * Phase 23 changes:
 *   - Ring buffer enlarged to AUDIO_RING_BUF_SAMPLES (44100) for prefill.
 *   - hal_audio_get_ring_count() accessor added.
 *   - hal_audio_play() now generates a real square wave (not a stub).
 *   - hal_audio_init() uses codec_board + codec_init for proven hardware path.
 *   - Audio init must be called BEFORE hal_epaper_init() — GDMA order matters.
 *
 * IDF 5.3 note: CONFIG_GDMA_ISR_IRAM_SAFE must NOT be set with IDF < 5.5
 * (bug #15533). See sdkconfig.defaults.
 */

#include "hal_audio.h"
#include <string.h>

#ifdef CONFIG_BSP_AUDIO
#include "codec_board.h"
#include "codec_init.h"
#include "esp_codec_dev.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "hal_audio";
#endif /* CONFIG_BSP_AUDIO */

/* -------------------------------------------------------------------------
 * Ring buffer — static internal SRAM (DMA-safe on target).
 *
 * SPSC invariant: head written by producer, tail by consumer.
 * On Xtensa LX7, 32-bit-aligned word accesses are naturally atomic.
 * s_ring_count is volatile to prevent compiler reordering across the SPSC
 * boundary when CONFIG_BSP_AUDIO is active (producer in main task, consumer
 * in audio_drain_task).
 * -------------------------------------------------------------------------
 */
static int16_t           s_ring_buf[AUDIO_RING_BUF_SAMPLES];
static uint32_t          s_ring_head;    /**< Write pointer (producer). */
static uint32_t          s_ring_tail;    /**< Read pointer  (consumer). */
static volatile uint32_t s_ring_count;   /**< Samples currently queued.  */

static uint8_t s_initialized;

/* -------------------------------------------------------------------------
 * Target-only state.
 * -------------------------------------------------------------------------
 */
#ifdef CONFIG_BSP_AUDIO
static esp_codec_dev_handle_t s_play_dev;
static TaskHandle_t           s_audio_task_handle;

/* Drain chunk in mono samples. At 22050 Hz this is ~11.6 ms per write. */
#define AUDIO_DRAIN_CHUNK_SAMPLES  256u

/* -------------------------------------------------------------------------
 * audio_drain_task — FreeRTOS task: drain ring buffer into codec.
 *
 * Reads up to AUDIO_DRAIN_CHUNK_SAMPLES mono samples, expands mono→stereo
 * (duplicate L+R), then calls esp_codec_dev_write() which blocks until DMA
 * consumes the data. When the ring is empty, writes silence to keep the codec
 * clock alive and prevent I2S underrun artifacts.
 * -------------------------------------------------------------------------
 */
static void audio_drain_task(void *arg)
{
    static int16_t stereo_frames[AUDIO_DRAIN_CHUNK_SAMPLES * 2u];

    while (1) {
        size_t n = 0u;

        while (n < AUDIO_DRAIN_CHUNK_SAMPLES && s_ring_count > 0u) {
            int16_t sample = s_ring_buf[s_ring_tail];
            s_ring_tail = (s_ring_tail + 1u) % AUDIO_RING_BUF_SAMPLES;
            s_ring_count--;
            stereo_frames[n * 2u]      = sample;   /* Left  */
            stereo_frames[n * 2u + 1u] = sample;   /* Right */
            n++;
        }

        /* Pad with silence if ring ran short. */
        while (n < AUDIO_DRAIN_CHUNK_SAMPLES) {
            stereo_frames[n * 2u]      = 0;
            stereo_frames[n * 2u + 1u] = 0;
            n++;
        }

        int bytes = (int)(AUDIO_DRAIN_CHUNK_SAMPLES * 2u * sizeof(int16_t));
        int ret = esp_codec_dev_write(s_play_dev, stereo_frames, bytes);
        if (ret != ESP_CODEC_DEV_OK) {
            ESP_LOGW(TAG, "codec_dev_write error %d", ret);
        }
    }
}

/* -------------------------------------------------------------------------
 * audio_hw_init — Bring up GPIO power rail, codec (I2C + I2S + ES8311).
 *
 * Uses the factory codec_board + codec_init abstraction proven on the
 * S3_ePaper_1_54 board. Returns HAL_AUDIO_ERR_INIT on any failure.
 * -------------------------------------------------------------------------
 */
static hal_audio_err_t audio_hw_init(void)
{
    /* Power on audio rail (active-LOW per board schematic). */
    gpio_config_t pwr_cfg = {
        .pin_bit_mask = (1ULL << 42),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&pwr_cfg);
    gpio_set_level(GPIO_NUM_42, 0);  /* ON = LOW */
    ESP_LOGI(TAG, "Audio power ON (GPIO42=LOW)");

    /* Factory codec init — select board type for pin map and codec model. */
    set_codec_board_type("S3_ePaper_1_54");
    codec_init_cfg_t codec_cfg = {
        .in_mode    = CODEC_I2S_MODE_STD,
        .out_mode   = CODEC_I2S_MODE_STD,
        .in_use_tdm = false,
        .reuse_dev  = false,
    };
    if (init_codec(&codec_cfg) != 0) {
        ESP_LOGE(TAG, "init_codec failed");
        return HAL_AUDIO_ERR_INIT;
    }
    s_play_dev = get_playback_handle();
    if (!s_play_dev) {
        ESP_LOGE(TAG, "get_playback_handle returned NULL");
        return HAL_AUDIO_ERR_INIT;
    }
    ESP_LOGI(TAG, "Codec init OK");

    /* Open codec at project sample rate, 16-bit stereo. */
    esp_codec_dev_sample_info_t fs = {
        .sample_rate     = AUDIO_SAMPLE_RATE_HZ,
        .channel         = 2,
        .bits_per_sample = 16,
    };
    if (esp_codec_dev_open(s_play_dev, &fs) != ESP_CODEC_DEV_OK) {
        ESP_LOGE(TAG, "esp_codec_dev_open failed");
        return HAL_AUDIO_ERR_INIT;
    }
    esp_codec_dev_set_out_vol(s_play_dev, 80.0);
    ESP_LOGI(TAG, "Codec opened — %u Hz, 16-bit stereo, vol=80",
             (unsigned)AUDIO_SAMPLE_RATE_HZ);

    /* Start drain task at priority 5, 4 KB stack. */
    BaseType_t ok = xTaskCreate(audio_drain_task, "audio_drain", 4096,
                                NULL, 5, &s_audio_task_handle);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "xTaskCreate audio_drain failed");
        return HAL_AUDIO_ERR_INIT;
    }
    ESP_LOGI(TAG, "audio_drain_task started");
    return HAL_AUDIO_OK;
}
#endif /* CONFIG_BSP_AUDIO */

/* -------------------------------------------------------------------------
 * Public API implementation.
 * -------------------------------------------------------------------------
 */

hal_audio_err_t hal_audio_init(void)
{
    memset(s_ring_buf, 0, sizeof(s_ring_buf));
    s_ring_head   = 0u;
    s_ring_tail   = 0u;
    s_ring_count  = 0u;
    s_initialized = 1u;

#ifdef CONFIG_BSP_AUDIO
    hal_audio_err_t err = audio_hw_init();
    if (err != HAL_AUDIO_OK) {
        s_initialized = 0u;
        return err;
    }
#endif

    return HAL_AUDIO_OK;
}

hal_audio_err_t hal_audio_play(uint16_t freq_hz, uint16_t duration_ms)
{
    if (!s_initialized) {
        return HAL_AUDIO_ERR_INIT;
    }
    if (freq_hz == 0u) {
        return HAL_AUDIO_ERR_INVALID_FREQ;
    }

    /* Clamp duration to prevent ring buffer overflow. */
    uint16_t clamped_ms = (duration_ms > (uint16_t)AUDIO_MAX_TONE_MS)
                          ? (uint16_t)AUDIO_MAX_TONE_MS
                          : duration_ms;

    uint32_t n_samples   = ((uint32_t)AUDIO_SAMPLE_RATE_HZ * clamped_ms) / 1000u;
    uint32_t half_period = AUDIO_SAMPLE_RATE_HZ / (2u * (uint32_t)freq_hz);
    if (half_period == 0u) {
        half_period = 1u;
    }

    /* Generate square wave into 64-sample stack chunks. */
#define PLAY_CHUNK 64u
    int16_t chunk[PLAY_CHUNK];
    uint32_t phase     = 0u;
    uint32_t remaining = n_samples;

    while (remaining > 0u) {
        uint32_t chunk_len = (remaining < PLAY_CHUNK) ? remaining : PLAY_CHUNK;
        for (uint32_t i = 0u; i < chunk_len; i++) {
            chunk[i] = ((phase / half_period) & 1u)
                       ? (int16_t)16383 : (int16_t)-16383;
            phase++;
        }
        hal_audio_err_t r = hal_audio_write_samples(chunk, chunk_len);
        if (r == HAL_AUDIO_ERR_OVERFLOW) {
            break;
        }
        remaining -= chunk_len;
    }
#undef PLAY_CHUNK

    return HAL_AUDIO_OK;
}

hal_audio_err_t hal_audio_write_samples(const int16_t *buf, size_t count)
{
    if (!s_initialized) {
        return HAL_AUDIO_ERR_INIT;
    }
    if (buf == NULL) {
        return HAL_AUDIO_ERR_NULL;
    }

    hal_audio_err_t result = HAL_AUDIO_OK;

    for (size_t i = 0u; i < count; i++) {
        if (s_ring_count >= AUDIO_RING_BUF_SAMPLES) {
            result = HAL_AUDIO_ERR_OVERFLOW;
            break;
        }
        s_ring_buf[s_ring_head] = buf[i];
        s_ring_head = (s_ring_head + 1u) % AUDIO_RING_BUF_SAMPLES;
        s_ring_count++;
    }

    return result;
}

/**
 * hal_audio_get_ring_count — Return current ring buffer fill level.
 *
 * Thread-safe read of the volatile counter. Returns 0 if uninitialised.
 * Used by prefill_audio() to gate e-paper flushes until the ring is
 * at least 80% full.
 */
uint32_t hal_audio_get_ring_count(void)
{
    return s_ring_count;
}

hal_audio_err_t hal_audio_stop(void)
{
    s_ring_head  = 0u;
    s_ring_tail  = 0u;
    s_ring_count = 0u;
    return HAL_AUDIO_OK;
}

void hal_audio_deinit(void)
{
    s_initialized = 0u;
    s_ring_head   = 0u;
    s_ring_tail   = 0u;
    s_ring_count  = 0u;

#ifdef CONFIG_BSP_AUDIO
    if (s_audio_task_handle != NULL) {
        vTaskDelete(s_audio_task_handle);
        s_audio_task_handle = NULL;
    }
    if (s_play_dev != NULL) {
        esp_codec_dev_close(s_play_dev);
        s_play_dev = NULL;
    }
    deinit_codec();
    gpio_set_level(GPIO_NUM_42, 1);  /* PWR OFF (active-LOW, so HIGH = off) */
    ESP_LOGI(TAG, "hal_audio_deinit complete");
#endif
}

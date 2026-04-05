/**
 * hal_audio.c — Hardware Abstraction Layer: Audio / I2S + ES8311 Codec (target stub)
 *
 * This translation unit is the TARGET implementation linked into the ESP-IDF
 * firmware image. It is NOT compiled on the host — mock_hal_audio.c is used
 * there instead.
 *
 * Target behaviour (ESP-IDF v5.x, behind CONFIG_BSP_AUDIO):
 *   - hal_audio_init():
 *       Enables GPIO42 (Audio_PWR_PIN) and GPIO46 (PA_EN).
 *       Configures I2S peripheral: 16-bit signed mono, 22050 Hz, DMA
 *       double-buffer. DMA buffers MUST be allocated with
 *       MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL (PSRAM is not DMA-accessible).
 *       Initialises ES8311 codec over I2C (address 0x18, ASEL=LOW).
 *       Creates audio FreeRTOS task at priority 5, 4KB stack.
 *
 *   - hal_audio_play():
 *       Generates a square-wave burst of clamped duration (AUDIO_MAX_TONE_MS)
 *       into the ring buffer. Non-blocking.
 *
 *   - hal_audio_write_samples():
 *       Writes raw 16-bit PCM into the SPSC ring buffer. Audio task drains
 *       to I2S DMA. Overflow: newest samples dropped, returns ERR_OVERFLOW.
 *
 *   - hal_audio_stop():
 *       Flushes ring buffer.
 *
 *   - hal_audio_deinit():
 *       Signals audio task to stop, waits for termination, tears down I2S
 *       and I2C, powers down GPIO42/46.
 *
 * Stub policy: all functions compile cleanly without ESP-IDF headers.
 * Real ESP-IDF wiring is gated on CONFIG_BSP_AUDIO (defined in sdkconfig).
 */

#include "hal_audio.h"
#include <string.h>

/* -------------------------------------------------------------------------
 * Ring buffer (static internal SRAM — DMA-safe on target).
 * -------------------------------------------------------------------------
 */
static int16_t  s_ring_buf[AUDIO_RING_BUF_SAMPLES];
static uint32_t s_ring_head;   /**< Write pointer (producer). */
static uint32_t s_ring_tail;   /**< Read  pointer (consumer). */
static uint32_t s_ring_count;  /**< Samples currently queued. */

static uint8_t  s_initialized;

hal_audio_err_t hal_audio_init(void)
{
    memset(s_ring_buf, 0, sizeof(s_ring_buf));
    s_ring_head   = 0u;
    s_ring_tail   = 0u;
    s_ring_count  = 0u;
    s_initialized = 1u;

    /*
     * TODO (ESP-IDF wiring behind CONFIG_BSP_AUDIO):
     *   gpio_set_direction(42, GPIO_MODE_OUTPUT); gpio_set_level(42, 1);
     *   gpio_set_direction(46, GPIO_MODE_OUTPUT); gpio_set_level(46, 1);
     *   i2s_chan_config_t + i2s_new_channel() + i2s_channel_init_std_mode()
     *     .slot_cfg: I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)
     *     .clk_cfg:  .sample_rate_hz = 22050, .clk_src = I2S_CLK_SRC_DEFAULT
     *   DMA buffers: heap_caps_malloc(size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL)
     *   ES8311 I2C init: i2c_master_init() + es8311_init() at address 0x18
     *   xTaskCreate(audio_task, "audio", 4096, NULL, 5, &s_audio_task_handle)
     */
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
    uint16_t clamped_ms = duration_ms;
    if (clamped_ms > (uint16_t)AUDIO_MAX_TONE_MS) {
        clamped_ms = (uint16_t)AUDIO_MAX_TONE_MS;
    }

    (void)clamped_ms; /* Used in real wiring below. */

    /*
     * TODO (ESP-IDF wiring):
     *   uint32_t n_samples = ((uint32_t)AUDIO_SAMPLE_RATE_HZ * clamped_ms) / 1000u;
     *   uint32_t half_period = AUDIO_SAMPLE_RATE_HZ / (2u * freq_hz);
     *   Generate square wave into a local stack buffer, push to ring via
     *   hal_audio_write_samples(). For large n_samples, generate in chunks.
     */
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

    /* SPSC write: copy samples into ring, dropping newest on overflow. */
    for (size_t i = 0u; i < count; i++) {
        if (s_ring_count >= AUDIO_RING_BUF_SAMPLES) {
            /* Newest sample dropped — ring is full. */
            result = HAL_AUDIO_ERR_OVERFLOW;
            break;
        }
        s_ring_buf[s_ring_head] = buf[i];
        s_ring_head = (s_ring_head + 1u) % AUDIO_RING_BUF_SAMPLES;
        s_ring_count++;
    }

    return result;
}

hal_audio_err_t hal_audio_stop(void)
{
    /*
     * Flush ring buffer. Safe before init.
     * TODO (ESP-IDF wiring): signal audio task to drain/stop.
     */
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
    /*
     * TODO (ESP-IDF wiring):
     *   vTaskDelete(s_audio_task_handle); (with proper signalling)
     *   i2s_channel_disable(); i2s_del_channel();
     *   es8311_deinit();
     *   gpio_set_level(46, 0); gpio_set_level(42, 0);
     */
}

/**
 * hal_gpio.c — Hardware Abstraction Layer: GPIO / Button Input
 *
 * Target: ESP32-S3 (Waveshare ESP32-S3-ePaper-1.54 V2).
 * Compiled ONLY with idf.py build — NOT in host tests.
 * The host test suite links mock_hal_gpio.c instead of this file.
 *
 * Hardware:
 *   BOOT button (BTN_A): GPIO0  — active-low, internal pull-up
 *   PWR  button (BTN_B): GPIO18 — active-low, internal pull-up
 *
 * ISR strategy:
 *   - ISR fires on falling edge (active-low press).
 *   - Inside ISR: check esp_timer_get_time() vs last press for 50 ms debounce.
 *   - If debounce passes, post btn_id to a FreeRTOS queue (ISR-safe).
 *   - A dedicated gpio_task drains the queue and calls the registered callback
 *     in task context (safe for longer callback logic).
 *
 * Architecture constraint: This file is the BOTTOM layer.
 * It MUST NOT be included by game/, presentation/, or connectivity/.
 */

#include "hal_gpio.h"

#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

static const char *TAG = "hal_gpio";

/* -------------------------------------------------------------------------
 * Pin assignments.
 * -------------------------------------------------------------------------
 */
#define BTN_A_GPIO   GPIO_NUM_0
#define BTN_B_GPIO   GPIO_NUM_18

/* -------------------------------------------------------------------------
 * Debounce threshold: 50 ms in microseconds.
 * -------------------------------------------------------------------------
 */
#define DEBOUNCE_US  50000LL

/* -------------------------------------------------------------------------
 * FreeRTOS queue depth: at most 4 pending button events.
 * -------------------------------------------------------------------------
 */
#define BTN_QUEUE_DEPTH  4u

/* -------------------------------------------------------------------------
 * Module state.
 * -------------------------------------------------------------------------
 */
static hal_btn_callback_t s_callback;
static uint8_t            s_initialized;
static QueueHandle_t      s_btn_queue;
static TaskHandle_t       s_btn_task;

/* Per-button last ISR timestamp for debounce. */
static volatile int64_t s_last_isr_us[HAL_BTN_COUNT];

/* -------------------------------------------------------------------------
 * ISR handler — installed via gpio_isr_handler_add.
 *
 * arg is the btn_id cast to void* (passed as isr_handler_arg at init).
 * Enforces 50 ms debounce: checks esp_timer_get_time() vs s_last_isr_us.
 * Posts btn_id to queue from ISR context if debounce gate passes.
 * -------------------------------------------------------------------------
 */
static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    hal_btn_id_t btn_id = (hal_btn_id_t)(uintptr_t)arg;
    int64_t      now    = esp_timer_get_time();

    if ((uint32_t)btn_id >= (uint32_t)HAL_BTN_COUNT) {
        return;
    }

    /* Debounce gate. */
    if (now - s_last_isr_us[btn_id] < DEBOUNCE_US) {
        return;
    }
    s_last_isr_us[btn_id] = now;

    BaseType_t higher_prio_woken = pdFALSE;
    xQueueSendFromISR(s_btn_queue, &btn_id, &higher_prio_woken);
    if (higher_prio_woken) {
        portYIELD_FROM_ISR();
    }
}

/* -------------------------------------------------------------------------
 * gpio_task — Drains the button queue and calls the registered callback.
 * Runs at priority 5 (above idle, below display refresh).
 * -------------------------------------------------------------------------
 */
static void gpio_task(void *arg)
{
    (void)arg;
    hal_btn_id_t btn_id;

    while (1) {
        if (xQueueReceive(s_btn_queue, &btn_id, portMAX_DELAY) == pdTRUE) {
            if (s_callback) {
                s_callback(btn_id);
            }
        }
    }
}

/* -------------------------------------------------------------------------
 * Public API.
 * -------------------------------------------------------------------------
 */

hal_gpio_err_t hal_gpio_init(hal_btn_callback_t callback)
{
    if (!callback) {
        return HAL_GPIO_ERR_NULL;
    }

    s_callback = callback;

    /* Clear debounce timestamps. */
    s_last_isr_us[HAL_BTN_A] = 0;
    s_last_isr_us[HAL_BTN_B] = 0;

    /* Configure BTN_A (GPIO0) — active-low, pull-up, falling edge interrupt. */
    gpio_config_t io_conf = {};
    io_conf.intr_type    = GPIO_INTR_NEGEDGE;
    io_conf.mode         = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << BTN_A_GPIO) | (1ULL << BTN_B_GPIO);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en   = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    /* Create the button event queue. */
    s_btn_queue = xQueueCreate(BTN_QUEUE_DEPTH, sizeof(hal_btn_id_t));
    if (!s_btn_queue) {
        ESP_LOGE(TAG, "Failed to create button queue");
        return HAL_GPIO_ERR_INIT;
    }

    /* Install ISR service (shared across all GPIO interrupts). */
    gpio_install_isr_service(0);

    /* Register ISR handlers — btn_id passed as ISR argument. */
    gpio_isr_handler_add(BTN_A_GPIO, gpio_isr_handler,
                         (void *)(uintptr_t)HAL_BTN_A);
    gpio_isr_handler_add(BTN_B_GPIO, gpio_isr_handler,
                         (void *)(uintptr_t)HAL_BTN_B);

    /* Create the GPIO task. */
    BaseType_t rc = xTaskCreate(gpio_task, "gpio_task", 2048u, NULL, 5u,
                                &s_btn_task);
    if (rc != pdPASS) {
        ESP_LOGE(TAG, "Failed to create gpio_task");
        return HAL_GPIO_ERR_INIT;
    }

    s_initialized = 1u;
    ESP_LOGI(TAG, "GPIO init OK");
    return HAL_GPIO_OK;
}

hal_gpio_err_t hal_gpio_deinit(void)
{
    if (!s_initialized) {
        return HAL_GPIO_OK;
    }

    gpio_isr_handler_remove(BTN_A_GPIO);
    gpio_isr_handler_remove(BTN_B_GPIO);
    gpio_uninstall_isr_service();

    if (s_btn_task) {
        vTaskDelete(s_btn_task);
        s_btn_task = NULL;
    }
    if (s_btn_queue) {
        vQueueDelete(s_btn_queue);
        s_btn_queue = NULL;
    }

    s_callback    = (hal_btn_callback_t)0;
    s_initialized = 0u;
    return HAL_GPIO_OK;
}

uint8_t hal_gpio_is_pressed(hal_btn_id_t btn_id)
{
    if (!s_initialized || (uint32_t)btn_id >= (uint32_t)HAL_BTN_COUNT) {
        return 0u;
    }
    /* Active-low: GPIO reads 0 when pressed. */
    gpio_num_t pin = (btn_id == HAL_BTN_A) ? BTN_A_GPIO : BTN_B_GPIO;
    return (uint8_t)(!gpio_get_level(pin));
}

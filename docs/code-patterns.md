# Code Patterns — ESP-IDF / ESP32-S3-ePaper-1.54

All patterns sourced from `example/Example/ESP-IDF/V2/`.

---

## Entry Point

ESP-IDF applications use `app_main` (not `main()`). Since the BSP components are C++, declare it with C linkage:

```cpp
extern "C" void app_main(void)
{
    // Your initialization here
}
```

---

## Minimal App Template

```cpp
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "user_config.h"
#include "user_app.h"

extern "C" void app_main(void)
{
    user_app_init();   // Powers on rails, inits EPD, buttons
    // your code here
}
```

---

## Power Management (`board_power_bsp`)

Power rails must be explicitly enabled. Instantiate at file scope:

```cpp
#include "board_power_bsp.h"
#include "user_config.h"

board_power_bsp_t board_div(EPD_PWR_PIN, Audio_PWR_PIN, VBAT_PWR_PIN);

// In app_main():
board_div.VBAT_POWER_ON();     // Enable battery sense / system power
board_div.POWEER_EPD_ON();     // Enable e-paper power rail
board_div.POWEER_Audio_ON();   // Enable ES8311 audio codec power
```

Available methods: `POWEER_EPD_ON/OFF`, `POWEER_Audio_ON/OFF`, `VBAT_POWER_ON/OFF`

---

## e-Paper Driver (`epaper_driver_bsp`)

The driver is a C++ class. After `user_app_init()` it is accessible via the global `driver` pointer. To instantiate manually:

```cpp
#include "epaper_driver_bsp.h"
#include "user_config.h"

epaper_driver_display *driver = NULL;

// In app_main() (after power ON):
custom_lcd_spi_t cfg = {};
cfg.cs         = EPD_CS_PIN;
cfg.dc         = EPD_DC_PIN;
cfg.rst        = EPD_RST_PIN;
cfg.busy       = EPD_BUSY_PIN;
cfg.mosi       = EPD_MOSI_PIN;
cfg.scl        = EPD_SCK_PIN;
cfg.spi_host   = EPD_SPI_NUM;   // SPI2_HOST
cfg.buffer_len = 5000;

driver = new epaper_driver_display(EPD_WIDTH, EPD_HEIGHT, cfg);
driver->EPD_Init();
driver->EPD_Clear();
driver->EPD_DisplayPartBaseImage();
driver->EPD_Init_Partial();      // Must call after base image for partial refresh
```

### EPD Public API

| Method                                    | Description                              |
|-------------------------------------------|------------------------------------------|
| `EPD_Init()`                              | Full init sequence                       |
| `EPD_Clear()`                             | Clear internal buffer to white           |
| `EPD_Display()`                           | Full refresh push to panel               |
| `EPD_DisplayPartBaseImage()`              | Push base image for partial refresh      |
| `EPD_Init_Partial()`                      | Switch to partial refresh mode           |
| `EPD_DrawColorPixel(x, y, color)`         | Draw one pixel (`DRIVER_COLOR_BLACK/WHITE`) |
| `EPD_DisplayPart()`                       | Partial refresh push to panel            |

Color constants: `DRIVER_COLOR_BLACK = 0x00`, `DRIVER_COLOR_WHITE = 0xFF`

---

## I2C Initialization

```cpp
#include "i2c_bsp.h"

// In app_main(), call once before any I2C device:
i2c_master_Init();
```

---

## RTC — PCF85063 (`i2c_equipment`)

```cpp
#include "i2c_equipment.h"

i2c_equipment *rtc_dev = new i2c_equipment();

// Set time (year, month, day, hour, min, sec):
rtc_dev->set_rtcTime(2025, 9, 10, 8, 30, 30);

// Read time:
RtcDateTime_t dt = rtc_dev->get_rtcTime();
printf("%d/%d/%d %d:%d:%d\n", dt.year, dt.month, dt.day,
                               dt.hour, dt.minute, dt.second);
```

---

## Temp & Humidity — SHTC3 (`i2c_equipment_shtc3`)

```cpp
#include "i2c_equipment.h"

i2c_equipment_shtc3 *shtc3 = new i2c_equipment_shtc3();
shtc3_data_t data = shtc3->readTempHumi();
printf("RH: %.2f%%  Temp: %.2f C\n", data.RH, data.Temp);
```

---

## FreeRTOS Task Pattern

Sensor polling and background work uses pinned tasks. Sensor tasks typically run on **Core 0**, LVGL on **Core 1**.

```cpp
void my_task(void *arg)
{
    for (;;)
    {
        // ... do work ...
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// In app_main():
xTaskCreatePinnedToCore(
    my_task,          // function
    "task_name",      // name (for idf.py monitor)
    3 * 1024,         // stack in bytes
    NULL,             // arg
    2,                // priority (1=low, higher=more CPU)
    NULL,             // task handle (optional)
    0                 // core: 0 or 1
);
```

---

## LVGL v8 Integration (Full Pattern)

This is the complete pattern used in examples 07–12. LVGL runs on **Core 1** behind a mutex.

```cpp
#include "freertos/FreeRTOS.h"
#include "lvgl.h"
#include "user_app.h"      // driver extern, user_app_init(), user_ui_init()
#include "user_config.h"
#include "esp_timer.h"
#include "esp_log.h"

static SemaphoreHandle_t lvgl_mux = NULL;

// --- LVGL flush callback: maps LVGL pixel buffer → EPD driver ---
static void example_lvgl_flush_cb(lv_disp_drv_t *drv,
                                   const lv_area_t *area,
                                   lv_color_t *color_map)
{
    uint16_t *buf = (uint16_t *)color_map;
    driver->EPD_Clear();
    for (int y = area->y1; y <= area->y2; y++) {
        for (int x = area->x1; x <= area->x2; x++) {
            uint8_t color = (*buf < 0x7fff) ? DRIVER_COLOR_BLACK : DRIVER_COLOR_WHITE;
            driver->EPD_DrawColorPixel(x, y, color);
            buf++;
        }
    }
    driver->EPD_DisplayPart();
    lv_disp_flush_ready(drv);
}

// --- LVGL tick (called by esp_timer every 5 ms) ---
static void example_increase_lvgl_tick(void *arg)
{
    lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}

// --- LVGL handler task (Core 1) ---
static void example_lvgl_port_task(void *arg)
{
    uint32_t delay_ms = EXAMPLE_LVGL_TASK_MAX_DELAY_MS;
    for (;;) {
        if (xSemaphoreTake(lvgl_mux, portMAX_DELAY) == pdTRUE) {
            delay_ms = lv_timer_handler();
            xSemaphoreGive(lvgl_mux);
        }
        if (delay_ms > EXAMPLE_LVGL_TASK_MAX_DELAY_MS)
            delay_ms = EXAMPLE_LVGL_TASK_MAX_DELAY_MS;
        else if (delay_ms < EXAMPLE_LVGL_TASK_MIN_DELAY_MS)
            delay_ms = EXAMPLE_LVGL_TASK_MIN_DELAY_MS;
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

extern "C" void app_main(void)
{
    static lv_disp_draw_buf_t disp_buf;
    static lv_disp_drv_t disp_drv;

    user_app_init();   // powers rails, inits EPD, buttons

    lv_init();

    // Allocate draw buffers from OPI PSRAM
    lv_color_t *buf1 = (lv_color_t *)heap_caps_malloc(LVGL_SPIRAM_BUFF_LEN, MALLOC_CAP_SPIRAM);
    lv_color_t *buf2 = (lv_color_t *)heap_caps_malloc(LVGL_SPIRAM_BUFF_LEN, MALLOC_CAP_SPIRAM);
    assert(buf1 && buf2);
    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, EPD_WIDTH * EPD_HEIGHT);

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res      = EPD_WIDTH;   // 200
    disp_drv.ver_res      = EPD_HEIGHT;  // 200
    disp_drv.flush_cb     = example_lvgl_flush_cb;
    disp_drv.draw_buf     = &disp_buf;
    disp_drv.full_refresh = 1;           // MUST be 1 for e-paper
    lv_disp_drv_register(&disp_drv);

    // Start LVGL tick timer (5 ms period)
    esp_timer_create_args_t tick_args = {};
    tick_args.callback = &example_increase_lvgl_tick;
    tick_args.name     = "lvgl_tick";
    esp_timer_handle_t tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&tick_args, &tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000));

    // Create LVGL handler task on Core 1, priority 4
    lvgl_mux = xSemaphoreCreateMutex();
    assert(lvgl_mux);
    xTaskCreatePinnedToCore(example_lvgl_port_task, "LVGL", 8 * 1024, NULL, 4, NULL, 1);

    // Build initial UI (must hold mutex)
    if (xSemaphoreTake(lvgl_mux, portMAX_DELAY) == pdTRUE) {
        user_ui_init();   // calls setup_ui() from GUI Guider, creates tasks
        xSemaphoreGive(lvgl_mux);
    }
}
```

> **Key constraints:**
> - `full_refresh = 1` is **mandatory** for e-paper — never set to 0
> - LVGL buffers **must** come from PSRAM (`MALLOC_CAP_SPIRAM`)
> - Always acquire `lvgl_mux` before calling any `lv_*` API from outside the LVGL task
> - LVGL task runs on **Core 1**; sensor / button tasks on **Core 0**

---

## Deep Sleep + RTC Wake-up

```cpp
#include "esp_sleep.h"
#include "driver/rtc_io.h"

// Configure ext0 wake-up on BOOT button (GPIO0, active-low)
esp_sleep_enable_ext0_wakeup(ext_wakeup_pin_1, 0);

// Enter deep sleep (RTC will wake on pin trigger)
esp_deep_sleep_start();
```

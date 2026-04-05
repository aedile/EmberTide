# Pin Definitions — ESP32-S3-ePaper-1.54

Source: `example/Example/ESP-IDF/V2/07_BATT_PWR_Test/main/user_config.h`

Copy this file into your own project's `main/` directory as-is; all BSP components reference these macros.

---

## e-Paper Display (SPI2_HOST)

| Signal | GPIO Constant  | GPIO # | `user_config.h` Macro |
|--------|---------------|--------|-----------------------|
| D/C    | GPIO_NUM_10   | 10     | `EPD_DC_PIN`          |
| CS     | GPIO_NUM_11   | 11     | `EPD_CS_PIN`          |
| SCK    | GPIO_NUM_12   | 12     | `EPD_SCK_PIN`         |
| MOSI   | GPIO_NUM_13   | 13     | `EPD_MOSI_PIN`        |
| RST    | GPIO_NUM_9    | 9      | `EPD_RST_PIN`         |
| BUSY   | GPIO_NUM_8    | 8      | `EPD_BUSY_PIN`        |

```c
#define EPD_SPI_NUM   SPI2_HOST
#define EPD_WIDTH     200
#define EPD_HEIGHT    200
#define LVGL_SPIRAM_BUFF_LEN  (EPD_WIDTH * EPD_HEIGHT * 2)   // 80,000 bytes
```

---

## Power Rails

| Rail               | GPIO Constant  | GPIO # | `user_config.h` Macro |
|--------------------|---------------|--------|-----------------------|
| e-Paper Power EN   | GPIO_NUM_6    | 6      | `EPD_PWR_PIN`         |
| Audio Power EN     | GPIO_NUM_42   | 42     | `Audio_PWR_PIN`       |
| Battery Sense EN   | GPIO_NUM_17   | 17     | `VBAT_PWR_PIN`        |

All three rails are passed to `board_power_bsp_t` at construction time:
```cpp
board_power_bsp_t board_div(EPD_PWR_PIN, Audio_PWR_PIN, VBAT_PWR_PIN);
```

---

## Buttons

| Button | GPIO Constant | GPIO # | `user_config.h` Macro | HAL ID | Game Role | Icon on Case | Physical Position |
|--------|--------------|--------|-----------------------|--------|-----------|-------------|-------------------|
| BOOT   | GPIO_NUM_0   | 0      | `BOOT_BUTTON_PIN`     | `HAL_BTN_A` | Move/Cycle | ⏻ (power) | Closest to USB-C |
| PWR    | GPIO_NUM_18  | 18     | `PWR_BUTTON_PIN`      | `HAL_BTN_B` | OK/Select  | ☀ (sun)   | Far from USB-C   |

> **Button mapping (verified in hardware lab session 2026-04-05):**
> - The ⏻ (power icon) button closest to USB-C is GPIO0 / `HAL_BTN_A`. In-game this is the **Move/Cycle** button. UI text: `[PWR]`.
> - The ☀ (sun icon) button far from USB-C is GPIO18 / `HAL_BTN_B`. In-game this is the **OK/Select/Confirm** button. UI text: `[SUN]`.
> - The macro name `PWR_BUTTON_PIN` (GPIO18) is misleading — it refers to the ☀ sun button, not the ⏻ power icon button. This was confirmed by pressing each button and observing FSM events on serial monitor.

> GPIO0 doubles as the low-power external wake-up pin (`ext_wakeup_pin_1`).

---

## I2C Bus (I2C_NUM_0)

| Signal | GPIO Constant  | GPIO # | `user_config.h` Macro  |
|--------|---------------|--------|------------------------|
| SDA    | GPIO_NUM_47   | 47     | `ESP32_I2C_SDA_PIN`    |
| SCL    | GPIO_NUM_48   | 48     | `ESP32_I2C_SCL_PIN`    |

```c
#define ESP32_I2C_DEV_NUM  I2C_NUM_0
```

### I2C Device Addresses

| Device                | Address | `user_config.h` Macro     |
|-----------------------|---------|---------------------------|
| RTC (PCF85063)        | 0x51    | `I2C_RTC_DEV_Address`     |
| Temp/Humidity (SHTC3) | 0x70    | `I2C_SHTC3_DEV_Address`   |

---

## I2S — Audio (ES8311)

| Signal | GPIO # |
|--------|--------|
| MCLK   | 14     |
| SCLK   | 15     |
| LRCK   | 16     |
| DIN    | 38     |
| DOUT   | 45     |

---

## SD Card (SPI)

| Signal | GPIO # |
|--------|--------|
| CLK    | 39     |
| MISO   | 40     |
| MOSI   | 41     |

---

## Other

| Function   | GPIO # | Notes                          |
|------------|--------|--------------------------------|
| BAT_ADC    | 4      | Battery voltage divider        |
| PA_EN      | 46     | Speaker amplifier enable       |
| RTC_INT    | 5      | PCF85063 alarm interrupt output|

---

## LVGL Timing Constants

```c
#define EXAMPLE_LVGL_TICK_PERIOD_MS    5
#define EXAMPLE_LVGL_TASK_MAX_DELAY_MS 500
#define EXAMPLE_LVGL_TASK_MIN_DELAY_MS 100
```

# BSP Component Reference — ESP32-S3-ePaper-1.54

These components live in `components/` inside each ESP-IDF example project. Copy the ones you need into your own project's `components/` directory — they are self-contained and registered via their own `CMakeLists.txt`.

---

## `epaper_driver_bsp`

Provides the `epaper_driver_display` C++ class for driving the 1.54" SPI e-paper panel.

**Header:** `epaper_driver_bsp.h`

```cpp
// Color enum
DRIVER_COLOR_WHITE = 0xFF
DRIVER_COLOR_BLACK = 0x00
FONT_BACKGROUND    = DRIVER_COLOR_WHITE

// SPI config struct
typedef struct {
    uint8_t cs, dc, rst, busy, mosi, scl;
    int spi_host;    // SPI2_HOST
    int buffer_len;  // 5000 typical
} custom_lcd_spi_t;
```

**Public methods:**

| Method | Description |
|--------|-------------|
| `EPD_Init()` | Full hardware initialization |
| `EPD_Clear()` | Zero out pixel buffer (white) |
| `EPD_Display()` | Full refresh to panel |
| `EPD_DisplayPartBaseImage()` | Set base image for partial mode |
| `EPD_Init_Partial()` | Enter partial refresh mode |
| `EPD_DrawColorPixel(x, y, color)` | Write one pixel |
| `EPD_DisplayPart()` | Partial refresh push |

---

## `board_power_bsp`

Controls the three power rails on the board.

**Header:** `board_power_bsp.h`

```cpp
board_power_bsp_t(uint8_t epd_pin, uint8_t audio_pin, uint8_t vbat_pin);

void POWEER_EPD_ON();     void POWEER_EPD_OFF();
void POWEER_Audio_ON();   void POWEER_Audio_OFF();
void VBAT_POWER_ON();     void VBAT_POWER_OFF();
```

Call order in `app_main`: `VBAT_POWER_ON()` → `POWEER_EPD_ON()` → `POWEER_Audio_ON()`

---

## `button_bsp`

Multi-button handler with event groups. Used for BOOT and PWR button detection.

**Header:** `button_bsp.h`

```cpp
void user_button_init(void);          // Call in app_main()
extern EventGroupHandle_t pwr_groups; // Wait on this for button events
bool get_bit_button(EventBits_t bits, int bit_num);
```

Button events are posted as bits in `pwr_groups`. Example usage in `user_app.cpp`:
```cpp
EventBits_t ev = xEventGroupWaitBits(pwr_groups, set_bit_all, pdTRUE, pdFALSE, pdMS_TO_TICKS(2000));
if (get_bit_button(ev, 2)) { /* PWR button long press */ }
if (get_bit_button(ev, 3)) { /* PWR button short press */ }
```

---

## `SensorLib`

Third-party sensor library providing drivers for PCF85063 (RTC) and SHTC3 (temp/humidity). Wrapped by `i2c_equipment.h`.

**Headers used in examples:** `i2c_equipment.h`, `i2c_bsp.h`

```cpp
// I2C bus init (call once):
void i2c_master_Init();

// RTC:
i2c_equipment *rtc = new i2c_equipment();
rtc->set_rtcTime(year, month, day, hour, min, sec);
RtcDateTime_t dt = rtc->get_rtcTime();  // .year .month .day .hour .minute .second

// Temp/Humidity:
i2c_equipment_shtc3 *shtc3 = new i2c_equipment_shtc3();
shtc3_data_t d = shtc3->readTempHumi();  // .RH (%) .Temp (°C)
```

---

## `ui_bsp`

Auto-generated LVGL UI code produced by **NXP GUI Guider**. Contains:

| File | Description |
|------|-------------|
| `generated/gui_guider.h` | `setup_ui(lv_ui *ui)` — builds all screens |
| `generated/gui_guider.c` | Widget layout implementation |
| `generated/events_init.*` | Event callbacks |
| `generated/setup_scr_screen.c` | Individual screen setup |
| `custom/custom.c` | User-editable custom event logic |
| `custom/lv_conf_ext.h` | LVGL config extensions |

To use: call `setup_ui(&src_ui)` inside `user_ui_init()` while holding the LVGL mutex.

---

## `user_app`

Thin glue layer that wires all components together.

**Header:** `user_app.h`

```cpp
extern epaper_driver_display *driver;  // Global EPD driver pointer (set by user_app_init)

void user_app_init(void);  // Power on, EPD init, button init
void user_ui_init(void);   // Build UI via setup_ui(), launch app tasks
```

`user_app_init()` performs in order:
1. `board_div.VBAT_POWER_ON()`
2. `board_div.POWEER_EPD_ON()`
3. `board_div.POWEER_Audio_ON()`
4. Instantiate `epaper_driver_display` with config from `user_config.h`
5. `EPD_Init()` → `EPD_Clear()` → `EPD_DisplayPartBaseImage()` → `EPD_Init_Partial()`
6. `user_button_init()`

---

## XiaoZhi AI Firmware (Pre-built)

Pre-built firmware binaries are in `example/Firmware/`:

| File | Description |
|------|-------------|
| `V1-FactoryProgram.bin` | V1 factory demo |
| `V1-XiaoZhi.bin` | V1 AI voice assistant |
| `V2-FactoryProgram.bin` | V2 factory demo |
| `V2-XiaoZhi.bin` | V2 AI voice assistant |

Source zips in `example/Example/XiaoZhi/`. If building custom XiaoZhi, replace the `78__xiaozhi-fonts/` directory before compiling (required for B&W e-paper compatibility).

**Flash command:**
```bash
esptool.py --chip esp32s3 -p /dev/tty.usbmodem* write_flash 0x0 V2-XiaoZhi.bin
```

# FIESTAMON — Documentation Index

Platform: **Waveshare ESP32-S3-ePaper-1.54**  
Framework: **ESP-IDF** (≥ 4.1.0 required; v5.x recommended)  
Wiki: https://www.waveshare.com/wiki/ESP32-S3-ePaper-1.54

---

## Documents

| File | Description |
|------|-------------|
| [platform-overview.md](platform-overview.md) | MCU specs, peripherals, e-paper specs, hardware revisions |
| [pin-definitions.md](pin-definitions.md) | Full GPIO pinout with `user_config.h` macros |
| [esp-idf-setup.md](esp-idf-setup.md) | Toolchain install, sdkconfig, IDF Component Manager, build & flash |
| [code-patterns.md](code-patterns.md) | `app_main`, power, EPD driver, I2C, FreeRTOS tasks, LVGL v8 boilerplate, deep sleep |
| [component-reference.md](component-reference.md) | BSP component APIs: `epaper_driver_bsp`, `board_power_bsp`, `button_bsp`, `SensorLib`, `ui_bsp`, `user_app` |

---

## Quick Reference

### Key Hardware Facts
- **MCU:** ESP32-S3-PICO-1-N8R8 — 240 MHz dual-core LX7, 8 MB Flash, 8 MB OPI PSRAM
- **Display:** 1.54" e-paper, 200×200 B&W, SPI2_HOST (`full_refresh = 1` required)
- **RTC:** PCF85063 on I2C (GPIO47/48), addr 0x51
- **Sensor:** SHTC3 temp/humidity on I2C, addr 0x70
- **Audio:** ES8311 codec + MEMS mic + speaker header
- **Storage:** TF/SD card (SPI), GPIO39/40/41
- **Power:** USB-C + optional Li-battery with onboard charge management
- **Variants:** V1 and V2 hardware revisions — match firmware/examples to board label

### sdkconfig Key Settings (quick copy)
```
CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_USB_CDC_ENABLED=y
```

### Minimal Entry Point
```cpp
#include "user_config.h"
#include "user_app.h"

extern "C" void app_main(void)
{
    user_app_init();   // power on, EPD init, buttons
    // your code here
}
```

### Build & Flash
```bash
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/tty.usbmodem* flash monitor
```

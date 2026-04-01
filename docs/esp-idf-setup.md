# ESP-IDF Setup — ESP32-S3-ePaper-1.54

---

## 1. Install ESP-IDF

**Recommended:** VS Code + Espressif IDF extension (installs the toolchain automatically).

1. Install [VS Code](https://code.visualstudio.com/)
2. Install the **Espressif IDF** extension from the VS Code Marketplace
3. Follow the extension wizard to install ESP-IDF v5.x (v5.1+ recommended)

**CLI alternative:**
```bash
# Install prerequisites (macOS)
brew install cmake ninja dfu-util python3

# Clone and install ESP-IDF
mkdir -p ~/esp && cd ~/esp
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf && ./install.sh esp32s3
source export.sh
```

---

## 2. Target Configuration

Every project must be configured for the ESP32-S3 before the first build:

```bash
idf.py set-target esp32s3
```

---

## 3. sdkconfig Settings

The examples include a `sdkconfig.defaults` that pre-sets the critical options. The most important settings are:

| Key                                  | Value              | Reason                              |
|--------------------------------------|--------------------|-------------------------------------|
| `CONFIG_ESPTOOLPY_FLASHSIZE_8MB`     | `y`                | 8 MB Flash                          |
| `CONFIG_SPIRAM_MODE_OCT`             | `y`                | OPI PSRAM (8 MB)                    |
| `CONFIG_SPIRAM`                      | `y`                | Enable PSRAM                        |
| `CONFIG_PARTITION_TABLE_CUSTOM`      | `y`                | Use `partitions.csv`                |
| `CONFIG_USB_CDC_ENABLED`             | `y`                | USB CDC (Serial over USB-C)         |

After initial `idf.py set-target esp32s3`, copy `sdkconfig.defaults` from an example to your project root or run:

```bash
idf.py menuconfig
```

---

## 4. LVGL Dependency (IDF Component Manager)

LVGL is pulled in via the [IDF Component Manager](https://components.espressif.com/). Each example's `main/idf_component.yml` declares:

```yaml
dependencies:
  idf:
    version: '>=4.1.0'
  lvgl/lvgl: ^8.4.0
```

On first build the component manager downloads LVGL automatically. No manual library installation needed.

> **LVGL v8 vs v9:** Examples prefixed `09_LVGL_V8_Test` use `^8.4.0`. Examples prefixed `10_LVGL_V9_Test` declare `^9.x`. Do not mix versions in the same project.

---

## 5. Project Structure

Each ESP-IDF example follows this layout:

```
<example>/
├── CMakeLists.txt              # Top-level CMake; registers components
├── partitions.csv              # Custom partition table
├── sdkconfig                   # Generated config (do not edit by hand)
├── sdkconfig.defaults          # Defaults committed to repo
├── main/
│   ├── CMakeLists.txt
│   ├── idf_component.yml       # IDF Component Manager manifest
│   ├── main.cpp                # app_main() entry point
│   └── user_config.h           # Pin + timing constants — copy to your project
└── components/                 # Local BSP components (included automatically)
    ├── board_power_bsp/        # Power rail control
    ├── button_bsp/             # Button debounce + multi-button
    ├── epaper_driver_bsp/      # EPD SPI driver class
    ├── ui_bsp/                 # LVGL GUI Guider generated UI
    ├── user_app/               # App init + UI glue
    └── SensorLib/              # PCF85063 RTC + SHTC3 driver
```

---

## 6. Build & Flash

```bash
cd example/Example/ESP-IDF/V2/07_BATT_PWR_Test

# First build (downloads LVGL via component manager)
idf.py build

# Flash + monitor (replace /dev/tty.usbmodem* with your port)
idf.py -p /dev/tty.usbmodem* flash monitor

# Monitor only
idf.py -p /dev/tty.usbmodem* monitor
```

Exit monitor with **Ctrl+]**.

---

## 7. Available ESP-IDF Examples (V2)

| Folder                | Entry Point   | Description                         |
|-----------------------|---------------|-------------------------------------|
| `01_ADC_Test`         | `app_main`    | ADC battery voltage read            |
| `02_I2C_PCF85063`     | `app_main`    | PCF85063 RTC set/read               |
| `03_I2C_SHTC3`        | `app_main`    | SHTC3 temperature & humidity        |
| `04_SD_Card`          | `app_main`    | SD card read/write                  |
| `05_WIFI_AP`          | `app_main`    | Wi-Fi Access Point mode             |
| `06_WIFI_STA`         | `app_main`    | Wi-Fi Station mode                  |
| `07_BATT_PWR_Test`    | `app_main`    | Battery power + LVGL UI             |
| `08_Audio_Test`       | `app_main`    | ES8311 audio codec                  |
| `09_LVGL_V8_Test`     | `app_main`    | LVGL v8 GUI on e-paper              |
| `10_LVGL_V9_Test`     | `app_main`    | LVGL v9 GUI on e-paper              |
| `11_FactoryProgram`   | `app_main`    | Full factory test/demo              |
| `12_RTC_Sleep_Test`   | `app_main`    | Deep sleep + RTC wake-up            |

V1 examples are identical except `11_FactoryProgram` may differ in hardware-specific init.

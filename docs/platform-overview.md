# Platform Overview: Waveshare ESP32-S3-ePaper-1.54

**Official Wiki:** https://www.waveshare.com/wiki/ESP32-S3-ePaper-1.54  
**Schematic PDF:** https://files.waveshare.com/wiki/ESP32-S3-ePaper-1.54/ESP32-S3-Touch-ePaper-1.54-Schematic.pdf  
**ESP32-S3 Datasheet:** https://documentation.espressif.com/esp32-s3_datasheet_en.pdf  
**Development Framework:** ESP-IDF

---

## Product Description

The **ESP32-S3-ePaper-1.54** is an e-Paper AIoT development board powered by the ESP32-S3 microcontroller. It is designed for low-power, long-lasting IoT applications such as electronic shelf labels, portable displays, and IoT terminals. Two variants exist:

- **ESP32-S3-ePaper-1.54-EN** — Without lithium battery
- **ESP32-S3-ePaper-1.54** — With lithium battery

---

## Microcontroller

| Property     | Value                                     |
|--------------|-------------------------------------------|
| Module       | ESP32-S3-PICO-1-N8R8                      |
| Architecture | Xtensa® 32-bit LX7 dual-core              |
| Clock Speed  | Up to 240 MHz                             |
| Flash        | 8 MB (integrated)                         |
| PSRAM        | 8 MB OPI PSRAM (integrated)               |
| Wi-Fi        | 2.4 GHz 802.11 b/g/n                      |
| Bluetooth    | BT5 / BLE                                 |

---

## Onboard Peripherals

| Peripheral           | Details                                                     |
|----------------------|-------------------------------------------------------------|
| e-Paper Display      | 1.54-inch, 200×200 px, B&W, SPI (SPI2_HOST)                |
| RTC                  | PCF85063 (I2C addr: 0x51)                                   |
| Temp/Humidity Sensor | SHTC3 (I2C addr: 0x70)                                      |
| Audio Codec          | ES8311 low-power codec, MX1.25 speaker header               |
| Microphone           | Onboard MEMS mic                                            |
| SD Card              | TF card slot (SPI)                                          |
| Buttons              | BOOT (GPIO0), PWR (GPIO18)                                  |
| Battery Charging     | Onboard lithium charge management (With Lithium Battery ver)|
| USB                  | USB Type-C (programming + power)                            |
| Expansion            | 2 × 6-pin 2.54mm female headers                             |

---

## e-Paper Display Parameters

| Parameter        | Value              |
|------------------|--------------------|
| Panel Size       | 1.54 inch          |
| Resolution       | 200 × 200 px       |
| Display Area     | 27.60 × 27.60 mm   |
| Dot Pitch        | 0.138 × 0.138 mm   |
| Gray Levels      | 2 (black/white)    |
| Interface        | SPI (SPI2_HOST)    |
| Sunlight Readable| Yes                |
| Power (static)   | Near zero (power consumed only during refresh) |

---

## Hardware Revisions

The example code ships in two variants:

| Revision | ESP-IDF Path                   | Notes                         |
|----------|-------------------------------|-------------------------------|
| V1       | `example/Example/ESP-IDF/V1/` | First hardware revision       |
| V2       | `example/Example/ESP-IDF/V2/` | Current / recommended revision|

Use the firmware and example code that matches the **V** label printed on your board.

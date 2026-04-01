# Documentation Index

## Game Design

| Document | Description |
|----------|-------------|
| [fiestaquest-design-doc.md](fiestaquest-design-doc.md) | Complete game design: data model, combat mechanics, items, training, rebirth, legacy tree |
| [fiestaquest-architecture.md](fiestaquest-architecture.md) | Technical architecture: layer diagram, module contracts, API signatures, visual test harness |
| [sprite-map.md](sprite-map.md) | Sprite atlas: pixel coordinates for all 168 character frames, items, tiles, icons |
| [CREDITS.md](CREDITS.md) | Asset attribution |

## Hardware

| Document | Description |
|----------|-------------|
| [platform-overview.md](platform-overview.md) | ESP32-S3-ePaper-1.54 specs, peripherals, hardware revisions |
| [pin-definitions.md](pin-definitions.md) | Full GPIO pinout with macro definitions |
| [esp-idf-setup.md](esp-idf-setup.md) | Toolchain install, sdkconfig, build and flash |
| [HARDWARE_QA_CHECKLIST.md](HARDWARE_QA_CHECKLIST.md) | Physical device validation checklist |

## Development

| Document | Description |
|----------|-------------|
| [code-patterns.md](code-patterns.md) | ESP-IDF patterns: app_main, power, EPD driver, FreeRTOS tasks, deep sleep |
| [component-reference.md](component-reference.md) | BSP component APIs |
| [RETRO_LOG.md](RETRO_LOG.md) | Phase retrospectives, advisory tracking, review findings |
| [backlog/BACKLOG.md](backlog/BACKLOG.md) | 15-phase development backlog (all phases complete) |

## Quick Build Reference

```bash
# Host tests (no hardware)
cd test/host && cmake -B build && cmake --build build && ctest --output-on-failure

# Visual regression
cd test/visual && cmake -B build && cmake --build build
./build/render_all_screens && python3 diff_screens.py

# ESP32 target
. ~/esp/esp-idf/export.sh && idf.py build
```

# Phase 12: Hardware Abstraction & HAL 1

## Item 1: Implement E-Paper Display Driver

### User Story
As a firmware developer, I need to translate my beautiful 200x200 `fq_fb_t` framebuffer into the exact SPI command sequences required by the Waveshare 1.54" V2 E-paper display.

### Acceptance Criteria
- [ ] `components/hal/include/hal_epaper.h` provides `hal_epaper_init()` and `hal_epaper_flush(const fq_fb_t* fb)`.
- [ ] Implements the partial RAM update commands (0x24) and Display Update Control (0x22).
- [ ] Correctly initializes the SPI master bus within the ESP-IDF constraints.

### Negative Test Requirements (from spec-challenger)
- **Busy Pin Deadlock:** The `hal_epaper.c` driver must poll the hardware BUSY pin before sending data. If the display controller crashes and holds BUSY high indefinitely, ensure the while-loop uses an `esp_timer` timeout (e.g. 5 seconds) to abort the flush and reset the SPI bus, preventing the entire OS from freezing.
- **SPI Frame Corruption:** Validate that passing a perfectly 0-filled `fq_fb_t` struct does not cause the DMA engine or SPI queue to truncate the 5000-byte transaction.

### Implementation Steps
1. Translate Waveshare Python examples or generic C drivers into strict ESP-IDF `spi_master.h` calls.
2. Add LUTs for fast partial refresh vs full refresh (every 5th update to clear ghosting).

### Test Expectations
- Host builds mock `hal_epaper.c` that just ignores flush operations. Target build writes to physical pins. (No visual unit test for the pure HAL, relied upon in Phase 15).

### Files to Create/Modify
- `components/hal/include/hal_epaper.h`
- `components/hal/src/hal_epaper.c`
- `components/hal/src/mock_hal_epaper.c`

### Commit Messages
- `feat: SPI driver implementation for Waveshare 1-bit e-paper`

---

## Item 2: Implement LittleFS Flash Abstraction

### User Story
As the game state manager, I need to read/write the binary `save_format.h` payload reliably to the ESP32's non-volatile flash partition.

### Acceptance Criteria
- [ ] `components/hal/include/hal_flash.h` implements `hal_flash_read_save(uint8_t *buf)` and `write_save(buf)`.
- [ ] Uses ESP-IDF `esp_vfs_littlefs` to mount the `littlefs` partition defined in Phase 1.
- [ ] Safely creates the file if it does not exist (first boot).

### Negative Test Requirements (from spec-challenger)
- **First-Boot Mount Failure:** If the `littlefs` partition was never formatted (e.g. factory fresh chip), `esp_vfs_littlefs_register` will fail to mount. Verify the code catches `ESP_FAIL`, automatically invokes `format_if_mount_failed=true`, and transparently yields a blank save file.
- **Mid-Write Power Loss:** Since saving takes ~50ms, a power loss during `fwrite` is possible. Test that writes happen to a temporary file (`save.tmp`) and are explicitly renamed (`rename()`) to `save.dat` via atomic VFS operations, preventing half-written file corruption natively.

### Implementation Steps
1. Configure `esp_littlefs` component in CMake.
2. Standard `fopen("/storage/save.dat", "wb")` logic, utilizing atomic rename patterns.

### Test Expectations
- Requires an integration test flashed to target to write payload, deep sleep, wake, and read payload back accurately.

### Files to Create/Modify
- `components/hal/include/hal_flash.h`
- `components/hal/src/hal_flash.c`

### Commit Messages
- `feat: VFS LittleFS mounting and binary blob persistence`

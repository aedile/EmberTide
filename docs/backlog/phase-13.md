# Phase 13: Hardware Abstraction & HAL 2

## Item 1: Implement GPIO Button Driver

### User Story
As a user, clicking the physical buttons must instantly navigate menus, requiring a debounced GPIO interrupt driver that posts to the `event_bus`.

### Acceptance Criteria
- [ ] `components/hal/include/hal_gpio.h` exposes `hal_gpio_init()`.
- [ ] Registers an ESP-IDF `gpio_isr_handler` on Pins X and Y.
- [ ] ISR uses a 50ms software debounce mechanism and calls `fq_post_event` passing `EVT_BTN_A` or `B`.

### Negative Test Requirements (from spec-challenger)
- **Interrupt Storm Matrix:** If a malicious user bridges the button pin with a bare wire, oscillating the pin at 50,000 times a second, ensure the `isr_handler` utilizes a strict timer diff check (`esp_timer_get_time()`) BEFORE querying the Event Bus, absorbing the interrupt storm and preventing FreeRTOS from crashing due to starvation.
- **ISR Watchdog Starvation:** Ensure NO `printf` or blocking delay calls exist inside the hardware ISR, preventing the ESP32's Hardware Watchdog Timer (WDT) from resetting the chip unexpectedly on button press. 

### Implementation Steps
1. Setup interrupts `GPIO_INTR_NEGEDGE`.
2. Push queue event from ISR natively (`xQueueSendFromISR`).

### Test Expectations
- Mocks out the hardware interrupt on the host. Host tests inject events manually. Target test confirms physical pushes register.

### Files to Create/Modify
- `components/hal/src/hal_gpio.c`

### Commit Messages
- `feat: debounced ISR handlers for physical pushbuttons`

---

## Item 2: Implement Audio Piezo Driver

### User Story
As an immersive feature, I need tiny 8-bit style "beeps" and "boops" when attacking or navigating.

### Acceptance Criteria
- [ ] `hal_audio_play(int freq_hz, int duration_ms)` defined in `hal_audio.h`.
- [ ] Uses ESP-IDF LEDC (PWM) peripheral to generate hardware square waves at the given frequency.
- [ ] Non-blocking execution (starts PWM, sets a timer interrupt to turn it off after N ms).

### Negative Test Requirements (from spec-challenger)
- **Overlapping Frequency Churn:** The FSM sends two `hal_audio_play` commands sequentially within 5 milliseconds. Ensure the PWM timer safely resyncs and overrides the previous tone rather than triggering multiple clashing hardware timers that overwrite the LEDC configurations.
- **Stuck Tone Silence:** The tone turn-off timer fails to fire due to a higher-priority task consuming the core. Provide a watchdog fallback inside the main app loop `while(1)` or ensure the hardware timer peripheral is completely immune to FreeRTOS thread starvation.

### Implementation Steps
1. Configure `ledc_timer_config` and `ledc_channel_config`.
2. Start PWM duty cycle.

### Test Expectations
- Empty mock on host. 

### Files to Create/Modify
- `components/hal/src/hal_audio.c`

### Commit Messages
- `feat: non-blocking LEDC square wave piezo generator`

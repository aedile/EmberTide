# Phase 11: Application Event Loop

## Item 1: Implement App Event Bus

### User Story
As an application architect, I need an decoupled Event Bus so that the HAL (e.g., button press interrupts or BLE packets) can asynchronously signal the main game loop without crossing dependency layers or corrupting the PRNG state mid-computation.

### Acceptance Criteria
- [ ] `main/include/event_bus.h` defines `fq_post_event(fq_event_id_t id, void* data)`.
- [ ] Based on FreeRTOS Queues (or a tight `while()` inbox for the host).
- [ ] Defines distinct events: `EVT_BTN_A_PRESS`, `EVT_BTN_B_PRESS`, `EVT_BLE_PACKET_RX`, `EVT_TIMER_TICK`.

### Negative Test Requirements (from spec-challenger)
- **Queue Saturation:** Push 100 events continuously into the mock bus without popping any. Ensure the `xQueueSend` wrapper safely drops the 101st event and logs a recognizable buffer overflow warning rather than hanging the FreeRTOS scheduler indefinitely.
- **Null Payload Injection:** Post an event requiring data (e.g. `EVT_BLE_PACKET_RX`) but pass `NULL` as the data pointer. Assert that the event processing loop safely drops the event rather than dereferencing the payload struct.

### Implementation Steps
1. Create a `freertos/queue.h` wrapper for the target build, and a mock array queue for the `test/host/` build.
2. Ensure pushing to the bus from an ISR context (`fromISR` APIs) is handled correctly if compiled for ESP-IDF.

### Test Expectations
- `test_event_bus.c` queues 50 events in a mock queue and pops them in strict FIFO order, verifying data unboxing.

### Files to Create/Modify
- `main/include/event_bus.h`
- `main/event_bus.c`
- `test/host/test_event_bus.c`

### Commit Messages
- `feat: implement non-blocking event bus for main IO separation`

---

## Item 2: Implement Root State Machine

### User Story
As a player turning on the device, the software needs a root orchestrator that boots up, shows the Title screen, loads my save, and transitions into the Home screen.

### Acceptance Criteria
- [ ] `main/app_main.c` contains the primary `while(1)` loop.
- [ ] FSM handles `STATE_BOOT`, `STATE_TITLE`, `STATE_HOME`, `STATE_BATTLE`, etc.
- [ ] On state transition, it invokes the corresponding View Model Builder.

### Negative Test Requirements (from spec-challenger)
- **Ghost Event Processing:** From `STATE_TITLE`, dispatch an `EVT_BATTLE_ROUND_TICK` (an event only relevant during combat). Ensure the FSM `switch` block drops the event securely rather than attempting to render a combat screen without an initialized `fq_combat_ctx_t`.
- **Render Loop Deadlock:** Ensure that if `hal_epaper_flush` blocks for 1 second (due to hardware busy flags), the `event_bus_pop` can still accrue inbound button presses in the background queue rather than dropping user input entirely.

### Implementation Steps
1. Wire `event_bus_pop()` at the top of the loop.
2. Delegate button presses to the current state handler.
3. Call `test/visual/` renderer logic (for host testing) or `hal_epaper_flush` (for target).

### Test Expectations
- `test_app_fsm.c` pushes a "START_PRESSED" event to the queue and verifies the global state variable transitions from TITLE to HOME.

### Files to Create/Modify
- `main/app_main.c`
- `test/host/test_app_fsm.c`

### Commit Messages
- `feat: implement main application thread FSM switchboard`

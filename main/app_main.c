/**
 * app_main.c — FiestaQuest Application Entry Point
 *
 * Bootstrap, FreeRTOS task creation, event loop, and view model builder.
 *
 * This is the ONLY module that has visibility into all layers:
 *   - Reads game state from game/ components.
 *   - Constructs view models and dispatches to presentation/.
 *   - Wires connectivity/ services to the event bus.
 *   - Calls platform/ services for hardware initialization.
 *
 * Architecture constraint: game/ and presentation/ are NEVER coupled to
 * each other. The application layer is the sole orchestrator.
 */

#include <stdint.h>

/**
 * app_main — ESP-IDF application entry point.
 *
 * Called by ESP-IDF after the FreeRTOS scheduler starts. All hardware
 * initialization, task creation, and event loop setup begins here.
 *
 * Phase 1: empty skeleton. Filled in during later phases.
 */
void app_main(void)
{
    /* Phase 1 skeleton — no implementation yet. */
}

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
 *
 * Phase 11: event bus and root FSM wired. Main loop skeleton present.
 */

#include <stdint.h>
#include <string.h>

#include "types.h"
#include "event_bus.h"
#include "app_fsm.h"

/**
 * app_main — ESP-IDF application entry point.
 *
 * Called by ESP-IDF after the FreeRTOS scheduler starts. All hardware
 * initialization, task creation, and event loop setup begins here.
 *
 * Phase 11: Initializes player, inventory, and the root application context.
 * The main event loop skeleton is present as a comment. Full BLE, display,
 * and input wiring is deferred to the HAL integration phase (see Rule 8
 * advisory logged against this PR).
 */
void app_main(void)
{
    static fq_character_t player;
    static fq_inventory_t inventory;
    static fq_app_ctx_t   app;

    memset(&player,    0, sizeof(player));
    memset(&inventory, 0, sizeof(inventory));
    fq_app_init(&app, &player, &inventory);

    /* Main event loop — wired to hardware inputs and FreeRTOS queues
     * in the HAL integration phase. For now the loop body is a comment
     * to allow idf.py build to succeed without FreeRTOS task deps.
     *
     * while (1) {
     *     fq_event_t evt;
     *     if (fq_event_bus_pop(&app.bus, &evt)) {
     *         fq_app_dispatch(&app, &evt);
     *     }
     *     // Platform sleep / vTaskDelay here.
     * }
     */
}

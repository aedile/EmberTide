/**
 * event_bus.h — FiestaQuest Application Layer: Event Bus
 *
 * A portable, statically-allocated ring-buffer event queue. On device this
 * would be backed by a FreeRTOS queue; this implementation is a plain C ring
 * buffer for host-test compatibility and maximum determinism.
 *
 * Architecture placement: main/ (application layer).
 *   - This header MUST NOT include any hal_*.h, game/, or presentation/
 *     headers. It is a pure transport mechanism.
 *
 * Design invariants:
 *   - All storage is in the fq_event_bus_t struct (no globals, no heap).
 *   - All functions are NULL-safe: passing a NULL bus or NULL out pointer is
 *     a safe no-op that returns the failure sentinel (0).
 *   - overflow_count saturates at UINT8_MAX and never wraps.
 *   - head/tail are uint8_t; modulo FQ_EVENT_QUEUE_SIZE wraps them correctly.
 *     Since FQ_EVENT_QUEUE_SIZE (16) divides 256 evenly, no special wrapping
 *     logic is needed — plain % is used.
 *
 * Constitution Priority 0: This file contains no PRNG calls and no combat
 * logic. It is a pure data-structure module.
 *
 * Host-compilable: no hal_*.h included.
 */

#ifndef FIESTAQUEST_MAIN_EVENT_BUS_H
#define FIESTAQUEST_MAIN_EVENT_BUS_H

#include <stdint.h>

/** Number of event slots in the ring buffer. */
#define FQ_EVENT_QUEUE_SIZE 16u

/* ---------------------------------------------------------------------------
 * fq_event_id_t — all application event identifiers.
 *
 * Values are pinned to their integer positions — must not be reordered.
 * Adding new events must append before FQ_EVT_COUNT.
 * ---------------------------------------------------------------------------*/
typedef enum {
    FQ_EVT_NONE                  = 0,  /**< Sentinel — unused, never posted. */
    FQ_EVT_BTN_A_PRESS           = 1,  /**< Button A short press. */
    FQ_EVT_BTN_B_PRESS           = 2,  /**< Button B short press. */
    FQ_EVT_BTN_A_LONG            = 3,  /**< Button A long press (held > threshold). */
    FQ_EVT_BTN_B_LONG            = 4,  /**< Button B long press. */
    FQ_EVT_TIMER_TICK            = 5,  /**< Periodic timer tick (e.g., 100 ms). */
    FQ_EVT_BLE_PACKET_RX         = 6,  /**< BLE packet received; data = packet ptr (cast). */
    FQ_EVT_BLE_CONNECTED         = 7,  /**< BLE peer connected. */
    FQ_EVT_BLE_DISCONNECTED      = 8,  /**< BLE peer disconnected. */
    FQ_EVT_COMBAT_ROUND_COMPLETE = 9,  /**< One combat round resolved; data = finished flag. */
    FQ_EVT_SAVE_COMPLETE         = 10, /**< NVS save finished. */
    FQ_EVT_OTA_PROGRESS          = 11, /**< OTA update progress; data = percent 0-100. */
    FQ_EVT_AUTO_SLEEP_TIMEOUT    = 12, /**< Inactivity sleep timer expired. */
    FQ_EVT_COUNT                 = 13  /**< Sentinel — number of valid event IDs. */
} fq_event_id_t;

/* ---------------------------------------------------------------------------
 * fq_event_t — a single event in the bus.
 *
 * data is a generic 32-bit payload. Callers cast to/from pointer or value
 * as appropriate for each event ID.
 * ---------------------------------------------------------------------------*/
typedef struct {
    fq_event_id_t id;    /**< Event identifier. */
    uint32_t      data;  /**< Generic payload (cast to pointer or value as needed). */
} fq_event_t;

/* ---------------------------------------------------------------------------
 * fq_event_bus_t — the full ring buffer state.
 *
 * head       : next write position (index of next slot to fill on post).
 * tail       : next read position (index of oldest unconsumed event on pop).
 * count      : number of events currently in the ring (0..FQ_EVENT_QUEUE_SIZE).
 * overflow_count: events dropped due to a full queue; saturates at UINT8_MAX.
 *
 * All fields are uint8_t — adequate for a 16-slot ring.
 * ---------------------------------------------------------------------------*/
typedef struct {
    fq_event_t events[FQ_EVENT_QUEUE_SIZE]; /**< Ring buffer storage. */
    uint8_t    head;                        /**< Next write index. */
    uint8_t    tail;                        /**< Next read index. */
    uint8_t    count;                       /**< Current occupancy. */
    uint8_t    overflow_count;              /**< Dropped events since last init. */
} fq_event_bus_t;

/* ---------------------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------------------*/

/**
 * fq_event_bus_init() — Initialize (or reinitialize) an event bus.
 *
 * Zeroes all fields: head, tail, count, overflow_count. Callers may call
 * this on an already-used bus to discard all pending events.
 *
 * NULL-safe: no-op if bus is NULL.
 *
 * @param bus  Event bus to initialize.
 */
void fq_event_bus_init(fq_event_bus_t *bus);

/**
 * fq_event_bus_post() — Post an event to the bus.
 *
 * If the bus is full (count == FQ_EVENT_QUEUE_SIZE), the event is dropped and
 * overflow_count is incremented (saturating at UINT8_MAX). Returns 0 on drop.
 *
 * NULL-safe: returns 0 if bus is NULL.
 *
 * @param bus   Event bus. NULL-safe.
 * @param id    Event identifier to post.
 * @param data  Generic 32-bit payload.
 * @return      1 if the event was queued, 0 if dropped (overflow or NULL).
 */
uint8_t fq_event_bus_post(fq_event_bus_t *bus, fq_event_id_t id, uint32_t data);

/**
 * fq_event_bus_pop() — Remove the oldest event from the bus.
 *
 * If the bus is empty, returns 0 and does not modify *out.
 *
 * NULL-safe: returns 0 if bus or out is NULL; if bus is NULL the bus is not
 * modified. If out is NULL the event is NOT removed (caller gets another
 * chance to provide a valid out pointer).
 *
 * @param bus  Event bus. NULL-safe.
 * @param out  Output event. Not modified on failure. NULL-safe (returns 0).
 * @return     1 if an event was popped, 0 if empty or NULL argument.
 */
uint8_t fq_event_bus_pop(fq_event_bus_t *bus, fq_event_t *out);

/**
 * fq_event_bus_pending() — Return the number of pending events.
 *
 * NULL-safe: returns 0 if bus is NULL.
 *
 * @param bus  Event bus (const, not modified).
 * @return     Number of events currently in the ring (0..FQ_EVENT_QUEUE_SIZE).
 */
uint8_t fq_event_bus_pending(const fq_event_bus_t *bus);

#endif /* FIESTAQUEST_MAIN_EVENT_BUS_H */

/**
 * event_bus.c — FiestaQuest Application Layer: Event Bus Implementation
 *
 * Portable ring-buffer implementation of fq_event_bus_t. All state is
 * contained in the passed struct; no global state, no heap allocation.
 *
 * Ring buffer mechanics:
 *   - head  : index of the next empty slot (written on post, advanced modulo 16).
 *   - tail  : index of the oldest populated slot (read on pop, advanced modulo 16).
 *   - count : number of events currently in the ring [0, FQ_EVENT_QUEUE_SIZE].
 *
 * Since FQ_EVENT_QUEUE_SIZE == 16 and all indices are uint8_t, the modulo
 * operation keeps indices in range with no additional wraparound logic.
 *
 * overflow_count saturates at UINT8_MAX rather than wrapping — a saturated
 * diagnostic counter is more useful than a reset one.
 *
 * Constitution Priority 0: No PRNG calls. No floating point. No combat logic.
 */

#include "event_bus.h"
#include <string.h>

/* ---------------------------------------------------------------------------
 * fq_event_bus_init
 * ---------------------------------------------------------------------------*/
void fq_event_bus_init(fq_event_bus_t *bus)
{
    if (bus == NULL) {
        return;
    }
    /* Single memset zeros all fields (head, tail, count, overflow_count) and
     * the events array in one operation, preventing stale data across reinits. */
    memset(bus, 0, sizeof(*bus));
}

/* ---------------------------------------------------------------------------
 * fq_event_bus_post
 * ---------------------------------------------------------------------------*/
uint8_t fq_event_bus_post(fq_event_bus_t *bus, fq_event_id_t id, uint32_t data)
{
    if (bus == NULL) {
        return 0u;
    }
    if (bus->count >= FQ_EVENT_QUEUE_SIZE) {
        /* Queue full — drop event and increment saturating overflow counter. */
        if (bus->overflow_count < 0xFFu) {
            bus->overflow_count++;
        }
        return 0u;
    }
    bus->events[bus->head].id   = id;
    bus->events[bus->head].data = data;
    bus->head = (uint8_t)((bus->head + 1u) % FQ_EVENT_QUEUE_SIZE);
    bus->count++;
    return 1u;
}

/* ---------------------------------------------------------------------------
 * fq_event_bus_pop
 * ---------------------------------------------------------------------------*/
uint8_t fq_event_bus_pop(fq_event_bus_t *bus, fq_event_t *out)
{
    if (bus == NULL || out == NULL) {
        return 0u;
    }
    if (bus->count == 0u) {
        return 0u;
    }
    *out      = bus->events[bus->tail];
    bus->tail = (uint8_t)((bus->tail + 1u) % FQ_EVENT_QUEUE_SIZE);
    bus->count--;
    return 1u;
}

/* ---------------------------------------------------------------------------
 * fq_event_bus_pending
 * ---------------------------------------------------------------------------*/
uint8_t fq_event_bus_pending(const fq_event_bus_t *bus)
{
    if (bus == NULL) {
        return 0u;
    }
    return bus->count;
}

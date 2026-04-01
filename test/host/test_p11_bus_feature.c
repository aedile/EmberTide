/**
 * test_p11_bus_feature.c — Phase 11 Feature Tests: Event Bus
 *
 * Happy-path contract for fq_event_bus_t:
 *   - Init produces an empty bus (count=0, head=0, tail=0, overflow_count=0).
 *   - Single post/pop round-trip preserves id and data.
 *   - FIFO ordering across multiple posts.
 *   - fq_event_bus_pending() returns the correct count.
 *   - All FQ_EVT_* enum values are defined and non-negative.
 *   - FQ_EVENT_QUEUE_SIZE equals 16.
 *   - FQ_EVT_COUNT has the correct integer value.
 */

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>

#include "test_assert.h"
#include "event_bus.h"

int main(void)
{
    /* -----------------------------------------------------------------------
     * Compile-time checks on constants.
     * ----------------------------------------------------------------------- */
    _Static_assert(FQ_EVENT_QUEUE_SIZE == 16u,
        "FQ_EVENT_QUEUE_SIZE must be exactly 16");

    /* -----------------------------------------------------------------------
     * fq_event_bus_init() produces zeroed state.
     * ----------------------------------------------------------------------- */
    fq_event_bus_t bus;
    /* Poison the memory first to prove init actually zeroes */
    memset(&bus, 0xAB, sizeof(bus));
    fq_event_bus_init(&bus);

    TEST_ASSERT_EQUAL_UINT8(0u, bus.head);
    TEST_ASSERT_EQUAL_UINT8(0u, bus.tail);
    TEST_ASSERT_EQUAL_UINT8(0u, bus.count);
    TEST_ASSERT_EQUAL_UINT8(0u, bus.overflow_count);
    TEST_ASSERT_EQUAL_UINT8(0u, fq_event_bus_pending(&bus));

    /* -----------------------------------------------------------------------
     * Single post/pop round-trip.
     * ----------------------------------------------------------------------- */
    uint8_t rc = fq_event_bus_post(&bus, FQ_EVT_BTN_A_PRESS, 42u);
    TEST_ASSERT_EQUAL_UINT8(1u, rc);
    TEST_ASSERT_EQUAL_UINT8(1u, fq_event_bus_pending(&bus));

    fq_event_t out;
    memset(&out, 0xFF, sizeof(out));
    rc = fq_event_bus_pop(&bus, &out);
    TEST_ASSERT_EQUAL_UINT8(1u, rc);
    TEST_ASSERT_EQUAL_INT((int)FQ_EVT_BTN_A_PRESS, (int)out.id);
    TEST_ASSERT_EQUAL_UINT32(42u, out.data);
    TEST_ASSERT_EQUAL_UINT8(0u, fq_event_bus_pending(&bus));

    /* -----------------------------------------------------------------------
     * FIFO ordering: post 5 distinct events, pop in order.
     * ----------------------------------------------------------------------- */
    fq_event_bus_init(&bus);
    fq_event_id_t ids[5] = {
        FQ_EVT_BTN_A_PRESS,
        FQ_EVT_BTN_B_PRESS,
        FQ_EVT_TIMER_TICK,
        FQ_EVT_BLE_CONNECTED,
        FQ_EVT_SAVE_COMPLETE
    };
    for (int i = 0; i < 5; i++) {
        rc = fq_event_bus_post(&bus, ids[i], (uint32_t)(i * 10u));
        TEST_ASSERT_EQUAL_UINT8(1u, rc);
    }
    TEST_ASSERT_EQUAL_UINT8(5u, fq_event_bus_pending(&bus));

    for (int i = 0; i < 5; i++) {
        fq_event_t popped;
        rc = fq_event_bus_pop(&bus, &popped);
        TEST_ASSERT_EQUAL_UINT8(1u, rc);
        TEST_ASSERT_EQUAL_INT((int)ids[i], (int)popped.id);
        TEST_ASSERT_EQUAL_UINT32((uint32_t)(i * 10u), popped.data);
    }
    TEST_ASSERT_EQUAL_UINT8(0u, fq_event_bus_pending(&bus));

    /* -----------------------------------------------------------------------
     * Fill to capacity and drain — no overflow on exactly 16 posts.
     * ----------------------------------------------------------------------- */
    fq_event_bus_init(&bus);
    for (uint8_t i = 0u; i < FQ_EVENT_QUEUE_SIZE; i++) {
        rc = fq_event_bus_post(&bus, FQ_EVT_TIMER_TICK, (uint32_t)i);
        TEST_ASSERT_EQUAL_UINT8(1u, rc);
    }
    TEST_ASSERT_EQUAL_UINT8(0u, bus.overflow_count);
    TEST_ASSERT_EQUAL_UINT8(FQ_EVENT_QUEUE_SIZE, fq_event_bus_pending(&bus));

    /* Drain */
    for (uint8_t i = 0u; i < FQ_EVENT_QUEUE_SIZE; i++) {
        fq_event_t popped;
        rc = fq_event_bus_pop(&bus, &popped);
        TEST_ASSERT_EQUAL_UINT8(1u, rc);
        TEST_ASSERT_EQUAL_UINT32((uint32_t)i, popped.data);
    }
    TEST_ASSERT_EQUAL_UINT8(0u, fq_event_bus_pending(&bus));

    /* -----------------------------------------------------------------------
     * Event id enum range check — all IDs are valid values.
     * ----------------------------------------------------------------------- */
    TEST_ASSERT_EQUAL_INT(0,  (int)FQ_EVT_NONE);
    TEST_ASSERT_EQUAL_INT(1,  (int)FQ_EVT_BTN_A_PRESS);
    TEST_ASSERT_EQUAL_INT(2,  (int)FQ_EVT_BTN_B_PRESS);
    TEST_ASSERT_EQUAL_INT(3,  (int)FQ_EVT_BTN_A_LONG);
    TEST_ASSERT_EQUAL_INT(4,  (int)FQ_EVT_BTN_B_LONG);
    TEST_ASSERT_EQUAL_INT(5,  (int)FQ_EVT_TIMER_TICK);
    TEST_ASSERT_EQUAL_INT(6,  (int)FQ_EVT_BLE_PACKET_RX);
    TEST_ASSERT_EQUAL_INT(7,  (int)FQ_EVT_BLE_CONNECTED);
    TEST_ASSERT_EQUAL_INT(8,  (int)FQ_EVT_BLE_DISCONNECTED);
    TEST_ASSERT_EQUAL_INT(9,  (int)FQ_EVT_COMBAT_ROUND_COMPLETE);
    TEST_ASSERT_EQUAL_INT(10, (int)FQ_EVT_SAVE_COMPLETE);
    TEST_ASSERT_EQUAL_INT(11, (int)FQ_EVT_OTA_PROGRESS);
    TEST_ASSERT_EQUAL_INT(12, (int)FQ_EVT_AUTO_SLEEP_TIMEOUT);
    TEST_ASSERT_EQUAL_INT(13, (int)FQ_EVT_COUNT);

    /* -----------------------------------------------------------------------
     * Data payload round-trip — post with max uint32_t data.
     * ----------------------------------------------------------------------- */
    fq_event_bus_init(&bus);
    fq_event_bus_post(&bus, FQ_EVT_BLE_PACKET_RX, 0xDEADBEEFu);
    fq_event_t max_data_evt;
    fq_event_bus_pop(&bus, &max_data_evt);
    TEST_ASSERT_EQUAL_UINT32(0xDEADBEEFu, max_data_evt.data);

    printf("test_p11_bus_feature: PASS\n");
    return 0;
}

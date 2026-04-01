/**
 * test_p11_bus_bounds.c — Phase 11 Bound Tests: Event Bus
 *
 * Rule 22: Written BEFORE implementation to prove the system REJECTS:
 *   E1:  Queue overflow — 17th post to a 16-slot queue is dropped (overflow_count++)
 *   E2a: NULL bus pointer on post returns 0 safely
 *   E2b: NULL bus pointer on pop returns 0 safely
 *   E2c: NULL out pointer on pop returns 0 safely
 *   E2d: NULL bus on init is a no-op (no crash)
 *   E2e: NULL bus on pending returns 0
 *   N2:  Pop from empty queue returns 0
 *   N7:  Ring-buffer index wraparound: 256+ events with interleaved pops
 *        (head/tail must wrap correctly with uint8_t arithmetic when count
 *         cycles through the 16-slot ring many times)
 *
 * Constitution Priority 0: PRNG isolation check is in test_p11_fsm_bounds.c.
 */

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>

#include "test_assert.h"
/* Include under test — these headers do not exist yet (RED phase). */
#include "event_bus.h"

int main(void)
{
    /* -----------------------------------------------------------------------
     * E2d: NULL bus on init is a no-op (no crash, no UB).
     * ----------------------------------------------------------------------- */
    fq_event_bus_init(NULL);   /* must not crash */
    printf("[PASS] E2d: fq_event_bus_init(NULL) did not crash\n");

    /* -----------------------------------------------------------------------
     * E2e: NULL bus on pending returns 0.
     * ----------------------------------------------------------------------- */
    TEST_ASSERT_EQUAL_UINT8(0u, fq_event_bus_pending(NULL));

    /* -----------------------------------------------------------------------
     * E2a: NULL bus on post returns 0.
     * ----------------------------------------------------------------------- */
    TEST_ASSERT_EQUAL_UINT8(0u, fq_event_bus_post(NULL, FQ_EVT_BTN_A_PRESS, 0u));

    /* -----------------------------------------------------------------------
     * E2b: NULL bus on pop returns 0.
     * ----------------------------------------------------------------------- */
    fq_event_t evt;
    TEST_ASSERT_EQUAL_UINT8(0u, fq_event_bus_pop(NULL, &evt));

    /* -----------------------------------------------------------------------
     * E2c: NULL out pointer on pop returns 0.
     * ----------------------------------------------------------------------- */
    fq_event_bus_t bus;
    fq_event_bus_init(&bus);
    fq_event_bus_post(&bus, FQ_EVT_BTN_A_PRESS, 0u);
    TEST_ASSERT_EQUAL_UINT8(0u, fq_event_bus_pop(&bus, NULL));
    /* event must still be pending since the pop failed */
    TEST_ASSERT_EQUAL_UINT8(1u, fq_event_bus_pending(&bus));

    /* -----------------------------------------------------------------------
     * N2: Pop from empty queue returns 0.
     * ----------------------------------------------------------------------- */
    fq_event_bus_t empty_bus;
    fq_event_bus_init(&empty_bus);
    TEST_ASSERT_EQUAL_UINT8(0u, fq_event_bus_pop(&empty_bus, &evt));

    /* -----------------------------------------------------------------------
     * E1: Queue overflow — post 16 events (fills queue), 17th is dropped.
     * overflow_count must be 1 after the 17th post.
     * ----------------------------------------------------------------------- */
    fq_event_bus_t full_bus;
    fq_event_bus_init(&full_bus);

    /* Fill the queue completely */
    for (uint8_t i = 0u; i < FQ_EVENT_QUEUE_SIZE; i++) {
        uint8_t rc = fq_event_bus_post(&full_bus, FQ_EVT_TIMER_TICK, (uint32_t)i);
        TEST_ASSERT_EQUAL_UINT8(1u, rc);
    }
    /* Queue is now full */
    TEST_ASSERT_EQUAL_UINT8(FQ_EVENT_QUEUE_SIZE, fq_event_bus_pending(&full_bus));
    TEST_ASSERT_EQUAL_UINT8(0u, full_bus.overflow_count);

    /* 17th post must be dropped */
    uint8_t overflow_rc = fq_event_bus_post(&full_bus, FQ_EVT_BTN_A_PRESS, 99u);
    TEST_ASSERT_EQUAL_UINT8(0u, overflow_rc);
    TEST_ASSERT_EQUAL_UINT8(1u, full_bus.overflow_count);

    /* Count must still be FQ_EVENT_QUEUE_SIZE (no slot consumed) */
    TEST_ASSERT_EQUAL_UINT8(FQ_EVENT_QUEUE_SIZE, fq_event_bus_pending(&full_bus));

    /* -----------------------------------------------------------------------
     * E1 extended: 100-post test — 100 posts to a fresh queue; only first 16
     * succeed; overflow_count = 84.
     * ----------------------------------------------------------------------- */
    fq_event_bus_t bus100;
    fq_event_bus_init(&bus100);
    uint8_t success_count = 0u;
    for (uint8_t i = 0u; i < 100u; i++) {
        if (fq_event_bus_post(&bus100, FQ_EVT_TIMER_TICK, (uint32_t)i)) {
            success_count++;
        }
    }
    TEST_ASSERT_EQUAL_UINT8(FQ_EVENT_QUEUE_SIZE, success_count);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)(100u - FQ_EVENT_QUEUE_SIZE), bus100.overflow_count);
    TEST_ASSERT_EQUAL_UINT8(FQ_EVENT_QUEUE_SIZE, fq_event_bus_pending(&bus100));

    /* -----------------------------------------------------------------------
     * N7: Ring-buffer wraparound — post/pop 300 events interleaved.
     *
     * Strategy: alternately post 8 events then pop 8 events, repeating until
     * 300 events have been round-tripped. Verify each popped event id and data
     * match what was posted (FIFO order). Head/tail must wrap correctly.
     * ----------------------------------------------------------------------- */
    fq_event_bus_t ring_bus;
    fq_event_bus_init(&ring_bus);

    uint32_t post_seq = 0u;
    uint32_t pop_seq  = 0u;
    const uint32_t TOTAL_EVENTS = 300u;

    while (pop_seq < TOTAL_EVENTS) {
        /* Post up to 8 events or until total posted == TOTAL_EVENTS */
        uint8_t batch = 8u;
        while (batch > 0u && post_seq < TOTAL_EVENTS) {
            uint8_t rc = fq_event_bus_post(&ring_bus, FQ_EVT_TIMER_TICK, post_seq);
            TEST_ASSERT_EQUAL_UINT8(1u, rc);
            post_seq++;
            batch--;
        }
        /* Pop all pending events in this batch */
        while (fq_event_bus_pending(&ring_bus) > 0u) {
            fq_event_t popped;
            uint8_t pop_rc = fq_event_bus_pop(&ring_bus, &popped);
            TEST_ASSERT_EQUAL_UINT8(1u, pop_rc);
            TEST_ASSERT_EQUAL_INT((int)FQ_EVT_TIMER_TICK, (int)popped.id);
            TEST_ASSERT_EQUAL_UINT32(pop_seq, popped.data);
            pop_seq++;
        }
    }
    TEST_ASSERT_EQUAL_UINT32(TOTAL_EVENTS, post_seq);
    TEST_ASSERT_EQUAL_UINT32(TOTAL_EVENTS, pop_seq);
    TEST_ASSERT_EQUAL_UINT8(0u, fq_event_bus_pending(&ring_bus));
    TEST_ASSERT_EQUAL_UINT8(0u, ring_bus.overflow_count);

    /* -----------------------------------------------------------------------
     * N1: Double init clears queue — post some events, re-init, verify empty.
     * ----------------------------------------------------------------------- */
    fq_event_bus_t reinit_bus;
    fq_event_bus_init(&reinit_bus);
    fq_event_bus_post(&reinit_bus, FQ_EVT_BTN_A_PRESS, 0u);
    fq_event_bus_post(&reinit_bus, FQ_EVT_BTN_B_PRESS, 1u);
    TEST_ASSERT_EQUAL_UINT8(2u, fq_event_bus_pending(&reinit_bus));

    fq_event_bus_init(&reinit_bus);   /* double init */
    TEST_ASSERT_EQUAL_UINT8(0u, fq_event_bus_pending(&reinit_bus));
    TEST_ASSERT_EQUAL_UINT8(0u, reinit_bus.overflow_count);
    TEST_ASSERT_EQUAL_UINT8(0u, reinit_bus.head);
    TEST_ASSERT_EQUAL_UINT8(0u, reinit_bus.tail);

    printf("test_p11_bus_bounds: PASS\n");
    return 0;
}

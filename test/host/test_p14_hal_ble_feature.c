/**
 * test_p14_hal_ble_feature.c — Phase 14 Feature tests for hal_ble BLE GATT API.
 *
 * Rule 22: Written BEFORE implementation (FEATURE RED).
 * Tested happy-path behaviours:
 *   - init with valid callback returns OK
 *   - start_advertising() after init → state becomes ADVERTISING
 *   - simulate_connect() transitions state to CONNECTED
 *   - send while connected captures data in mock and returns OK
 *   - inject_rx() fires the registered rx callback with correct data
 *   - disconnect() transitions state to DISCONNECTED
 *   - state_after_disconnect remains DISCONNECTED (not IDLE)
 *   - re-advertise after disconnect is accepted
 *   - second connect after re-advertise → CONNECTED
 *   - get_last_sent returns exact bytes passed to hal_ble_send()
 *   - mock_ble_reset() wipes all state
 *   - deinit after active connection is safe (state returns to IDLE)
 *   - disconnect() from ADVERTISING → stays ADVERTISING (no-op)
 *   - disconnect() from IDLE → stays IDLE (no-op)
 */

#include "mock_hal_ble.h"
#include "hal_ble.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define ASSERT_EQ(label, expected, actual)                              \
    do {                                                                \
        if ((int)(expected) != (int)(actual)) {                         \
            printf("FAIL [%s]: expected %d got %d\n",                  \
                   (label), (int)(expected), (int)(actual));            \
            return 1;                                                   \
        }                                                               \
        printf("PASS [%s]\n", (label));                                 \
    } while (0)

/* RX callback tracking. */
static uint8_t  g_rx_buf[64];
static uint16_t g_rx_len;
static uint32_t g_rx_fire_count;

static void test_rx_cb(const uint8_t *data, uint16_t len)
{
    g_rx_fire_count++;
    if (len <= (uint16_t)sizeof(g_rx_buf)) {
        memcpy(g_rx_buf, data, len);
    }
    g_rx_len = len;
}

int main(void)
{
    uint8_t send_buf[16];
    uint8_t recv_buf[16];
    uint8_t rx_payload[8] = {0x01u, 0x02u, 0x03u, 0x04u, 0xFF, 0xFE, 0x00u, 0x80u};

    /* ------------------------------------------------------------------ */
    /* 1. Init with valid callback returns OK; state starts IDLE.          */
    /* ------------------------------------------------------------------ */
    mock_ble_reset();
    g_rx_fire_count = 0u;
    memset(g_rx_buf, 0, sizeof(g_rx_buf));

    ASSERT_EQ("init_ok",
              (int)HAL_BLE_OK,
              (int)hal_ble_init(test_rx_cb));
    ASSERT_EQ("state_after_init_idle",
              (int)HAL_BLE_STATE_IDLE,
              (int)hal_ble_get_state());

    /* ------------------------------------------------------------------ */
    /* 2. start_advertising() succeeds; state transitions to ADVERTISING.  */
    /* ------------------------------------------------------------------ */
    ASSERT_EQ("start_advertising_ok",
              (int)HAL_BLE_OK,
              (int)hal_ble_start_advertising());
    ASSERT_EQ("state_after_adv_is_advertising",
              (int)HAL_BLE_STATE_ADVERTISING,
              (int)hal_ble_get_state());

    /* ------------------------------------------------------------------ */
    /* 3. simulate_connect() transitions state to CONNECTED.               */
    /* ------------------------------------------------------------------ */
    mock_ble_simulate_connect();
    ASSERT_EQ("state_after_connect_is_connected",
              (int)HAL_BLE_STATE_CONNECTED,
              (int)hal_ble_get_state());

    /* ------------------------------------------------------------------ */
    /* 4. send() while connected returns OK; data is captured by mock.     */
    /* ------------------------------------------------------------------ */
    memset(send_buf, 0x55u, sizeof(send_buf));
    ASSERT_EQ("send_while_connected_ok",
              (int)HAL_BLE_OK,
              (int)hal_ble_send(send_buf, sizeof(send_buf)));

    memset(recv_buf, 0, sizeof(recv_buf));
    {
        uint16_t sent_len = mock_ble_get_last_sent(recv_buf, sizeof(recv_buf));
        ASSERT_EQ("get_last_sent_len", (int)sizeof(send_buf), (int)sent_len);
        ASSERT_EQ("get_last_sent_byte0", 0x55, (int)recv_buf[0]);
        ASSERT_EQ("get_last_sent_byte15", 0x55, (int)recv_buf[15]);
    }

    /* ------------------------------------------------------------------ */
    /* 5. inject_rx() fires the registered rx callback with correct data.  */
    /* ------------------------------------------------------------------ */
    g_rx_fire_count = 0u;
    memset(g_rx_buf, 0, sizeof(g_rx_buf));
    mock_ble_inject_rx(rx_payload, sizeof(rx_payload));

    ASSERT_EQ("rx_callback_fired_once", 1u, g_rx_fire_count);
    ASSERT_EQ("rx_len_correct", (int)sizeof(rx_payload), (int)g_rx_len);
    ASSERT_EQ("rx_byte0", 0x01, (int)g_rx_buf[0]);
    ASSERT_EQ("rx_byte3", 0x04, (int)g_rx_buf[3]);
    ASSERT_EQ("rx_byte7", 0x80, (int)(g_rx_buf[7]));

    /* ------------------------------------------------------------------ */
    /* 6. disconnect() transitions state to DISCONNECTED.                  */
    /* ------------------------------------------------------------------ */
    ASSERT_EQ("disconnect_ok",
              (int)HAL_BLE_OK,
              (int)hal_ble_disconnect());
    ASSERT_EQ("state_after_disconnect_is_disconnected",
              (int)HAL_BLE_STATE_DISCONNECTED,
              (int)hal_ble_get_state());

    /* ------------------------------------------------------------------ */
    /* 7. simulate_disconnect() via mock also sets DISCONNECTED.           */
    /* ------------------------------------------------------------------ */
    mock_ble_reset();
    hal_ble_init(test_rx_cb);
    hal_ble_start_advertising();
    mock_ble_simulate_connect();
    mock_ble_simulate_disconnect();
    ASSERT_EQ("state_after_sim_disconnect",
              (int)HAL_BLE_STATE_DISCONNECTED,
              (int)hal_ble_get_state());

    /* ------------------------------------------------------------------ */
    /* 8. Re-advertise after disconnect returns OK.                        */
    /* ------------------------------------------------------------------ */
    ASSERT_EQ("re_advertise_after_disconnect_ok",
              (int)HAL_BLE_OK,
              (int)hal_ble_start_advertising());
    ASSERT_EQ("state_after_re_adv_is_advertising",
              (int)HAL_BLE_STATE_ADVERTISING,
              (int)hal_ble_get_state());

    /* ------------------------------------------------------------------ */
    /* 9. Second connect after re-advertise works.                         */
    /* ------------------------------------------------------------------ */
    mock_ble_simulate_connect();
    ASSERT_EQ("second_connect_ok",
              (int)HAL_BLE_STATE_CONNECTED,
              (int)hal_ble_get_state());

    /* ------------------------------------------------------------------ */
    /* 10. mock_ble_reset() wipes all state; state returns to IDLE.        */
    /* ------------------------------------------------------------------ */
    mock_ble_reset();
    ASSERT_EQ("state_after_mock_reset_is_idle",
              (int)HAL_BLE_STATE_IDLE,
              (int)hal_ble_get_state());

    /* ------------------------------------------------------------------ */
    /* 11. deinit after active connection is safe; state returns to IDLE.  */
    /* ------------------------------------------------------------------ */
    hal_ble_init(test_rx_cb);
    hal_ble_start_advertising();
    mock_ble_simulate_connect();
    hal_ble_deinit();
    ASSERT_EQ("state_after_deinit_is_idle",
              (int)HAL_BLE_STATE_IDLE,
              (int)hal_ble_get_state());

    /* ------------------------------------------------------------------ */
    /* 12. disconnect() from ADVERTISING state is a no-op (stays           */
    /*     ADVERTISING). Only CONNECTED transitions to DISCONNECTED.       */
    /* ------------------------------------------------------------------ */
    mock_ble_reset();
    hal_ble_init(test_rx_cb);
    hal_ble_start_advertising();
    ASSERT_EQ("disconnect_from_advertising_ok",
              (int)HAL_BLE_OK,
              (int)hal_ble_disconnect());
    ASSERT_EQ("state_after_disconnect_from_adv_stays_advertising",
              (int)HAL_BLE_STATE_ADVERTISING,
              (int)hal_ble_get_state());

    /* ------------------------------------------------------------------ */
    /* 13. disconnect() from IDLE state is a no-op (stays IDLE).           */
    /* ------------------------------------------------------------------ */
    mock_ble_reset();
    ASSERT_EQ("disconnect_from_idle_ok",
              (int)HAL_BLE_OK,
              (int)hal_ble_disconnect());
    ASSERT_EQ("state_after_disconnect_from_idle_stays_idle",
              (int)HAL_BLE_STATE_IDLE,
              (int)hal_ble_get_state());

    return 0;
}

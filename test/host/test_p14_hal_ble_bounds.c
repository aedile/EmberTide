/**
 * test_p14_hal_ble_bounds.c — Phase 14 Bound tests for hal_ble BLE GATT API.
 *
 * Rule 22: Written BEFORE feature tests and BEFORE implementation (BOUND RED).
 * Tests prove the system REJECTS:
 *   - NULL rx callback at init                  → HAL_BLE_ERR_NULL
 *   - send() before init()                      → HAL_BLE_ERR_INIT
 *   - send() with NULL data pointer             → HAL_BLE_ERR_NULL
 *   - send() with len > HAL_BLE_MAX_MTU         → HAL_BLE_ERR_MTU_EXCEEDED
 *   - send() exactly at HAL_BLE_MAX_MTU (256)   → HAL_BLE_ERR_MTU_EXCEEDED (>256 only)
 *   - send() when not connected                 → HAL_BLE_ERR_NOT_CONNECTED
 *   - inject_rx() with NULL data                → safe (no crash, no callback fire)
 *   - HAL_BLE_MAX_MTU constant value locked to 256
 *   - enum value contracts locked
 *   - double deinit is safe
 *   - get_state() before init returns IDLE
 *   - start_advertising() before init           → HAL_BLE_ERR_INIT
 */

#include "hal_ble.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Forward-declare mock accessors (defined in mock_hal_ble.c). */
void     mock_ble_reset(void);
void     mock_ble_simulate_connect(void);
void     mock_ble_simulate_disconnect(void);
void     mock_ble_inject_rx(const uint8_t *data, uint16_t len);
uint16_t mock_ble_get_last_sent(uint8_t *buf, uint16_t buf_len);

#define ASSERT_EQ(label, expected, actual)                              \
    do {                                                                \
        if ((int)(expected) != (int)(actual)) {                         \
            printf("FAIL [%s]: expected %d got %d\n",                  \
                   (label), (int)(expected), (int)(actual));            \
            return 1;                                                   \
        }                                                               \
        printf("PASS [%s]\n", (label));                                 \
    } while (0)

#define ASSERT_TRUE(label, cond)                                        \
    do {                                                                \
        if (!(cond)) {                                                  \
            printf("FAIL [%s]: condition was false\n", (label));        \
            return 1;                                                   \
        }                                                               \
        printf("PASS [%s]\n", (label));                                 \
    } while (0)

/* Dummy callback — only used to satisfy init. */
static uint32_t g_rx_fire_count = 0u;
static void dummy_rx_cb(const uint8_t *data, uint16_t len)
{
    (void)data;
    (void)len;
    g_rx_fire_count++;
}

int main(void)
{
    /* Scratch buffer for send calls. */
    static uint8_t big_buf[257];
    memset(big_buf, 0xAB, sizeof(big_buf));

    mock_ble_reset();

    /* ------------------------------------------------------------------
     * Constant contract locks.
     * HAL_BLE_MAX_MTU must equal 256.
     * ------------------------------------------------------------------ */
    ASSERT_EQ("ble_max_mtu_is_256", 256, (int)HAL_BLE_MAX_MTU);

    /* ------------------------------------------------------------------
     * Enum value contract locks.
     * ------------------------------------------------------------------ */
    ASSERT_EQ("ble_ok_is_zero",              0, (int)HAL_BLE_OK);
    ASSERT_EQ("ble_err_init_is_one",         1, (int)HAL_BLE_ERR_INIT);
    ASSERT_EQ("ble_err_not_connected_is_two",2, (int)HAL_BLE_ERR_NOT_CONNECTED);
    ASSERT_EQ("ble_err_mtu_exceeded_is_three",3,(int)HAL_BLE_ERR_MTU_EXCEEDED);
    ASSERT_EQ("ble_err_null_is_four",        4, (int)HAL_BLE_ERR_NULL);
    ASSERT_EQ("ble_err_send_is_five",        5, (int)HAL_BLE_ERR_SEND);

    ASSERT_EQ("ble_state_idle_is_zero",      0, (int)HAL_BLE_STATE_IDLE);
    ASSERT_EQ("ble_state_advertising_is_one",1, (int)HAL_BLE_STATE_ADVERTISING);
    ASSERT_EQ("ble_state_connected_is_two",  2, (int)HAL_BLE_STATE_CONNECTED);
    ASSERT_EQ("ble_state_disconnected_is_three",3,(int)HAL_BLE_STATE_DISCONNECTED);

    /* ------------------------------------------------------------------
     * get_state() before init must return IDLE.
     * ------------------------------------------------------------------ */
    mock_ble_reset();
    ASSERT_EQ("get_state_before_init_is_idle",
              (int)HAL_BLE_STATE_IDLE,
              (int)hal_ble_get_state());

    /* ------------------------------------------------------------------
     * NULL rx callback rejected at init.
     * ------------------------------------------------------------------ */
    ASSERT_EQ("null_rx_callback_rejected",
              HAL_BLE_ERR_NULL,
              hal_ble_init(NULL));

    /* ------------------------------------------------------------------
     * start_advertising() before init returns ERR_INIT.
     * ------------------------------------------------------------------ */
    mock_ble_reset();
    ASSERT_EQ("advertise_before_init_rejected",
              HAL_BLE_ERR_INIT,
              hal_ble_start_advertising());

    /* ------------------------------------------------------------------
     * send() before init() must return ERR_INIT (not a crash).
     * Test NULL data FIRST (guard order: NULL -> MTU -> connected -> init).
     * ------------------------------------------------------------------ */
    mock_ble_reset();

    /* NULL data pointer — highest priority guard. */
    ASSERT_EQ("send_null_data_before_init",
              HAL_BLE_ERR_NULL,
              hal_ble_send(NULL, 10u));

    /* Non-NULL data, MTU exceeded — second guard. */
    ASSERT_EQ("send_mtu_exceeded_before_init",
              HAL_BLE_ERR_MTU_EXCEEDED,
              hal_ble_send(big_buf, HAL_BLE_MAX_MTU + 1u));

    /* Non-NULL data, valid MTU, but not initialised — init guard. */
    ASSERT_EQ("send_before_init_rejected",
              HAL_BLE_ERR_INIT,
              hal_ble_send(big_buf, 10u));

    /* ------------------------------------------------------------------
     * After init, send() when not connected → ERR_NOT_CONNECTED.
     * ------------------------------------------------------------------ */
    mock_ble_reset();
    g_rx_fire_count = 0u;
    hal_ble_init(dummy_rx_cb);
    hal_ble_start_advertising();

    ASSERT_EQ("send_when_advertising_not_connected",
              HAL_BLE_ERR_NOT_CONNECTED,
              hal_ble_send(big_buf, 10u));

    /* ------------------------------------------------------------------
     * MTU overflow: len == HAL_BLE_MAX_MTU + 1 (257) is over the limit.
     * The check is: len > HAL_BLE_MAX_MTU; so 257 must be rejected.
     * ------------------------------------------------------------------ */
    mock_ble_reset();
    hal_ble_init(dummy_rx_cb);
    mock_ble_simulate_connect();

    ASSERT_EQ("send_257_bytes_rejected",
              HAL_BLE_ERR_MTU_EXCEEDED,
              hal_ble_send(big_buf, 257u));

    /* ------------------------------------------------------------------
     * MTU boundary: len == HAL_BLE_MAX_MTU (256) must be accepted when
     * connected (not rejected). This proves the check is strictly >, not >=.
     * We just assert the error is NOT HAL_BLE_ERR_MTU_EXCEEDED.
     * ------------------------------------------------------------------ */
    {
        hal_ble_err_t rc = hal_ble_send(big_buf, 256u);
        ASSERT_TRUE("send_exactly_256_not_mtu_error",
                    rc != HAL_BLE_ERR_MTU_EXCEEDED);
    }

    /* ------------------------------------------------------------------
     * inject_rx() with NULL data must not invoke the callback.
     * The mock must guard on data == NULL and silently skip the callback.
     * ------------------------------------------------------------------ */
    mock_ble_reset();
    g_rx_fire_count = 0u;
    hal_ble_init(dummy_rx_cb);
    mock_ble_inject_rx(NULL, 5u);
    ASSERT_EQ("inject_null_data_no_callback", 0u, (uint32_t)g_rx_fire_count);

    /* ------------------------------------------------------------------
     * Double deinit is safe — no crash, no undefined behaviour.
     * ------------------------------------------------------------------ */
    mock_ble_reset();
    hal_ble_init(dummy_rx_cb);
    hal_ble_deinit();
    hal_ble_deinit(); /* must not crash */
    ASSERT_TRUE("double_deinit_safe", 1);

    /* ------------------------------------------------------------------
     * State after deinit is IDLE.
     * ------------------------------------------------------------------ */
    ASSERT_EQ("state_after_deinit_is_idle",
              (int)HAL_BLE_STATE_IDLE,
              (int)hal_ble_get_state());

    return 0;
}

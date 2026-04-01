/**
 * mock_hal_ble.c — Host mock for hal_ble BLE GATT server driver.
 *
 * Linked by test/host/ targets instead of components/hal/src/hal_ble.c.
 * Simulates BLE GATT behaviour entirely in RAM:
 *   - Stores the registered rx_callback pointer.
 *   - mock_ble_simulate_connect()    — sets state to CONNECTED.
 *   - mock_ble_simulate_disconnect() — sets state to DISCONNECTED.
 *   - mock_ble_inject_rx(data, len)  — validates data != NULL, then calls
 *     the registered rx_callback with (data, len).
 *   - mock_ble_get_last_sent(buf, buf_len) — copies the last data passed to
 *     hal_ble_send() into buf (up to buf_len bytes). Returns the number of
 *     bytes that were sent in the last call (not the clamped copy size).
 *   - mock_ble_get_send_count() — returns the total count of successful sends
 *     since the last mock_ble_reset() or hal_ble_init().
 *   - mock_ble_reset() — clears all state to power-on defaults.
 *
 * Guard order for hal_ble_send():  NULL -> MTU -> init -> connected -> capture.
 * This order is consistent with the header documentation and the bound tests.
 *
 * Design note: The NimBLE MTU negotiation and GAP supervisor timeout logic
 * present in the real target driver are NOT simulated here. Tests inject
 * events directly through the mock_ accessor functions.
 */

#include "mock_hal_ble.h"
#include "hal_ble.h"
#include <string.h>

/* -------------------------------------------------------------------------
 * Internal mock state (all file-scope static).
 * -------------------------------------------------------------------------
 */
static hal_ble_rx_callback_t s_rx_callback;
static uint8_t               s_initialized;
static hal_ble_state_t       s_state;

/* Capture buffer for the last successful hal_ble_send() call. */
static uint8_t  s_last_sent_buf[HAL_BLE_MAX_MTU];
static uint16_t s_last_sent_len;
static uint32_t s_send_count;

/* -------------------------------------------------------------------------
 * Public hal_ble API — mock implementations.
 * -------------------------------------------------------------------------
 */

hal_ble_err_t hal_ble_init(hal_ble_rx_callback_t rx_cb)
{
    if (!rx_cb) {
        return HAL_BLE_ERR_NULL;
    }
    s_rx_callback   = rx_cb;
    s_initialized   = 1u;
    s_state         = HAL_BLE_STATE_IDLE;
    s_last_sent_len = 0u;
    s_send_count    = 0u;
    return HAL_BLE_OK;
}

hal_ble_err_t hal_ble_start_advertising(void)
{
    if (!s_initialized) {
        return HAL_BLE_ERR_INIT;
    }
    s_state = HAL_BLE_STATE_ADVERTISING;
    return HAL_BLE_OK;
}

hal_ble_err_t hal_ble_send(const uint8_t *data, uint16_t len)
{
    /* Guard 1: NULL data pointer. */
    if (!data) {
        return HAL_BLE_ERR_NULL;
    }
    /* Guard 2: MTU limit. */
    if (len > (uint16_t)HAL_BLE_MAX_MTU) {
        return HAL_BLE_ERR_MTU_EXCEEDED;
    }
    /* Guard 3: driver must be initialised. */
    if (!s_initialized) {
        return HAL_BLE_ERR_INIT;
    }
    /* Guard 4: must be connected. */
    if (s_state != HAL_BLE_STATE_CONNECTED) {
        return HAL_BLE_ERR_NOT_CONNECTED;
    }
    /* Capture the payload for test inspection. */
    memcpy(s_last_sent_buf, data, len);
    s_last_sent_len = len;
    s_send_count++;
    return HAL_BLE_OK;
}

hal_ble_state_t hal_ble_get_state(void)
{
    return s_state;
}

hal_ble_err_t hal_ble_disconnect(void)
{
    if (s_state == HAL_BLE_STATE_CONNECTED) {
        s_state = HAL_BLE_STATE_DISCONNECTED;
    }
    return HAL_BLE_OK;
}

void hal_ble_deinit(void)
{
    s_rx_callback   = (hal_ble_rx_callback_t)0;
    s_initialized   = 0u;
    s_state         = HAL_BLE_STATE_IDLE;
    s_last_sent_len = 0u;
}

/* -------------------------------------------------------------------------
 * Test accessor functions — host-only, declared in mock_hal_ble.h.
 * -------------------------------------------------------------------------
 */

/**
 * mock_ble_simulate_connect — Inject a GAP connection event.
 *
 * Sets state to HAL_BLE_STATE_CONNECTED, mirroring the target behaviour
 * where the NimBLE BLE_GAP_EVENT_CONNECT callback fires after pairing.
 * No-op (silently ignored) if the driver has not been initialised.
 */
void mock_ble_simulate_connect(void)
{
    if (!s_initialized) {
        return;
    }
    s_state = HAL_BLE_STATE_CONNECTED;
}

/**
 * mock_ble_simulate_disconnect — Inject a GAP disconnect / supervisor timeout.
 *
 * Sets state to HAL_BLE_STATE_DISCONNECTED, mirroring the target behaviour
 * where BLE_GAP_EVENT_DISCONNECT fires after peer resets or link supervison
 * timeout expires.
 * No-op (silently ignored) if the driver has not been initialised.
 */
void mock_ble_simulate_disconnect(void)
{
    if (!s_initialized) {
        return;
    }
    s_state = HAL_BLE_STATE_DISCONNECTED;
}

/**
 * mock_ble_inject_rx — Deliver a received payload to the registered rx callback.
 *
 * @p data NULL guard: if data is NULL, the callback is NOT invoked and the
 * function returns immediately. This prevents the callback from receiving a
 * NULL pointer, which would be undefined behaviour on the target anyway.
 *
 * @param data  Pointer to the received bytes (must not be NULL for callback to fire).
 * @param len   Number of bytes in @p data.
 */
void mock_ble_inject_rx(const uint8_t *data, uint16_t len)
{
    if (!data) {
        return;
    }
    if (s_rx_callback) {
        s_rx_callback(data, len);
    }
}

/**
 * mock_ble_get_last_sent — Copy the last data passed to hal_ble_send().
 *
 * Copies min(s_last_sent_len, buf_len) bytes into @p buf.
 * Returns s_last_sent_len regardless of @p buf_len (so the caller can detect
 * truncation if needed).
 *
 * Returns 0 and does not touch @p buf if no successful send has occurred
 * since the last mock_ble_reset() or hal_ble_init().
 *
 * @param buf      Destination buffer. Must not be NULL if buf_len > 0.
 * @param buf_len  Size of @p buf in bytes.
 * @return         Number of bytes that were in the last sent payload.
 */
uint16_t mock_ble_get_last_sent(uint8_t *buf, uint16_t buf_len)
{
    uint16_t copy_len;
    if (!buf || s_last_sent_len == 0u) {
        return s_last_sent_len;
    }
    copy_len = (s_last_sent_len < buf_len) ? s_last_sent_len : buf_len;
    memcpy(buf, s_last_sent_buf, copy_len);
    return s_last_sent_len;
}

/**
 * mock_ble_get_send_count — Return the total number of successful sends.
 *
 * Counts every hal_ble_send() call that passed all guards and captured data
 * since the last mock_ble_reset() or hal_ble_init().
 *
 * @return  Total successful send count.
 */
uint32_t mock_ble_get_send_count(void)
{
    return s_send_count;
}

/**
 * mock_ble_reset — Reset all mock state to power-on defaults.
 *
 * Clears callback, initialized flag, state, send capture buffer, and send count.
 * Call at the start of each test main() for a clean slate.
 */
void mock_ble_reset(void)
{
    s_rx_callback   = (hal_ble_rx_callback_t)0;
    s_initialized   = 0u;
    s_state         = HAL_BLE_STATE_IDLE;
    s_last_sent_len = 0u;
    s_send_count    = 0u;
    memset(s_last_sent_buf, 0, sizeof(s_last_sent_buf));
}

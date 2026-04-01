/**
 * hal_ble.c — Hardware Abstraction Layer: BLE GATT Server (target stub)
 *
 * This translation unit is the TARGET implementation linked into the ESP-IDF
 * firmware image. It is NOT compiled on the host — mock_hal_ble.c is used
 * there instead.
 *
 * Target behaviour (ESP-IDF NimBLE):
 *   - Initialises NimBLE host with a single GATT service UUID "FQ01".
 *   - Exposes a writable characteristic for receiving fq_packet_team_sync_t.
 *   - GATT Write callback validates len <= HAL_BLE_MAX_MTU before any copy.
 *   - BLE_GAP_EVENT_DISCONNECT transitions state to HAL_BLE_STATE_DISCONNECTED
 *     to prevent Device A hanging after Device B's chip resets (spec-challenger
 *     silent-drop requirement).
 *
 * Stub policy: all functions compile cleanly without ESP-IDF / NimBLE headers
 * so that `idf.py build` succeeds as a cross-compilation smoke-test.
 * When ESP-IDF NimBLE headers are available, replace the stub bodies with
 * real nimble_port_init(), ble_hs_cfg, ble_svc_gatt_init() calls.
 *
 * Guard order for hal_ble_send(): NULL -> MTU -> init -> connected -> send.
 */

#include "hal_ble.h"
#include <string.h>

static hal_ble_rx_callback_t s_rx_callback;
static uint8_t               s_initialized;
static hal_ble_state_t       s_state;

hal_ble_err_t hal_ble_init(hal_ble_rx_callback_t rx_cb)
{
    if (!rx_cb) {
        return HAL_BLE_ERR_NULL;
    }
    s_rx_callback = rx_cb;
    s_initialized = 1u;
    s_state       = HAL_BLE_STATE_IDLE;
    /*
     * TODO (ESP-IDF wiring): nimble_port_init(); ble_hs_cfg.sync_cb = on_sync;
     * ble_svc_gap_init(); ble_svc_gatt_init(); register FQ01 service table.
     * nimble_port_freertos_init(ble_host_task);
     */
    return HAL_BLE_OK;
}

hal_ble_err_t hal_ble_start_advertising(void)
{
    if (!s_initialized) {
        return HAL_BLE_ERR_INIT;
    }
    s_state = HAL_BLE_STATE_ADVERTISING;
    /*
     * TODO (ESP-IDF wiring): populate struct ble_gap_adv_params,
     * ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
     *                   &adv_params, ble_gap_event_cb, NULL);
     */
    return HAL_BLE_OK;
}

hal_ble_err_t hal_ble_send(const uint8_t *data, uint16_t len)
{
    if (!data) {
        return HAL_BLE_ERR_NULL;
    }
    if (len > (uint16_t)HAL_BLE_MAX_MTU) {
        return HAL_BLE_ERR_MTU_EXCEEDED;
    }
    if (!s_initialized) {
        return HAL_BLE_ERR_INIT;
    }
    if (s_state != HAL_BLE_STATE_CONNECTED) {
        return HAL_BLE_ERR_NOT_CONNECTED;
    }
    /*
     * TODO (ESP-IDF wiring): om = ble_hs_mbuf_from_flat(data, len);
     * rc = ble_gattc_notify_custom(conn_handle, chr_val_handle, om);
     * return (rc == 0) ? HAL_BLE_OK : HAL_BLE_ERR_SEND;
     */
    (void)len;
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
        /*
         * TODO (ESP-IDF wiring): ble_gap_terminate(conn_handle,
         *   BLE_ERR_REM_USER_CONN_TERM);
         */
    }
    return HAL_BLE_OK;
}

void hal_ble_deinit(void)
{
    s_rx_callback = (hal_ble_rx_callback_t)0;
    s_initialized = 0u;
    s_state       = HAL_BLE_STATE_IDLE;
    /*
     * TODO (ESP-IDF wiring): nimble_port_deinit();
     */
}

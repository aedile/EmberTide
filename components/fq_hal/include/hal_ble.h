/**
 * hal_ble.h — Hardware Abstraction Layer: BLE GATT Server
 *
 * Target: ESP32-S3-PICO-1-N8R8 via ESP-IDF NimBLE stack.
 *
 * Architecture constraint: hal/ is the BOTTOM layer. It MUST NOT be included
 * by game/, presentation/, or connectivity/. Upper layers reach hardware
 * exclusively through the platform/ services layer.
 *
 * This header is intentionally host-compilable — it contains no ESP-IDF types.
 * The target implementation (hal_ble.c) uses NimBLE internally; the host mock
 * (mock_hal_ble.c) replaces it entirely for unit tests.
 *
 * Malicious MTU guard (spec-challenger requirement):
 *   hal_ble_send() MUST check len <= HAL_BLE_MAX_MTU before any copy or
 *   transmission. A payload larger than HAL_BLE_MAX_MTU returns
 *   HAL_BLE_ERR_MTU_EXCEEDED without touching the radio buffer.
 *
 * Silent-drop / supervisor timeout (spec-challenger requirement):
 *   The target GAP event callback MUST transition state to
 *   HAL_BLE_STATE_DISCONNECTED on BLE_GAP_EVENT_DISCONNECT so Device A never
 *   hangs in STATE_CONNECTED waiting for packets from a crashed Device B.
 *
 * Phase-20 audit fix (DC-2): hal_ble_get_mac() added.
 *   Returns the device's 6-byte BLE public address. Used by the BLE combat
 *   protocol to seed the nonce with a device-unique value.
 */

#ifndef FIESTAQUEST_HAL_BLE_H
#define FIESTAQUEST_HAL_BLE_H

#include <stdint.h>

/* -------------------------------------------------------------------------
 * Constants.
 * HAL_BLE_MAX_MTU is the hard ceiling for a single BLE write payload.
 * Payloads larger than this value MUST be rejected before any memcpy.
 * -------------------------------------------------------------------------
 */
#define HAL_BLE_MAX_MTU     256u            /**< Maximum BLE write payload (bytes). */
#define HAL_BLE_SERVICE_UUID "FQ01"         /**< FiestaQuest GATT service UUID string. */

/** Length of a BLE MAC address in bytes. */
#define HAL_BLE_MAC_LEN     6u

/* -------------------------------------------------------------------------
 * Return codes for hal_ble operations.
 * -------------------------------------------------------------------------
 */
typedef enum {
    HAL_BLE_OK              = 0, /**< Operation completed successfully. */
    HAL_BLE_ERR_INIT        = 1, /**< Driver not initialised. */
    HAL_BLE_ERR_NOT_CONNECTED = 2, /**< No peer connected; send not possible. */
    HAL_BLE_ERR_MTU_EXCEEDED  = 3, /**< Payload exceeds HAL_BLE_MAX_MTU. */
    HAL_BLE_ERR_NULL          = 4, /**< Caller passed a NULL pointer. */
    HAL_BLE_ERR_SEND          = 5  /**< Underlying BLE send failed. */
} hal_ble_err_t;

/* -------------------------------------------------------------------------
 * BLE connection state.
 * -------------------------------------------------------------------------
 */
typedef enum {
    HAL_BLE_STATE_IDLE          = 0, /**< Driver not started. */
    HAL_BLE_STATE_ADVERTISING   = 1, /**< Advertising; no peer connected. */
    HAL_BLE_STATE_CONNECTED     = 2, /**< Peer device connected. */
    HAL_BLE_STATE_DISCONNECTED  = 3  /**< Peer disconnected (or timed out). */
} hal_ble_state_t;

/* -------------------------------------------------------------------------
 * Callback type invoked when a BLE write is received from the peer.
 *
 * On the target this is called from the NimBLE host task after MTU validation.
 * On the host mock it is called directly from mock_ble_inject_rx().
 *
 * @param data  Pointer to received bytes (never NULL when invoked).
 * @param len   Number of bytes in @p data (always <= HAL_BLE_MAX_MTU).
 * -------------------------------------------------------------------------
 */
typedef void (*hal_ble_rx_callback_t)(const uint8_t *data, uint16_t len);

/**
 * hal_ble_init — Initialise BLE GATT server and register RX callback.
 *
 * @param rx_cb  Function called on each validated incoming write. Must not be NULL.
 * @return HAL_BLE_OK       on success.
 *         HAL_BLE_ERR_NULL if rx_cb is NULL.
 */
hal_ble_err_t hal_ble_init(hal_ble_rx_callback_t rx_cb);

/**
 * hal_ble_start_advertising — Begin GAP advertising with the FiestaQuest UUID.
 *
 * Transitions state from HAL_BLE_STATE_IDLE or HAL_BLE_STATE_DISCONNECTED to
 * HAL_BLE_STATE_ADVERTISING. Requires prior successful hal_ble_init().
 *
 * @return HAL_BLE_OK        on success.
 *         HAL_BLE_ERR_INIT  if hal_ble_init() was not called.
 */
hal_ble_err_t hal_ble_start_advertising(void);

/**
 * hal_ble_send — Transmit @p len bytes to the connected peer.
 *
 * Guard order: NULL check -> MTU check -> connected check -> send.
 *
 * @param data  Buffer to transmit. Must not be NULL.
 * @param len   Number of bytes to transmit. Must be <= HAL_BLE_MAX_MTU.
 * @return HAL_BLE_OK                on success.
 *         HAL_BLE_ERR_NULL          if data is NULL.
 *         HAL_BLE_ERR_MTU_EXCEEDED  if len > HAL_BLE_MAX_MTU.
 *         HAL_BLE_ERR_NOT_CONNECTED if no peer is connected.
 *         HAL_BLE_ERR_INIT          if driver was not initialised.
 *         HAL_BLE_ERR_SEND          on underlying send failure.
 */
hal_ble_err_t hal_ble_send(const uint8_t *data, uint16_t len);

/**
 * hal_ble_get_state — Return the current BLE connection state.
 *
 * Safe to call at any time (never requires init).
 *
 * @return Current hal_ble_state_t value.
 */
hal_ble_state_t hal_ble_get_state(void);

/**
 * hal_ble_get_mac — Read the device's 6-byte BLE public MAC address.
 *
 * On the target this reads the ESP32 efuse BLE address via esp_read_mac().
 * The stub/mock returns a fixed test address {0x01, 0x02, 0x03, 0x04, 0x05, 0x06}.
 * The mock allows the address to be overridden via mock_ble_set_mac() for
 * test scenarios that require a specific address.
 *
 * Guard: mac_out must not be NULL — returns HAL_BLE_ERR_NULL if it is.
 *
 * @param mac_out  Output buffer of exactly HAL_BLE_MAC_LEN (6) bytes.
 *                 Written MSB-first (mac_out[0] = most significant byte).
 * @return HAL_BLE_OK       on success.
 *         HAL_BLE_ERR_NULL if mac_out is NULL.
 */
hal_ble_err_t hal_ble_get_mac(uint8_t mac_out[6]);

/**
 * hal_ble_disconnect — Terminate the active connection.
 *
 * No-op (returns HAL_BLE_OK) if not currently connected.
 * Transitions state to HAL_BLE_STATE_DISCONNECTED.
 *
 * @return HAL_BLE_OK      always.
 */
hal_ble_err_t hal_ble_disconnect(void);

/**
 * hal_ble_deinit — Tear down the BLE stack and release all resources.
 *
 * Safe to call without a preceding init (no-op in that case).
 * Safe to call multiple times.
 */
void hal_ble_deinit(void);

#endif /* FIESTAQUEST_HAL_BLE_H */

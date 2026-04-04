/**
 * mock_hal_ble.h — Public interface for the hal_ble host mock.
 *
 * Test files #include this header instead of using forward declarations.
 * Only compiled in the host test environment — never on the target.
 *
 * Phase-20 audit fix (DC-2): mock_ble_get_mac() and mock_ble_set_mac() added
 * for test injection of the device BLE MAC address.
 */

#ifndef FIESTAQUEST_MOCK_HAL_BLE_H
#define FIESTAQUEST_MOCK_HAL_BLE_H

#include <stdint.h>
#include "hal_ble.h"

void     mock_ble_reset(void);
void     mock_ble_simulate_connect(void);
void     mock_ble_simulate_disconnect(void);
void     mock_ble_inject_rx(const uint8_t *data, uint16_t len);
uint16_t mock_ble_get_last_sent(uint8_t *buf, uint16_t buf_len);
uint32_t mock_ble_get_send_count(void);

/**
 * mock_ble_set_mac — Override the MAC address returned by hal_ble_get_mac().
 *
 * Copies @p mac (HAL_BLE_MAC_LEN bytes) into the mock's MAC buffer.
 * Subsequent calls to hal_ble_get_mac() will return this value.
 * mock_ble_reset() restores the default test MAC.
 *
 * @param mac  Pointer to a 6-byte MAC address. Must not be NULL.
 */
void     mock_ble_set_mac(const uint8_t mac[6]);

/**
 * mock_ble_get_mac_call_count — Return the number of hal_ble_get_mac() calls.
 *
 * Resets to 0 on mock_ble_reset().
 *
 * @return  Number of times hal_ble_get_mac() was called since last reset.
 */
uint32_t mock_ble_get_mac_call_count(void);

#endif /* FIESTAQUEST_MOCK_HAL_BLE_H */

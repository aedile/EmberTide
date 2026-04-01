/**
 * mock_hal_ble.h — Public interface for the hal_ble host mock.
 *
 * Test files #include this header instead of using forward declarations.
 * Only compiled in the host test environment — never on the target.
 */

#ifndef FIESTAQUEST_MOCK_HAL_BLE_H
#define FIESTAQUEST_MOCK_HAL_BLE_H

#include <stdint.h>

void     mock_ble_reset(void);
void     mock_ble_simulate_connect(void);
void     mock_ble_simulate_disconnect(void);
void     mock_ble_inject_rx(const uint8_t *data, uint16_t len);
uint16_t mock_ble_get_last_sent(uint8_t *buf, uint16_t buf_len);
uint32_t mock_ble_get_send_count(void);

#endif /* FIESTAQUEST_MOCK_HAL_BLE_H */

/**
 * mock_hal_wifi.h — Public interface for the hal_wifi host mock.
 *
 * Test files #include this header instead of using forward declarations.
 * Only compiled in the host test environment — never on the target.
 */

#ifndef FIESTAQUEST_MOCK_HAL_WIFI_H
#define FIESTAQUEST_MOCK_HAL_WIFI_H

void        mock_wifi_reset(void);
const char *mock_wifi_get_last_ssid(void);
const char *mock_wifi_get_last_password(void);
void        mock_wifi_simulate_connected(void);
void        mock_wifi_simulate_link_lost(void);

#endif /* FIESTAQUEST_MOCK_HAL_WIFI_H */

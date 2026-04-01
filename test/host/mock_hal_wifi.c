/**
 * mock_hal_wifi.c — Host mock for hal_wifi WiFi driver (SoftAP + STA).
 *
 * Linked by test/host/ targets instead of components/hal/src/hal_wifi.c.
 * Simulates WiFi behaviour entirely in RAM:
 *   - Tracks initialisation state and current hal_wifi_state_t.
 *   - Validates SSID/password inputs identically to the real driver.
 *   - Captures the last SSID passed to start_ap() or connect_sta().
 *   - mock_wifi_reset() clears all state to power-on defaults.
 *   - mock_wifi_get_last_ssid() returns the last captured SSID string.
 *
 * Guard order for hal_wifi_start_ap():   NULL -> length -> init -> start AP.
 * Guard order for hal_wifi_connect_sta(): NULL(ssid) -> NULL(pass) -> length -> init.
 *
 * Design note: HTTPD socket limits, recv_wait_timeout, and NVS flash writes
 * present in the real target driver are NOT simulated here.
 */

#include "hal_wifi.h"
#include <string.h>

/* -------------------------------------------------------------------------
 * Internal mock state (all file-scope static).
 * -------------------------------------------------------------------------
 */
static uint8_t          s_initialized;
static hal_wifi_state_t s_state;

/* Capture buffer for the last SSID passed to start_ap or connect_sta. */
static char s_last_ssid[HAL_WIFI_SSID_MAX];

/* -------------------------------------------------------------------------
 * Public hal_wifi API — mock implementations.
 * -------------------------------------------------------------------------
 */

hal_wifi_err_t hal_wifi_init(void)
{
    s_initialized = 1u;
    s_state       = HAL_WIFI_STATE_IDLE;
    return HAL_WIFI_OK;
}

hal_wifi_err_t hal_wifi_start_ap(const char *ssid)
{
    /* Guard 1: NULL SSID. */
    if (!ssid) {
        return HAL_WIFI_ERR_NULL;
    }
    /* Guard 2: SSID length.  strlen(ssid) must be < HAL_WIFI_SSID_MAX to
     * leave room for the NUL terminator inside the ESP32 WiFi driver's
     * 32-byte ssid field. */
    if (strlen(ssid) >= (size_t)HAL_WIFI_SSID_MAX) {
        return HAL_WIFI_ERR_SSID_TOO_LONG;
    }
    /* Guard 3: driver must be initialised. */
    if (!s_initialized) {
        return HAL_WIFI_ERR_INIT;
    }
    /* Capture SSID and update state. */
    strncpy(s_last_ssid, ssid, HAL_WIFI_SSID_MAX - 1u);
    s_last_ssid[HAL_WIFI_SSID_MAX - 1u] = '\0';
    s_state = HAL_WIFI_STATE_AP_MODE;
    return HAL_WIFI_OK;
}

hal_wifi_err_t hal_wifi_connect_sta(const char *ssid, const char *password)
{
    /* Guard 1: NULL SSID. */
    if (!ssid) {
        return HAL_WIFI_ERR_NULL;
    }
    /* Guard 2: NULL password (even empty string "" is valid for open nets). */
    if (!password) {
        return HAL_WIFI_ERR_NULL;
    }
    /* Guard 3: SSID length. */
    if (strlen(ssid) >= (size_t)HAL_WIFI_SSID_MAX) {
        return HAL_WIFI_ERR_SSID_TOO_LONG;
    }
    /* Guard 4: driver must be initialised. */
    if (!s_initialized) {
        return HAL_WIFI_ERR_INIT;
    }
    /* Capture SSID and update state. */
    strncpy(s_last_ssid, ssid, HAL_WIFI_SSID_MAX - 1u);
    s_last_ssid[HAL_WIFI_SSID_MAX - 1u] = '\0';
    s_state = HAL_WIFI_STATE_STA_CONNECTED;
    return HAL_WIFI_OK;
}

hal_wifi_state_t hal_wifi_get_state(void)
{
    return s_state;
}

hal_wifi_err_t hal_wifi_disconnect(void)
{
    s_state = HAL_WIFI_STATE_IDLE;
    return HAL_WIFI_OK;
}

void hal_wifi_deinit(void)
{
    s_initialized = 0u;
    s_state       = HAL_WIFI_STATE_IDLE;
}

/* -------------------------------------------------------------------------
 * Test accessor functions — host-only, not declared in hal_wifi.h.
 * -------------------------------------------------------------------------
 */

/**
 * mock_wifi_get_last_ssid — Return a pointer to the last captured SSID.
 *
 * Returns the SSID passed to the most recent successful hal_wifi_start_ap()
 * or hal_wifi_connect_sta() call.  Returns an empty string ("") if no
 * successful AP or STA start has occurred since mock_wifi_reset().
 *
 * The returned pointer is valid until the next mock_wifi_reset() call.
 *
 * @return  Pointer to the null-terminated SSID string. Never NULL.
 */
const char *mock_wifi_get_last_ssid(void)
{
    return s_last_ssid;
}

/**
 * mock_wifi_reset — Reset all mock state to power-on defaults.
 *
 * Clears initialized flag, state, and captured SSID.
 * Call at the start of each test main() for a clean slate.
 */
void mock_wifi_reset(void)
{
    s_initialized = 0u;
    s_state       = HAL_WIFI_STATE_IDLE;
    memset(s_last_ssid, 0, sizeof(s_last_ssid));
}

/**
 * hal_wifi.c — Hardware Abstraction Layer: WiFi SoftAP + STA (target stub)
 *
 * This translation unit is the TARGET implementation linked into the ESP-IDF
 * firmware image. It is NOT compiled on the host — mock_hal_wifi.c is used
 * there instead.
 *
 * Target behaviour (ESP-IDF WiFi + HTTPD):
 *   - hal_wifi_start_ap() configures WIFI_MODE_AP, starts the NVS-backed
 *     credential form HTTPD on 192.168.4.1.
 *   - HTTPD config MUST set max_open_sockets=4 and recv_wait_timeout=3 to
 *     prevent socket starvation from rapid browser refreshes (spec-challenger
 *     DoS mitigation requirement).
 *   - hal_wifi_connect_sta() configures WIFI_MODE_STA and calls
 *     esp_wifi_connect().
 *   - SSID is validated before any ESP-IDF call to prevent driver crash
 *     from >32-char SSID (spec-challenger buffer-overflow requirement).
 *
 * Stub policy: all functions compile cleanly without ESP-IDF headers so that
 * `idf.py build` succeeds as a cross-compilation smoke-test.
 *
 * Guard order for start_ap():     NULL -> length -> init -> start.
 * Guard order for connect_sta():  NULL(ssid) -> NULL(pass) -> length -> init.
 */

#include "hal_wifi.h"
#include <string.h>

static uint8_t          s_initialized;
static hal_wifi_state_t s_state;

hal_wifi_err_t hal_wifi_init(void)
{
    s_initialized = 1u;
    s_state       = HAL_WIFI_STATE_IDLE;
    /*
     * TODO (ESP-IDF wiring): esp_netif_init(); esp_event_loop_create_default();
     * esp_netif_create_default_wifi_ap(); wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
     * esp_wifi_init(&cfg);
     */
    return HAL_WIFI_OK;
}

hal_wifi_err_t hal_wifi_start_ap(const char *ssid)
{
    if (!ssid) {
        return HAL_WIFI_ERR_NULL;
    }
    if (strlen(ssid) >= (size_t)HAL_WIFI_SSID_MAX) {
        return HAL_WIFI_ERR_SSID_TOO_LONG;
    }
    if (!s_initialized) {
        return HAL_WIFI_ERR_INIT;
    }
    s_state = HAL_WIFI_STATE_AP_MODE;
    /*
     * TODO (ESP-IDF wiring):
     *   wifi_config_t cfg = {0};
     *   strncpy((char*)cfg.ap.ssid, ssid, sizeof(cfg.ap.ssid));
     *   cfg.ap.max_connection = 4;
     *   esp_wifi_set_mode(WIFI_MODE_AP);
     *   esp_wifi_set_config(WIFI_IF_AP, &cfg);
     *   esp_wifi_start();
     *   httpd_config_t http_cfg = HTTPD_DEFAULT_CONFIG();
     *   http_cfg.max_open_sockets = 4;
     *   http_cfg.recv_wait_timeout = 3;
     *   httpd_start(&s_httpd_handle, &http_cfg);
     *   httpd_register_uri_handler(s_httpd_handle, &root_handler);
     */
    return HAL_WIFI_OK;
}

hal_wifi_err_t hal_wifi_connect_sta(const char *ssid, const char *password)
{
    if (!ssid) {
        return HAL_WIFI_ERR_NULL;
    }
    if (!password) {
        return HAL_WIFI_ERR_NULL;
    }
    if (strlen(ssid) >= (size_t)HAL_WIFI_SSID_MAX) {
        return HAL_WIFI_ERR_SSID_TOO_LONG;
    }
    if (!s_initialized) {
        return HAL_WIFI_ERR_INIT;
    }
    s_state = HAL_WIFI_STATE_STA_CONNECTING;
    /*
     * TODO (ESP-IDF wiring):
     *   wifi_config_t cfg = {0};
     *   strncpy((char*)cfg.sta.ssid,     ssid,     sizeof(cfg.sta.ssid));
     *   strncpy((char*)cfg.sta.password, password, sizeof(cfg.sta.password));
     *   esp_wifi_set_mode(WIFI_MODE_STA);
     *   esp_wifi_set_config(WIFI_IF_STA, &cfg);
     *   esp_wifi_start();
     *   esp_wifi_connect();
     *   (state updated to STA_CONNECTED/STA_DISCONNECTED via WIFI_EVENT handler)
     */
    (void)password;
    return HAL_WIFI_OK;
}

hal_wifi_state_t hal_wifi_get_state(void)
{
    return s_state;
}

hal_wifi_err_t hal_wifi_disconnect(void)
{
    s_state = HAL_WIFI_STATE_IDLE;
    /*
     * TODO (ESP-IDF wiring): esp_wifi_disconnect(); esp_wifi_stop();
     * If httpd_handle != NULL: httpd_stop(s_httpd_handle);
     */
    return HAL_WIFI_OK;
}

void hal_wifi_deinit(void)
{
    s_initialized = 0u;
    s_state       = HAL_WIFI_STATE_IDLE;
    /*
     * TODO (ESP-IDF wiring): esp_wifi_deinit(); esp_netif_deinit();
     */
}

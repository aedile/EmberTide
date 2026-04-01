/**
 * hal_wifi.h — Hardware Abstraction Layer: WiFi (SoftAP captive portal + STA)
 *
 * Target: ESP32-S3-PICO-1-N8R8 via ESP-IDF WiFi + HTTPD stack.
 *
 * Architecture constraint: hal/ is the BOTTOM layer. It MUST NOT be included
 * by game/, presentation/, or connectivity/. Upper layers reach hardware
 * exclusively through the platform/ services layer.
 *
 * This header is intentionally host-compilable — it contains no ESP-IDF types.
 * The target implementation (hal_wifi.c) uses ESP-IDF internally; the host mock
 * (mock_hal_wifi.c) replaces it entirely for unit tests.
 *
 * SSID buffer-overflow guard (spec-challenger requirement):
 *   hal_wifi_start_ap() and hal_wifi_connect_sta() MUST check that ssid fits
 *   within HAL_WIFI_SSID_MAX bytes (including NUL terminator). Longer strings
 *   return HAL_WIFI_ERR_SSID_TOO_LONG without touching ESP-IDF WiFi drivers,
 *   which crash if initialised with an ssid > 32 characters.
 *
 * HTTPD DoS mitigation (spec-challenger requirement):
 *   The target httpd_config_t MUST set max_open_sockets=4 and
 *   recv_wait_timeout=3 to prevent socket starvation from rapid refreshes.
 */

#ifndef FIESTAQUEST_HAL_WIFI_H
#define FIESTAQUEST_HAL_WIFI_H

#include <stdint.h>

/* -------------------------------------------------------------------------
 * Constants.
 * -------------------------------------------------------------------------
 */
#define HAL_WIFI_SSID_MAX  32u  /**< Max SSID length including NUL terminator. */
#define HAL_WIFI_PASS_MAX  64u  /**< Max password length including NUL terminator. */

/* -------------------------------------------------------------------------
 * Return codes for hal_wifi operations.
 * -------------------------------------------------------------------------
 */
typedef enum {
    HAL_WIFI_OK               = 0, /**< Operation completed successfully. */
    HAL_WIFI_ERR_INIT         = 1, /**< Driver not initialised. */
    HAL_WIFI_ERR_CONNECT      = 2, /**< WiFi connection attempt failed. */
    HAL_WIFI_ERR_NULL         = 3, /**< Caller passed a NULL pointer. */
    HAL_WIFI_ERR_SSID_TOO_LONG = 4, /**< SSID length exceeds HAL_WIFI_SSID_MAX. */
    HAL_WIFI_ERR_NOT_CONNECTED = 5  /**< Not connected; operation not available. */
} hal_wifi_err_t;

/* -------------------------------------------------------------------------
 * WiFi operational state.
 * -------------------------------------------------------------------------
 */
typedef enum {
    HAL_WIFI_STATE_IDLE            = 0, /**< Driver not started. */
    HAL_WIFI_STATE_AP_MODE         = 1, /**< SoftAP mode active (captive portal). */
    HAL_WIFI_STATE_STA_CONNECTING  = 2, /**< Station mode; associating. */
    HAL_WIFI_STATE_STA_CONNECTED   = 3, /**< Station mode; IP obtained. */
    HAL_WIFI_STATE_STA_DISCONNECTED = 4 /**< Station mode; link lost. */
} hal_wifi_state_t;

/**
 * hal_wifi_init — Initialise WiFi subsystem.
 *
 * Must be called before hal_wifi_start_ap() or hal_wifi_connect_sta().
 * Safe to call multiple times (re-init resets state to IDLE).
 *
 * @return HAL_WIFI_OK        on success.
 *         HAL_WIFI_ERR_INIT  if underlying ESP-IDF init fails (target only).
 */
hal_wifi_err_t hal_wifi_init(void);

/**
 * hal_wifi_start_ap — Enter SoftAP (captive portal) mode.
 *
 * Starts HTTPD on 192.168.4.1 serving the credential form.
 * SSID must be non-NULL and fit within HAL_WIFI_SSID_MAX bytes.
 *
 * Guard order: NULL check -> length check -> init check -> start AP.
 *
 * @param ssid  Access point name. Must not be NULL. Length (excl. NUL) must
 *              be < HAL_WIFI_SSID_MAX (i.e. at most 31 printable chars).
 * @return HAL_WIFI_OK                on success.
 *         HAL_WIFI_ERR_NULL          if ssid is NULL.
 *         HAL_WIFI_ERR_SSID_TOO_LONG if strlen(ssid) >= HAL_WIFI_SSID_MAX.
 *         HAL_WIFI_ERR_INIT          if hal_wifi_init() was not called.
 */
hal_wifi_err_t hal_wifi_start_ap(const char *ssid);

/**
 * hal_wifi_connect_sta — Connect to an existing access point (station mode).
 *
 * Guard order: NULL check (ssid) -> NULL check (password) -> length check -> init check.
 *
 * @param ssid      Target network name. Must not be NULL.
 * @param password  Pre-shared key. Must not be NULL (pass "" for open networks).
 * @return HAL_WIFI_OK                on success.
 *         HAL_WIFI_ERR_NULL          if ssid or password is NULL.
 *         HAL_WIFI_ERR_SSID_TOO_LONG if strlen(ssid) >= HAL_WIFI_SSID_MAX.
 *         HAL_WIFI_ERR_INIT          if hal_wifi_init() was not called.
 *         HAL_WIFI_ERR_CONNECT       if association fails (target only).
 */
hal_wifi_err_t hal_wifi_connect_sta(const char *ssid, const char *password);

/**
 * hal_wifi_get_state — Return the current WiFi state.
 *
 * Safe to call at any time (never requires init).
 *
 * @return Current hal_wifi_state_t value.
 */
hal_wifi_state_t hal_wifi_get_state(void);

/**
 * hal_wifi_disconnect — Disconnect from AP or stop SoftAP mode.
 *
 * No-op (returns HAL_WIFI_OK) if already idle.
 * Transitions state to HAL_WIFI_STATE_IDLE.
 *
 * @return HAL_WIFI_OK      always.
 */
hal_wifi_err_t hal_wifi_disconnect(void);

/**
 * hal_wifi_deinit — Tear down WiFi subsystem and release resources.
 *
 * Safe to call without a preceding init (no-op in that case).
 * Safe to call multiple times.
 */
void hal_wifi_deinit(void);

#endif /* FIESTAQUEST_HAL_WIFI_H */

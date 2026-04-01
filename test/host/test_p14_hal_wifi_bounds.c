/**
 * test_p14_hal_wifi_bounds.c — Phase 14 Bound tests for hal_wifi API.
 *
 * Rule 22: Written BEFORE feature tests and BEFORE implementation (BOUND RED).
 * Tests prove the system REJECTS:
 *   - NULL SSID to start_ap() before init         → HAL_WIFI_ERR_NULL
 *   - NULL SSID to start_ap() after init          → HAL_WIFI_ERR_NULL
 *   - SSID of exactly HAL_WIFI_SSID_MAX chars (32) → HAL_WIFI_ERR_SSID_TOO_LONG
 *   - SSID longer than HAL_WIFI_SSID_MAX (250 chars)→ HAL_WIFI_ERR_SSID_TOO_LONG
 *   - start_ap() before init()                    → HAL_WIFI_ERR_INIT
 *   - NULL SSID to connect_sta()                  → HAL_WIFI_ERR_NULL
 *   - NULL password to connect_sta()              → HAL_WIFI_ERR_NULL
 *   - SSID too long to connect_sta()              → HAL_WIFI_ERR_SSID_TOO_LONG
 *   - password of exactly HAL_WIFI_PASS_MAX (64)  → HAL_WIFI_ERR_PASS_TOO_LONG
 *   - connect_sta() before init()                 → HAL_WIFI_ERR_INIT
 *   - get_state() before init returns IDLE
 *   - double deinit is safe
 *   - HAL_WIFI_SSID_MAX constant locked to 32
 *   - HAL_WIFI_PASS_MAX constant locked to 64
 *   - enum value contracts locked
 */

#include "mock_hal_wifi.h"
#include "hal_wifi.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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

/*
 * long_ssid_32: exactly HAL_WIFI_SSID_MAX (32) printable chars + NUL.
 * strlen == 32 >= HAL_WIFI_SSID_MAX, so the driver must reject it.
 */
static const char k_long_ssid_32[33] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ012345";

/*
 * long_ssid_250: 250 'X' chars + NUL.  Well over the limit.
 * Populated via memset at test start — avoids GCC-only range initialiser.
 */
static char k_long_ssid_250[251];

/*
 * long_pass_64: exactly HAL_WIFI_PASS_MAX (64) printable chars + NUL.
 * strlen == 64 >= HAL_WIFI_PASS_MAX, so the driver must reject it.
 */
static char k_long_pass_64[65];

int main(void)
{
    static const char valid_ssid[] = "FiestaQuest-AP";
    static const char valid_pass[] = "secret123";

    /* Populate long strings without GCC range-initialiser extension. */
    memset(k_long_ssid_250, 'X', 250);
    k_long_ssid_250[250] = '\0';

    memset(k_long_pass_64, 'P', 64);
    k_long_pass_64[64] = '\0';

    mock_wifi_reset();

    /* ------------------------------------------------------------------
     * Constant contract locks.
     * ------------------------------------------------------------------ */
    ASSERT_EQ("wifi_ssid_max_is_32",  32, (int)HAL_WIFI_SSID_MAX);
    ASSERT_EQ("wifi_pass_max_is_64",  64, (int)HAL_WIFI_PASS_MAX);

    /* ------------------------------------------------------------------
     * Enum value contract locks.
     * ------------------------------------------------------------------ */
    ASSERT_EQ("wifi_ok_is_zero",               0, (int)HAL_WIFI_OK);
    ASSERT_EQ("wifi_err_init_is_one",           1, (int)HAL_WIFI_ERR_INIT);
    ASSERT_EQ("wifi_err_connect_is_two",        2, (int)HAL_WIFI_ERR_CONNECT);
    ASSERT_EQ("wifi_err_null_is_three",         3, (int)HAL_WIFI_ERR_NULL);
    ASSERT_EQ("wifi_err_ssid_too_long_is_four", 4, (int)HAL_WIFI_ERR_SSID_TOO_LONG);
    ASSERT_EQ("wifi_err_not_connected_is_five", 5, (int)HAL_WIFI_ERR_NOT_CONNECTED);
    ASSERT_EQ("wifi_err_pass_too_long_is_six",  6, (int)HAL_WIFI_ERR_PASS_TOO_LONG);

    ASSERT_EQ("wifi_state_idle_is_zero",            0, (int)HAL_WIFI_STATE_IDLE);
    ASSERT_EQ("wifi_state_ap_mode_is_one",           1, (int)HAL_WIFI_STATE_AP_MODE);
    ASSERT_EQ("wifi_state_sta_connecting_is_two",    2, (int)HAL_WIFI_STATE_STA_CONNECTING);
    ASSERT_EQ("wifi_state_sta_connected_is_three",   3, (int)HAL_WIFI_STATE_STA_CONNECTED);
    ASSERT_EQ("wifi_state_sta_disconnected_is_four", 4, (int)HAL_WIFI_STATE_STA_DISCONNECTED);

    /* ------------------------------------------------------------------
     * get_state() before init must return IDLE.
     * ------------------------------------------------------------------ */
    mock_wifi_reset();
    ASSERT_EQ("get_state_before_init_is_idle",
              (int)HAL_WIFI_STATE_IDLE,
              (int)hal_wifi_get_state());

    /* ------------------------------------------------------------------
     * NULL SSID to start_ap() before init → ERR_NULL.
     * The NULL guard fires before the init guard.
     * ------------------------------------------------------------------ */
    mock_wifi_reset();
    ASSERT_EQ("null_ssid_before_init",
              HAL_WIFI_ERR_NULL,
              hal_wifi_start_ap(NULL));

    /* ------------------------------------------------------------------
     * start_ap() before init() → ERR_INIT.
     * ------------------------------------------------------------------ */
    mock_wifi_reset();
    ASSERT_EQ("start_ap_before_init",
              HAL_WIFI_ERR_INIT,
              hal_wifi_start_ap(valid_ssid));

    /* ------------------------------------------------------------------
     * NULL SSID to start_ap() after init → ERR_NULL.
     * ------------------------------------------------------------------ */
    mock_wifi_reset();
    hal_wifi_init();
    ASSERT_EQ("start_ap_null_ssid",
              HAL_WIFI_ERR_NULL,
              hal_wifi_start_ap(NULL));

    /* ------------------------------------------------------------------
     * SSID exactly 32 chars (HAL_WIFI_SSID_MAX) → ERR_SSID_TOO_LONG.
     * strlen == HAL_WIFI_SSID_MAX means it does not fit with NUL terminator.
     * ------------------------------------------------------------------ */
    ASSERT_EQ("start_ap_ssid_32_chars_rejected",
              HAL_WIFI_ERR_SSID_TOO_LONG,
              hal_wifi_start_ap(k_long_ssid_32));

    /* ------------------------------------------------------------------
     * SSID 250 chars → ERR_SSID_TOO_LONG.
     * ------------------------------------------------------------------ */
    ASSERT_EQ("start_ap_ssid_250_chars_rejected",
              HAL_WIFI_ERR_SSID_TOO_LONG,
              hal_wifi_start_ap(k_long_ssid_250));

    /* ------------------------------------------------------------------
     * connect_sta() before init() → ERR_INIT.
     * ------------------------------------------------------------------ */
    mock_wifi_reset();
    ASSERT_EQ("connect_sta_before_init",
              HAL_WIFI_ERR_INIT,
              hal_wifi_connect_sta(valid_ssid, valid_pass));

    /* ------------------------------------------------------------------
     * NULL SSID to connect_sta() → ERR_NULL.
     * ------------------------------------------------------------------ */
    mock_wifi_reset();
    hal_wifi_init();
    ASSERT_EQ("connect_sta_null_ssid",
              HAL_WIFI_ERR_NULL,
              hal_wifi_connect_sta(NULL, valid_pass));

    /* ------------------------------------------------------------------
     * NULL password to connect_sta() → ERR_NULL.
     * ------------------------------------------------------------------ */
    ASSERT_EQ("connect_sta_null_password",
              HAL_WIFI_ERR_NULL,
              hal_wifi_connect_sta(valid_ssid, NULL));

    /* ------------------------------------------------------------------
     * SSID too long to connect_sta() → ERR_SSID_TOO_LONG.
     * ------------------------------------------------------------------ */
    ASSERT_EQ("connect_sta_ssid_too_long",
              HAL_WIFI_ERR_SSID_TOO_LONG,
              hal_wifi_connect_sta(k_long_ssid_32, valid_pass));

    /* ------------------------------------------------------------------
     * Password exactly 64 chars (HAL_WIFI_PASS_MAX) → ERR_PASS_TOO_LONG.
     * strlen == HAL_WIFI_PASS_MAX means it does not fit with NUL terminator
     * inside ESP-IDF's wifi_config_t.sta.password[64] field.
     * ------------------------------------------------------------------ */
    ASSERT_EQ("connect_sta_pass_64_chars_rejected",
              HAL_WIFI_ERR_PASS_TOO_LONG,
              hal_wifi_connect_sta(valid_ssid, k_long_pass_64));

    /* ------------------------------------------------------------------
     * Double deinit is safe — no crash, no undefined behaviour.
     * ------------------------------------------------------------------ */
    mock_wifi_reset();
    hal_wifi_init();
    hal_wifi_deinit();
    hal_wifi_deinit(); /* must not crash */
    ASSERT_TRUE("double_deinit_safe", 1);

    /* ------------------------------------------------------------------
     * State after deinit is IDLE.
     * ------------------------------------------------------------------ */
    ASSERT_EQ("state_after_deinit_is_idle",
              (int)HAL_WIFI_STATE_IDLE,
              (int)hal_wifi_get_state());

    return 0;
}

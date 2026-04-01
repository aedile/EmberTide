/**
 * test_p14_hal_wifi_feature.c — Phase 14 Feature tests for hal_wifi API.
 *
 * Rule 22: Written BEFORE implementation (FEATURE RED).
 * Tested happy-path behaviours:
 *   - init returns OK; state starts IDLE
 *   - start_ap() with valid short SSID → state becomes AP_MODE
 *   - get_last_ssid captures the SSID passed to start_ap()
 *   - disconnect() from AP_MODE → state returns to IDLE
 *   - connect_sta() with valid SSID + password → state becomes STA_CONNECTING
 *   - mock_wifi_simulate_connected() advances state to STA_CONNECTED
 *   - mock_wifi_simulate_link_lost() advances state to STA_DISCONNECTED
 *   - get_last_ssid captures the SSID passed to connect_sta()
 *   - disconnect() from STA_DISCONNECTED → state becomes IDLE
 *   - mock_wifi_reset() wipes all state
 *   - deinit after AP mode is safe
 *   - empty-string password is allowed (open network)
 *   - SSID of exactly HAL_WIFI_SSID_MAX - 1 chars (31) is accepted
 *   - disconnect() from IDLE is a no-op (returns OK, stays IDLE)
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

/* A valid 31-char SSID (exactly HAL_WIFI_SSID_MAX - 1 usable chars). */
static const char ssid_31[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ01234"; /* 31 chars */

int main(void)
{
    const char *captured;

    /* ------------------------------------------------------------------ */
    /* 1. init() returns OK; state is IDLE.                                */
    /* ------------------------------------------------------------------ */
    mock_wifi_reset();
    ASSERT_EQ("init_ok",
              (int)HAL_WIFI_OK,
              (int)hal_wifi_init());
    ASSERT_EQ("state_after_init_is_idle",
              (int)HAL_WIFI_STATE_IDLE,
              (int)hal_wifi_get_state());

    /* ------------------------------------------------------------------ */
    /* 2. start_ap() with valid SSID → state becomes AP_MODE.             */
    /* ------------------------------------------------------------------ */
    ASSERT_EQ("start_ap_ok",
              (int)HAL_WIFI_OK,
              (int)hal_wifi_start_ap("FiestaQuest-AP"));
    ASSERT_EQ("state_after_start_ap_is_ap_mode",
              (int)HAL_WIFI_STATE_AP_MODE,
              (int)hal_wifi_get_state());

    /* ------------------------------------------------------------------ */
    /* 3. get_last_ssid captures SSID passed to start_ap().               */
    /* ------------------------------------------------------------------ */
    captured = mock_wifi_get_last_ssid();
    ASSERT_TRUE("last_ssid_not_null", captured != NULL);
    ASSERT_EQ("last_ssid_matches_ap",
              0,
              strcmp(captured, "FiestaQuest-AP"));

    /* ------------------------------------------------------------------ */
    /* 4. disconnect() from AP_MODE → state returns to IDLE.              */
    /* ------------------------------------------------------------------ */
    ASSERT_EQ("disconnect_from_ap_ok",
              (int)HAL_WIFI_OK,
              (int)hal_wifi_disconnect());
    ASSERT_EQ("state_after_disconnect_ap_is_idle",
              (int)HAL_WIFI_STATE_IDLE,
              (int)hal_wifi_get_state());

    /* ------------------------------------------------------------------ */
    /* 5. connect_sta() with valid credentials → state becomes             */
    /*    STA_CONNECTING immediately (asynchronous IP event not yet fired).*/
    /* ------------------------------------------------------------------ */
    mock_wifi_reset();
    hal_wifi_init();
    ASSERT_EQ("connect_sta_ok",
              (int)HAL_WIFI_OK,
              (int)hal_wifi_connect_sta("HomeNetwork", "password123"));
    ASSERT_EQ("state_after_connect_sta_is_connecting",
              (int)HAL_WIFI_STATE_STA_CONNECTING,
              (int)hal_wifi_get_state());

    /* ------------------------------------------------------------------ */
    /* 5b. simulate_connected() fires the IP-obtained event → STA_CONNECTED.*/
    /* ------------------------------------------------------------------ */
    mock_wifi_simulate_connected();
    ASSERT_EQ("state_after_simulate_connected_is_sta_connected",
              (int)HAL_WIFI_STATE_STA_CONNECTED,
              (int)hal_wifi_get_state());

    /* ------------------------------------------------------------------ */
    /* 5c. simulate_link_lost() fires the disconnect event →              */
    /*     STA_DISCONNECTED.                                               */
    /* ------------------------------------------------------------------ */
    mock_wifi_simulate_link_lost();
    ASSERT_EQ("state_after_simulate_link_lost_is_sta_disconnected",
              (int)HAL_WIFI_STATE_STA_DISCONNECTED,
              (int)hal_wifi_get_state());

    /* ------------------------------------------------------------------ */
    /* 6. get_last_ssid captures SSID passed to connect_sta().            */
    /* ------------------------------------------------------------------ */
    captured = mock_wifi_get_last_ssid();
    ASSERT_TRUE("last_ssid_sta_not_null", captured != NULL);
    ASSERT_EQ("last_ssid_matches_sta",
              0,
              strcmp(captured, "HomeNetwork"));

    /* ------------------------------------------------------------------ */
    /* 7. disconnect() from STA_DISCONNECTED → state returns to IDLE.     */
    /* ------------------------------------------------------------------ */
    ASSERT_EQ("disconnect_from_sta_ok",
              (int)HAL_WIFI_OK,
              (int)hal_wifi_disconnect());
    ASSERT_EQ("state_after_disconnect_sta_is_idle",
              (int)HAL_WIFI_STATE_IDLE,
              (int)hal_wifi_get_state());

    /* ------------------------------------------------------------------ */
    /* 8. Empty-string password is accepted for open networks.             */
    /* ------------------------------------------------------------------ */
    ASSERT_EQ("connect_sta_empty_pass_ok",
              (int)HAL_WIFI_OK,
              (int)hal_wifi_connect_sta("OpenNet", ""));

    /* ------------------------------------------------------------------ */
    /* 9. SSID of exactly 31 chars (HAL_WIFI_SSID_MAX - 1) is accepted.   */
    /* ------------------------------------------------------------------ */
    mock_wifi_reset();
    hal_wifi_init();
    ASSERT_EQ("ssid_31_chars_accepted_sta",
              (int)HAL_WIFI_OK,
              (int)hal_wifi_connect_sta(ssid_31, "pw"));

    mock_wifi_reset();
    hal_wifi_init();
    ASSERT_EQ("ssid_31_chars_accepted_ap",
              (int)HAL_WIFI_OK,
              (int)hal_wifi_start_ap(ssid_31));

    /* ------------------------------------------------------------------ */
    /* 10. mock_wifi_reset() wipes all state; state returns to IDLE.       */
    /* ------------------------------------------------------------------ */
    mock_wifi_reset();
    ASSERT_EQ("state_after_mock_reset_is_idle",
              (int)HAL_WIFI_STATE_IDLE,
              (int)hal_wifi_get_state());

    /* ------------------------------------------------------------------ */
    /* 11. deinit after AP mode is safe.                                   */
    /* ------------------------------------------------------------------ */
    hal_wifi_init();
    hal_wifi_start_ap("TestAP");
    hal_wifi_deinit();
    ASSERT_TRUE("deinit_mid_ap_safe", 1);
    ASSERT_EQ("state_after_deinit_is_idle",
              (int)HAL_WIFI_STATE_IDLE,
              (int)hal_wifi_get_state());

    /* ------------------------------------------------------------------ */
    /* 12. disconnect() from IDLE is a no-op — returns OK, stays IDLE.    */
    /* ------------------------------------------------------------------ */
    mock_wifi_reset();
    ASSERT_EQ("disconnect_from_idle_ok",
              (int)HAL_WIFI_OK,
              (int)hal_wifi_disconnect());
    ASSERT_EQ("state_after_disconnect_idle_stays_idle",
              (int)HAL_WIFI_STATE_IDLE,
              (int)hal_wifi_get_state());

    return 0;
}

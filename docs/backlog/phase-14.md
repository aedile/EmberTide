# Phase 14: Connectivity & OTA Protocol

## Item 1: BLE GATT Server

### User Story
As a combatant, my device must act as a BLE peripheral that another device can discover, connect to, and exchange the serialized combat packets implemented in Phase 10.

### Acceptance Criteria
- [ ] `hal_ble.c` initializes the NimBLE stack.
- [ ] Exposes a `FiestaQuest` Service UUID.
- [ ] Exposes a writable Characteristic for receiving incoming `team_sync_t` structs.
- [ ] Automatically pushes validated packets onto the global Event Bus.

### Negative Test Requirements (from spec-challenger)
- **Malicious MTU Padding:** Ensure the GATT Write callback safely rejects payloads larger than 255 bytes explicitly, rather than executing a blind `memcpy` that causes stack overflow via arbitrary code execution over Bluetooth.
- **Silent Drop Persistence:** Device B forcibly resets its ESP32 chip while connected. Ensure Device A's `ble_gap_event` callback immediately detects the GAP supervisor timeout, dropping the connection state securely to `STATE_DISCONNECTED` instead of hanging in `STATE_READY` waiting for packets forever.

### Implementation Steps
1. Set up NimBLE host.
2. Advertise `FiestaQuest` if in `STATE_WAITING_TEAM`.
3. Handle MTU negotiation to ensure our 60-byte payload structs arrive unbroken.

### Test Expectations
- Flashed target visible in nRF Connect smartphone app with the correct custom UUID.

### Files to Create/Modify
- `components/hal/src/hal_ble.c`

### Commit Messages
- `feat: NimBLE GATT server implementation for combat link`

---

## Item 2: WiFi Captive Portal

### User Story
As a user performing a firmware upgrade, I want to press and hold Button A on boot to open a WiFi access point "FiestaQuest-AP", go to `192.168.4.1`, and paste my home WiFi credentials.

### Acceptance Criteria
- [ ] `hal_wifi.c` enters SoftAP mode if requested.
- [ ] Starts an `httpd` instance serving a basic HTML form.
- [ ] On form POST, writes the SSID/Password securely to NVS flash.
- [ ] Reboot.

### Negative Test Requirements (from spec-challenger)
- **SSID Buffer Overflow:** Form POST submits a 250-character SSID network name. Assert `hal_wifi` cleanly truncates or rejects it, given ESP32 WiFi drivers crash if initialized with `ssid` arrays exceeding `32` characters.
- **Denial of Service HTTPD:** A user rapidly refreshes the captive portal page 50 times. Since HTTPD allocates sockets, ensure the `httpd_config_t` implements strict max connection limits (`max_open_sockets=4`) and short timeouts (`recv_wait_timeout=3`) to prevent socket starvation.

### Implementation Steps
1. Setup ESP HTTP server.
2. Inject a simple static HTML array.

### Test Expectations
- Target test boots device into AP mode securely.

### Files to Create/Modify
- `components/hal/src/hal_wifi.c`
- `components/hal/src/captive_portal.c`

### Commit Messages
- `feat: httpd captive portal and AP configuration for internet access`

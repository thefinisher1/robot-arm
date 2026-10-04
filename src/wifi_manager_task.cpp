/**
 * @file wifi_manager_task.cpp
 * @brief Implementation of WiFi provisioning via tzapu's WiFiManager,
 *        FreeRTOS Event Group synchronization, WiFi events, and exponential backoff.
 */

#include "wifi_manager_task.h"
#include <WiFi.h>
#include <WiFiManager.h>
#include "config.h"

// FreeRTOS Event Group
EventGroupHandle_t g_networkEventGroup = nullptr;

// Internal task prototypes
static void wifiManagerTask(void* pvParameters);
static void wifiWatchdogTask(void* pvParameters);
static void checkButtonReset();

/**
 * @brief ESP32 WiFi event callback handler for connection lifecycle tracking.
 */
static void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
    switch (event) {
        case ARDUINO_EVENT_WIFI_STA_GOT_IP: {
            Serial.println("[WIFI] Status: Connected & IP Acquired!");
            Serial.printf("[WIFI] IP Address: %s\n", WiFi.localIP().toString().c_str());
            Serial.printf("[WIFI] Signal Strength (RSSI): %d dBm\n", WiFi.RSSI());
            Serial.printf("[WIFI] MAC Address: %s\n", WiFi.macAddress().c_str());

            if (g_networkEventGroup) {
                xEventGroupSetBits(g_networkEventGroup, WIFI_CONNECTED_BIT);
            }
            break;
        }

        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED: {
            uint8_t reason = info.wifi_sta_disconnected.reason;
            Serial.printf("[WIFI] WARN: Disconnected from AP! Reason Code: %u\n", reason);

            if (g_networkEventGroup) {
                xEventGroupClearBits(g_networkEventGroup, WIFI_CONNECTED_BIT);
            }
            break;
        }

        default:
            break;
    }
}

/**
 * @brief Checks if the BOOT button (GPIO 0) is held during power-on to clear NVS settings.
 */
static void checkButtonReset() {
    pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
    delay(10); // Allow pin pullup to stabilize

    if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
        Serial.println("[WIFI] BOOT button detected LOW at startup. Hold 3 seconds to reset WiFi...");
        uint32_t pressStart = millis();
        bool resetTriggered = false;

        while (digitalRead(BOOT_BUTTON_PIN) == LOW) {
            if (millis() - pressStart >= RESET_BUTTON_HOLD_MS) {
                resetTriggered = true;
                break;
            }
            delay(50);
        }

        if (resetTriggered) {
            Serial.println("[WIFI] >> 3s elapsed! Resetting WiFi credentials in NVS Flash <<");
            WiFi.disconnect(true, true); // Erase WiFi config from NVS
            WiFiManager tempWm;
            tempWm.resetSettings();
            Serial.println("[WIFI] NVS reset complete. Captive portal will open on next boot.");
            delay(500);
            ESP.restart();
        } else {
            Serial.println("[WIFI] Button released before 3s. Continuing normal boot.");
        }
    }
}

/**
 * @brief Dedicated FreeRTOS task handling captive portal provisioning.
 */
static void wifiManagerTask(void* pvParameters) {
    (void)pvParameters;
    Serial.println("[WIFI] WifiManagerTask started on Core 0.");

    // Validate WPA2 password requirement
    if (strlen(WM_AP_PASSWORD) < 8) {
        Serial.println("[WIFI] FATAL: WM_AP_PASSWORD must be at least 8 characters for WPA2!");
        vTaskDelete(NULL);
        return;
    }

    bool connectionSucceeded = false;

    // -------------------------------------------------------------------------
    // CRITICAL: Scoped block for WiFiManager instance
    // Placing WiFiManager inside this scoped block ensures its internal WebServer,
    // DNSServer, and memory buffers are fully torn down and port 80 is released
    // BEFORE the application AsyncWebServer starts.
    // -------------------------------------------------------------------------
    {
        WiFiManager wm;

        // Configure timeouts
        wm.setConfigPortalTimeout(WM_PORTAL_TIMEOUT_SEC); // E.g., 180 seconds portal limit
        wm.setConnectTimeout(WM_CONNECT_TIMEOUT_SEC);     // E.g., 20 seconds saved-credentials attempt
        wm.setConfigPortalBlocking(false);                // Non-blocking so FreeRTOS idle task can run!
        wm.setDebugOutput(true);

        Serial.printf("[WIFI] Starting autoConnect with portal SSID: '%s'...\n", WM_AP_NAME);

        // Try connecting to saved credentials or start captive portal in non-blocking mode
        if (wm.autoConnect(WM_AP_NAME, WM_AP_PASSWORD)) {
            Serial.println("[WIFI] autoConnect connected to existing credentials!");
            connectionSucceeded = true;
        } else {
            Serial.printf("[WIFI] Captive portal running on SoftAP '%s' (IP: 192.168.4.1).\n", WM_AP_NAME);

            // Yield regularly to allow FreeRTOS IDLE0 task to run, preventing Task Watchdog timeout
            while (wm.getConfigPortalActive()) {
                wm.process();
                vTaskDelay(pdMS_TO_TICKS(20)); // 20ms yield: feeds TWDT and services DNS/HTTP
            }

            connectionSucceeded = (WiFi.status() == WL_CONNECTED);
            if (connectionSucceeded) {
                Serial.println("[WIFI] Connected to WiFi via captive portal!");
            } else {
                Serial.println("[WIFI] WARN: Captive portal closed without active connection.");
            }
        }

        if (!connectionSucceeded && WM_REBOOT_ON_TIMEOUT) {
            Serial.println("[WIFI] Rebooting ESP32 in 3 seconds to retry provisioning cycle...");
            vTaskDelay(pdMS_TO_TICKS(3000));
            ESP.restart();
        }
    } // <-- WiFiManager destructor called here. Port 80 is now completely free!

    // Start background reconnection watchdog task if connected
    if (connectionSucceeded) {
        xTaskCreatePinnedToCore(
            wifiWatchdogTask,
            "WifiWatchdog",
            WIFI_WATCHDOG_STACK_SIZE,
            nullptr,
            WIFI_WATCHDOG_PRIORITY,
            nullptr,
            WIFI_WATCHDOG_CORE
        );
    }

    Serial.println("[WIFI] Provisioning complete. Deleting WifiManagerTask.");
    vTaskDelete(NULL);
}

/**
 * @brief Low-priority watchdog task performing reconnect with exponential backoff.
 */
static void wifiWatchdogTask(void* pvParameters) {
    (void)pvParameters;
    Serial.println("[WIFI] Reconnection Watchdog Task active.");

    uint32_t currentBackoffMs = WIFI_INITIAL_BACKOFF_MS;

    for (;;) {
        // Sleep between health checks
        vTaskDelay(pdMS_TO_TICKS(1000));

        // Check if connection was lost
        EventBits_t bits = xEventGroupGetBits(g_networkEventGroup);
        if (!(bits & WIFI_CONNECTED_BIT)) {
            Serial.printf("[WIFI] Watchdog: Link down. Attempting reconnect in %u ms...\n", currentBackoffMs);
            vTaskDelay(pdMS_TO_TICKS(currentBackoffMs));

            // Check again in case link restored during delay
            if (!(xEventGroupGetBits(g_networkEventGroup) & WIFI_CONNECTED_BIT)) {
                WiFi.reconnect();

                // Increase backoff exponentially up to max limit
                currentBackoffMs = (currentBackoffMs * 2 > WIFI_MAX_BACKOFF_MS)
                                       ? WIFI_MAX_BACKOFF_MS
                                       : currentBackoffMs * 2;
            }
        } else {
            // Link is healthy: reset backoff to minimum
            currentBackoffMs = WIFI_INITIAL_BACKOFF_MS;
        }
    }
}

void startWifiProvisioning() {
    Serial.println("[WIFI] Initializing FreeRTOS Event Group & WiFi subsystems...");

    // 1. Create FreeRTOS Event Group
    g_networkEventGroup = xEventGroupCreate();
    if (g_networkEventGroup == nullptr) {
        Serial.println("[WIFI] FATAL: Failed to create g_networkEventGroup!");
        return;
    }

    // 2. Set WiFi modes and persistence
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);

    // 3. Register WiFi event callbacks
    WiFi.onEvent(onWiFiEvent);

    // 4. Check for physical factory reset request
    checkButtonReset();

    // 5. Spawn dedicated provisioning task on Core 0 with >= 8192 bytes stack
    xTaskCreatePinnedToCore(
        wifiManagerTask,
        "WifiManagerTask",
        WM_TASK_STACK_SIZE,
        nullptr,
        WM_TASK_PRIORITY,
        nullptr,
        WM_TASK_CORE
    );
}

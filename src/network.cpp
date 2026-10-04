/**
 * @file network.cpp
 * @brief WiFi connection manager, AsyncWebServer, WebSocket parser,
 *        and JSON telemetry transmission.
 */

#include "network.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <Update.h>
#include "servo_driver.h"
#include "web_ui.h"
#include "wifi_manager_task.h"

static AsyncWebServer s_server(HTTP_SERVER_PORT);
static AsyncWebSocket s_ws(WS_ENDPOINT_PATH);
static bool s_serverStarted = false;
static bool s_otaEnabled = false;

// Helper function prototypes
static void parseMoveCommand(const JsonDocument& doc);
static void parseEnableCommand(const JsonDocument& doc);
static void parseHomeCommand();
static void handleEnableOTA();
static void handleWebSocketMessage(void* arg, uint8_t* data, size_t len);
static void onWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                             AwsEventType type, void* arg, uint8_t* data, size_t len);

static void parseMoveCommand(const JsonDocument& doc) {
    JsonArrayConst joints = doc["joints"].as<JsonArrayConst>();
    if (joints.isNull()) {
        Serial.println("[NET] ERROR: 'move' cmd missing 'joints' array!");
        return;
    }

    if (joints.size() != NUM_JOINTS) {
        Serial.printf("[NET] ERROR: Expected %d joint angles, got %d!\n", NUM_JOINTS, joints.size());
        return;
    }

    CommandMsg msg = {};
    msg.type = CMD_MOVE;
    msg.speedPercent = doc["speed"].is<float>() ? doc["speed"].as<float>() : (doc["speed"].as<int>() ? doc["speed"].as<int>() : DEFAULT_SPEED_PERCENT);
    msg.speedPercent = constrain(msg.speedPercent, 1.0f, 100.0f);

    for (size_t i = 0; i < NUM_JOINTS; ++i) {
        if (joints[i].isNull()) {
            Serial.printf("[NET] ERROR: Joint index %d is null or missing!\n", i);
            return;
        }
        float rawAngle = joints[i].as<float>();
        // Clamp to joint calibration limits
        msg.targetAngle[i] = constrain(rawAngle, JOINT_CONFIGS[i].minAngle, JOINT_CONFIGS[i].maxAngle);
    }

    // Post to FreeRTOS queue without blocking network task
    if (xQueueSend(g_commandQueue, &msg, 0) != pdTRUE) {
        Serial.println("[NET] WARN: Command queue full! Dropping frame.");
    }
}

static void parseHomeCommand() {
    CommandMsg msg = {};
    msg.type = CMD_HOME;
    msg.speedPercent = DEFAULT_SPEED_PERCENT;
    for (size_t i = 0; i < NUM_JOINTS; ++i) {
        msg.targetAngle[i] = JOINT_CONFIGS[i].homeAngle;
    }
    if (xQueueSend(g_commandQueue, &msg, 0) != pdTRUE) {
        Serial.println("[NET] WARN: Command queue full! Dropping home command.");
    }
}

static void parseEnableCommand(const JsonDocument& doc) {
    if (doc["value"].isNull()) {
        Serial.println("[NET] ERROR: 'enable' cmd missing 'value'!");
        return;
    }
    CommandMsg msg = {};
    msg.type = CMD_ENABLE;
    msg.enable = doc["value"].as<bool>();
    if (xQueueSend(g_commandQueue, &msg, 0) != pdTRUE) {
        Serial.println("[NET] WARN: Command queue full! Dropping enable command.");
    }
}

void startOTA() {
    if (s_otaEnabled) return;

    ArduinoOTA.setHostname("esp32-robotarm");
    ArduinoOTA.setPort(3232);

    ArduinoOTA.onStart([]() {
        String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
        Serial.println("\n[OTA] ArduinoOTA update starting: " + type);
        servoSetHardwareEnable(false); // Safety: de-energize servos during flash
    });

    ArduinoOTA.onEnd([]() {
        Serial.println("\n[OTA] Update successfully completed! Rebooting...");
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        static unsigned int s_lastPct = 999;
        unsigned int pct = (progress / (total / 100));
        if (pct != s_lastPct && pct % 10 == 0) {
            s_lastPct = pct;
            Serial.printf("[OTA] Flashing: %u%%\n", pct);
        }
    });

    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("[OTA] ERROR [%u]: ", error);
        if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

    ArduinoOTA.begin();
    s_otaEnabled = true;

    Serial.println("==================================================");
    Serial.println("           OTA WIRELESS FLASHING ENABLED          ");
    Serial.println("==================================================");
    Serial.printf("[OTA] Device IP:   %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("[OTA] Port:        3232\n");
    Serial.printf("[OTA] Hostname:    esp32-robotarm.local\n");
    Serial.printf("[OTA] PlatformIO:  pio run -t upload --upload-port %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("[OTA] Browser UI:  http://%s/ (OTA upload section)\n", WiFi.localIP().toString().c_str());
    Serial.println("==================================================");
}

bool isOTAEnabled() {
    return s_otaEnabled;
}

static void handleEnableOTA() {
    startOTA();

    // Broadcast OTA status to all connected web clients
    JsonDocument doc;
    doc["type"] = "ota_status";
    doc["enabled"] = true;
    doc["ip"] = WiFi.localIP().toString();
    doc["port"] = 3232;
    doc["hostname"] = "esp32-robotarm";

    char buffer[192];
    size_t len = serializeJson(doc, buffer, sizeof(buffer));
    s_ws.textAll(buffer, len);
}

static void handleWebSocketMessage(void* arg, uint8_t* data, size_t len) {
    AwsFrameInfo* info = static_cast<AwsFrameInfo*>(arg);
    if (!info->final || info->index != 0 || info->len != len || info->opcode != WS_TEXT) {
        return; // Only process complete single-frame text messages
    }

    // Allocate JSON document on stack
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, data, len);
    if (err) {
        Serial.printf("[NET] ERROR: JSON deserialization failed: %s\n", err.c_str());
        return;
    }

    const char* cmd = doc["cmd"] | "";
    if (strcmp(cmd, "move") == 0) {
        parseMoveCommand(doc);
    } else if (strcmp(cmd, "home") == 0) {
        parseHomeCommand();
    } else if (strcmp(cmd, "enable") == 0) {
        parseEnableCommand(doc);
    } else if (strcmp(cmd, "enable_ota") == 0) {
        handleEnableOTA();
    } else {
        Serial.printf("[NET] WARN: Unknown command: '%s'\n", cmd);
    }
}

static void onWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                             AwsEventType type, void* arg, uint8_t* data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("[NET] WebSocket Client #%u connected from %s\n",
                          client->id(), client->remoteIP().toString().c_str());
            // If OTA is already active, notify client immediately
            if (s_otaEnabled) {
                JsonDocument doc;
                doc["type"] = "ota_status";
                doc["enabled"] = true;
                doc["ip"] = WiFi.localIP().toString();
                doc["port"] = 3232;
                doc["hostname"] = "esp32-robotarm";
                char buffer[192];
                size_t l = serializeJson(doc, buffer, sizeof(buffer));
                client->text(buffer, l);
            }
            break;
        case WS_EVT_DISCONNECT:
            Serial.printf("[NET] WebSocket Client #%u disconnected\n", client->id());
            break;
        case WS_EVT_DATA:
            handleWebSocketMessage(arg, data, len);
            break;
        case WS_EVT_PONG:
        case WS_EVT_ERROR:
            break;
    }
}

void networkInit() {
    Serial.println("[NET] NetworkTask waiting for WiFi via FreeRTOS Event Group...");

    // Block with zero CPU consumption until WiFiManager finishes and IP is acquired
    xEventGroupWaitBits(
        g_networkEventGroup,
        WIFI_CONNECTED_BIT,
        pdFALSE,        // Do not clear bit on exit
        pdTRUE,         // Wait for WIFI_CONNECTED_BIT
        portMAX_DELAY   // Block indefinitely
    );

    if (!s_serverStarted) {
        // Attach WebSocket handler
        s_ws.onEvent(onWebSocketEvent);
        s_server.addHandler(&s_ws);

        // Serve web control interface from PROGMEM
        s_server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
            request->send(200, "text/html", INDEX_HTML);
        });

        // Web Browser OTA firmware upload endpoint
        s_server.on("/update", HTTP_POST, [](AsyncWebServerRequest* request) {
            bool updateFailed = Update.hasError();
            AsyncWebServerResponse* response = request->beginResponse(
                200, "text/plain", updateFailed ? "Update Failed!\n" : "Update Succeeded! Rebooting...\n"
            );
            response->addHeader("Connection", "close");
            request->send(response);
            if (!updateFailed) {
                delay(1000);
                ESP.restart();
            }
        }, [](AsyncWebServerRequest* request, String filename, size_t index, uint8_t* data, size_t len, bool final) {
            if (!index) {
                Serial.printf("[OTA] Browser firmware upload started: %s\n", filename.c_str());
                servoSetHardwareEnable(false); // Safety: de-energize servos during flash
                if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
                    Update.printError(Serial);
                }
            }
            if (Update.write(data, len) != len) {
                Update.printError(Serial);
            }
            if (final) {
                if (Update.end(true)) {
                    Serial.printf("[OTA] Browser firmware upload finished (%u bytes)!\n", index + len);
                } else {
                    Update.printError(Serial);
                }
            }
        });

        s_server.begin();
        s_serverStarted = true;
        Serial.println("[NET] AsyncWebServer & WebSocket started on port 80 (Portal cleanly closed).");
    }
}

void networkLoop() {
    // Free up resources from disconnected WebSocket clients
    s_ws.cleanupClients();

    // Handle incoming ArduinoOTA network packets if enabled
    if (s_otaEnabled) {
        ArduinoOTA.handle();
    }
}

void networkBroadcastState(const RobotState& state) {
    if (s_ws.count() == 0) {
        return; // Do not waste cycles serializing if no clients are listening
    }

    // Outgoing format: {"type":"state","joints":[90.0,45.2,119.8,90.0],"enabled":true}
    JsonDocument doc;
    doc["type"] = "state";
    JsonArray joints = doc["joints"].to<JsonArray>();
    for (size_t i = 0; i < NUM_JOINTS; ++i) {
        joints.add(roundf(state.currentAngle[i] * 10.0f) / 10.0f);
    }
    doc["enabled"] = state.enabled;
    doc["moving"]  = state.moving;
    doc["speed"]   = state.currentSpeedPercent;

    char buffer[192];
    size_t len = serializeJson(doc, buffer, sizeof(buffer));
    s_ws.textAll(buffer, len);
}

/**
 * @file network.h
 * @brief WiFi networking, AsyncWebServer, WebSocket command parsing,
 *        and JSON telemetry broadcasting.
 */

#pragma once

#include <Arduino.h>
#include "types.h"

// FreeRTOS Queue Handle for decoupled inter-task communication
extern QueueHandle_t g_commandQueue;

/**
 * @brief Connects to WiFi in Station mode with 10s fallback to SoftAP.
 *        Starts HTTP server and WebSocket endpoint.
 */
void networkInit();

/**
 * @brief Housekeeping loop for network stack (e.g. cleaning disconnected WS clients).
 */
void networkLoop();

/**
 * @brief Broadcast current joint angles and enable status to all connected WebSocket clients.
 * 
 * Outgoing JSON format:
 *   {"type":"state","joints":[90.0,45.2,119.8,90.0],"enabled":true}
 * 
 * @param state Snapshot of current robot state.
 */
void networkBroadcastState(const RobotState& state);

/**
 * @brief Initializes and enables ArduinoOTA wireless flashing.
 */
void startOTA();

/**
 * @brief Returns true if OTA wireless flashing service is currently running.
 */
bool isOTAEnabled();

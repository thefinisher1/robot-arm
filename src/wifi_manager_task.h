/**
 * @file wifi_manager_task.h
 * @brief WiFi provisioning module for ESP32 using tzapu's WiFiManager,
 *        FreeRTOS Event Groups, WiFi event callbacks, and an exponential
 *        backoff connection watchdog.
 */

#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include "config.h"

// FreeRTOS Event Group handle and synchronization bits
extern EventGroupHandle_t g_networkEventGroup;
constexpr EventBits_t WIFI_CONNECTED_BIT = BIT0;

/**
 * @brief Initializes the FreeRTOS event group, registers system WiFi events,
 *        checks for BOOT button credential reset, and spawns the WifiManagerTask.
 */
void startWifiProvisioning();

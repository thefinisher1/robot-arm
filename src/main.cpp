/**
 * @file main.cpp
 * @brief Firmware entry point for the ESP32 4-DOF Robotic Arm.
 *        Initializes serial diagnostics and boots the FreeRTOS multitasking architecture.
 */

#include <Arduino.h>
#include <esp_system.h>
#include "config.h"
#include "tasks.h"

static void printResetReason() {
    esp_reset_reason_t reason = esp_reset_reason();
    Serial.printf("[MAIN] Boot / Reset Reason: (%d) ", reason);
    switch (reason) {
        case ESP_RST_POWERON:   Serial.println("Power-on reset"); break;
        case ESP_RST_EXT:       Serial.println("External pin reset"); break;
        case ESP_RST_SW:        Serial.println("Software reset via esp_restart"); break;
        case ESP_RST_PANIC:     Serial.println("Software reset due to exception/panic"); break;
        case ESP_RST_INT_WDT:   Serial.println("Interrupt watchdog reset"); break;
        case ESP_RST_TASK_WDT:  Serial.println("Task watchdog reset (TWDT)"); break;
        case ESP_RST_WDT:       Serial.println("Other watchdog reset"); break;
        case ESP_RST_DEEPSLEEP: Serial.println("Deep sleep reset"); break;
        case ESP_RST_BROWNOUT:  Serial.println("Brownout reset (insufficient power/voltage dip)"); break;
        case ESP_RST_SDIO:      Serial.println("SDIO reset"); break;
        default:                Serial.println("Other / Unknown reset"); break;
    }
}

void setup() {
    // 1. Initialize high-speed serial debugging
    Serial.begin(115200);
    delay(1000); // Allow UART bridge and USB connection to settle

    Serial.println();
    Serial.println("==================================================");
    Serial.println("       ESP32 4-DOF ROBOTIC ARM FIRMWARE           ");
    Serial.println("    FreeRTOS Architecture with PCA9685 Driver     ");
    Serial.println("==================================================");
    printResetReason();
    Serial.printf("[MAIN] Compiled: %s %s\n", __DATE__, __TIME__);
    Serial.printf("[MAIN] CPU Frequency: %u MHz | Chip Revision: %d\n", ESP.getCpuFreqMHz(), ESP.getChipRevision());
    Serial.printf("[MAIN] Free Heap: %u bytes\n", ESP.getFreeHeap());

    // 2. Initialize FreeRTOS Queues, Mutexes, and Core-Pinned Tasks
    createSystemTasks();

    Serial.println("[MAIN] Setup complete. Background tasks running.");
}

void loop() {
    // Execution is delegated to FreeRTOS tasks.
    // Yield periodically to allow the Arduino loopTask to reset its Task Watchdog (TWDT).
    vTaskDelay(pdMS_TO_TICKS(1000));
}
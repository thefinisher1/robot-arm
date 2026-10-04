/**
 * @file tasks.cpp
 * @brief FreeRTOS task implementations, deterministic 50 Hz servo loop,
 *        safety watchdog, and telemetry broadcast.
 */

#include "tasks.h"
#include "config.h"
#include "servo_driver.h"
#include "motion.h"
#include "network.h"
#include "wifi_manager_task.h"

// FreeRTOS IPC primitives
QueueHandle_t     g_commandQueue = nullptr;
SemaphoreHandle_t g_stateMutex   = nullptr;
RobotState        g_robotState;

// Task handles for diagnostics and stack monitoring
static TaskHandle_t s_hNetworkTask = nullptr;
static TaskHandle_t s_hServoTask   = nullptr;
static TaskHandle_t s_hSafetyTask  = nullptr;
static TaskHandle_t s_hStatusTask  = nullptr;

void networkTask(void* parameter) {
    (void)parameter;
    Serial.println("[NET] NetworkTask started on Core 0.");

    // Initialize WiFi, WebServer, and WebSocket handlers
    networkInit();

    for (;;) {
        // Periodic network housekeeping (e.g. prune dead WebSocket sockets & service OTA)
        networkLoop();
        vTaskDelay(pdMS_TO_TICKS(isOTAEnabled() ? 10 : 50));
    }
}

void servoControlTask(void* parameter) {
    (void)parameter;
    Serial.println("[SERVO] ServoControlTask started on Core 1.");

    // Initialize PCA9685 hardware on dedicated Core 1 context
    if (!servoDriverInit()) {
        Serial.println("[SERVO] WARN: PCA9685 not detected or I2C failed. Arm servos idling.");
        // Mark disabled in shared state
        if (xSemaphoreTake(g_stateMutex, portMAX_DELAY) == pdTRUE) {
            g_robotState.enabled = false;
            xSemaphoreGive(g_stateMutex);
        }
        // Idle safely instead of deleting task (avoids dangling handle in safetyTask)
        for (;;) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    // Initialize internal state
    if (xSemaphoreTake(g_stateMutex, portMAX_DELAY) == pdTRUE) {
        motionInit(g_robotState);
        xSemaphoreGive(g_stateMutex);
    }

    // Set hardware PWM registers to HOME configuration before energizing coils
    servoWriteAllHome();
    vTaskDelay(pdMS_TO_TICKS(50));

    // Enable hardware Output Enable (OE pin LOW)
    servoSetHardwareEnable(true);

    // Initialize fixed-frequency timer (50 Hz = 20ms interval)
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(1000.0f / SERVO_LOOP_FREQ_HZ);

    RobotState localState;

    for (;;) {
        // Deterministic delay: guarantees exact 20ms period regardless of execution duration
        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        // 1. Check for incoming commands from NetworkTask (drain queue)
        CommandMsg msg;
        while (xQueueReceive(g_commandQueue, &msg, 0) == pdTRUE) {
            if (xSemaphoreTake(g_stateMutex, portMAX_DELAY) == pdTRUE) {
                switch (msg.type) {
                    case CMD_MOVE:
                        for (size_t i = 0; i < NUM_JOINTS; ++i) {
                            g_robotState.targetAngle[i] = msg.targetAngle[i];
                        }
                        g_robotState.currentSpeedPercent = msg.speedPercent;
                        g_robotState.lastCommandTimestampMs = millis();
                        break;

                    case CMD_HOME:
                        for (size_t i = 0; i < NUM_JOINTS; ++i) {
                            g_robotState.targetAngle[i] = JOINT_CONFIGS[i].homeAngle;
                        }
                        g_robotState.lastCommandTimestampMs = millis();
                        break;

                    case CMD_ENABLE:
                        g_robotState.enabled = msg.enable;
                        servoSetHardwareEnable(msg.enable);
                        g_robotState.lastCommandTimestampMs = millis();
                        break;
                }
                xSemaphoreGive(g_stateMutex);
            }
        }

        // 2. Take local copy of state for processing
        if (xSemaphoreTake(g_stateMutex, portMAX_DELAY) == pdTRUE) {
            localState = g_robotState;
            xSemaphoreGive(g_stateMutex);
        }

        // 3. Update motion profile and write PWM if servos are enabled
        if (localState.enabled) {
            motionUpdate(localState);

            for (size_t i = 0; i < NUM_JOINTS; ++i) {
                servoWriteAngle(i, localState.currentAngle[i]);
            }

            // Write back updated current angles and moving flag to shared state
            if (xSemaphoreTake(g_stateMutex, portMAX_DELAY) == pdTRUE) {
                for (size_t i = 0; i < NUM_JOINTS; ++i) {
                    g_robotState.currentAngle[i] = localState.currentAngle[i];
                }
                g_robotState.moving = localState.moving;
                xSemaphoreGive(g_stateMutex);
            }
        }
    }
}

void safetyTask(void* parameter) {
    (void)parameter;
    Serial.println("[SAFETY] SafetyTask started on Core 0.");

    uint32_t lastDiagnosticsMs = 0;
    // Single warning latch shared across the if/else branches.
    // Previously there were two separate static bools in different scopes,
    // so the reset in the else-branch never actually cleared the latch.
    static bool s_timeoutWarned = false;

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(500));
        uint32_t now = millis();

        // 1. Check for command timeout
        uint32_t lastCmd = 0;
        bool isEnabled = false;

        if (xSemaphoreTake(g_stateMutex, portMAX_DELAY) == pdTRUE) {
            lastCmd = g_robotState.lastCommandTimestampMs;
            isEnabled = g_robotState.enabled;
            xSemaphoreGive(g_stateMutex);
        }

        if (isEnabled && (now - lastCmd > COMMAND_TIMEOUT_MS)) {
            if (!s_timeoutWarned) {
                Serial.printf("[SAFETY] WARN: No command received for %u ms (timeout threshold %u ms).\n",
                              now - lastCmd, COMMAND_TIMEOUT_MS);
                s_timeoutWarned = true;

                if (RETURN_HOME_ON_TIMEOUT) {
                    Serial.println("[SAFETY] Action: Returning arm to safe HOME pose.");
                    CommandMsg homeMsg = {};
                    homeMsg.type = CMD_HOME;
                    homeMsg.speedPercent = DEFAULT_SPEED_PERCENT;
                    xQueueSend(g_commandQueue, &homeMsg, 0);
                }

                if (DISABLE_SERVO_ON_TIMEOUT) {
                    Serial.println("[SAFETY] Action: Disabling servo hardware OE pin.");
                    CommandMsg disMsg = {};
                    disMsg.type = CMD_ENABLE;
                    disMsg.enable = false;
                    xQueueSend(g_commandQueue, &disMsg, 0);
                }
            }
        } else {
            // Reset warning latch once new commands arrive
            s_timeoutWarned = false;
        }

        // 2. Stack high-water mark diagnostics (every 10 seconds)
        if (now - lastDiagnosticsMs >= 10000) {
            lastDiagnosticsMs = now;
            UBaseType_t netHwm    = (s_hNetworkTask != nullptr) ? uxTaskGetStackHighWaterMark(s_hNetworkTask) : 0;
            UBaseType_t servoHwm  = (s_hServoTask != nullptr)   ? uxTaskGetStackHighWaterMark(s_hServoTask) : 0;
            UBaseType_t safetyHwm = (s_hSafetyTask != nullptr)  ? uxTaskGetStackHighWaterMark(s_hSafetyTask) : 0;
            UBaseType_t statusHwm = (s_hStatusTask != nullptr)  ? uxTaskGetStackHighWaterMark(s_hStatusTask) : 0;

            Serial.printf("[STACK] Free Stack (Words) -> Net: %u | Servo: %u | Safety: %u | Status: %u\n",
                          (unsigned)netHwm, (unsigned)servoHwm, (unsigned)safetyHwm, (unsigned)statusHwm);
        }
    }
}

void statusTask(void* parameter) {
    (void)parameter;
    Serial.println("[STATUS] StatusTask started on Core 0.");

    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(1000.0f / STATUS_BROADCAST_FREQ_HZ);

    RobotState localCopy;

    for (;;) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        // Only broadcast if WiFi is connected and network stack is active
        if (g_networkEventGroup && (xEventGroupGetBits(g_networkEventGroup) & WIFI_CONNECTED_BIT)) {
            // Read snapshot of current arm telemetry under mutex
            if (xSemaphoreTake(g_stateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                localCopy = g_robotState;
                xSemaphoreGive(g_stateMutex);

                // Broadcast OUTSIDE mutex to avoid holding it during I/O
                networkBroadcastState(localCopy);
            }
        }
    }
}

void createSystemTasks() {
    Serial.println("[MAIN] Creating FreeRTOS Queues and Mutexes...");

    // Create command queue for inter-task communication
    g_commandQueue = xQueueCreate(COMMAND_QUEUE_DEPTH, sizeof(CommandMsg));
    if (g_commandQueue == nullptr) {
        Serial.println("[MAIN] FATAL: Failed to create g_commandQueue!");
        return;
    }

    // Create mutex for protecting RobotState
    g_stateMutex = xSemaphoreCreateMutex();
    if (g_stateMutex == nullptr) {
        Serial.println("[MAIN] FATAL: Failed to create g_stateMutex!");
        return;
    }

    // Initialize WiFi Provisioning via WiFiManager (tzapu) on Core 0
    startWifiProvisioning();

    Serial.println("[MAIN] Spawning pinned FreeRTOS tasks...");

    // 1. NetworkTask (Core 0, Priority 2)
    xTaskCreatePinnedToCore(
        networkTask,
        "NetworkTask",
        NETWORK_TASK_STACK_SIZE,
        nullptr,
        NETWORK_TASK_PRIORITY,
        &s_hNetworkTask,
        NETWORK_TASK_CORE
    );

    // 2. ServoControlTask (Core 1, Priority 3 - Dedicated real-time core)
    xTaskCreatePinnedToCore(
        servoControlTask,
        "ServoControlTask",
        SERVO_TASK_STACK_SIZE,
        nullptr,
        SERVO_TASK_PRIORITY,
        &s_hServoTask,
        SERVO_TASK_CORE
    );

    // 3. SafetyTask (Core 0, Priority 1)
    xTaskCreatePinnedToCore(
        safetyTask,
        "SafetyTask",
        SAFETY_TASK_STACK_SIZE,
        nullptr,
        SAFETY_TASK_PRIORITY,
        &s_hSafetyTask,
        SAFETY_TASK_CORE
    );

    // 4. StatusTask (Core 0, Priority 1)
    xTaskCreatePinnedToCore(
        statusTask,
        "StatusTask",
        STATUS_TASK_STACK_SIZE,
        nullptr,
        STATUS_TASK_PRIORITY,
        &s_hStatusTask,
        STATUS_TASK_CORE
    );

    Serial.println("[MAIN] All FreeRTOS tasks successfully launched.");
}

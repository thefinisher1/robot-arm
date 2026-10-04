/**
 * @file tasks.h
 * @brief FreeRTOS task declarations, shared queues, mutexes, and system bootstrap.
 */

#pragma once

#include <Arduino.h>
#include "types.h"

// Shared FreeRTOS IPC handles
extern QueueHandle_t     g_commandQueue;
extern SemaphoreHandle_t g_stateMutex;
extern RobotState        g_robotState;

/**
 * @brief FreeRTOS task prototypes
 */
void networkTask(void* parameter);
void servoControlTask(void* parameter);
void safetyTask(void* parameter);
void statusTask(void* parameter);

/**
 * @brief Allocates queues, mutexes, and spawns pinned FreeRTOS tasks.
 */
void createSystemTasks();

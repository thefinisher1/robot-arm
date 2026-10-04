/**
 * @file types.h
 * @brief Common data structures, enums, and message formats passed between
 *        FreeRTOS tasks and network layers in the robot arm system.
 */

#pragma once

#include <Arduino.h>
#include "config.h"

/**
 * @brief Types of commands that can be issued over WebSocket or internally.
 */
enum CommandType : uint8_t {
    CMD_MOVE = 0,   // Move arm joints to target positions at specified speed
    CMD_HOME,       // Move arm joints to predefined safe home configuration
    CMD_ENABLE      // Toggle servo power via hardware OE pin and software state
};

/**
 * @brief Command message passed via FreeRTOS queue from NetworkTask to ServoControlTask.
 * 
 * Passing by value through the queue decouples the asynchronous network callbacks
 * from the time-critical servo control loop without memory allocations or locking delays.
 */
struct CommandMsg {
    CommandType type;                   // Type of command requested
    float       targetAngle[NUM_JOINTS];// Requested joint angles in degrees
    float       speedPercent;           // Movement speed percentage (1.0f - 100.0f)
    bool        enable;                 // Servo enable/disable state for CMD_ENABLE
};

/**
 * @brief Shared robot state representing current positions, targets, and health.
 * 
 * Protected by a FreeRTOS Mutex when read or written across tasks.
 */
struct RobotState {
    float    currentAngle[NUM_JOINTS];   // Current filtered joint angles (degrees)
    float    targetAngle[NUM_JOINTS];    // Target setpoint joint angles (degrees)
    float    currentSpeedPercent;        // Active speed profile (1.0f - 100.0f)
    bool     enabled;                    // True if servos are energized (OE low)
    bool     moving;                     // True if current angles have not yet reached target
    uint32_t lastCommandTimestampMs;     // Milliseconds timestamp of last valid command
};

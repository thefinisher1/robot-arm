/**
 * @file config.h
 * @brief Global hardware pinouts, network credentials, joint calibration tables,
 *        motion profiles, and FreeRTOS task configuration for the 4-DOF Robot Arm.
 */

#pragma once

#include <Arduino.h>

// =============================================================================
// 1. HARDWARE PIN DEFINITIONS
// =============================================================================
// I2C Pins for PCA9685 Servo Driver
constexpr int I2C_SDA_PIN          = 21;
constexpr int I2C_SCL_PIN          = 22;
constexpr uint32_t I2C_FREQ_HZ     = 400000; // 400 kHz Fast-mode I2C

// PCA9685 Output Enable Pin (Active LOW)
// HIGH = All outputs disabled (high-impedance / servos de-energized)
// LOW  = All outputs enabled
constexpr int SERVO_OE_PIN         = 5;

// =============================================================================
// 2. PCA9685 SERVO DRIVER SETTINGS (REFERENCE CALIBRATION)
// =============================================================================
constexpr uint8_t  PCA9685_I2C_ADDR    = 0x40;   // Default I2C address (A0-A5 open)
constexpr float    PCA9685_PWM_FREQ    = 50.0f;  // Standard servo frequency (50 Hz = 20ms period)
constexpr uint32_t PCA9685_OSC_HZ     = 25000000; // 25 MHz internal oscillator

// Standard PCA9685 12-bit servo pulse limits (out of 4096 counts per 20ms frame)
// Matches reference: SERVO_MIN = 150 (approx 0 deg), SERVO_MAX = 600 (approx 180 deg)
constexpr uint16_t SERVO_MIN_TICKS     = 150;
constexpr uint16_t SERVO_MAX_TICKS     = 600;

// =============================================================================
// 3. ROBOT KINEMATICS & CALIBRATION TABLE
// =============================================================================
// Number of active joints. To add a 5th joint (e.g., gripper):
// 1. Change NUM_JOINTS from 4 to 5.
// 2. Add a new row to JOINT_CALIBRATION_TABLE below.
constexpr size_t NUM_JOINTS = 4;

struct JointConfig {
    uint8_t  channel;       // PCA9685 output channel (0-15)
    uint16_t minPulseTicks; // PCA9685 ticks at minimum angle (reference: 150)
    uint16_t maxPulseTicks; // PCA9685 ticks at maximum angle (reference: 600)
    float    minAngle;      // Minimum allowable mechanical angle (degrees)
    float    maxAngle;      // Maximum allowable mechanical angle (degrees)
    float    homeAngle;     // Neutral/safe parking angle (degrees)
    bool     invert;        // True if clockwise rotation needs pulse-reversal
};

// Calibration table for 4x MG995 servos using reference 150-600 pulse tick limits:
// Channel 0: Base rotation (0° - 180°)
// Channel 1: Shoulder joint (15° - 165° safety restricted)
// Channel 2: Elbow joint (10° - 170° safety restricted)
// Channel 3: Wrist pitch (0° - 180°)
constexpr JointConfig JOINT_CONFIGS[NUM_JOINTS] = {
    // ch, minTicks, maxTicks, minDeg, maxDeg, homeDeg, invert
    {   0, SERVO_MIN_TICKS, SERVO_MAX_TICKS,    0.0f,  180.0f,   90.0f,  false }, // Joint 0: Base
    {   1, SERVO_MIN_TICKS, SERVO_MAX_TICKS,   15.0f,  165.0f,   45.0f,  false }, // Joint 1: Shoulder
    {   2, SERVO_MIN_TICKS, SERVO_MAX_TICKS,   10.0f,  170.0f,  120.0f,  false }, // Joint 2: Elbow
    {   3, SERVO_MIN_TICKS, SERVO_MAX_TICKS,    0.0f,  180.0f,   90.0f,  false }  // Joint 3: Wrist Pitch
    // To add gripper:
    // { 4, SERVO_MIN_TICKS, SERVO_MAX_TICKS,    0.0f,  100.0f,   10.0f,  false }  // Joint 4: Gripper
};

// Friendly joint names matching JOINT_CONFIGS indices
static const char* const JOINT_NAMES[NUM_JOINTS] = {
    "Base", "Shoulder", "Elbow", "Wrist"
};

// =============================================================================
// 4. MOTION & SMOOTHING CONFIGURATION
// =============================================================================
constexpr float SERVO_LOOP_FREQ_HZ         = 50.0f;  // Control loop update rate (50 Hz = 20ms)
constexpr float REFERENCE_SMOOTH_FACTOR    = 0.05f;  // Nominal smoothing factor (current += diff * 0.05)
constexpr float DEFAULT_SPEED_PERCENT      = 50.0f;  // Default motion speed (1-100%)

// =============================================================================
// 5. SAFETY MONITOR CONFIGURATION
// =============================================================================
constexpr uint32_t COMMAND_TIMEOUT_MS      = 5000;   // 5 seconds command watchdog timeout
constexpr bool RETURN_HOME_ON_TIMEOUT      = false;  // False: Hold last safe pose; True: Return to home
constexpr bool DISABLE_SERVO_ON_TIMEOUT    = false;  // False: Keep servos holding; True: Pull OE high

// =============================================================================
// 6. WIFIMANAGER PROVISIONING & WATCHDOG CONFIGURATION
// =============================================================================
// SoftAP captive portal configuration (shown if no credentials or connection fails)
constexpr const char* WM_AP_NAME           = "RobotArm-Setup";
// NOTE: WPA2 requires password to be >= 8 characters, or AP fails to start!
constexpr const char* WM_AP_PASSWORD       = "robotarm123";

// Timeouts
constexpr uint32_t WM_PORTAL_TIMEOUT_SEC   = 180;    // Portal active for 3 minutes before action
constexpr uint32_t WM_CONNECT_TIMEOUT_SEC  = 20;     // Time spent trying saved NVS credentials
constexpr bool     WM_REBOOT_ON_TIMEOUT    = false;  // False: Keep running without bootlooping if WiFi fails

// Hardware credential reset button (DevKit V1 BOOT button = GPIO 0, Active LOW)
constexpr int      BOOT_BUTTON_PIN         = 0;
constexpr uint32_t RESET_BUTTON_HOLD_MS    = 3000;   // Hold for 3s during power-up to wipe NVS

// Web Application Server & WebSocket Endpoint
constexpr uint16_t HTTP_SERVER_PORT        = 80;
constexpr const char* WS_ENDPOINT_PATH     = "/ws";
constexpr float    STATUS_BROADCAST_FREQ_HZ= 5.0f;   // 5 Hz = every 200ms

// Reconnection Watchdog Configuration (Exponential Backoff)
constexpr uint32_t WIFI_INITIAL_BACKOFF_MS = 2000;   // Initial retry delay: 2s
constexpr uint32_t WIFI_MAX_BACKOFF_MS     = 60000;  // Maximum backoff cap: 60s
constexpr uint32_t WIFI_WATCHDOG_STACK_SIZE= 3072;
constexpr UBaseType_t WIFI_WATCHDOG_PRIORITY = 1;
constexpr BaseType_t  WIFI_WATCHDOG_CORE   = 0;

// Task: WifiManagerTask (Core 0 - Runs captive portal web server & DNS)
constexpr uint32_t WM_TASK_STACK_SIZE      = 10240;  // 10KB stack for portal HTML generation
constexpr UBaseType_t WM_TASK_PRIORITY     = 1;      // Priority 1 ensures IDLE0 task gets CPU time
constexpr BaseType_t  WM_TASK_CORE         = 0;

// =============================================================================
// 7. FREERTOS TASK SCHEDULING, CORES & STACKS
// =============================================================================
// Queue depth for incoming motion commands
constexpr size_t COMMAND_QUEUE_DEPTH       = 10;

// Task: ServoControlTask (Core 1 - Dedicated real-time hardware loop)
constexpr uint32_t SERVO_TASK_STACK_SIZE   = 4096;
constexpr UBaseType_t SERVO_TASK_PRIORITY  = 3;      // High priority for jitter-free PWM
constexpr BaseType_t SERVO_TASK_CORE       = 1;

// Task: NetworkTask (Core 0 - WiFi, WebServer, WebSocket processing)
constexpr uint32_t NETWORK_TASK_STACK_SIZE = 8192;
constexpr UBaseType_t NETWORK_TASK_PRIORITY= 2;
constexpr BaseType_t NETWORK_TASK_CORE      = 0;

// Task: SafetyTask (Core 0 - Watchdog & hardware enable monitoring)
constexpr uint32_t SAFETY_TASK_STACK_SIZE  = 3072;
constexpr UBaseType_t SAFETY_TASK_PRIORITY = 1;
constexpr BaseType_t SAFETY_TASK_CORE      = 0;

// Task: StatusTask (Core 0 - Periodic telemetry broadcast to WebSocket)
constexpr uint32_t STATUS_TASK_STACK_SIZE  = 4096;
constexpr UBaseType_t STATUS_TASK_PRIORITY = 1;
constexpr BaseType_t STATUS_TASK_CORE      = 0;

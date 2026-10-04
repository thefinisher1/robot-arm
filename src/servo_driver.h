/**
 * @file servo_driver.h
 * @brief PCA9685 16-channel PWM driver interface for servo positioning,
 *        hardware enable control, and pulse-to-tick math.
 */

#pragma once

#include <Arduino.h>
#include "config.h"

/**
 * @brief Initialize Wire (I2C), PCA9685 PWM controller, and hardware OE pin.
 *        Keeps OE disabled (HIGH) until home pose PWM registers are primed.
 * @return True if PCA9685 responded over I2C, false otherwise.
 */
bool servoDriverInit();

/**
 * @brief Converts a joint's physical angle in degrees to 12-bit PCA9685 timer counts.
 * 
 * Formula:
 *   ticks = pulse_us / (1,000,000 / freq / 4096)
 * 
 * @param config Joint configuration and calibration bounds.
 * @param angleDeg Desired angle in degrees.
 * @return 12-bit tick count (0 - 4095).
 */
uint16_t servoAngleToTicks(const JointConfig& config, float angleDeg);

/**
 * @brief Clamps angle to joint limits and writes corresponding PWM to PCA9685.
 * 
 * @param jointIndex Index of joint (0 to NUM_JOINTS - 1).
 * @param angleDeg Desired angle in degrees.
 */
void servoWriteAngle(uint8_t jointIndex, float angleDeg);

/**
 * @brief Controls the PCA9685 OE (Output Enable) pin.
 * 
 * @param enable True to enable outputs (OE LOW); False to disable (OE HIGH).
 */
void servoSetHardwareEnable(bool enable);

/**
 * @brief Write all calibrated joints to their home configuration.
 */
void servoWriteAllHome();

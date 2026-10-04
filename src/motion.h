/**
 * @file motion.h
 * @brief Trajectory smoothing and rate-limited interpolation engine for
 *        multi-joint articulated robotic arms.
 */

#pragma once

#include <Arduino.h>
#include "types.h"

/**
 * @brief Initialize robot state vectors to configured home poses.
 * 
 * @param state RobotState instance to initialize.
 */
void motionInit(RobotState& state);

/**
 * @brief Advances joint current angles toward target angles using smooth exponential ease-out.
 * 
 * Math and Smoothing:
 *   difference = targetAngle - currentAngle
 *   currentAngle += difference * smoothingFactor   // (e.g. 0.05 from reference code)
 *   if (abs(difference) < 0.5) currentAngle = targetAngle
 * 
 * Creates natural, organic deceleration curves, eliminates jitter, and protects servo gears.
 * 
 * @param state Robot state containing current and target angles.
 * @return True if any joint is still in motion, false if all joints reached setpoints.
 */
bool motionUpdate(RobotState& state);

/**
 * @file motion.cpp
 * @brief Implementation of trajectory smoothing, speed scaling, and setpoint stepping.
 */

#include "motion.h"
#include <math.h>

void motionInit(RobotState& state) {
    for (size_t i = 0; i < NUM_JOINTS; ++i) {
        state.currentAngle[i] = JOINT_CONFIGS[i].homeAngle;
        state.targetAngle[i]  = JOINT_CONFIGS[i].homeAngle;
    }
    state.currentSpeedPercent      = DEFAULT_SPEED_PERCENT;
    state.enabled                  = true;
    state.moving                   = false;
    state.lastCommandTimestampMs   = millis();
    Serial.println("[MOTION] Motion engine initialized to home positions.");
}

bool motionUpdate(RobotState& state) {
    // 1. Clamp speed percentage to valid operating envelope (1% to 100%)
    float speedPct = constrain(state.currentSpeedPercent, 1.0f, 100.0f);

    // 2. Compute smooth motion factor derived from reference code:
    //    Reference: currentAngle += (targetAngle - currentAngle) * 0.05;
    //    At default 50% speed: factor is 0.05
    //    At 100% speed: factor is 0.10
    //    At 10% speed: factor is 0.01
    float smoothingFactor = (speedPct / 100.0f) * (REFERENCE_SMOOTH_FACTOR * 2.0f);
    smoothingFactor = constrain(smoothingFactor, 0.005f, 0.40f);

    bool anyJointMoving = false;

    for (size_t i = 0; i < NUM_JOINTS; ++i) {
        float difference = state.targetAngle[i] - state.currentAngle[i];

        // Reference target reached condition: abs(difference) < 0.5
        if (fabsf(difference) < 0.5f) {
            state.currentAngle[i] = state.targetAngle[i];
        } else {
            // Smooth exponential ease-out movement toward target
            state.currentAngle[i] += difference * smoothingFactor;
            anyJointMoving = true;
        }
    }

    state.moving = anyJointMoving;
    return anyJointMoving;
}

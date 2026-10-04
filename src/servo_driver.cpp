/**
 * @file servo_driver.cpp
 * @brief Implementation of PCA9685 driver communication, pulse-to-tick math,
 *        and OE hardware enable control.
 */

#include "servo_driver.h"
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

// Static driver instance
static Adafruit_PWMServoDriver s_pwmDriver(PCA9685_I2C_ADDR);

void servoSetHardwareEnable(bool enable) {
    // PCA9685 OE is active LOW:
    // Writing LOW  -> Turns ON internal MOSFET gates (servos receive PWM pulses)
    // Writing HIGH -> Floats all outputs to high impedance (servos de-energized)
    digitalWrite(SERVO_OE_PIN, enable ? LOW : HIGH);
    Serial.printf("[SERVO] Hardware Output Enable set to: %s (GPIO%d = %s)\n",
                  enable ? "ENABLED" : "DISABLED",
                  SERVO_OE_PIN,
                  enable ? "LOW" : "HIGH");
}

bool servoDriverInit() {
    Serial.println("[SERVO] Initializing I2C bus and PCA9685 driver...");

    // Configure OE pin first and disable outputs immediately to prevent
    // uncontrolled servo twitching while configuring registers.
    pinMode(SERVO_OE_PIN, OUTPUT);
    digitalWrite(SERVO_OE_PIN, HIGH);

    // Initialize I2C with defined pins and fast clock
    if (!Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQ_HZ)) {
        Serial.println("[SERVO] ERROR: Wire.begin() failed on pins 21/22!");
        return false;
    }

    // Ping PCA9685 I2C address to verify communication
    Wire.beginTransmission(PCA9685_I2C_ADDR);
    if (Wire.endTransmission() != 0) {
        Serial.printf("[SERVO] ERROR: PCA9685 not detected at I2C address 0x%02X!\n", PCA9685_I2C_ADDR);
        return false;
    }

    // Initialize PCA9685 library
    s_pwmDriver.begin();
    s_pwmDriver.setOscillatorFrequency(PCA9685_OSC_HZ);
    s_pwmDriver.setPWMFreq(PCA9685_PWM_FREQ);

    // Give PCA9685 internal oscillator time to settle
    delay(10);

    Serial.printf("[SERVO] PCA9685 online at 0x%02X with %0.1f Hz PWM.\n",
                  PCA9685_I2C_ADDR, PCA9685_PWM_FREQ);
    return true;
}

uint16_t servoAngleToTicks(const JointConfig& config, float angleDeg) {
    // 1. Clamp angle strictly within configured mechanical safety boundaries
    float clampedAngle = constrain(angleDeg, config.minAngle, config.maxAngle);

    // 2. Handle inversion if servo horn is mounted mirrored/reversed
    float effectiveAngle = config.invert
        ? (config.maxAngle - (clampedAngle - config.minAngle))
        : clampedAngle;

    // 3. Map mechanical angle to PCA9685 PWM pulse tick count [150, 600]
    //    Matches reference: int pulse = map(angle, 0, 180, SERVO_MIN, SERVO_MAX);
    float angleRatio = (effectiveAngle - config.minAngle) / (config.maxAngle - config.minAngle);
    int pulse = static_cast<int>(roundf(config.minPulseTicks + angleRatio * (config.maxPulseTicks - config.minPulseTicks)));

    // 4. Clamp ticks to 12-bit PCA9685 range [0, 4095]
    return static_cast<uint16_t>(constrain(pulse, 0, 4095));
}

void servoWriteAngle(uint8_t jointIndex, float angleDeg) {
    if (jointIndex >= NUM_JOINTS) {
        Serial.printf("[SERVO] ERROR: Joint index %d exceeds NUM_JOINTS (%d)\n",
                      jointIndex, NUM_JOINTS);
        return;
    }

    const JointConfig& cfg = JOINT_CONFIGS[jointIndex];
    uint16_t ticks = servoAngleToTicks(cfg, angleDeg);

    // PCA9685 setPWM(channel, on_tick, off_tick):
    // Turn pulse ON at tick 0 and OFF at target tick count.
    s_pwmDriver.setPWM(cfg.channel, 0, ticks);
}

void servoWriteAllHome() {
    Serial.println("[SERVO] Writing all joints to calibrated HOME angles...");
    for (size_t i = 0; i < NUM_JOINTS; ++i) {
        servoWriteAngle(i, JOINT_CONFIGS[i].homeAngle);
    }
}

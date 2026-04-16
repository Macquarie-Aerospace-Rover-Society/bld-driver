#pragma once
#include <Arduino.h>

// —— Motor state ——
enum MovementState {
  STATE_IDLE,
  STATE_ENABLED,
  STATE_MOVING_FORWARD,
  STATE_MOVING_BACKWARD
};

// Defined in rover-bld-driver.ino
extern const uint8_t  FR_PINS[4];
extern const uint8_t  EN_PINS[4];
extern const uint8_t  SV_PINS[4];
extern const uint32_t PWM_RESOLUTION;

// Defined in rover_control.h
extern int            turnDir;
extern int32_t        currentSpeed;
extern MovementState  currentState;

// —— Motor algorithm tuning ——
const uint32_t SLOW_SIDE_MAX_PERCENT_DIFF = 90;  // max inner-side reduction (%)
const float    outerBoostPct              = 20.0f; // outer-side boost (%)

// v_speed: 0..PWM_RESOLUTION
// Uses globals: turnDir, currentSpeed, PWM_RESOLUTION
void setRobotSpeed(int32_t v_speed) {
  if (v_speed < 0) v_speed = 0;
  if (v_speed > (int32_t)PWM_RESOLUTION) v_speed = PWM_RESOLUTION;
  currentSpeed = constrain(v_speed, 0L, (long)PWM_RESOLUTION);

  float tf = (float)constrain(turnDir, -100, 100) / 100.0f;

  if (fabs(tf) < 0.0001f) {
    for (int i = 0; i < 4; ++i) analogWrite(SV_PINS[i], currentSpeed);
    return;
  }

  float slowPct     = fabs(tf) * (float)SLOW_SIDE_MAX_PERCENT_DIFF;
  float innerScale  = (100.0f - slowPct) / 100.0f;
  float boostFactor = 1.0f + (outerBoostPct / 100.0f) * fabs(tf);

  int32_t leftSpeed  = currentSpeed;
  int32_t rightSpeed = currentSpeed;

  if (tf > 0.0f) {
    // Turning right: right is inner (slow), left is outer (boost)
    rightSpeed = (int32_t)round((float)currentSpeed * innerScale);
    leftSpeed  = (int32_t)round(constrain((float)currentSpeed * boostFactor,
                                          0.0f, (float)PWM_RESOLUTION));
  } else {
    // Turning left: left is inner (slow), right is outer (boost)
    leftSpeed  = (int32_t)round((float)currentSpeed * innerScale);
    rightSpeed = (int32_t)round(constrain((float)currentSpeed * boostFactor,
                                          0.0f, (float)PWM_RESOLUTION));
  }

  analogWrite(SV_PINS[1], constrain(rightSpeed, 0, 255)); // front-right
  analogWrite(SV_PINS[0], constrain(leftSpeed,  0, 255)); // front-left
  analogWrite(SV_PINS[3], constrain(rightSpeed, 0, 255)); // back-right
  analogWrite(SV_PINS[2], constrain(leftSpeed,  0, 255)); // back-left
}

void setRobotDirection(bool dir) {
  int i = 0;
  digitalWrite(FR_PINS[i++], dir ? HIGH : LOW);
  digitalWrite(FR_PINS[i++], dir ? LOW  : HIGH);
  digitalWrite(FR_PINS[i++], dir ? HIGH : LOW);
  digitalWrite(FR_PINS[i++], dir ? LOW  : HIGH);
}

void enableMotors() {
  for (int i = 0; i < 4; i++) {
    pinMode(EN_PINS[i], OUTPUT);
    digitalWrite(EN_PINS[i], LOW);
  }
}

void disableMotors() {
  for (int i = 0; i < 4; i++) {
    pinMode(EN_PINS[i], INPUT);
  }
  setRobotSpeed(0);
  currentState = STATE_IDLE;
}

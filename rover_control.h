#pragma once
#include <Arduino.h>
#include "motor_driver.h"

// Defined in rover-bld-driver.ino
extern const uint32_t DEFAULT_SPEED;

// —— Movement state ——
MovementState currentState  = STATE_IDLE;
unsigned long movementStart = 0;
const unsigned long movementDur = 3000; // ms, only used for manual control

// —— Speed & turning ——
int     turnDir      = 0;   // -100..+100 (center = 0)
int32_t currentSpeed = 0;   // 0..DEFAULT_SPEED

// —— Control mode ——
bool isGamepadMode = false;

void beginMovement(bool forward) {
  if (currentState != STATE_ENABLED) {
    Serial.println(F("Motors not enabled"));
    return;
  }
  setRobotDirection(forward);
  setRobotSpeed(DEFAULT_SPEED);
  movementStart = millis();
  currentState  = forward ? STATE_MOVING_FORWARD : STATE_MOVING_BACKWARD;
}

/**
 * onControl: action = "forward"/"backward"/"stop"/"start" or empty
 * sliderValue = -100..100
 */
void onControl(const String& action, int sliderValue) {
  isGamepadMode = false;

  if (sliderValue < -100) sliderValue = -100;
  if (sliderValue >  100) sliderValue =  100;

  if (turnDir != sliderValue) {
    turnDir = sliderValue;
    setRobotSpeed(currentSpeed); // reapply current speed with new turn
  }

  if (action == "forward") {
    Serial.println(F("Forward"));
    beginMovement(true);
  } else if (action == "backward") {
    Serial.println(F("Back"));
    beginMovement(false);
  } else if (action == "stop") {
    disableMotors();
    currentState = STATE_IDLE;
    turnDir = 0;
    Serial.println(F("■ Stopped & disabled"));
  } else if (action == "start") {
    enableMotors();
    currentState = STATE_ENABLED;
    Serial.println(F("▶ Motors enabled (speed=0)"));
  }

  Serial.print("Slider at: ");
  Serial.println(sliderValue);
}

/**
 * onGamepadControl: continuous real-time input from gamepad
 * speed = -255..255 (negative=backward, positive=forward, 0=stop)
 * turn  = -100..100 (negative=left, positive=right, 0=straight)
 */
void onGamepadControl(int speed, int turn) {
  isGamepadMode = true;

  turnDir = constrain(turn, -100, 100);

  if (speed == 0 && turn == 0) {
    if (currentState != STATE_ENABLED) {
      setRobotSpeed(0);
      currentState = STATE_ENABLED;
    }
  } else if (speed == 0) {
    setRobotSpeed(0);
  } else {
    enableMotors();
    if (currentState == STATE_IDLE)
      currentState = STATE_ENABLED;

    bool forward = (speed > 0);
    setRobotDirection(forward);

    int absSpeed = constrain(abs(speed), 0, 255);
    setRobotSpeed(absSpeed);

    currentState = forward ? STATE_MOVING_FORWARD : STATE_MOVING_BACKWARD;
  }

  static unsigned long lastLog = 0;
  if (millis() - lastLog > 200 || speed == 0) {
    Serial.print("Gamepad - Speed: ");
    Serial.print(speed);
    Serial.print(", Turn: ");
    Serial.println(turn);
    lastLog = millis();
  }
}

void printHelp() {
  Serial.println(F("Commands:"));
  Serial.println(F("  w → forward 3s"));
  Serial.println(F("  s → backward 3s"));
  Serial.println(F("  x → stop & disable"));
  Serial.println(F("  p → enable motors"));
}

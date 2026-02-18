#include <Arduino.h>
#include "bldrouter.h"

// —— Pin assignments ——
const uint8_t FR_PINS[4] = {4, 5, 6, 7};     // F/R = HIGH: forward, LOW: reverse
const uint8_t EN_PINS[4] = {15, 16, 17, 18}; // change to your wiring
const uint8_t SV_PINS[4] = {11, 12, 13, 14}; // PWM output pins (0…255)

// Func prototypes
void setRobotDirection(bool forward);
void setRobotSpeed(int32_t v_speed); // accepts 0..DEFAULT_SPEED
void printHelp();
void onManualControl(const String &action, int sliderValue);
void onGamepadControl(int speed, int turn);

// —— PWM configuration ——
const uint32_t PWM_RESOLUTION = 255; // analogWrite range 0..255
#ifndef TARGET_DUTY_CYCLE_PERCENT
#define TARGET_DUTY_CYCLE_PERCENT 80
#endif
const uint32_t DEFAULT_SPEED = (PWM_RESOLUTION * TARGET_DUTY_CYCLE_PERCENT) / 100;

// —— Movement timing ——
enum MovementState
{
  STATE_IDLE,
  STATE_ENABLED,
  STATE_MOVING_FORWARD,
  STATE_MOVING_BACKWARD
};
MovementState currentState = STATE_IDLE;
unsigned long movementStart = 0;
const unsigned long movementDur = 3000; // ms - only used for manual control

// Movement Speed & Turning
int turnDir = 0;                                // now -100..+100 (center=0)
int32_t currentSpeed = 0;                       // 0..DEFAULT_SPEED
const uint32_t SLOW_SIDE_MAX_PERCENT_DIFF = 90; // max reduction percent on inner side (increased)
const float outerBoostPct = 20.0f;              // percent boost for outer side (0..100)

// Control mode tracking
bool isGamepadMode = false; // true when gamepad is active

void setup()
{
  Serial.begin(115200);

  for (int i = 0; i < 4; i++)
  {
    pinMode(EN_PINS[i], INPUT); // High-Z => disabled
    pinMode(FR_PINS[i], OUTPUT);
    pinMode(SV_PINS[i], OUTPUT);
    analogWrite(SV_PINS[i], 0);
  }

  currentState = STATE_IDLE;
  setRobotDirection(true);
  Serial.println(F("Commands: w=forward, s=backward, x=stop, p=enable, ?=help"));

  setupAP(onManualControl, onGamepadControl);
}

void loop()
{
  while (Serial.available())
  {
    char cmd = Serial.read();
    if (cmd == '\n' || cmd == '\r')
      continue;
    switch (cmd)
    {
    case 'w':
      Serial.println(F("Forward"));
      beginMovement(true);
      break;
    case 's':
      Serial.println(F("Back"));
      beginMovement(false);
      break;
    case 'x':
      disableMotors();
      currentState = STATE_IDLE;
      Serial.println(F("■ Stopped & disabled"));
      break;
    case 'p':
      enableMotors();
      currentState = STATE_ENABLED;
      Serial.println(F("▶ Motors enabled (speed=0)"));
      break;
    default:
      printHelp();
      break;
    }
  }

  // Auto-stop timeout only applies to manual control mode
  if (!isGamepadMode &&
      (currentState == STATE_MOVING_FORWARD || currentState == STATE_MOVING_BACKWARD) &&
      ((millis() - movementStart) >= movementDur))
  {
    setRobotSpeed(0);
    currentState = STATE_ENABLED;
    Serial.println(F("✓ Movement complete"));
  }

  serverBLD.handleClient();
}

void beginMovement(bool forward)
{
  if (currentState != STATE_ENABLED)
  {
    Serial.println(F("Motors not enabled"));
    return;
  }
  setRobotDirection(forward);
  setRobotSpeed(DEFAULT_SPEED); // use DEFAULT_SPEED as requested travel speed
  movementStart = millis();
  currentState = forward ? STATE_MOVING_FORWARD : STATE_MOVING_BACKWARD;
}

void enableMotors()
{
  for (int i = 0; i < 4; i++)
  {
    pinMode(EN_PINS[i], OUTPUT);
    digitalWrite(EN_PINS[i], LOW);
  }
}

void disableMotors()
{
  for (int i = 0; i < 4; i++)
  {
    pinMode(EN_PINS[i], INPUT);
  }
  setRobotSpeed(0);
}

void setRobotDirection(bool dir)
{
  int i = 0;
  digitalWrite(FR_PINS[i++], dir ? HIGH : LOW);
  digitalWrite(FR_PINS[i++], dir ? LOW : HIGH);
  digitalWrite(FR_PINS[i++], dir ? HIGH : LOW);
  digitalWrite(FR_PINS[i++], dir ? LOW : HIGH);
}

// v_speed: 0..DEFAULT_SPEED
// turnDir: -100..+100, where -100 = full left, 0 = straight, +100 = full right
void setRobotSpeed(int32_t v_speed)
{
  // clamp speed
  if (v_speed < 0)
    v_speed = 0;
  if (v_speed > (int32_t)PWM_RESOLUTION)
    v_speed = PWM_RESOLUTION;
  // currentSpeed uses requested v_speed (not constrained to DEFAULT_SPEED)
  currentSpeed = constrain(v_speed, 0L, (long)PWM_RESOLUTION);

  // turn factor -1.0 .. 0 .. +1.0
  float tf = (float)constrain(turnDir, -100, 100) / 100.0f;

  // Straight
  if (fabs(tf) < 0.0001f)
  {
    for (int i = 0; i < 4; ++i)
      analogWrite(SV_PINS[i], currentSpeed);
    return;
  }

  // inner slowdown percent = |tf| * SLOW_SIDE_MAX_PERCENT_DIFF (0..SLOW_SIDE_MAX_PERCENT_DIFF)
  float slowPct = fabs(tf) * (float)SLOW_SIDE_MAX_PERCENT_DIFF;
  float innerScale = (100.0f - slowPct) / 100.0f; // 1.0 .. (1 - max)

  // outer boost factor based on outerBoostPct and turn magnitude
  float boostFactor = 1.0f + (outerBoostPct / 100.0f) * fabs(tf);

  // Decide left/right speeds.
  // SV_PINS assumed order: [front-right, front-left, back-right, back-left]
  int32_t leftSpeed = currentSpeed;
  int32_t rightSpeed = currentSpeed;

  if (tf > 0.0f)
  {
    // turning right -> right side is inner -> slow right, boost left (outer)
    rightSpeed = (int32_t)round((float)currentSpeed * innerScale);
    leftSpeed = (int32_t)round(constrain((float)currentSpeed * boostFactor, 0.0f, (float)PWM_RESOLUTION));
  }
  else
  {
    // turning left -> left side is inner -> slow left, boost right (outer)
    leftSpeed = (int32_t)round((float)currentSpeed * innerScale);
    rightSpeed = (int32_t)round(constrain((float)currentSpeed * boostFactor, 0.0f, (float)PWM_RESOLUTION));
  }

  // Apply speeds
  analogWrite(SV_PINS[0], constrain(rightSpeed, 0, 255)); // front-right
  analogWrite(SV_PINS[1], constrain(leftSpeed, 0, 255));  // front-left
  analogWrite(SV_PINS[2], constrain(rightSpeed, 0, 255)); // back-right
  analogWrite(SV_PINS[3], constrain(leftSpeed, 0, 255));  // back-left
}

/**
 * onManualControl: Handles discrete button commands and slider input
 * action = "forward"/"backward"/"stop"/"start" or empty
 * sliderValue = -100..100 (turn direction)
 */
void onManualControl(const String &action, int sliderValue)
{
  isGamepadMode = false; // Switch to manual control mode

  // Handle turn slider control
  if (sliderValue < -100)
    sliderValue = -100;
  if (sliderValue > 100)
    sliderValue = 100;

  if (turnDir != sliderValue)
  {
    turnDir = sliderValue;
    // Only reapply speed if motors are actively moving
    if (currentSpeed > 0)
    {
      setRobotSpeed(currentSpeed);
    }
  }

  // Handle button commands
  if (action == "forward")
  {
    Serial.println(F("Manual: Forward"));
    beginMovement(true);
  }
  else if (action == "backward")
  {
    Serial.println(F("Manual: Backward"));
    beginMovement(false);
  }
  else if (action == "stop")
  {
    disableMotors();
    currentState = STATE_IDLE;
    turnDir = 0; // Reset turn on stop
    Serial.println(F("■ Manual: Stopped & disabled"));
  }
  else if (action == "start")
  {
    enableMotors();
    currentState = STATE_ENABLED;
    Serial.println(F("▶ Manual: Motors enabled (speed=0)"));
  }

  if (action.length() > 0 || turnDir != 0)
  {
    Serial.print("Manual - Action: ");
    Serial.print(action);
    Serial.print(", Turn: ");
    Serial.println(turnDir);
  }
}

/**
 * onGamepadControl: Handles continuous real-time input from gamepad
 * speed = -255 to 255 (negative=backward, positive=forward, 0=stop)
 * turn = -100 to 100 (negative=left, positive=right, 0=straight)
 */
void onGamepadControl(int speed, int turn)
{
  isGamepadMode = true; // Switch to gamepad control mode

  // Update turn direction
  turnDir = constrain(turn, -100, 100);

  // Handle speed and direction
  if (speed == 0 && turn == 0)
  {
    // Complete stop - return to enabled state
    if (currentState != STATE_ENABLED)
    {
      setRobotSpeed(0);
      currentState = STATE_ENABLED;
    }
  }
  else if (speed == 0)
  {
    // Only turning input, but no forward/backward speed
    // Keep current state but set speed to 0
    setRobotSpeed(0);
  }
  else
  {
    // Active movement
    // Ensure motors are enabled
    if (currentState == STATE_IDLE)
    {
      enableMotors();
      currentState = STATE_ENABLED;
    }

    // Set direction based on speed sign
    bool forward = (speed > 0);
    setRobotDirection(forward);

    // Apply speed (use absolute value)
    int absSpeed = constrain(abs(speed), 0, 255);
    setRobotSpeed(absSpeed);

    // Update state
    currentState = forward ? STATE_MOVING_FORWARD : STATE_MOVING_BACKWARD;
  }

  // Gamepad gives continuous feedback, so we log less frequently
  static unsigned long lastLog = 0;
  if (millis() - lastLog > 200 || speed == 0) // Log every 200ms or on stop
  {
    Serial.print("Gamepad - Speed: ");
    Serial.print(speed);
    Serial.print(", Turn: ");
    Serial.println(turn);
    lastLog = millis();
  }
}

void printHelp()
{
  Serial.println(F("Commands:"));
  Serial.println(F("  w → forward 3s"));
  Serial.println(F("  s → backward 3s"));
  Serial.println(F("  x → stop & disable"));
  Serial.println(F("  p → enable motors"));
}

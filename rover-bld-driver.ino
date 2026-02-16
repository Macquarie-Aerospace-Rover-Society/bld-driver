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
void onControl(const String &action, int sliderValue);
void cycleSpeedSetting();

// —— PWM configuration ——
const uint32_t PWM_RESOLUTION = 255; // analogWrite range 0..255
#ifndef TARGET_DUTY_CYCLE_PERCENT
#define TARGET_DUTY_CYCLE_PERCENT 80
#endif
const uint32_t DEFAULT_SPEED = (PWM_RESOLUTION * TARGET_DUTY_CYCLE_PERCENT) / 100;

// —— Speed Settings ——
const uint8_t NUM_SPEED_SETTINGS = 4;
const float SPEED_SETTINGS[NUM_SPEED_SETTINGS] = {0.25f, 0.50f, 0.75f, 1.0f}; // 25%, 50%, 75%, 100%
uint8_t currentSpeedSetting = 2;                                              // Start at 75% (index 2)

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
const unsigned long movementDur = 3000; // ms

// Movement Speed & Turning
int turnDir = 0;                                // now -100..+100 (center=0)
int32_t currentSpeed = 0;                       // 0..DEFAULT_SPEED
const uint32_t SLOW_SIDE_MAX_PERCENT_DIFF = 90; // max reduction percent on inner side (increased)
const float outerBoostPct = 20.0f;              // percent boost for outer side (0..100)

// —— Gamepad Auto-Idle ——
unsigned long lastGamepadInput = 0;
const unsigned long GAMEPAD_IDLE_TIMEOUT = 500; // ms - return to idle if no input for 500ms

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

  setupAP(onControl);
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

  if ((currentState == STATE_MOVING_FORWARD || currentState == STATE_MOVING_BACKWARD) && ((millis() - movementStart) >= movementDur))
  {
    setRobotSpeed(0);
    currentState = STATE_ENABLED;
    Serial.println(F("✓ Movement complete"));
  }

  // Check for gamepad timeout - return to idle if no input received
  if (currentState != STATE_IDLE &&
      lastGamepadInput > 0 &&
      (millis() - lastGamepadInput) > GAMEPAD_IDLE_TIMEOUT)
  {
    setRobotSpeed(0);
    if (currentState != STATE_ENABLED)
    {
      currentState = STATE_ENABLED;
      Serial.println(F("⏸ Gamepad idle - returned to enabled state"));
    }
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
  int i = 0;
  analogWrite(SV_PINS[i++], constrain(rightSpeed, 0, 255)); // front-right
  analogWrite(SV_PINS[i++], constrain(leftSpeed, 0, 255));  // front-left
  analogWrite(SV_PINS[i++], constrain(rightSpeed, 0, 255)); // back-right
  analogWrite(SV_PINS[i++], constrain(leftSpeed, 0, 255));  // back-left
}

/**
 * handleGamepadInput: processes gamepad axes and buttons
 * axisX: -100..100 (turning)
 * axisY: -100..100 (forward/backward speed)
 * btn4: cycle speed setting
 * btn5: set to idle
 */
void handleGamepadInput(int axisX, int axisY, bool btn4, bool btn5)
{
  lastGamepadInput = millis();

  // Handle buttons
  static bool btn4WasPressed = false;
  static bool btn5WasPressed = false;

  if (btn5 && !btn5WasPressed)
  {
    // Button 5 pressed - set to idle
    disableMotors();
    currentState = STATE_IDLE;
    Serial.println(F("■ Button 5 - Set to IDLE"));
  }
  btn5WasPressed = btn5;

  if (btn4 && !btn4WasPressed)
  {
    // Button 4 pressed - cycle speed
    cycleSpeedSetting();
  }
  btn4WasPressed = btn4;

  // If in idle state and receiving input, enable motors
  if (currentState == STATE_IDLE && (abs(axisX) > 5 || abs(axisY) > 5))
  {
    enableMotors();
    currentState = STATE_ENABLED;
    Serial.println(F("▶ Gamepad input - motors enabled"));
  }

  // Update turning direction
  turnDir = constrain(axisX, -100, 100);

  // Handle speed and direction from Y axis
  // Positive Y = forward, Negative Y = backward
  if (abs(axisY) < 5)
  {
    // Dead zone - stop motors but keep enabled
    setRobotSpeed(0);
    if (currentState == STATE_MOVING_FORWARD || currentState == STATE_MOVING_BACKWARD)
    {
      currentState = STATE_ENABLED;
    }
  }
  else
  {
    // Apply speed with speed setting multiplier
    float speedMultiplier = SPEED_SETTINGS[currentSpeedSetting];
    int32_t targetSpeed = (int32_t)((float)DEFAULT_SPEED * speedMultiplier * (abs(axisY) / 100.0f));

    // Set direction based on Y axis
    bool forward = (axisY > 0);
    setRobotDirection(forward);
    setRobotSpeed(targetSpeed);

    currentState = forward ? STATE_MOVING_FORWARD : STATE_MOVING_BACKWARD;
  }
}

/**
 * onControl: handles both button actions and gamepad input
 * action = "forward"/"backward"/"stop"/"start"/"gamepad"/"cycleSpeed"/"setIdle"
 * sliderValue = -100..100 (for slider or gamepad axes)
 *
 * For gamepad mode, expects query params:
 *   axisX (turning): -100..100
 *   axisY (speed/direction): -100..100
 *   btn4 (cycle speed): 0 or 1
 *   btn5 (set idle): 0 or 1
 */
void onControl(const String &action, int sliderValue)
{
  if (action == "gamepad")
  {
    // This is handled in the web server with multiple parameters
    // We'll update bldrouter.h to parse gamepad data
    lastGamepadInput = millis();
    return;
  }

  if (action == "cycleSpeed")
  {
    cycleSpeedSetting();
    return;
  }

  if (action == "setIdle")
  {
    disableMotors();
    currentState = STATE_IDLE;
    Serial.println(F("■ Button 5 - Set to IDLE"));
    return;
  }

  // Expect sliderValue in -100..100. If your UI sends 0..100, convert externally.
  if (sliderValue < -100)
    sliderValue = -100;
  if (sliderValue > 100)
    sliderValue = 100;

  if (turnDir != sliderValue)
  {
    turnDir = sliderValue;
    setRobotSpeed(currentSpeed); // reapply current speed with new turn
  }

  if (action == "forward")
  {
    Serial.println(F("Forward"));
    beginMovement(true);
  }
  else if (action == "backward")
  {
    Serial.println(F("Back"));
    beginMovement(false);
  }
  else if (action == "stop")
  {
    disableMotors();
    currentState = STATE_IDLE;
    Serial.println(F("■ Stopped & disabled"));
  }
  else if (action == "start")
  {
    enableMotors();
    currentState = STATE_ENABLED;
    Serial.println(F("▶ Motors enabled (speed=0)"));
  }

  Serial.print("Slider at: ");
  Serial.println(sliderValue);
}

void cycleSpeedSetting()
{
  currentSpeedSetting = (currentSpeedSetting + 1) % NUM_SPEED_SETTINGS;
  Serial.print(F("Speed setting: "));
  Serial.print((int)(SPEED_SETTINGS[currentSpeedSetting] * 100));
  Serial.println(F("%"));
}

void printHelp()
{
  Serial.println(F("Commands:"));
  Serial.println(F("  w → forward 3s"));
  Serial.println(F("  s → backward 3s"));
  Serial.println(F("  x → stop & disable"));
  Serial.println(F("  p → enable motors"));
}

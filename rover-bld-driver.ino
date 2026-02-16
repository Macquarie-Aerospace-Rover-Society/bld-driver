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
void onGamepadControl(const String &action, int forwardSpeed, int steering);

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
const unsigned long movementDur = 3000; // ms

// Movement Speed & Turning
int turnDir = 0;                                // now -100..+100 (center=0)
int32_t currentSpeed = 0;                       // 0..DEFAULT_SPEED
const uint32_t SLOW_SIDE_MAX_PERCENT_DIFF = 90; // max reduction percent on inner side (increased)
const float outerBoostPct = 20.0f;              // percent boost for outer side (0..100)

// Speed cycling for gamepad
const int NUM_SPEED_MODES = 4;
const float SPEED_MODES[NUM_SPEED_MODES] = {0.25f, 0.50f, 0.75f, 1.0f}; // 25%, 50%, 75%, 100%
int currentSpeedMode = 3;                                               // start at 100%

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

  setupAP(onControl, onGamepadControl);
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
 * onControl: action = "forward"/"backward"/"stop"/"start" or empty; sliderValue = -100..100
 */
void onControl(const String &action, int sliderValue)
{
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

void printHelp()
{
  Serial.println(F("Commands:"));
  Serial.println(F("  w → forward 3s"));
  Serial.println(F("  s → backward 3s"));
  Serial.println(F("  x → stop & disable"));
  Serial.println(F("  p → enable motors"));
}

/**
 * onGamepadControl: Gamepad callback function
 * @param action - Button actions: "toggle_enable", "cycle_speed", or empty
 * @param forwardSpeed - Vertical axis value -100..100 (negative=backward, positive=forward)
 * @param steering - Horizontal axis value -100..100 (negative=left, positive=right)
 */
void onGamepadControl(const String &action, int forwardSpeed, int steering)
{
  static int lastForwardSpeed = 0;
  static int lastSteering = 0;
  static bool wasInDeadzone = true;

  // Handle button actions
  if (action == "toggle_enable")
  {
    if (currentState == STATE_IDLE)
    {
      enableMotors();
      currentState = STATE_ENABLED;
      Serial.println(F("🎮 Motors ENABLED via gamepad"));
    }
    else
    {
      disableMotors();
      currentState = STATE_IDLE;
      Serial.println(F("🎮 Motors DISABLED via gamepad"));
    }
    return; // Don't process stick input when toggling
  }
  else if (action == "cycle_speed")
  {
    currentSpeedMode = (currentSpeedMode + 1) % NUM_SPEED_MODES;
    Serial.print(F("🎮 Speed mode changed: "));
    Serial.print((int)(SPEED_MODES[currentSpeedMode] * 100));
    Serial.println(F("%"));
    return; // Don't process stick input when cycling speed
  }

  // Only process stick input if motors are enabled
  if (currentState == STATE_IDLE)
  {
    return;
  }

  // Update steering (turning direction)
  steering = constrain(steering, -100, 100);
  bool steeringChanged = (turnDir != steering);
  if (steeringChanged)
  {
    turnDir = steering;
    Serial.print(F("🎮 Steering: "));
    Serial.print(steering);
    if (steering < -10)
      Serial.println(F(" (LEFT)"));
    else if (steering > 10)
      Serial.println(F(" (RIGHT)"));
    else
      Serial.println(F(" (CENTER)"));
  }

  // Process forward/backward speed with deadzone
  forwardSpeed = constrain(forwardSpeed, -100, 100);
  const int DEADZONE = 5; // ignore small stick movements
  bool inDeadzone = (abs(forwardSpeed) < DEADZONE);

  if (inDeadzone)
  {
    // Stick centered - stop movement but stay enabled
    if (currentState != STATE_ENABLED)
    {
      setRobotSpeed(0);
      currentState = STATE_ENABLED;
      if (!wasInDeadzone)
      {
        Serial.println(F("🎮 STOPPED (deadzone)"));
        wasInDeadzone = true;
      }
    }
  }
  else
  {
    // Moving forward or backward
    bool isForward = (forwardSpeed > 0);
    bool directionChanged = false;

    // Check for direction change
    if (currentState == STATE_MOVING_BACKWARD && isForward)
    {
      directionChanged = true;
      Serial.println(F("🎮 Direction: FORWARD"));
    }
    else if (currentState == STATE_MOVING_FORWARD && !isForward)
    {
      directionChanged = true;
      Serial.println(F("🎮 Direction: BACKWARD"));
    }
    else if (wasInDeadzone)
    {
      Serial.print(F("🎮 Moving: "));
      Serial.println(isForward ? F("FORWARD") : F("BACKWARD"));
    }

    setRobotDirection(isForward);

    // Calculate speed based on stick position and current speed mode
    float speedPercent = (float)abs(forwardSpeed) / 100.0f;
    int32_t targetSpeed = (int32_t)(DEFAULT_SPEED * speedPercent * SPEED_MODES[currentSpeedMode]);
    setRobotSpeed(targetSpeed);

    // Log speed changes (only when significant change occurs)
    if (wasInDeadzone || directionChanged || abs(forwardSpeed - lastForwardSpeed) > 15)
    {
      Serial.print(F("🎮 Input: "));
      Serial.print(forwardSpeed);
      Serial.print(F(" | Speed: "));
      Serial.print(targetSpeed);
      Serial.print(F("/"));
      Serial.print(DEFAULT_SPEED);
      Serial.print(F(" ("));
      Serial.print((int)(speedPercent * SPEED_MODES[currentSpeedMode] * 100));
      Serial.println(F("%)"));
    }

    // Update state
    if (currentState != STATE_MOVING_FORWARD && currentState != STATE_MOVING_BACKWARD)
    {
      currentState = isForward ? STATE_MOVING_FORWARD : STATE_MOVING_BACKWARD;
    }
    else if (directionChanged)
    {
      currentState = isForward ? STATE_MOVING_FORWARD : STATE_MOVING_BACKWARD;
    }

    wasInDeadzone = false;
  }

  // Update last values for change detection
  lastForwardSpeed = forwardSpeed;
  lastSteering = steering;
}

#include "bldrouter.h"
#include <esp32-hal-ledc.h>

// —— Pin assignments ——
const uint8_t FR_PINS[4]  = {  4,  5,  6,  7 };  // F/R = HIGH: forward, LOW: reverse
const uint8_t EN_PINS[4]  = { 15, 16, 17, 18 };  // change to your wiring
const uint8_t SV_PINS[4]  = { 11, 12, 13, 14 };  // PWM input (0…range)

// Func prototypes

void setRobotDirection(bool forward);

// —— PWM configuration ——
const uint32_t PWM_FREQ       = 20000;  // 20 kHz
const uint32_t PWM_RESOLUTION = 1023;   // 10-bit (0…1023)

// —— Movement timing ——
enum MovementState {
  STATE_IDLE,             // motors disabled
  STATE_ENABLED,          // motors enabled, speed = 0
  STATE_MOVING_FORWARD,
  STATE_MOVING_BACKWARD
};
MovementState currentState       = STATE_IDLE;
unsigned long  movementStart     = 0;
const unsigned long movementDur  = 3000;  // ms

// Movement Speed
#ifndef TARGET_DUTY_CYCLE_PERCENT
#define TARGET_DUTY_CYCLE_PERCENT 50
#endif
const uint32_t DEFAULT_SPEED = ( PWM_RESOLUTION * TARGET_DUTY_CYCLE_PERCENT ) / 100;
int turnDir = 0;
uint32_t currentSpeed = 0;

// Movement angle
// at half speed the the outside travels twice as far as the inside
const uint32_t SLOW_SIDE_MAX_PERCENT_DIFF = 50;

void setup() {
//  // Configure global PWM settings
//  for (int i = 0; i < 4; i++) {
//    analogWriteResolution(SV_PINS[i],PWM_RESOLUTION);
//    analogWriteFrequency (SV_PINS[i],PWM_FREQ);
//  }
  
  // Initialize controllers: motors disabled, direction = FWD, speed = 0
  for (int i = 0; i < 4; i++) {
    pinMode(EN_PINS[i], INPUT);          // High-Z => disabled
    pinMode(FR_PINS[i], OUTPUT);
    ledcSetup(i, PWM_FREQ, 10); // channel, freq, resolution 
    ledcAttachPin(SV_PINS[i], i); // pin, channel
    analogWrite(SV_PINS[i], 0);          // Zero speed
  }
  currentState = STATE_IDLE;
  setRobotDirection(true);               // Forward??
  Serial.begin(115200);
  Serial.println(F("Commands: w=forward, s=backward, x=stop, p=enable, ?=help"));

  setupAP(onControl);
}

void loop() {
  
  while (Serial.available()) {
    char cmd = Serial.read();
    if (cmd == '\n' || cmd == '\r') continue;

    switch (cmd) {
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

  // non-blocking timeout check
  if ((currentState == STATE_MOVING_FORWARD || currentState == STATE_MOVING_BACKWARD)
      && ((millis() - movementStart) >= movementDur)) {
    setRobotSpeed(0);
    currentState = STATE_ENABLED;
    Serial.println(F("✓ Movement complete"));
  }


  serverBLD.handleClient();
}


void beginMovement(bool forward) {
  if(currentState != STATE_ENABLED){
    Serial.println(F("Motors not enabled"));
    return;
  }
  setRobotDirection(forward);
  setRobotSpeed(PWM_RESOLUTION);
  movementStart   = millis();
  currentState    = forward ? STATE_MOVING_FORWARD : STATE_MOVING_BACKWARD;
}

void enableMotors() {
  for(int i = 0; i< 4; i++){
    pinMode(EN_PINS[i], OUTPUT);
    digitalWrite(EN_PINS[i], LOW);
  }
}

/// Disable a given motor by returning EN pin to high-impedance
void disableMotors() {
  for(int i = 0; i< 4; i++){
    pinMode(EN_PINS[i], INPUT);
    setRobotSpeed(0);
  }
}

/// dir = true → forward, false → reverse
void setRobotDirection(bool dir) {
  int i = 0;
  digitalWrite(FR_PINS[i++], dir ? HIGH : LOW);
  digitalWrite(FR_PINS[i++], dir ? LOW : HIGH);
  digitalWrite(FR_PINS[i++], dir ? HIGH : LOW);
  digitalWrite(FR_PINS[i++], dir ? LOW : HIGH);
}

/// speed: 0 … PWM_RESOLUTION
void setRobotSpeed(uint32_t v_speed) {
//  speed = constrain(speed, 0, DEFAULT_SPEED);
//  currentSpeed = speed;
  currentSpeed = constrain(v_speed, 0, DEFAULT_SPEED);
  if(turnDir == 0){
    for(int i = 0; i< 4; i++){
      analogWrite(SV_PINS[i], currentSpeed);
    }
    return;
  }
  uint32_t fwd_speed = currentSpeed;
  uint32_t bak_speed = currentSpeed;
  // Only runs if there is a turn radius
  if(turnDir > 0){
    // turnDir positive
    bak_speed = currentSpeed * 
      (SLOW_SIDE_MAX_PERCENT_DIFF - ( ( SLOW_SIDE_MAX_PERCENT_DIFF * turnDir ) / 100));
  } else {
    // turnDir negative
    fwd_speed = currentSpeed * 
      (SLOW_SIDE_MAX_PERCENT_DIFF + ( ( SLOW_SIDE_MAX_PERCENT_DIFF * turnDir ) / 100));
  }
  
  int i = 0;
  analogWrite(SV_PINS[i++], fwd_speed);
  analogWrite(SV_PINS[i++], bak_speed);
  analogWrite(SV_PINS[i++], fwd_speed);
  analogWrite(SV_PINS[i++], bak_speed);
}

/**
 * @brief  Example callback: handles incoming control events.
 * @param  action       "forward", "backward", or empty if only slider moved
 * @param  sliderValue  Current slider position (0–100)
 */
void onControl(const String& action, int sliderValue) {
  if(turnDir != sliderValue){
    turnDir = sliderValue;
    setRobotSpeed(currentSpeed);
  }
  if (action == "forward") {
    Serial.println(F("Forward"));
    beginMovement(true);
  }
  else if (action == "backward") {
    Serial.println(F("Back"));
    beginMovement(false);
  }
  else if (action == "stop") {
    disableMotors();
    currentState = STATE_IDLE;
    Serial.println(F("■ Stopped & disabled"));
  }
  else if (action == "start") {
    enableMotors();
    currentState = STATE_ENABLED;
    Serial.println(F("▶ Motors enabled (speed=0)"));
  }
  Serial.print("Slider at: ");
  Serial.println(sliderValue);
}

void printHelp() {
  Serial.println(F("Commands:"));
  Serial.println(F("  w → forward 3s"));
  Serial.println(F("  s → backward 3s"));
  Serial.println(F("  x → stop & disable"));
  Serial.println(F("  p → enable motors"));
}

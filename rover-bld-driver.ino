// —— Pin assignments ——
const uint8_t EN_PINS[4]  = { 15, 16, 17, 18 };  // change to your wiring
const uint8_t FR_PINS[4]  = { 11, 12, 13, 14 };  // F/R = HIGH: forward, LOW: reverse
const uint8_t SV_PINS[4]  = {  4,  5,  6,  7 };  // PWM input (0…range)

// Func prototypes

void setRobotDirection(bool forward);

// —— PWM configuration ——
const uint32_t PWM_FREQ       = 20000;  // 20 kHz
const uint32_t PWM_RESOLUTION = 1023;  // 10-bit (0…1023)

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

void setup() {
  // Configure global PWM settings
  for (int i = 0; i < 4; i++) {
    analogWriteResolution(FR_PINS[i],PWM_RESOLUTION);
    analogWriteFrequency (FR_PINS[i],PWM_FREQ);
  }
  
  // Initialize controllers: motors disabled, direction = FWD, speed = 0
  for (int i = 0; i < 4; i++) {
    pinMode(EN_PINS[i], INPUT);          // High-Z => disabled
    pinMode(FR_PINS[i], OUTPUT);
    pinMode(SV_PINS[i], OUTPUT);
    analogWrite(SV_PINS[i], 0);          // Zero speed
  }
  currentState = STATE_IDLE;
  setRobotDirection(true);               // Forward??
  Serial.begin(115200);
  Serial.println(F("Commands: w=forward, s=backward, x=stop, p=enable, ?=help"));
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
      && (millis() - movementStart >= movementDur)) {
    setRobotSpeed(0);
    currentState = STATE_ENABLED;
    Serial.println(F("✓ Movement complete"));
  }
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

/// Enable a given motor (0–3) by driving EN pin HIGH
void enableMotors() {
  for(int i = 4; i< 4; i++){
    pinMode(EN_PINS[i], OUTPUT);
    digitalWrite(EN_PINS[i], LOW);
  }
}

/// Disable a given motor by returning EN pin to high-impedance
void disableMotors() {
  for(int i = 4; i< 4; i++){
    pinMode(EN_PINS[i], INPUT);
  }
}

/// dir = true → forward, false → reverse
void setRobotDirection(bool dir) {
  int i = 0;
  digitalWrite(FR_PINS[i++], dir ? HIGH : LOW);
  digitalWrite(FR_PINS[i++], dir ? LOW : HIGH);
  digitalWrite(FR_PINS[i++], dir ? LOW : HIGH);
  digitalWrite(FR_PINS[i++], dir ? HIGH : LOW);
}

/// speed: 0 … PWM_RESOLUTION
void setRobotSpeed(uint32_t speed) {
  for(int i = 4; i< 4; i++){
    speed = constrain(speed, 0, PWM_RESOLUTION);
    analogWrite(SV_PINS[i], speed);
  }
}

void printHelp() {
  Serial.println(F("Commands:"));
  Serial.println(F("  w → forward 3s"));
  Serial.println(F("  s → backward 3s"));
  Serial.println(F("  x → stop & disable"));
  Serial.println(F("  p → enable motors"));
}

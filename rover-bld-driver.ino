#include <Arduino.h>
#include "motor_driver.h"
#include "rover_control.h"
#include "bldrouter.h"

// —— Pin assignments ——
const uint8_t FR_PINS[4] = {  4,  5,  6,  7 };  // F/R: HIGH=forward, LOW=reverse
const uint8_t EN_PINS[4] = { 15, 16, 17, 18 };  // Motor enable pins
const uint8_t SV_PINS[4] = { 11, 12, 13, 14 };  // PWM speed output pins (0..255)

// —— PWM configuration ——
const uint32_t PWM_RESOLUTION = 255;
#ifndef TARGET_DUTY_CYCLE_PERCENT
#define TARGET_DUTY_CYCLE_PERCENT 80
#endif
const uint32_t DEFAULT_SPEED = (PWM_RESOLUTION * TARGET_DUTY_CYCLE_PERCENT) / 100;

// —— WiFi configuration ——
const char* WIFI_SSID     = "mars-Wally";
const char* WIFI_PASSWORD = ""; // Open network — set a password (min 8 chars) to restrict access

// —— Optional static IP (uncomment all three and pass to softAPConfig to use) ——
// IPAddress AP_LOCAL_IP(192, 168, 1, 1);
// IPAddress AP_GATEWAY_IP(192, 168, 1, 254);
// IPAddress AP_NETWORK_MASK(255, 255, 255, 0);

void setup() {
  Serial.begin(115200);

  for (int i = 0; i < 4; i++) {
    pinMode(EN_PINS[i], INPUT); // High-Z => disabled
    pinMode(FR_PINS[i], OUTPUT);
    pinMode(SV_PINS[i], OUTPUT);
    analogWrite(SV_PINS[i], 0);
  }

  currentState = STATE_IDLE;
  setRobotDirection(true);
  Serial.println(F("Commands: w=forward, s=backward, x=stop, p=enable, ?=help"));

  setupAP(WIFI_SSID, WIFI_PASSWORD, onControl, onGamepadControl);
}

void loop() {
  // Disable motors if the control client stops sending keepalives
  checkClientKeepalive(disableMotors);

  while (Serial.available()) {
    char cmd = Serial.read();
    if (cmd == '\n' || cmd == '\r') continue;
    switch (cmd) {
      case 'w': Serial.println(F("Forward")); beginMovement(true);  break;
      case 's': Serial.println(F("Back"));    beginMovement(false); break;
      case 'x': disableMotors(); currentState = STATE_IDLE; Serial.println(F("■ Stopped & disabled")); break;
      case 'p': enableMotors();  currentState = STATE_ENABLED; Serial.println(F("▶ Motors enabled (speed=0)")); break;
      default:  printHelp(); break;
    }
  }

  // Auto-stop timeout (manual control only)
  if ((currentState == STATE_MOVING_FORWARD || currentState == STATE_MOVING_BACKWARD)
      && ((millis() - movementStart) >= movementDur)) {
    setRobotSpeed(0);
    currentState = STATE_ENABLED;
    Serial.println(F("✓ Movement complete"));
  }

  handleBLDClients();
}

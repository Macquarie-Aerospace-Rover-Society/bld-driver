#pragma once
#include <WiFi.h>
#include <WebServer.h>
#include "html/manual_control.h"
#include "html/gamepad_control.h"

// Create a web server running on port 80
WebServer serverBLD(80);

// Define the callback signatures
typedef void (*ManualControlCallback)(const String &action, int sliderValue);
typedef void (*GamepadControlCallback)(int speed, int turn);
typedef void (*DisableMotors)();

// ===== KEEPALIVE =====
// millis is an unsigned long which will wrap back to zero after a very long time
// (much much longer than we will ever have to worry about)
unsigned long lastHeartbeat = 0;
const unsigned long KEEPALIVE_TIMEOUT_MS = 500;
bool clientConnected = false; 

/**
 * @brief  Initializes and starts the web server with separate routes for manual and gamepad control.
 * @param  onManualControl  Function pointer called for manual button/slider input.
 * @param  onGamepadControl Function pointer called for gamepad input.
 */
void setupWebServer(ManualControlCallback onControl, GamepadControlCallback onGamepadControl)
{
  // ===== KEEPALIVE ROUTE =====
  serverBLD.on("/keepalive", HTTP_GET, []() {
    lastHeartbeat = millis();
    clientConnected = true;
    serverBLD.send(200, "text/plain", "OK");
  });

  // ===== MANUAL CONTROL ROUTE =====
  serverBLD.on("/", HTTP_GET, [onControl]() {
    // Read query parameters
    String action    = serverBLD.arg("action");
    int sliderValue  = serverBLD.arg("slider").toInt();

    // Invoke user callback if there's any input
    if (action.length() || serverBLD.hasArg("slider")) {
      lastHeartbeat = millis(); // Count an input signal as a heartbeat
      clientConnected = true;
      onControl(action, sliderValue);
    }

    serverBLD.sendHeader("Content-Encoding", "gzip");
    serverBLD.send_P(200, "text/html",
                     (PGM_P)MANUAL_CONTROL_HTML_GZ,
                     MANUAL_CONTROL_HTML_GZ_LEN);
  });

  // ===== GAMEPAD CONTROL ROUTE =====
  serverBLD.on("/gamepad", HTTP_GET, [onGamepadControl]() {
    // Read gamepad input parameters
    int speed = serverBLD.arg("speed").toInt();  // -255 to 255
    int turn = serverBLD.arg("turn").toInt();    // -100 to 100
    
    // Invoke gamepad control callback
    if (serverBLD.hasArg("speed") || serverBLD.hasArg("turn")) {
      lastHeartbeat = millis(); // Update heartbeat on command
      clientConnected = true;
      onGamepadControl(speed, turn);
    }

    serverBLD.sendHeader("Content-Encoding", "gzip");
    serverBLD.send_P(200, "text/html",
                     (PGM_P)GAMEPAD_CONTROL_HTML_GZ,
                     GAMEPAD_CONTROL_HTML_GZ_LEN);
  });

  serverBLD.begin();
}

void setupAP(const char* ssid, const char* password,
             ManualControlCallback onManualControl,
             GamepadControlCallback onGamepadControl) {

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  // Uncomment and configure to use a static IP:
  // WiFi.softAPConfig(AP_LOCAL_IP, AP_GATEWAY_IP, AP_NETWORK_MASK);
  // WiFi.softAP(ssid, password, /*channel*/1, /*hidden*/false, /*maxConn*/4);

  Serial.println("Configuring access point");
  for(int i = 0; WiFi.status() != WL_CONNECTED
        && i < 50; i++){
    Serial.print(".");
    delay(200);
  }

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP \"");
  Serial.print(ssid);
  Serial.print("\" started. IP address: ");
  Serial.println(IP);

  // Initialize the server, passing our callbacks
  setupWebServer(onManualControl, onGamepadControl);
}

/**
 * @brief Checks if the client has disconnected or stopped sending data.
 */
inline void checkClientKeepalive(DisableMotors disableMotors) {
  if (clientConnected && (millis() - lastHeartbeat > KEEPALIVE_TIMEOUT_MS)) {
    Serial.println("Timeout! Stopping rover.");

    // Stop the rover immediately
    disableMotors();

    // Prevent spamming the stop command
    clientConnected = false;
  }
}

inline void handleBLDClients() {
  serverBLD.handleClient();
}
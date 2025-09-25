#include <WiFi.h>
#include <WebServer.h>

// Start ESP as an access point
const char* ssid     = "mars-Wally";
// Need to have at least 8 characters
//const char* password = "marsmarsmars";
const char* password = ""; // Open network

// Create a web server running on port 80
WebServer serverBLD(80);

// Define the callback signature
typedef void (*ControlCallback)(const String& action,
                int sliderValue);

/**
 * @brief  Initializes and starts the web server.
 * @param  onControl  Function pointer that will be called whenever
 *                    a button is pressed or the slider changes.
 */
void setupWebServer(ControlCallback onControl) {
  serverBLD.on("/", HTTP_GET, [onControl]() {
    // Read query parameters
    String action    = serverBLD.arg("action");
    int sliderValue  = serverBLD.arg("slider").toInt();

    // Invoke user callback if there's any input
    if (action.length() || serverBLD.hasArg("slider")) {
      onControl(action, sliderValue);
    }

    // HTML page with two buttons and a slider
    String html = R"rawliteral(
      <!DOCTYPE html>
      <html lang="en">
      <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0" />
        <title>Robot Controller</title>
        <style>
          body { font-family: Arial, sans-serif; text-align: center; margin-top: 50px; }
          button { margin: 5px; padding: 10px 20px; font-size: 16px; }
          input[type=range] { width: 80%; margin-top: 20px; }
        </style>
      </head>
      <body>
        <h1>Control Panel</h1>
        <button onclick="sendAction('forward')">Forward</button>
        <button onclick="sendAction('backward')">Backward</button>
        <br><br>
        <input type="range" id="slider" min="0" max="100" oninput="sendSlider(this.value)" value="50"/>
        <p>Value: <span id="val">50</span></p>
        <button onclick="sendAction('start')">START</button>
        <button onclick="sendAction('stop')">STOP</button>
        <script>
          const valDisplay = document.getElementById('val');
          function sendAction(act) {
            const slider = document.getElementById('slider').value;
            fetch(`/?action=${act}&slider=${slider}`);
          }
          function sendSlider(val) {
            valDisplay.textContent = val;
            fetch(`/?slider=${val}`);
          }
        </script>
      </body>
      </html>)rawliteral";

    serverBLD.send(200, "text/html", html);
  });

  serverBLD.begin();
}

// Manual IP Configuration for Soft AP
IPAddress AP_LOCAL_IP(192, 168, 1, 1);
IPAddress AP_GATEWAY_IP(192, 168, 1, 254);
IPAddress AP_NETWORK_MASK(255, 255, 255, 0);

void setupAP(ControlCallback onControl) {

  WiFi.mode(WIFI_AP);
//  WiFi.softAPConfig(AP_LOCAL_IP,
//      AP_GATEWAY_IP, AP_NETWORK_MASK);
  WiFi.softAP(ssid, password);
//  WiFi.softAP(ssid, password, /*channel*/1,
//      /*hidden*/false, /*maxConn*/4);

  Serial.println("Configuring access point");
  for(int i = 0; WiFi.status() != WL_CONNECTED
        && i < 50; i++){
    Serial.print(".");
    delay(200);
//    return;
  }

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP \"");
  Serial.print(ssid);
  Serial.print("\" started. IP address: ");
  Serial.println(IP);

  // Initialize the server, passing our callback
  setupWebServer(onControl);
}

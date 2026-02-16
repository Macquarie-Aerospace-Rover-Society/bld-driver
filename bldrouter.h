#include <WiFi.h>
#include <WebServer.h>

// Start ESP as an access point
const char *ssid = "mars-Wally";
// Need to have at least 8 characters
// const char* password = "marsmarsmars";
const char *password = ""; // Open network

// Create a web server running on port 80
WebServer serverBLD(80);

// Define the callback signature
typedef void (*ControlCallback)(const String &action,
                                int sliderValue);

// Forward declaration of gamepad handler from main sketch
extern void handleGamepadInput(int axisX, int axisY, bool btn4, bool btn5);

/**
 * @brief  Initializes and starts the web server.
 * @param  onControl  Function pointer that will be called whenever
 *                    a button is pressed or the slider changes.
 */
void setupWebServer(ControlCallback onControl)
{
  // Gamepad input endpoint
  serverBLD.on("/gamepad", HTTP_GET, []()
               {
    int axisX = serverBLD.arg("axisX").toInt();  // -100..100
    int axisY = serverBLD.arg("axisY").toInt();  // -100..100
    bool btn4 = serverBLD.arg("btn4") == "1";
    bool btn5 = serverBLD.arg("btn5") == "1";
    
    handleGamepadInput(axisX, axisY, btn4, btn5);
    
    serverBLD.send(200, "text/plain", "OK"); });

  serverBLD.on("/", HTTP_GET, [onControl]()
               {
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
          body { 
            font-family: Arial, sans-serif; 
            text-align: center; 
            margin: 20px;
            background: #1a1a1a;
            color: #fff;
          }
          .container {
            max-width: 800px;
            margin: 0 auto;
          }
          h1 { color: #4CAF50; }
          h2 { color: #2196F3; margin-top: 30px; }
          button { 
            margin: 5px; 
            padding: 12px 24px; 
            font-size: 16px;
            border: none;
            border-radius: 5px;
            cursor: pointer;
            background: #4CAF50;
            color: white;
            transition: background 0.3s;
          }
          button:hover { background: #45a049; }
          button.stop { background: #f44336; }
          button.stop:hover { background: #da190b; }
          input[type=range] { 
            width: 80%; 
            margin-top: 20px;
            height: 10px;
          }
          .status-panel {
            background: #2a2a2a;
            border-radius: 10px;
            padding: 20px;
            margin: 20px 0;
            border: 2px solid #4CAF50;
          }
          .status-row {
            display: flex;
            justify-content: space-around;
            margin: 10px 0;
            flex-wrap: wrap;
          }
          .status-item {
            background: #3a3a3a;
            padding: 15px;
            border-radius: 5px;
            margin: 5px;
            min-width: 150px;
          }
          .status-label {
            font-size: 12px;
            color: #888;
            text-transform: uppercase;
          }
          .status-value {
            font-size: 24px;
            font-weight: bold;
            color: #4CAF50;
            margin-top: 5px;
          }
          .gamepad-indicator {
            display: inline-block;
            width: 15px;
            height: 15px;
            border-radius: 50%;
            background: #f44336;
            margin-left: 10px;
          }
          .gamepad-indicator.connected {
            background: #4CAF50;
          }
          .visualizer {
            background: #2a2a2a;
            border-radius: 10px;
            padding: 20px;
            margin: 20px 0;
          }
          .joystick-display {
            width: 200px;
            height: 200px;
            background: #1a1a1a;
            border: 2px solid #4CAF50;
            border-radius: 50%;
            position: relative;
            margin: 20px auto;
          }
          .joystick-indicator {
            width: 20px;
            height: 20px;
            background: #2196F3;
            border-radius: 50%;
            position: absolute;
            top: 50%;
            left: 50%;
            transform: translate(-50%, -50%);
            transition: all 0.1s;
          }
          .speed-bar {
            width: 80%;
            height: 30px;
            background: #1a1a1a;
            border: 2px solid #4CAF50;
            border-radius: 5px;
            margin: 10px auto;
            position: relative;
            overflow: hidden;
          }
          .speed-bar-fill {
            height: 100%;
            background: linear-gradient(90deg, #4CAF50, #2196F3);
            width: 0%;
            transition: width 0.2s;
          }
        </style>
      </head>
      <body>
        <div class="container">
          <h1>🤖 Robot Control Panel</h1>
          
          <div class="status-panel">
            <h3>Gamepad Status <span class="gamepad-indicator" id="gpIndicator"></span></h3>
            <div id="gpStatus">No gamepad detected</div>
          </div>

          <h2>Manual Controls</h2>
          <button onclick="sendAction('forward')">⬆ Forward</button>
          <button onclick="sendAction('backward')">⬇ Backward</button>
          <br><br>
          <label for="slider">Turn Control:</label>
          <input type="range" id="slider" min="-100" max="100" value="0" oninput="sendSlider(this.value)"/>
          <p>Turn: <span id="val">0</span></p>
          <button onclick="sendAction('start')">▶ START</button>
          <button class="stop" onclick="sendAction('stop')">⬛ STOP</button>

          <div class="visualizer">
            <h2>Gamepad Visualization</h2>
            <div class="status-row">
              <div class="status-item">
                <div class="status-label">Speed Setting</div>
                <div class="status-value" id="speedSetting">—</div>
              </div>
              <div class="status-item">
                <div class="status-label">Current Speed</div>
                <div class="status-value" id="currentSpeed">0%</div>
              </div>
              <div class="status-item">
                <div class="status-label">Turn Direction</div>
                <div class="status-value" id="turnValue">0</div>
              </div>
            </div>
            
            <h3>Left Stick Position</h3>
            <div class="joystick-display">
              <div class="joystick-indicator" id="stickIndicator"></div>
            </div>
            
            <h3>Speed Output</h3>
            <div class="speed-bar">
              <div class="speed-bar-fill" id="speedBar"></div>
            </div>
          </div>
        </div>

        <script>
          const valDisplay = document.getElementById('val');
          const gpIndicator = document.getElementById('gpIndicator');
          const gpStatus = document.getElementById('gpStatus');
          const stickIndicator = document.getElementById('stickIndicator');
          const speedBar = document.getElementById('speedBar');
          const speedSetting = document.getElementById('speedSetting');
          const currentSpeed = document.getElementById('currentSpeed');
          const turnValue = document.getElementById('turnValue');
          
          let gamepadConnected = false;
          let lastBtn4 = false;
          let lastBtn5 = false;
          const speedSettings = [25, 50, 75, 100];
          let currentSpeedIndex = 2; // Start at 75%
          
          function sendAction(act) {
            const slider = document.getElementById('slider').value;
            fetch(`/?action=${act}&slider=${slider}`);
          }
          
          function sendSlider(val) {
            valDisplay.textContent = val;
            fetch(`/?slider=${val}`);
          }
          
          function updateJoystickDisplay(x, y) {
            // x and y are -1 to 1, convert to pixel offset from center
            const offsetX = x * 90; // 90px max offset (200px/2 - 10px for indicator)
            const offsetY = y * 90;
            stickIndicator.style.transform = `translate(calc(-50% + ${offsetX}px), calc(-50% + ${offsetY}px))`;
          }
          
          function updateSpeedBar(speed) {
            speedBar.style.width = speed + '%';
          }
          
          function pollGamepad() {
            const gamepads = navigator.getGamepads();
            let gp = null;
            
            for (let i = 0; i < gamepads.length; i++) {
              if (gamepads[i]) {
                gp = gamepads[i];
                break;
              }
            }
            
            if (gp) {
              if (!gamepadConnected) {
                gamepadConnected = true;
                gpIndicator.classList.add('connected');
                gpStatus.textContent = `Connected: ${gp.id}`;
              }
              
              // Read axes (axes[0] = left stick X, axes[1] = left stick Y)
              const axisX = gp.axes[0] || 0;
              const axisY = gp.axes[1] || 0;
              
              // Convert to -100..100 range
              const axisXInt = Math.round(axisX * 100);
              const axisYInt = Math.round(-axisY * 100); // Invert Y for intuitive control
              
              // Read buttons (buttons[4] and buttons[5])
              const btn4 = gp.buttons[4] ? gp.buttons[4].pressed : false;
              const btn5 = gp.buttons[5] ? gp.buttons[5].pressed : false;
              
              // Update visualizations
              updateJoystickDisplay(axisX, axisY);
              turnValue.textContent = axisXInt;
              
              // Calculate display speed (accounting for speed setting)
              const speedPct = Math.abs(axisYInt) * (speedSettings[currentSpeedIndex] / 100);
              currentSpeed.textContent = Math.round(speedPct) + '%';
              updateSpeedBar(speedPct);
              speedSetting.textContent = speedSettings[currentSpeedIndex] + '%';
              
              // Handle button 4 (cycle speed) - edge detection
              if (btn4 && !lastBtn4) {
                currentSpeedIndex = (currentSpeedIndex + 1) % speedSettings.length;
                speedSetting.textContent = speedSettings[currentSpeedIndex] + '%';
              }
              lastBtn4 = btn4;
              lastBtn5 = btn5;
              
              // Send to server if there's meaningful input
              if (Math.abs(axisXInt) > 5 || Math.abs(axisYInt) > 5 || btn4 || btn5) {
                fetch(`/gamepad?axisX=${axisXInt}&axisY=${axisYInt}&btn4=${btn4?1:0}&btn5=${btn5?1:0}`);
              }
            } else {
              if (gamepadConnected) {
                gamepadConnected = false;
                gpIndicator.classList.remove('connected');
                gpStatus.textContent = 'No gamepad detected';
                updateJoystickDisplay(0, 0);
                updateSpeedBar(0);
                currentSpeed.textContent = '0%';
                turnValue.textContent = '0';
              }
            }
          }
          
          // Poll gamepad every 50ms
          setInterval(pollGamepad, 50);
          
          // Initialize display
          speedSetting.textContent = speedSettings[currentSpeedIndex] + '%';
        </script>
      </body>
      </html>)rawliteral";

    serverBLD.send(200, "text/html", html); });

  serverBLD.begin();
}

// Manual IP Configuration for Soft AP
IPAddress AP_LOCAL_IP(192, 168, 1, 1);
IPAddress AP_GATEWAY_IP(192, 168, 1, 254);
IPAddress AP_NETWORK_MASK(255, 255, 255, 0);

void setupAP(ControlCallback onControl)
{

  WiFi.mode(WIFI_AP);
  //  WiFi.softAPConfig(AP_LOCAL_IP,
  //      AP_GATEWAY_IP, AP_NETWORK_MASK);
  WiFi.softAP(ssid, password);
  //  WiFi.softAP(ssid, password, /*channel*/1,
  //      /*hidden*/false, /*maxConn*/4);

  Serial.println("Configuring access point");
  for (int i = 0; WiFi.status() != WL_CONNECTED && i < 50; i++)
  {
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

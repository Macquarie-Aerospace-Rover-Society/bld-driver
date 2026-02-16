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

// Calback signature for gamepad
// String for button presses / command choice
// forwardSpeed / steering for controller throttle / steering
typedef void (*ControlGamepadCallback)(const String &action,
                                       int forwardSpeed, int steering);

/**
 * @brief  Initializes and starts the web server.
 * @param  onControl  Function pointer that will be called whenever
 *                    a button is pressed or the slider changes.
 */
void setupWebServer(ControlCallback onControl, ControlGamepadCallback gamePadControl)
{
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
        <input type="range" id="slider" min="-100" max="100" oninput="sendSlider(this.value)" value="50"/>
        <p>Value: <span id="val">0</span></p>
        <button onclick="sendAction('start')">START</button>
        <button onclick="sendAction('stop')">STOP</button>
        <br><br>
        <a href="/gamepad" style="display: inline-block; margin-top: 20px; padding: 12px 24px; background: #667eea; color: white; text-decoration: none; border-radius: 8px; font-weight: bold;">🎮 Gamepad Controller</a>
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

    serverBLD.send(200, "text/html", html); });

  // Gamepad route - dedicated gamepad control page
  serverBLD.on("/gamepad", HTTP_GET, [gamePadControl]()
               {
    // Check if this is an API call (has query parameters)
    if (serverBLD.hasArg("forward") || serverBLD.hasArg("steering") || serverBLD.hasArg("action")) {
      String action = serverBLD.arg("action");
      int forwardSpeed = serverBLD.arg("forward").toInt();
      int steering = serverBLD.arg("steering").toInt();
      
      // Invoke gamepad callback with received values
      gamePadControl(action, forwardSpeed, steering);
      
      // Send quick response to avoid timeout
      serverBLD.send(200, "text/plain", "OK");
      return;
    }
    
    // Serve the gamepad control HTML page
    String html = R"rawliteral(
      <!DOCTYPE html>
      <html lang="en">
      <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0" />
        <title>Gamepad Controller</title>
        <style>
          * { margin: 0; padding: 0; box-sizing: border-box; }
          body { 
            font-family: 'Segoe UI', Arial, sans-serif; 
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            display: flex;
            justify-content: center;
            align-items: center;
            padding: 20px;
          }
          .container {
            background: white;
            border-radius: 20px;
            padding: 30px;
            box-shadow: 0 20px 60px rgba(0,0,0,0.3);
            max-width: 600px;
            width: 100%;
          }
          h1 { 
            text-align: center; 
            color: #333; 
            margin-bottom: 10px;
            font-size: 28px;
          }
          .status {
            text-align: center;
            padding: 10px;
            border-radius: 8px;
            margin: 15px 0;
            font-weight: 600;
            font-size: 14px;
          }
          .status.connected { background: #d4edda; color: #155724; }
          .status.disconnected { background: #f8d7da; color: #721c24; }
          
          .viz-grid {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 20px;
            margin: 25px 0;
          }
          .viz-section {
            background: #f8f9fa;
            border-radius: 12px;
            padding: 15px;
          }
          .viz-section h3 {
            font-size: 14px;
            color: #666;
            margin-bottom: 15px;
            text-transform: uppercase;
            letter-spacing: 0.5px;
          }
          .meter {
            position: relative;
            height: 200px;
            background: #e9ecef;
            border-radius: 8px;
            overflow: hidden;
            margin-bottom: 10px;
          }
          .meter-fill {
            position: absolute;
            bottom: 0;
            width: 100%;
            background: linear-gradient(180deg, #667eea 0%, #764ba2 100%);
            transition: height 0.1s ease;
            border-radius: 8px 8px 0 0;
          }
          .meter-center {
            position: absolute;
            top: 50%;
            left: 0;
            right: 0;
            height: 2px;
            background: #dc3545;
            transform: translateY(-50%);
          }
          .meter-value {
            position: absolute;
            top: 10px;
            left: 0;
            right: 0;
            text-align: center;
            font-weight: 700;
            font-size: 24px;
            color: #333;
            z-index: 10;
          }
          .meter-label {
            text-align: center;
            font-size: 12px;
            color: #666;
            font-weight: 600;
          }
          
          .info-grid {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 10px;
            margin-top: 20px;
          }
          .info-box {
            background: #e9ecef;
            padding: 12px;
            border-radius: 8px;
            text-align: center;
          }
          .info-box label {
            display: block;
            font-size: 11px;
            color: #666;
            text-transform: uppercase;
            margin-bottom: 5px;
          }
          .info-box value {
            display: block;
            font-size: 18px;
            font-weight: 700;
            color: #333;
          }
          
          .help {
            background: #fff3cd;
            border-left: 4px solid #ffc107;
            padding: 12px;
            margin-top: 20px;
            border-radius: 4px;
            font-size: 12px;
            line-height: 1.6;
          }
          .help strong { color: #856404; }
        </style>
      </head>
      <body>
        <div class="container">
          <h1>🎮 Gamepad Control</h1>
          <a href="/" style="display: inline-block; margin-bottom: 15px; padding: 8px 16px; background: #6c757d; color: white; text-decoration: none; border-radius: 6px; font-size: 14px;">← Back to Controller</a>
          <div id="status" class="status disconnected">⚠ No gamepad detected</div>
          
          <div class="viz-grid">
            <div class="viz-section">
              <h3>📊 Input</h3>
              <div class="meter">
                <div class="meter-center"></div>
                <div id="inputBar" class="meter-fill"></div>
                <div class="meter-value" id="inputVal">0</div>
              </div>
              <div class="meter-label">Forward/Back</div>
              
              <div style="margin-top: 15px;">
                <div class="meter">
                  <div class="meter-center"></div>
                  <div id="steerBar" class="meter-fill"></div>
                  <div class="meter-value" id="steerVal">0</div>
                </div>
                <div class="meter-label">Steering</div>
              </div>
            </div>
            
            <div class="viz-section">
              <h3>📤 Output</h3>
              <div class="info-grid">
                <div class="info-box">
                  <label>Motor State</label>
                  <value id="motorState">IDLE</value>
                </div>
                <div class="info-box">
                  <label>Speed Mode</label>
                  <value id="speedMode">100%</value>
                </div>
                <div class="info-box">
                  <label>Direction</label>
                  <value id="direction">-</value>
                </div>
                <div class="info-box">
                  <label>Turn</label>
                  <value id="turn">0</value>
                </div>
              </div>
              
              <div class="info-box" style="margin-top: 10px;">
                <label>Effective Speed</label>
                <value id="effSpeed">0%</value>
              </div>
            </div>
          </div>
          
          <div class="help">
            <strong>📋 Controls:</strong><br>
            • <strong>Left Stick</strong> - Forward/Back (Y-axis) + Steering (X-axis)<br>
            • <strong>LB (Button 4)</strong> - Cycle Speed: 25% → 50% → 75% → 100%<br>
            • <strong>RB (Button 5)</strong> - Toggle Motor Enable/Disable<br>
            <br>
            <strong>💡 Tips:</strong><br>
            • Dead zone threshold: ±5 (prevents drift)<br>
            • Update rate: 10 updates/sec to ESP32<br>
            • Visualization updates: ~60 FPS
          </div>
        </div>
        
        <script>
          // DOM element references
          const status = document.getElementById('status');
          const inputVal = document.getElementById('inputVal');
          const steerVal = document.getElementById('steerVal');
          const inputBar = document.getElementById('inputBar');
          const steerBar = document.getElementById('steerBar');
          const motorState = document.getElementById('motorState');
          const speedMode = document.getElementById('speedMode');
          const direction = document.getElementById('direction');
          const turn = document.getElementById('turn');
          const effSpeed = document.getElementById('effSpeed');
          
          // State variables
          let gamepadConnected = false;
          let lastButtonState = {};
          let currentSpeedModeIdx = 3; // Start at 100%
          const speedModes = [25, 50, 75, 100];
          let motorEnabled = false;
          
          // Gamepad event listeners
          window.addEventListener('gamepadconnected', (e) => {
            console.log('Gamepad connected:', e.gamepad.id);
            gamepadConnected = true;
            status.className = 'status connected';
            status.textContent = '✓ Gamepad: ' + e.gamepad.id;
          });
          
          window.addEventListener('gamepaddisconnected', (e) => {
            console.log('Gamepad disconnected');
            gamepadConnected = false;
            status.className = 'status disconnected';
            status.textContent = '⚠ No gamepad detected';
            // Reset displays to zero
            updateMeter(inputBar, inputVal, 0);
            updateMeter(steerBar, steerVal, 0);
            direction.textContent = '-';
            effSpeed.textContent = '0%';
            turn.textContent = '0';
          });
          
          // Update visual meter display
          function updateMeter(barEl, valEl, value) {
            valEl.textContent = value;
            // Convert -100..100 range to 0..100% height
            const height = ((value + 100) / 2);
            barEl.style.height = height + '%';
          }
          
          // Poll gamepad state and update UI
          function pollGamepad() {
            const gamepads = navigator.getGamepads();
            if (!gamepads) return;
            
            const gamepad = gamepads[0];
            if (!gamepad) return;
            
            // Read left stick axes (axes[0] = horizontal, axes[1] = vertical)
            const steering = Math.round(gamepad.axes[0] * 100);
            const forward = Math.round(-gamepad.axes[1] * 100); // Invert Y axis
            
            // Update input visualization
            updateMeter(inputBar, inputVal, forward);
            updateMeter(steerBar, steerVal, steering);
            
            // Update output information
            turn.textContent = steering;
            
            const absForward = Math.abs(forward);
            if (absForward > 5) { // Dead zone threshold
              direction.textContent = forward > 0 ? 'FWD' : 'BCK';
              const effSpeedVal = Math.round((absForward / 100) * speedModes[currentSpeedModeIdx]);
              effSpeed.textContent = effSpeedVal + '%';
            } else {
              direction.textContent = '-';
              effSpeed.textContent = '0%';
            }
            
            // Send periodic data to server
            sendGamepadData('', forward, steering);
            
            // Check button presses (with debouncing)
            // LB (Button 4) - Cycle speed mode
            if (gamepad.buttons[4] && gamepad.buttons[4].pressed && !lastButtonState[4]) {
              currentSpeedModeIdx = (currentSpeedModeIdx + 1) % 4;
              speedMode.textContent = speedModes[currentSpeedModeIdx] + '%';
              sendGamepadData('cycle_speed', forward, steering);
              console.log('Speed mode changed to:', speedModes[currentSpeedModeIdx] + '%');
            }
            
            // RB (Button 5) - Toggle motor enable
            if (gamepad.buttons[5] && gamepad.buttons[5].pressed && !lastButtonState[5]) {
              motorEnabled = !motorEnabled;
              motorState.textContent = motorEnabled ? 'ENABLED' : 'IDLE';
              motorState.style.color = motorEnabled ? '#28a745' : '#6c757d';
              sendGamepadData('toggle_enable', forward, steering);
              console.log('Motor enabled:', motorEnabled);
            }
            
            // Update button states for next cycle
            lastButtonState[4] = gamepad.buttons[4] ? gamepad.buttons[4].pressed : false;
            lastButtonState[5] = gamepad.buttons[5] ? gamepad.buttons[5].pressed : false;
          }
          
          // Network communication
          let lastSendTime = 0;
          const SEND_INTERVAL = 100; // ms between updates
          
          function sendGamepadData(action, forward, steering) {
            const now = Date.now();
            // Throttle regular updates, but always send button actions
            if (action === '' && now - lastSendTime < SEND_INTERVAL) {
              return;
            }
            lastSendTime = now;
            
            // Send data to ESP32
            fetch('/gamepad?action=' + action + '&forward=' + forward + '&steering=' + steering)
              .catch(err => console.error('Network error:', err));
          }
          
          // Start polling at ~60fps
          setInterval(pollGamepad, 16);
          
          // Initial gamepad check
          setTimeout(() => {
            const gamepads = navigator.getGamepads();
            if (gamepads && gamepads[0]) {
              status.className = 'status connected';
              status.textContent = '✓ Gamepad: ' + gamepads[0].id;
              gamepadConnected = true;
            }
          }, 100);
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

void setupAP(ControlCallback onControl, ControlGamepadCallback gamePadControl)
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

  // Initialize the server, passing our callbacks
  setupWebServer(onControl, gamePadControl);
}

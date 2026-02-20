#include <WiFi.h>
#include <WebServer.h>

// Start ESP as an access point
const char *ssid = "mars-Wally";
// Need to have at least 8 characters
// const char* password = "marsmarsmars";
const char *password = ""; // Open network

// Create a web server running on port 80
WebServer serverBLD(80);

// Define the callback signatures
typedef void (*ManualControlCallback)(const String &action, int sliderValue);
typedef void (*GamepadControlCallback)(int speed, int turn);

/**
 * @brief  Initializes and starts the web server with separate routes for manual and gamepad control.
 * @param  onManualControl  Function pointer called for manual button/slider input.
 * @param  onGamepadControl Function pointer called for gamepad input.
 */
void setupWebServer(ManualControlCallback onManualControl, GamepadControlCallback onGamepadControl)
{
  // ===== MANUAL CONTROL ROUTE =====
  serverBLD.on("/", HTTP_GET, [onManualControl]()
               {
    // Read query parameters for manual control
    String action = serverBLD.arg("action");
    int sliderValue = serverBLD.arg("slider").toInt();
    
    // Invoke manual control callback if there's any input
    if (action.length() || serverBLD.hasArg("slider")) {
      onManualControl(action, sliderValue);
    }

    // Manual control HTML page
    String html = R"rawliteral(
      <!DOCTYPE html>
      <html lang="en">
      <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0" />
        <title>Rover Manual Controller</title>
         <style>
          body { 
            font-family: Arial, sans-serif; 
            text-align: center; 
            margin-top: 20px; 
            background: #1a1a1a; 
            color: #fff; 
          }
          button { 
            margin: 5px; 
            padding: 15px 30px; 
            font-size: 18px; 
            background: #007bff; 
            color: white; 
            border: none; 
            border-radius: 5px; 
            cursor: pointer; 
            min-width: 120px;
          }
          button:hover { background: #0056b3; }
          button:active { background: #004085; }
          button.stop { background: #dc3545; }
          button.stop:hover { background: #c82333; }
          button.start { background: #28a745; }
          button.start:hover { background: #218838; }
          input[type=range] { 
            width: 80%; 
            max-width: 400px;
            margin: 20px auto; 
            height: 40px;
          }
          .panel { 
            display: inline-block; 
            margin: 20px; 
            padding: 30px; 
            background: #2a2a2a; 
            border-radius: 10px; 
            min-width: 400px;
          }
          .turn-display {
            font-size: 24px;
            margin: 15px 0;
            padding: 10px;
            background: #333;
            border-radius: 5px;
            display: inline-block;
            min-width: 150px;
          }
          .nav-link { 
            display: inline-block; 
            margin: 10px; 
            color: #007bff; 
            text-decoration: none; 
            font-size: 16px;
          }
          .nav-link:hover { text-decoration: underline; }
          .instructions {
            background: #333;
            padding: 15px;
            border-radius: 5px;
            margin-top: 20px;
            text-align: left;
          }
          .instructions h3 { margin-top: 0; }
          .instructions ul { margin: 10px 0; }
        </style>
      </head>
      <body>
        <h1>🚗 Rover Manual Controller</h1>
        <a href="/gamepad" class="nav-link">Switch to Gamepad Control →</a>
        
        <div class="panel">
          <h2>Movement Controls</h2>
          <div style="margin: 20px 0;">
            <button onclick="sendAction('forward')">▲ Forward</button>
          </div>
          <div style="margin: 20px 0;">
            <button onclick="sendAction('backward')">▼ Backward</button>
          </div>
          
          <h3 style="margin-top: 40px;">Turn Direction</h3>
          <input type="range" id="slider" min="-100" max="100" oninput="sendSlider(this.value)" value="0"/>
          <div class="turn-display">
            <span id="direction">◀ </span>
            <span id="val">0</span>
            <span id="direction-r"> ▶</span>
          </div>
          
          <div style="margin-top: 40px;">
            <button class="start" onclick="sendAction('start')">▶ START / ENABLE</button>
            <button class="stop" onclick="sendAction('stop')">■ STOP / DISABLE</button>
          </div>

          <div class="instructions">
            <h3>Instructions:</h3>
            <ul>
              <li><strong>START/ENABLE:</strong> Powers up motors (speed = 0)</li>
              <li><strong>FORWARD/BACKWARD:</strong> Moves rover for 3 seconds</li>
              <li><strong>Turn Slider:</strong> Adjusts turn direction (-100 = left, 0 = straight, +100 = right)</li>
              <li><strong>STOP/DISABLE:</strong> Immediately stops and disables motors</li>
            </ul>
          </div>
        </div>

        <script>
          const valDisplay = document.getElementById('val');
          const dirLeft = document.getElementById('direction');
          const dirRight = document.getElementById('direction-r');

          function sendAction(act) {
            const slider = document.getElementById('slider').value;
            fetch(`/?action=${act}&slider=${slider}`)
              .then(() => console.log('Action sent:', act))
              .catch(e => console.error('Error:', e));
          }
          
          function sendSlider(val) {
            valDisplay.textContent = val;
            
            // Update visual direction indicators
            const absVal = Math.abs(val);
            if (val < -10) {
              dirLeft.textContent = '◀◀ ';
              dirRight.textContent = '';
            } else if (val > 10) {
              dirLeft.textContent = '';
              dirRight.textContent = ' ▶▶';
            } else {
              dirLeft.textContent = '◀ ';
              dirRight.textContent = ' ▶';
            }
            
            fetch(`/?slider=${val}`)
              .catch(e => console.error('Error:', e));
          }

          // Initialize display
          sendSlider(0);
        </script>
      </body>
      </html>)rawliteral";

    serverBLD.send(200, "text/html", html); });

  // ===== GAMEPAD CONTROL ROUTE =====
  serverBLD.on("/gamepad", HTTP_GET, [onGamepadControl]()
               {
    // Read gamepad input parameters
    int speed = serverBLD.arg("speed").toInt();  // -255 to 255
    int turn = serverBLD.arg("turn").toInt();    // -100 to 100
    
    // Invoke gamepad control callback
    if (serverBLD.hasArg("speed") || serverBLD.hasArg("turn")) {
      onGamepadControl(speed, turn);
    }

    // Serve gamepad-specific HTML page
    String html = R"rawliteral(
      <!DOCTYPE html>
      <html lang="en">
      <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0" />
        <title>Rover Gamepad Controller</title>
        <style>
          body { font-family: Arial, sans-serif; text-align: center; margin-top: 20px; background: #1a1a1a; color: #fff; }
          .panel { display: inline-block; margin: 20px; padding: 20px; background: #2a2a2a; border-radius: 10px; }
          .status { font-size: 18px; margin: 10px; }
          .connected { color: #28a745; }
          .disconnected { color: #dc3545; }
          .visualizer { width: 300px; height: 300px; border: 2px solid #444; border-radius: 10px; margin: 20px auto; position: relative; background: #333; }
          .stick { width: 30px; height: 30px; background: #007bff; border-radius: 50%; position: absolute; transform: translate(-50%, -50%); }
          .output-bar { width: 80%; height: 30px; background: #444; margin: 10px auto; position: relative; border-radius: 5px; overflow: hidden; }
          .output-fill { height: 100%; background: linear-gradient(90deg, #28a745, #ffc107, #dc3545); transition: width 0.1s; }
          .info { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; max-width: 600px; margin: 0 auto; text-align: left; }
          .info-item { background: #333; padding: 10px; border-radius: 5px; }
          button { margin: 5px; padding: 10px 20px; font-size: 16px; background: #007bff; color: white; border: none; border-radius: 5px; cursor: pointer; }
          button:hover { background: #0056b3; }
          .nav-link { display: inline-block; margin: 10px; color: #007bff; text-decoration: none; }
          .nav-link:hover { text-decoration: underline; }
        </style>
      </head>
      <body>
        <h1>Rover Gamepad Controller</h1>
        <a href="/" class="nav-link">Switch to Manual Control</a>
        
        <div class="panel">
          <h2>Gamepad Status</h2>
          <div class="status" id="gamepad-status">
            <span class="disconnected">⬤ Not Connected</span>
          </div>
          <p>Speed Mode: <span id="speed-mode">Normal (80%)</span></p>
          <p>Rover State: <span id="rover-state">Disabled</span></p>
          
          <h3>Input Visualization</h3>
          <div class="visualizer">
            <div class="stick" id="stick" style="left: 150px; top: 150px;"></div>
          </div>
          
          <div class="info">
            <div class="info-item">RT (Forward): <span id="btn-7">0.00</span></div>
            <div class="info-item">LT (Backward): <span id="btn-6">0.00</span></div>
            <div class="info-item">Left Stick X (Turn): <span id="axis-0">0.00</span></div>
            <div class="info-item">Speed: <span id="speed-val">0</span></div>
            <div class="info-item">Turn: <span id="turn-val">0</span></div>
          </div>

          <h3>Motor Output</h3>
          <div style="text-align: left; max-width: 400px; margin: 0 auto;">
            <p>Left Motors: <span id="left-speed">0</span></p>
            <div class="output-bar"><div class="output-fill" id="left-bar" style="width: 0%;"></div></div>
            <p>Right Motors: <span id="right-speed">0</span></p>
            <div class="output-bar"><div class="output-fill" id="right-bar" style="width: 0%;"></div></div>
          </div>

          <div style="margin-top: 20px;">
            <p><strong>Controls:</strong></p>
            <p>LB (Button 4) - Cycle Speed Mode</p>
            <p>RB (Button 5) - Enable/Disable Rover</p>
            <p>RT (Right Trigger) - Forward</p>
            <p>LT (Left Trigger) - Backward</p>
            <p>Left Stick X-Axis - Turn</p>
          </div>
        </div>

        <script>
          let currentGamepad = null;
          let speedMode = 1; // 0=Slow, 1=Normal, 2=Fast
          const speedModes = ['Slow (50%)', 'Normal (80%)', 'Fast (100%)'];
          const speedMultipliers = [0.5, 0.8, 1.0];
          let roverEnabled = false;
          let lastButton4 = false;
          let lastButton5 = false;
          const DEADZONE = 0.15;
          const TURN_DEADZONE = 0.1;
          const UPDATE_INTERVAL = 50; // ms

          function sendGamepadData(speed, turn) {
            fetch(`/gamepad?speed=${speed}&turn=${turn}`).catch(e => console.error(e));
          }

          function updateVisualizer(x, y) {
            const stick = document.getElementById('stick');
            const posX = (x + 1) * 150;
            const posY = (y + 1) * 150;
            stick.style.left = posX + 'px';
            stick.style.top = posY + 'px';
          }

          function updateOutputBars(speed, turn) {
            // Calculate differential drive motor speeds
            let leftSpeed = Math.abs(speed);
            let rightSpeed = Math.abs(speed);
            
            if (turn > 0.01) { // Turning right
              rightSpeed = Math.max(0, Math.abs(speed) * (1 - Math.abs(turn) * 0.9));
            } else if (turn < -0.01) { // Turning left
              leftSpeed = Math.max(0, Math.abs(speed) * (1 - Math.abs(turn) * 0.9));
            }
            
            document.getElementById('left-speed').textContent = Math.round(leftSpeed);
            document.getElementById('right-speed').textContent = Math.round(rightSpeed);
            document.getElementById('left-bar').style.width = (leftSpeed / 255 * 100) + '%';
            document.getElementById('right-bar').style.width = (rightSpeed / 255 * 100) + '%';
          }

          function pollGamepad() {
            const gamepads = navigator.getGamepads();
            currentGamepad = null;
            
            for (let i = 0; i < gamepads.length; i++) {
              if (gamepads[i]) {
                currentGamepad = gamepads[i];
                break;
              }
            }

            if (currentGamepad) {
              document.getElementById('gamepad-status').innerHTML = '<span class="connected">⬤ Connected: ' + currentGamepad.id + '</span>';
              
              // Read triggers and axis
              const button7Value = (currentGamepad.buttons[7] && currentGamepad.buttons[7].value) || 0; // RT (Forward)
              const button6Value = (currentGamepad.buttons[6] && currentGamepad.buttons[6].value) || 0; // LT (Backward)
              const axisTurn = currentGamepad.axes[0] || 0; // Left stick X-axis
              
              // Display raw values
              document.getElementById('btn-7').textContent = button7Value.toFixed(2);
              document.getElementById('btn-6').textContent = button6Value.toFixed(2);
              document.getElementById('axis-0').textContent = axisTurn.toFixed(2);
              
              // Update visualizer
              const visualY = button6Value - button7Value;
              updateVisualizer(axisTurn, visualY);
              
              // Button 4 - cycle speed mode
              const button4 = currentGamepad.buttons[4] && currentGamepad.buttons[4].pressed;
              if (button4 && !lastButton4) {
                speedMode = (speedMode + 1) % 3;
                document.getElementById('speed-mode').textContent = speedModes[speedMode];
              }
              lastButton4 = button4;
              
              // Button 5 - enable/disable rover dead-man switch (enable only while held)
              const button5 = currentGamepad.buttons[5] && currentGamepad.buttons[5].pressed;
              if (button5) {
                if (!roverEnabled) {
                  roverEnabled = true;
                  document.getElementById('rover-state').textContent = 'Enabled';
                }
              } else {
                if (roverEnabled) {
                  roverEnabled = false;
                  document.getElementById('rover-state').textContent = 'Disabled';
                  sendGamepadData(0, 0); // Stop rover immediately when Button 5 is released
                }
              }
              lastButton5 = button5;
              
              // Apply deadzones
              const processedTurn = Math.abs(axisTurn) < TURN_DEADZONE ? 0 : axisTurn;
              
              // Calculate speed from triggers
              let netSpeed = 0;
              if (button7Value > DEADZONE) {
                netSpeed = button7Value * 255 * speedMultipliers[speedMode];
              } else if (button6Value > DEADZONE) {
                netSpeed = -button6Value * 255 * speedMultipliers[speedMode];
              }
              
              const speed = Math.round(netSpeed);
              const turn = Math.round(processedTurn * 100);
              
              document.getElementById('speed-val').textContent = speed;
              document.getElementById('turn-val').textContent = turn;
              
              // Update output visualization
              updateOutputBars(Math.abs(speed), turn / 100);
              
              // Send to rover if enabled
              if (roverEnabled) {
                sendGamepadData(speed, turn);
              }
            } else {
              document.getElementById('gamepad-status').innerHTML = '<span class="disconnected">⬤ Not Connected</span>';
              updateVisualizer(0, 0);
              document.getElementById('btn-7').textContent = '0.00';
              document.getElementById('btn-6').textContent = '0.00';
              document.getElementById('axis-0').textContent = '0.00';
              document.getElementById('speed-val').textContent = '0';
              document.getElementById('turn-val').textContent = '0';
              updateOutputBars(0, 0);
            }
            
            setTimeout(pollGamepad, UPDATE_INTERVAL);
          }

          // Event listeners
          window.addEventListener('gamepadconnected', (e) => {
            console.log('Gamepad connected:', e.gamepad);
          });
          
          window.addEventListener('gamepaddisconnected', (e) => {
            console.log('Gamepad disconnected');
            roverEnabled = false;
            document.getElementById('rover-state').textContent = 'Disabled';
            sendGamepadData(0, 0);
          });
          
          // Start polling
          pollGamepad();
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

void setupAP(ManualControlCallback onManualControl, GamepadControlCallback onGamepadControl)
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
  setupWebServer(onManualControl, onGamepadControl);
}

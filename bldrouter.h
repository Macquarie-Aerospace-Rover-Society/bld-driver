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

/**
 * @brief  Initializes and starts the web server.
 * @param  onControl  Function pointer that will be called whenever
 *                    a button is pressed or the slider changes.
 */
void setupWebServer(ControlCallback onControl)
{
  serverBLD.on("/", HTTP_GET, [onControl]()
               {
    // Read query parameters
    String action    = serverBLD.arg("action");
    int sliderValue  = serverBLD.arg("slider").toInt();
    
    // Check for gamepad input
    if (serverBLD.hasArg("gamepad") && serverBLD.arg("gamepad") == "1") {
      int speed = serverBLD.arg("speed").toInt();  // -255 to 255
      int turn = serverBLD.arg("turn").toInt();    // -100 to 100
      
      // Create action string with gamepad data
      String gamepadAction = "gamepad:" + String(speed) + ":" + String(turn);
      onControl(gamepadAction, turn);
    }
    // Invoke user callback if there's any input
    else if (action.length() || serverBLD.hasArg("slider")) {
      onControl(action, sliderValue);
    }

    // HTML page with gamepad support
    String html = R"rawliteral(
      <!DOCTYPE html>
      <html lang="en">
      <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0" />
        <title>Robot Controller</title>
        <style>
          body { font-family: Arial, sans-serif; text-align: center; margin-top: 20px; background: #1a1a1a; color: #fff; }
          button { margin: 5px; padding: 10px 20px; font-size: 16px; background: #007bff; color: white; border: none; border-radius: 5px; cursor: pointer; }
          button:hover { background: #0056b3; }
          input[type=range] { width: 80%; margin-top: 20px; }
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
        </style>
      </head>
      <body>
        <h1>Rover Controller</h1>
        
        <div class="panel">
          <h2>Manual Controls</h2>
          <button onclick="sendAction('forward')">Forward</button>
          <button onclick="sendAction('backward')">Backward</button>
          <br><br>
          <input type="range" id="slider" min="-100" max="100" oninput="sendSlider(this.value)" value="0"/>
          <p>Turn: <span id="val">0</span></p>
          <button onclick="sendAction('start')">START</button>
          <button onclick="sendAction('stop')">STOP</button>
        </div>

        <div class="panel">
          <h2>Gamepad</h2>
          <div class="status" id="gamepad-status">
            <span class="disconnected">⬤ Not Connected</span>
          </div>
          <p>Speed Mode: <span id="speed-mode">Normal</span></p>
          <p>Rover State: <span id="rover-state">Disabled</span></p>
          
          <h3>Input Visualization</h3>
          <div class="visualizer">
            <div class="stick" id="stick" style="left: 150px; top: 150px;"></div>
          </div>
          
          <div class="info">
            <div class="info-item">Axis[0][0]: <span id="axis-0">0.00</span></div>
            <div class="info-item">Axis[0][1]: <span id="axis-1">0.00</span></div>
            <div class="info-item">Speed: <span id="speed-val">0</span></div>
            <div class="info-item">Turn: <span id="turn-val">0</span></div>
          </div>

          <h3>Output</h3>
          <div style="text-align: left; max-width: 400px; margin: 0 auto;">
            <p>Left Motors: <span id="left-speed">0</span></p>
            <div class="output-bar"><div class="output-fill" id="left-bar" style="width: 0%;"></div></div>
            <p>Right Motors: <span id="right-speed">0</span></p>
            <div class="output-bar"><div class="output-fill" id="right-bar" style="width: 0%;"></div></div>
          </div>
        </div>

        <script>
          const valDisplay = document.getElementById('val');
          let currentGamepad = null;
          let speedMode = 0; // 0=Slow, 1=Normal, 2=Fast
          const speedModes = ['Slow (50%)', 'Normal (80%)', 'Fast (100%)'];
          const speedMultipliers = [0.5, 0.8, 1.0];
          let roverEnabled = false;
          let lastButton4 = false;
          let lastButton5 = false;
          let idleTimeout = null;
          const IDLE_DELAY = 500; // ms before returning to idle

          function sendAction(act) {
            const slider = document.getElementById('slider').value;
            fetch(`/?action=${act}&slider=${slider}`).catch(e => console.error(e));
          }
          
          function sendSlider(val) {
            valDisplay.textContent = val;
            fetch(`/?slider=${val}`).catch(e => console.error(e));
          }

          function sendGamepadData(speed, turn) {
            fetch(`/?gamepad=1&speed=${speed}&turn=${turn}`).catch(e => console.error(e));
          }

          function updateVisualizer(x, y) {
            const stick = document.getElementById('stick');
            // Map -1..1 to 0..300
            const posX = (x + 1) * 150;
            const posY = (y + 1) * 150;
            stick.style.left = posX + 'px';
            stick.style.top = posY + 'px';
          }

          function updateOutputBars(speed, turn) {
            // Calculate left/right motor speeds (simplified visualization)
            let leftSpeed = speed;
            let rightSpeed = speed;
            
            if (turn > 0) { // Turning right
              rightSpeed = Math.max(0, speed * (1 - Math.abs(turn) * 0.9));
            } else if (turn < 0) { // Turning left
              leftSpeed = Math.max(0, speed * (1 - Math.abs(turn) * 0.9));
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
              
              // Read axes [0][0] and [0][1] (axes 0 and 1 - left stick)
              const axisY = currentGamepad.axes[1] || 0; // Axis [0][1] - forward/backward
              const axisX = currentGamepad.axes[0] || 0; // Axis [0][0] - left/right (turning)
              
              // Display axis values
              document.getElementById('axis-0').textContent = axisY.toFixed(2);
              document.getElementById('axis-1').textContent = axisX.toFixed(2);
              
              // Update visualizer
              updateVisualizer(axisX, axisY);
              
              // Button 4 (index 4) - cycle speed
              const button4 = currentGamepad.buttons[4] && currentGamepad.buttons[4].pressed;
              if (button4 && !lastButton4) {
                speedMode = (speedMode + 1) % 3;
                document.getElementById('speed-mode').textContent = speedModes[speedMode];
              }
              lastButton4 = button4;
              
              // Button 5 (index 5) - enable/disable rover
              const button5 = currentGamepad.buttons[5] && currentGamepad.buttons[5].pressed;
              if (button5 && !lastButton5) {
                roverEnabled = !roverEnabled;
                document.getElementById('rover-state').textContent = roverEnabled ? 'Enabled' : 'Disabled';
                sendAction(roverEnabled ? 'start' : 'stop');
              }
              lastButton5 = button5;
              
              // Apply deadzone
              const deadzone = 0.15;
              const processedY = Math.abs(axisY) < deadzone ? 0 : axisY;
              const processedX = Math.abs(axisX) < deadzone ? 0 : axisX;
              
              // Calculate speed and turn
              // Speed: -255 to 255 (negative for reverse)
              const rawSpeed = -processedY * 255 * speedMultipliers[speedMode];
              const speed = Math.round(rawSpeed);
              
              // Turn: -100 to 100
              const turn = Math.round(processedX * 100);
              
              document.getElementById('speed-val').textContent = speed;
              document.getElementById('turn-val').textContent = turn;
              
              // Update output visualization
              updateOutputBars(Math.abs(speed), turn / 100);
              
              // Send to rover if enabled and there's input
              if (roverEnabled && (Math.abs(processedY) > 0.001 || Math.abs(processedX) > 0.001)) {
                clearTimeout(idleTimeout);
                sendGamepadData(speed, turn);
              } else if (roverEnabled) {
                // Start idle countdown
                if (!idleTimeout) {
                  idleTimeout = setTimeout(() => {
                    sendGamepadData(0, 0); // Return to idle
                    idleTimeout = null;
                  }, IDLE_DELAY);
                }
              }
            } else {
              document.getElementById('gamepad-status').innerHTML = '<span class="disconnected">⬤ Not Connected</span>';
              updateVisualizer(0, 0);
              document.getElementById('axis-0').textContent = '0.00';
              document.getElementById('axis-1').textContent = '0.00';
              document.getElementById('speed-val').textContent = '0';
              document.getElementById('turn-val').textContent = '0';
              updateOutputBars(0, 0);
            }
            
            requestAnimationFrame(pollGamepad);
          }

          // Start polling
          window.addEventListener('gamepadconnected', (e) => {
            console.log('Gamepad connected:', e.gamepad);
          });
          
          window.addEventListener('gamepaddisconnected', (e) => {
            console.log('Gamepad disconnected');
          });
          
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

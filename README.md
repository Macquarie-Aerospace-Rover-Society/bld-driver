# BLD-510B Driver

This is a quick repo with code to drive the Robot with 4 brushless DC motors controlled by 4 BLD-510B motor drivers.

> [!WARNING]
> This code has not been real world tested, be careful.

> [!WARNING]
> The ESP32 **MUST** be connected to the correct drivers, incorrect connections could cause the robot to tear itself apart.



## Usage

Controlled via USB serial at 115200 BAUD.
Must enable motors before movement, for demo reasons robot will only drive 3 secs before stopping.

### WiFi AP controls

1. Connect to the access point
2. Navigate to the network gateway `192.168.4.1`
3. Enable motors
4. **START**

### Serial Controls

- x to zero the speed of the motors and **SHUTDOWN** the motors.
- w/s for forward / backward (hopefully).
- p to re-enable power to the motors.


## TODO In the future

Swap the polarity of all of the wires on either the left or the right such that the same signals to any driver result in the same direction of motion. This will reduce the chance of incorrect wiring.

Change pin groups so it is clearer what connects to what.




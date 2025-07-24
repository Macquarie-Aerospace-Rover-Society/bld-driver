# BLD-510B Driver

This is a quick repo with code to drive the Robot with 4 brushless DC motors controlled by 4 BLD-510B motor drivers.


> [!WARNING]
> The ESP32 **MUST** be connected to the correct drivers, incorrect connections could cause the robot to tear itself apart.


## Usage

Controlled via USB serial at 115200 BAUD.
Must enable motors before movement, for demo reasons robot will only drive 3 secs before stopping.

### Controls

w/s for forward / backward (hopefully).

# ESP32 4-DOF Robotic Arm Controller Firmware

An industrial-grade, real-time embedded firmware MVP for a 4-DOF articulated robotic arm built on the **ESP32 DevKit V1** and **PCA9685 16-Channel 12-Bit I2C PWM Driver**.

This project implements a dual-core **FreeRTOS** architecture separating deterministic, jitter-free servo motion planning from high-speed asynchronous network communication, web serving, and telemetry broadcasting.

---

## Table of Contents

- [Key Features](#key-features)
- [System Architecture](#system-architecture)
  - [Dual-Core Task Allocation](#dual-core-task-allocation)
  - [Inter-Task Communication & Synchronization](#inter-task-communication--synchronization)
- [Hardware Setup & Wiring](#hardware-setup--wiring)
  - [Components](#components)
  - [Pinout Configuration](#pinout-configuration)
  - [Power Distribution & Safety Rules](#power-distribution--safety-rules)
  - [Joint Channel Mapping](#joint-channel-mapping)
- [Software Stack & Libraries](#software-stack--libraries)
- [Quick Start Guide](#quick-start-guide)
  - [Prerequisites](#prerequisites)
  - [Build and Flash](#build-and-flash)
- [WiFi Provisioning & Captive Portal](#wifi-provisioning--captive-portal)
  - [Initial Connection](#initial-connection)
  - [Factory Reset (Clear Saved WiFi)](#factory-reset-clear-saved-wifi)
- [Web Interface & Control](#web-interface--control)
- [WebSocket API Protocol](#websocket-api-protocol)
  - [Client to ESP32 (Inbound Commands)](#client-to-esp32-inbound-commands)
  - [ESP32 to Client (Outbound Telemetry)](#esp32-to-client-outbound-telemetry)
- [Joint Calibration & Adding a 5th DOF (Gripper)](#joint-calibration--adding-a-5th-dof-gripper)
- [Safety Features & Diagnostics](#safety-features--diagnostics)
- [Troubleshooting & FAQ](#troubleshooting--faq)

---

## Key Features

- **Dual-Core FreeRTOS Multitasking:** Time-critical servo math is isolated on Core 1; WiFi, web sockets, DNS, and HTTP traffic run on Core 0.
- **Deterministic 50 Hz Control Loop:** Rate-limited linear interpolation with `vTaskDelayUntil()` guarantees zero phase jitter on servo pulses.
- **PCA9685 12-Bit PWM Driver:** Offloads PWM generation over 400 kHz Fast I2C, providing 4096 discrete steps across a 20 ms frame.
- **Hardware Output Enable (OE) Safety:** Instant hardware de-energization via active-LOW pin (GPIO 5) during emergency stop or communication loss.
- **Captive Portal Provisioning:** Uses `WiFiManager` with non-blocking FreeRTOS yielding; connects to saved credentials or launches `RobotArm-Setup` SoftAP without blocking FreeRTOS idle watchdogs.
- **Embedded Glassmorphic Web UI:** Clean, responsive, dark-mode single-page interface embedded directly in ESP32 flash (`PROGMEM`). No internet connection or external CDN required.
- **Bi-Directional WebSocket Telemetry:** Real-time state synchronisation and low-latency joint angle command streaming at 5 Hz.
- **Watchdog Protection:** Firmware watchdog and hardware Task Watchdog Timer (TWDT) monitoring prevent lockups and silent task starvation.

---

## System Architecture

```
                    +------------------------------------------+
                    |           ESP32 DevKit V1                |
                    |                                          |
   CORE 0           |                     CORE 1               |
  [Networking & IO] |                    [Real-Time Control]   |
+-------------------+------------------+ +---------------------+
|                   |                  | |                     |
|  WifiManagerTask  |   NetworkTask    | |  ServoControlTask   |
|  (Captive Portal) | (AsyncWebServer) | |  (50 Hz Loop, 20ms) |
|         |         |        |         | |          |          |
|         v         |        v         | |          v          |
|  WifiWatchdogTask |  WebSocket (/ws) | |   motionUpdate()    |
| (Exp. Backoff)    |        |         | |          |          |
+-------------------+--------|---------+ +----------|----------+
                             |                      |
                    g_commandQueue (FreeRTOS Queue) |
                             |                      |
                             +--------------------->+
                                                    |
                                         g_stateMutex (FreeRTOS)
                                                    |
                                                    v
                                         PCA9685 I2C (400 kHz)
                                         [SDA: 21 | SCL: 22]
                                         [OE Enable: GPIO 5]
```

### Dual-Core Task Allocation

| Task Name | Core | Priority | Stack Size | Function |
|---|:---:|:---:|:---:|---|
| **ServoControlTask** | **1** | 3 (High) | 4096 B | Dedicated 50 Hz motion profile interpolation and PCA9685 register updates |
| **NetworkTask** | **0** | 2 (Med) | 8192 B | `AsyncWebServer` and `AsyncWebSocket` frame dispatcher |
| **WifiManagerTask** | **0** | 1 (Normal) | 10240 B | Non-blocking captive portal provisioning & NVS credential storage |
| **WifiWatchdog** | **0** | 1 (Low) | 3072 B | Background link monitor with exponential backoff reconnect |
| **SafetyTask** | **0** | 1 (Low) | 3072 B | Heartbeat timeout watchdog & FreeRTOS stack high-water diagnostics |
| **StatusTask** | **0** | 1 (Low) | 4096 B | Periodic 5 Hz WebSocket telemetry broadcast |
| **loopTask** | **1** | 1 (Low) | 8192 B | Arduino standard loop yielding to reset hardware TWDT |

### Inter-Task Communication & Synchronization

1. **`g_commandQueue` (QueueHandle_t, Depth: 10):**
   - Transmits incoming `CommandMsg` structs from `NetworkTask` to `ServoControlTask`.
   - Passes data by value, decoupling asynchronous network callbacks from real-time motor control without locking latency.
2. **`g_stateMutex` (SemaphoreHandle_t):**
   - Protects the shared `RobotState` struct against race conditions when read by `StatusTask` / `SafetyTask` and written by `ServoControlTask`.
3. **`g_networkEventGroup` (EventGroupHandle_t):**
   - Coordinates system states (`WIFI_CONNECTED_BIT`) so `NetworkTask` and `StatusTask` cleanly await IP acquisition before starting servers or streaming frames.

---

## Hardware Setup & Wiring

### Components

1. **Microcontroller:** ESP32 DevKit V1 (30-pin, ESP32-WROOM-32).
2. **Servo Controller:** PCA9685 16-Channel 12-Bit PWM Driver Module (I2C address: `0x40`).
3. **Actuators:** 4x MG995 High-Torque Metal Gear Servos.
4. **Power Supply:** External 5V DC (3A to 5A minimum rating).
5. **Decoupling Capacitor:** 1000 µF (16V or 25V) electrolytic capacitor across PCA9685 V+ and GND power terminals.

### Pinout Configuration

| ESP32 DevKit V1 Pin | PCA9685 Module Pin | Wire Function | Notes |
|---|---|---|---|
| **GPIO 21** | **SDA** | I2C Data | 400 kHz Fast Mode |
| **GPIO 22** | **SCL** | I2C Clock | 400 kHz Fast Mode |
| **GPIO 5** | **OE** | Output Enable | Active LOW (LOW = servos ON, HIGH = OFF) |
| **3V3** | **VCC** | Logic Power | Logic level (DO NOT connect to servo 5V) |
| **GND** | **GND** | Common Ground | Must tie to External 5V Power Supply GND |
| **GPIO 0 (BOOT)** | Built-in Button | Factory Reset | Hold for 3s during startup to wipe WiFi |

### Power Distribution & Safety Rules

> [!CAUTION]
> **NEVER power the MG995 servos directly from the ESP32 5V (VIN) or 3.3V pins.**
> MG995 servos draw up to 1.2A to 1.5A stall current each (over 5A combined under load). Drawing this from the ESP32 will permanently destroy the onboard linear regulator or cause severe brownout resets.

- **External 5V Power Supply:** Connect `+5V` directly to the PCA9685 **green screw terminal (V+)** and `GND` to the PCA9685 **GND screw terminal**.
- **Common Ground:** Connect an ESP32 `GND` pin to the PCA9685 ground rail. Both power systems must share the same reference potential.
- **Power Sequencing:** Power the external servo power supply *before* or *simultaneously* with the ESP32 USB cable.

### Joint Channel Mapping

| PCA9685 Channel | Joint Name | Default Safe Range | Default Home Angle | Inverted |
|:---:|:---:|:---:|:---:|:---:|
| **0** | Base Rotation | 0° – 180° | 90° | No |
| **1** | Shoulder | 15° – 165° | 45° | No |
| **2** | Elbow | 10° – 170° | 120° | No |
| **3** | Wrist Pitch | 0° – 180° | 90° | No |
| **4** | *(Reserved for Gripper)* | 0° – 100° | 10° | No |

---

## Software Stack & Libraries

Configured in [platformio.ini](file:///c:/Users/Sundaram%20Tripathi/Documents/PlatformIO/Projects/mini_project_arm_3rd%20year/platformio.ini):

- **Platform:** `espressif32 @ 6.10.0`
- **Framework:** `arduino` (ESP-IDF 4.4 / Arduino Core 2.0.17)
- **Dependencies:**
  - `adafruit/Adafruit PWM Servo Driver Library @ ^3.0.2`
  - `bblanchon/ArduinoJson @ ^7.0.4`
  - `ESPAsyncWebServer` (`https://github.com/me-no-dev/ESPAsyncWebServer.git`)
  - `AsyncTCP` (`https://github.com/me-no-dev/AsyncTCP.git`)
  - `WiFiManager` (`https://github.com/tzapu/WiFiManager.git`)

---

## Quick Start Guide

### Prerequisites

- Install [VS Code](https://code.visualstudio.com/) and the [PlatformIO IDE Extension](https://platformio.org/).
- Alternatively, install [PlatformIO Core CLI](https://docs.platformio.org/page/core.html).

### Build and Flash

1. **Clone or Open the Project:**
   Open the `mini_project_arm_3rd year` folder in PlatformIO.
2. **Build the Firmware:**
   ```bash
   pio run
   ```
3. **Upload to ESP32:**
   Connect the ESP32 via USB and run:
   ```bash
   pio run --target upload
   ```
   *(If the board does not auto-enter bootloader mode, hold down the physical `BOOT` button on the ESP32 when flashing starts).*
4. **Open Serial Monitor:**
   ```bash
   pio device monitor
   ```
   Baud rate is configured to **115200**.

---

## WiFi Provisioning & Captive Portal

### Initial Connection

1. On first boot (or when no saved network is reachable), the ESP32 starts a SoftAP:
   - **SSID:** `RobotArm-Setup`
   - **Password:** `robotarm123`
2. Connect your smartphone or laptop to `RobotArm-Setup`.
3. A captive portal popup will appear automatically. If not, open your browser and navigate to:
   ```
   http://192.168.4.1
   ```
4. Click **Configure WiFi**, select your local 2.4 GHz Wi-Fi network, and enter the password.
5. The ESP32 saves the credentials in non-volatile flash (NVS), closes the captive portal, releases port 80, and connects to your network.
6. The assigned IP address (e.g. `192.168.1.150`) will be printed to the Serial Monitor.

### Factory Reset (Clear Saved WiFi)

To wipe stored credentials and force the captive portal to appear again:
- Press and hold the physical **BOOT button (GPIO 0)** for **3 seconds** immediately after powering on the ESP32.
- The Serial Monitor will output:
  `[WIFI] >> 3s elapsed! Resetting WiFi credentials in NVS Flash <<`
- The board automatically reboots into setup mode.

---

## Web Interface & Control

Once connected to your local network, navigate to the ESP32's assigned IP address in any modern web browser:
```
http://<ESP32_IP_ADDRESS>/
```

### UI Features:
- **Individual Joint Sliders & Spinboxes:** Direct degree control with automatic bounds clamping.
- **Global Speed Slider:** Adjust speed dynamically between 1% and 100%.
- **One-Click Presets:**
  - **Home Pose:** Safe neutral parking configuration.
  - **Reach Forward:** Extended pose for picking tasks.
  - **Rest / Sleep:** Folded pose for power-down.
- **Hardware Enable Toggle:** Emergency cut-off toggling the PCA9685 Output Enable (OE) pin.
- **Live Telemetry & Latency HUD:** Shows actual filtered angles, moving status, and connection ping.

---

## WebSocket API Protocol

The WebSocket server listens on endpoint: `ws://<ESP32_IP_ADDRESS>/ws`

### Client to ESP32 (Inbound Commands)

#### 1. Move Joints (`CMD_MOVE`)
```json
{
  "cmd": "move",
  "joints": [90.0, 45.0, 120.0, 90.0],
  "speed": 60.0
}
```
- `joints`: Array of float target angles in degrees matching `NUM_JOINTS`.
- `speed`: Speed percentage (`1.0` to `100.0`).

#### 2. Return to Home (`CMD_HOME`)
```json
{
  "cmd": "home"
}
```

#### 3. Hardware Servo Enable/Disable (`CMD_ENABLE`)
```json
{
  "cmd": "enable",
  "value": true
}
```

#### 4. Enable Wireless OTA Service (`CMD_ENABLE_OTA`)
```json
{
  "cmd": "enable_ota"
}
```

### ESP32 to Client (Outbound Telemetry)

1. **State Telemetry (Broadcast at 5 Hz):**
```json
{
  "type": "state",
  "joints": [90.0, 45.0, 120.0, 90.0],
  "enabled": true,
  "moving": false,
  "speed": 60.0
}
```

2. **OTA Status Notification:**
```json
{
  "type": "ota_status",
  "enabled": true,
  "ip": "10.35.60.190",
  "port": 3232,
  "hostname": "esp32-robotarm"
}
```

---

## Wireless OTA Firmware Flashing

The firmware supports both **PlatformIO Network OTA** and **Direct Web Browser Flashing**:

### Method A: Direct Web Browser Flashing (No USB or CLI Needed)
1. Open the Web Interface at `http://<ESP32_IP>/`.
2. Click **`📡 Enable Wireless OTA`**.
3. The **Wireless OTA Firmware Flashing** card will appear, showing your ESP32's current IP and port.
4. Under *Direct Web Browser Upload*, click **Choose File** and select `.pio/build/esp32doit-devkit-v1/firmware.bin`.
5. Click **`⬆ Flash`**. The browser shows a real-time progress bar. Once completed, the ESP32 automatically reboots with the new firmware!

### Method B: PlatformIO Wireless Flash (Over Network)
1. In the Web UI, click **`📡 Enable Wireless OTA`**.
2. Click the **`📋 Copy`** button next to the generated command, or run in your terminal:
   ```bash
   pio run -t upload --upload-port <ESP32_IP>
   ```
   *(Example: `pio run -t upload --upload-port 10.35.60.190`)*

> [!NOTE]
> For hardware safety, whenever an OTA flash begins (via network or browser), the firmware automatically disables the servos via the PCA9685 Output Enable pin (GPIO 5) to prevent accidental joint movements while flash memory is written.

## Joint Calibration & Adding a 5th DOF (Gripper)

All mechanical calibration parameters reside in [src/config.h](file:///c:/Users/Sundaram%20Tripathi/Documents/PlatformIO/Projects/mini_project_arm_3rd%20year/src/config.h).

### Calibration Structure:
```cpp
struct JointConfig {
    uint8_t  channel;       // PCA9685 PWM channel (0-15)
    uint16_t minPulseTicks; // PCA9685 ticks at minimum angle (reference: 150)
    uint16_t maxPulseTicks; // PCA9685 ticks at maximum angle (reference: 600)
    float    minAngle;      // Minimum physical safety angle in degrees
    float    maxAngle;      // Maximum physical safety angle in degrees
    float    homeAngle;     // Neutral parking angle in degrees
    bool     invert;        // Invert direction if horn is mirrored
};
```

### Steps to Add a Gripper (5th Servo):
1. In [src/config.h](file:///c:/Users/Sundaram%20Tripathi/Documents/PlatformIO/Projects/mini_project_arm_3rd%20year/src/config.h):
   - Change `constexpr size_t NUM_JOINTS = 4;` to `5;`.
   - Add a row to `JOINT_CONFIGS`:
     ```cpp
     { 4, SERVO_MIN_TICKS, SERVO_MAX_TICKS, 0.0f, 100.0f, 10.0f, false } // Joint 4: Gripper
     ```
   - Add `"Gripper"` to `JOINT_NAMES`.
2. In [src/web_ui.h](file:///c:/Users/Sundaram%20Tripathi/Documents/PlatformIO/Projects/mini_project_arm_3rd%20year/src/web_ui.h):
   - Add a 5th slider card in HTML for the gripper.
3. Re-flash the firmware. The motion engine, queue sizing, and telemetry arrays automatically scale to `NUM_JOINTS`.

---

## Safety Features & Diagnostics

1. **Watchdog Timeout (`SafetyTask`):**
   - If no valid command is received over WebSocket within `COMMAND_TIMEOUT_MS` (5000 ms), the safety task logs a warning.
   - Configurable options in `config.h`:
     - `RETURN_HOME_ON_TIMEOUT = true`: Automatically commands all joints back to home position.
     - `DISABLE_SERVO_ON_TIMEOUT = true`: Pulls OE high to de-energize servos and eliminate holding strain.
2. **Stack High-Water Diagnostics:**
   - Every 10 seconds, `SafetyTask` prints the minimum remaining stack space (in 32-bit words) for all background tasks:
     ```
     [STACK] Free Stack (Words) -> Net: 4120 | Servo: 2840 | Safety: 1980 | Status: 2750
     ```
3. **Boot & Reset Reason Logging:**
   - On every boot, [src/main.cpp](file:///c:/Users/Sundaram%20Tripathi/Documents/PlatformIO/Projects/mini_project_arm_3rd%20year/src/main.cpp) queries `esp_reset_reason()` and outputs whether startup was due to a normal power-on, software reset, TWDT watchdog trigger, or brownout.

---

## Troubleshooting & FAQ

### 1. ESP32 enters a bootloop
- **Watchdog Timer (TWDT):** Verify `loop()` in `main.cpp` calls `vTaskDelay(pdMS_TO_TICKS(1000))` and does not block indefinitely on `portMAX_DELAY`.
- **Brownout Reset:** If the Serial Monitor prints `Brownout detector was triggered`, your USB port cannot supply enough current when the WiFi radio transmits. Use a high-quality USB cable connected to a powered USB 3.0 port, and ensure servos are powered by an external 5V supply.

### 2. `[SERVO] ERROR: PCA9685 not detected at I2C address 0x40!`
- Check I2C wiring: ESP32 GPIO 21 -> PCA9685 SDA, ESP32 GPIO 22 -> PCA9685 SCL.
- Confirm PCA9685 `VCC` is supplied with 3.3V from the ESP32.
- The firmware will gracefully idle the servo task instead of crashing if the driver is not detected.

### 3. Servos jitter or vibrate violently
- Ensure PCA9685 **GND** is tied to both the external 5V power supply ground AND the ESP32 ground.
- Place a 1000 µF capacitor across the PCA9685 V+ and GND screw terminals to absorb voltage dips caused by motor startup surges.
- Ensure external power supply provides at least 3A (5A recommended for 4x MG995 servos).

### 4. Captive Portal doesn't open
- Disconnect and reconnect to the `RobotArm-Setup` WiFi network.
- Manually open `http://192.168.4.1` in Chrome or Firefox.
- If previously configured, hold the **BOOT** button for 3 seconds on power-up to wipe stored credentials.

---

## License

MIT License. Designed for university robotics coursework, intermediate embedded developers, and educational robotics projects.

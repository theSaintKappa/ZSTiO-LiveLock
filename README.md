# ZSTiO LiveLock
A door monitoring system for tracking and logging room access states using ESP32 and Firebase Firestore.

> [!NOTE]
> ZSTiO LiveLock is a simple IoT solution that monitors the state of a door (open/closed) using a reed switch sensor and reports this data to Firebase Firestore in real-time. I designed it to use on my school's library door but it can be easily adapted for other use cases.

### Features
* **Real-time door state monitoring** using a reed switch sensor
* **Firestore integration** for storing current door state and logging all state changes
* **Visual feedback** via built-in LED:
    * Solid on: Connected to WiFi
    * Flashing: Connecting or reconnecting to WiFi
* **Automatic reconnection** if WiFi connection is lost
* **NTP time synchronization** for accurate timestamps

### Hardware Requirements
* ESP32 development board (e.g., ESP32 DevKitC)
* Reed switch (door/window sensor)
* Basic wiring components

### Connections
* Reed switch connected to **GPIO pin 14** (adjust in main.cpp if needed) and **GND**
* Built-in LED on **pin 2** *(standard for most ESP32 boards)*

### Software Requirements
* [PlatformIO](https://platformio.org/) (recommended) or Arduino IDE
* [FirebaseClient](https://github.com/mobizt/FirebaseClient) library by [mobizt](https://github.com/mobizt)

### Setup Instructions
1. Clone this repository
1. Copy `secrets.h.example` to `secrets.h` in the `include` directory
1. Edit `secrets.h` and add your:
    * WiFi credentials (AP_SSID, AP_PASSWORD)
    * GCP service account credentials:
        * Firebase project ID
        * Firebase client email
        * Firebase private key
1. Adjust `ROOM_NAME` in `main.cpp` with your room name
1. Build and upload the project using PlatformIO

### Firestore Data Structure
The application uses two Firestore collections:
1. **status** - Stores the current state of each room:
    ```json
    {
        "roomName": "library",
        "state": "open",
        "isOpen": true,
        "updatedAt": "2025-03-25T21:30:00.000Z"
    }
    ```
1. **logs** - Records all state changes:
    ```json
    {
        "roomName": "library",
        "newState": "closed",
        "isOpen": false,
        "timestamp": "2025-03-25T22:00:00.000Z"
    }
    ```
### LED Status Indicators
* **Flashing LED**: WiFi connecting or reconnecting
* **Solid LED on**: WiFi connected and system operational

### License
This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.
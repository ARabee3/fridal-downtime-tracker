# FRIDAL — Automated Downtime Tracking System

Industrial machine downtime tracking system for factory production lines. Automatically detects macro and micro stops using ESP32-S3 sensors (current sensor + IR sensor), replacing error-prone manual worker logging with precise automated measurements.

## Table of Contents

- [Architecture](#architecture)
- [How It Works](#how-it-works)
- [Project Structure](#project-structure)
- [Hardware Setup](#hardware-setup)
- [Setup & Run](#setup--run)
  - [1. Backend Server](#1-backend-server)
  - [2. Frontend (Served by Backend)](#2-frontend-served-by-backend)
  - [3. ESP32-S3 Flashing](#3-esp32-s3-flashing)
- [Configuration](#configuration)
  - [Main ESP](#main-esp)
  - [Secondary ESP](#secondary-esp)
- [API Reference](#api-reference)
- [ESP-Server Communication](#esp-server-communication)
- [Pages](#pages)
- [Export Format](#export-format)

---

## Architecture

```
┌──────────────────────────────────────────────────────────────┐
│                        FACTORY FLOOR                         │
│                                                              │
│  ┌──────────────────┐        ┌──────────────────┐           │
│  │   MAIN ESP32-S3  │◄──────►│ SECONDARY ESP32-S3│          │
│  │  (Current Sensor)│ ESP-NOW │   (IR Sensor)     │          │
│  │   GPIO 18        │        │    GPIO 18         │          │
│  └────────┬─────────┘        └───────────────────┘           │
│           │ HTTP POST                                         │
│           │ /api/sensor/*                                     │
│           ▼                                                   │
│  ┌──────────────────────────────────────────────────┐        │
│  │              BACKEND SERVER                       │        │
│  │         Express.js (port 3000)                    │        │
│  │   ┌────────────┐  ┌──────────────────┐           │        │
│  │   │  REST API  │  │  data.json       │           │        │
│  │   │  /api/*    │  │  causes.json     │           │        │
│  │   │            │  │  exports/*.xlsx   │           │        │
│  │   └────────────┘  └──────────────────┘           │        │
│  └────────┬─────────────────────────────────────────┘        │
│           │ serves static files + API                        │
│           ▼                                                  │
│  ┌──────────────────────────────────────────────────┐        │
│  │                   FRONTEND                        │        │
│  │  index.html (Admin Dashboard)                    │        │
│  │  worker.html (Worker Panel)                      │        │
│  │                                                  │        │
│  │  Polls GET /api/stops every 3s                   │        │
│  └──────────────────────────────────────────────────┘        │
└──────────────────────────────────────────────────────────────┘
```

## How It Works

### Failure Detection (ESP Side)

1. **Current Sensor** (Main ESP): Monitors conveyor motor current draw. When the conveyor stops or jams, current drops/changes, triggering a fault signal on GPIO 18.

2. **IR Sensor** (Secondary ESP): Mounted on the conveyor belt to detect passing bottles/products. If no object passes the beam for 30+ seconds, the system infers a line stoppage.

3. **ESP-NOW Communication**: The Secondary ESP sends real-time IR state (`"state":0/1`, `"idle":0/1`) to the Main ESP via ESP-NOW, a low-latency WiFi-direct protocol.

4. **Fault Logic** (Main ESP): A failure is declared when EITHER:
   - Current sensor detects a fault, OR
   - IR sensor has been idle (no bottle for 30+ seconds)

5. **Debouncing**: Failures must be CLEAR for 2 continuous seconds before the stop is ended, preventing false toggling from sensor noise.

### Downtime Recording (Server Side)

1. ESP sends `POST /api/sensor/failure-start` → Server creates an "active" stop record
2. ESP sends `POST /api/sensor/failure-end` → Server calculates duration, classifies as:
   - **Microstop**: duration ≤ 30 seconds (auto-closed with cause "Microstop")
   - **Macro Stop**: duration > 30 seconds → "pending" — requires worker classification

### Worker Classification

Workers use the Worker Panel (`/worker`) to assign cause and location to each pending stop. The Admin Dashboard (`/`) provides full management with Excel export.

---

## Project Structure

```
fridal/
├── README.md                          # This file
├── esp_code/
│   ├── esp_code_main/                 # Main ESP (current sensor + WiFi + HTTP)
│   │   ├── src/
│   │   │   ├── main.cpp               # Main loop + failure state machine
│   │   │   ├── CurrentSensor.cpp      # Digital GPIO current sensor driver
│   │   │   ├── EspNowDriver.cpp       # ESP-NOW receiver (gets IR data)
│   │   │   ├── EspNowDriver.h
│   │   │   └── ServerComms.cpp        # WiFi + HTTP client (talks to backend)
│   │   ├── include/
│   │   │   ├── config.h               # Pins, WiFi, server, MAC, thresholds
│   │   │   ├── CurrentSensor.h
│   │   │   └── ServerComms.h
│   │   └── platformio.ini
│   │
│   ├── esp_code_secondary/            # Secondary ESP (IR sensor + ESP-NOW)
│   │   ├── src/
│   │   │   ├── main.cpp               # IR detection + ESP-NOW sender
│   │   │   ├── IRSensor.cpp           # Digital GPIO IR sensor driver
│   │   │   └── EspNowDriver.cpp       # ESP-NOW sender
│   │   ├── include/
│   │   │   ├── config.h               # IR pin, MAC, idle timeout
│   │   │   ├── IRSensor.h
│   │   │   └── EspNowDriver.h
│   │   └── platformio.ini
│   │
│   └── esp_code_mac_check/            # Utility: test HTTP endpoints + print MAC
│       ├── src/main.cpp
│       ├── include/config.h
│       └── platformio.ini
│
└── Fridal/
    ├── backend/
    │   ├── server.js                  # Express server entry point
    │   ├── package.json
    │   ├── causes.json                # Downtime causes + locations list
    │   ├── data.json                  # Today's stop data (auto-created)
    │   ├── exports/                   # Daily Excel reports (.xlsx)
    │   └── src/
    │       ├── config/index.js        # Server config (port, paths, etc.)
    │       ├── routes/
    │       │   ├── sensorRoutes.js    # ESP endpoints: /failure-start, /failure-end
    │       │   ├── stopsRoutes.js     # Stops CRUD + Excel export
    │       │   ├── causesRoutes.js    # Causes/locations CRUD
    │       │   ├── exportsRoutes.js   # List export files
    │       │   └── simulateRoutes.js  # Simulator endpoints for testing
    │       ├── store/
    │       │   ├── dataStore.js       # data.json read/write
    │       │   └── causesStore.js     # causes.json read/write
    │       └── utils/
    │           └── dateUtils.js       # Time parsing, duration formatting
    │
    └── frontend/
        └── public/
            ├── index.html             # Admin dashboard (single-file SPA)
            ├── worker.html            # Worker panel (simplified view)
            └── logo.jpeg              # Brand logo
```

---

## Hardware Setup

### ESP32-S3-N8R2 Wiring

| Main ESP GPIO | Connects To |
|---------------|-------------|
| GPIO 18       | Current sensor digital output |
| 3.3V          | Current sensor VCC |
| GND           | Current sensor GND |

| Secondary ESP GPIO | Connects To |
|--------------------|-------------|
| GPIO 18            | IR sensor digital output |
| 3.3V               | IR sensor VCC |
| GND                | IR sensor GND |

### MAC Address Setup

Each ESP32-S3 needs to know the other's MAC address for ESP-NOW:

1. Flash `esp_code_mac_check` to each ESP to print its MAC
2. Copy the Secondary ESP's MAC into `esp_code_main/include/config.h`:
   ```c
   #define SECONDARY_PEER_MAC {0x48, 0xCA, 0x43, 0xAF, 0x8B, 0x78}
   ```
3. Copy the Main ESP's MAC into `esp_code_secondary/include/config.h`:
   ```c
   #define MAIN_PEER_MAC {0x74, 0x4D, 0xBD, 0x46, 0x1B, 0x24}
   ```

---

## Setup & Run

### 1. Backend Server

```bash
cd Fridal/backend
npm install
node server.js
```

Server starts on `http://localhost:3000`.

**One-command scripts** (from `Fridal/` folder):

- **Linux**: `chmod +x ./run-fridal.sh && ./run-fridal.sh`
- **Windows**: `powershell -ExecutionPolicy Bypass -File .\run-fridal.ps1`

These scripts auto-install Node.js if missing, run `npm install`, start the server, and open the browser.

### 2. Frontend (Served by Backend)

No separate frontend setup needed. The backend serves static files from `frontend/public/`.

- **Admin Dashboard**: http://localhost:3000
- **Worker Panel**: http://localhost:3000/worker

### 3. ESP32-S3 Flashing

**Prerequisites**: Install [PlatformIO](https://platformio.org/install) (VS Code extension recommended).

#### Main ESP (current sensor):
```bash
cd esp_code/esp_code_main

# Edit include/config.h to set:
#   - WIFI_SSID, WIFI_PASSWORD
#   - SERVER_HOST (IP of the machine running the backend)
#   - SECONDARY_PEER_MAC (MAC of the secondary ESP)

pio run --target upload
pio device monitor
```

#### Secondary ESP (IR sensor):
```bash
cd esp_code/esp_code_secondary

# Edit include/config.h to set:
#   - WIFI_SSID, WIFI_PASSWORD (same network as main ESP)
#   - MAIN_PEER_MAC (MAC of the main ESP)

pio run --target upload
pio device monitor
```

#### MAC Check Utility:
```bash
cd esp_code/esp_code_mac_check

# Edit include/config.h to set server IP
# Prints the local MAC address of each board

pio run --target upload
pio device monitor
```

---

## Configuration

### Main ESP

All settings in `esp_code/esp_code_main/include/config.h`:

| Setting | Description | Default |
|---------|-------------|---------|
| `CURRENT_SENSOR_PIN` | GPIO pin for current sensor | `18` |
| `CURRENT_ACTIVE_HIGH` | Polarity: `1`=HIGH means fault, `0`=LOW means fault | `0` |
| `CURRENT_DEBOUNCE_MS` | Debounce window for sensor noise | `100` |
| `WIFI_SSID` | WiFi network name | `"OHS"` |
| `WIFI_PASSWORD` | WiFi password | — |
| `SERVER_HOST` | Backend server IP/hostname | `"10.238.6.240"` |
| `SERVER_PORT` | Backend server port | `3000` |
| `MACHINE_ID` | Machine identifier | `"M1"` |
| `FAILURE_CLEAR_DEBOUNCE_MS` | How long fault must be clear before ending (ms) | `2000` |
| `SECONDARY_PEER_MAC` | MAC of the secondary ESP | — |
| `ESPNOW_STALE_MS` | Max age of ESP-NOW data before ignored | `5000` |
| `HTTP_TIMEOUT_MS` | HTTP request timeout | `5000` |
| `HTTP_RETRY_INTERVAL_MS` | Retry interval for failed requests | `2000` |
| `BOARD_DEBUG` | Enable Serial debug output | `1` |

### Secondary ESP

All settings in `esp_code/esp_code_secondary/include/config.h`:

| Setting | Description | Default |
|---------|-------------|---------|
| `IR_PIN` | GPIO pin for IR sensor | `18` |
| `IR_ACTIVE_HIGH` | Polarity: `1`=HIGH means object present | `1` |
| `IR_DEBOUNCE_MS` | Debounce window | `50` |
| `IR_IDLE_TIMEOUT_SEC` | Seconds without state change = failure | `30` |
| `WIFI_SSID` | WiFi network (same as main for channel sync) | `"OHS"` |
| `WIFI_PASSWORD` | WiFi password | — |
| `MAIN_PEER_MAC` | MAC of the main ESP | — |
| `BOARD_DEBUG` | Enable Serial debug output | `1` |

---

## API Reference

### ESP/Sensor Endpoints

| Method | Endpoint | Description |
|--------|----------|-------------|
| `POST` | `/api/sensor/failure-start` | Sensor reports failure began |
| `POST` | `/api/sensor/failure-end` | Sensor reports failure resolved |

#### `POST /api/sensor/failure-start`

Request body:
```json
{
  "machine_id": "M1",
  "timestamp": "08:30"    // optional — uses server time if omitted
}
```

Response:
```json
{
  "success": true,
  "stop_id": "abc-123-def",
  "start": "08:30"
}
```

#### `POST /api/sensor/failure-end`

Request body:
```json
{
  "machine_id": "M1",
  "stop_id": "abc-123-def",   // optional — auto-finds last active stop
  "timestamp": "09:15"        // optional — uses server time if omitted
}
```

Response:
```json
{
  "success": true,
  "stop": { "id": "abc-123", "duration": "45m 0s", ... }
}
```

### Testing (Simulator)

| Method | Endpoint | Description |
|--------|----------|-------------|
| `POST` | `/api/simulate/start` | Simulate a failure start |
| `POST` | `/api/simulate/end` | Simulate a failure end (supports `minutes` param) |

### Stops Management

| Method | Endpoint | Description |
|--------|----------|-------------|
| `GET` | `/api/stops` | Get today's stops + summary |
| `PATCH` | `/api/stops/:id` | Update cause/location on a stop |
| `POST` | `/api/stops/:id/lock` | Lock a stop for editing |
| `POST` | `/api/stops/:id/unlock` | Unlock a stop |
| `POST` | `/api/submit` | Export today to Excel |
| `GET` | `/api/download/:filename` | Download an export file |

### Causes & Exports

| Method | Endpoint | Description |
|--------|----------|-------------|
| `GET` | `/api/causes` | Get causes & locations lists |
| `PUT` | `/api/causes` | Update causes & locations |
| `GET` | `/api/exports` | List past exports |

---

## ESP-Server Communication

```
Secondary ESP ───ESP-NOW───► Main ESP ───HTTP POST───► Backend Server
   (IR sensor)                (current + IR sensor)      (Express :3000)

ESP-NOW payload: {"type":"ir","state":0,"idle":1}

HTTP POST URL:  http://<SERVER_HOST>:3000/api/sensor/failure-start
                {"machine_id":"M1","stop_id":"...","timestamp":"..."}
```

- ESP-NOW messages sent every 1 second + on every sensor state change
- HTTP requests sent only on state transitions (edge-triggered)
- Failed HTTP requests retried automatically every 2 seconds
- WiFi auto-reconnects if disconnected
- ESP-NOW data older than 5 seconds is ignored as stale

---

## Pages

### Admin Dashboard (`/`)

- Real-time stops table with color-coded status badges
- Stats cards: total stops, total downtime, microstop count, classified count
- Active failure banner with live elapsed timer
- Edit cause/location for pending stops directly in the table
- Manage causes & locations lists (add/remove)
- Sensor simulator for testing (simulate failure start/end events)
- Excel export with summary sheet
- Export history listing with downloads

### Worker Panel (`/worker`)

- Simplified tablet/kiosk interface for factory-floor operators
- Active failure banner with live timer when machine is down
- "Machine Running" green state when everything is clear
- Pending stops appear as form cards with location and cause dropdowns
- Smart rendering: cards never flicker or reset while the worker is interacting

---

## Export Format

Each daily export (`exports/downtime_YYYY-MM-DD.xlsx`) contains:

**Sheet 1 — Downtime Log:**
| Date | Machine | Start Time | End Time | Duration | Duration (sec) | Location | Cause | Type | Status |

**Sheet 2 — Summary:**
- Total stops, regular stops, microstops (≤30s)
- Total downtime (formatted + seconds)
- Breakdown by cause (count + total duration)

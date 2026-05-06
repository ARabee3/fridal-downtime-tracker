# Fridail Downtime Tracker

Industrial machine downtime tracking system for Fridal factory.

## Architecture

```
fridail-downtime/
├── backend/
│   ├── server.js          # Express API server
│   ├── data.json          # Today's stop data (auto-created)
│   ├── causes.json        # Causes & locations list (auto-created)
│   └── exports/           # Daily Excel reports saved here
└── frontend/
    └── public/
        └── index.html     # Single-page dashboard
```

## Setup & Run

```bash
cd backend
npm install
node server.js
```

Open browser: **http://localhost:3000**

## One-Command Start Scripts (Windows + Linux)

If you want a "from-zero" startup (including installing Node.js/npm when missing), use the scripts in this folder.

### Windows (PowerShell)

Run from the `Fridal` folder:

```powershell
powershell -ExecutionPolicy Bypass -File .\run-fridal.ps1
```

Options:

```powershell
powershell -ExecutionPolicy Bypass -File .\run-fridal.ps1 -SkipBrowser
```

### Linux (bash)

Run from the `Fridal` folder:

```bash
chmod +x ./run-fridal.sh
./run-fridal.sh
```

Options:

```bash
./run-fridal.sh --skip-browser
```

What the scripts do:
- Check/install `node` and `npm` if missing
- Run `npm install` in `backend/`
- Start `node server.js`
- Open `http://localhost:3000` (unless skipped)

---

## ESP / Sensor Integration

Your ESP32/ESP8266 should send HTTP POST requests to the backend.

### When machine failure STARTS:
```http
POST http://<server-ip>:3000/api/sensor/failure-start
Content-Type: application/json

{
  "machine_id": "M1",
  "timestamp": "08:30"   // optional — uses server time if omitted
}
```

### When machine failure ENDS:
```http
POST http://<server-ip>:3000/api/sensor/failure-end
Content-Type: application/json

{
  "machine_id": "M1",
  "stop_id": "...",       // optional — auto-finds last active stop
  "timestamp": "09:15"   // optional — uses server time if omitted
}
```

### Example Arduino/ESP32 code:
```cpp
#include <WiFi.h>
#include <HTTPClient.h>

const char* serverUrl = "http://192.168.1.100:3000";

void reportFailureStart() {
  HTTPClient http;
  http.begin(String(serverUrl) + "/api/sensor/failure-start");
  http.addHeader("Content-Type", "application/json");
  int code = http.POST("{\"machine_id\":\"M1\"}");
  http.end();
}

void reportFailureEnd() {
  HTTPClient http;
  http.begin(String(serverUrl) + "/api/sensor/failure-end");
  http.addHeader("Content-Type", "application/json");
  int code = http.POST("{\"machine_id\":\"M1\"}");
  http.end();
}
```

---

## API Reference

| Method | Endpoint | Description |
|--------|----------|-------------|
| POST | `/api/sensor/failure-start` | Report failure start |
| POST | `/api/sensor/failure-end` | Report failure end |
| GET | `/api/stops` | Get today's stops + summary |
| PATCH | `/api/stops/:id` | Update stop's cause/location |
| POST | `/api/submit` | Export today to Excel & download |
| GET | `/api/causes` | Get causes & locations lists |
| PUT | `/api/causes` | Update causes & locations |
| GET | `/api/exports` | List past exports |
| GET | `/api/download/:filename` | Download an export file |

---

## Excel Export Format

Each daily export (`exports/downtime_YYYY-MM-DD.xlsx`) contains:

**Sheet 1 — Downtime Log:**
- Date, Machine, Start Time, End Time, Duration, Duration (min), Location, Cause, Type, Status

**Sheet 2 — Summary:**
- Total stops, regular stops, microstops
- Total downtime
- Breakdown by cause

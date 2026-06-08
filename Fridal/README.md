# FRIDAL Downtime Tracker

Industrial machine downtime tracking system for FRIDAL factory.

## Architecture

```
fridal-downtime/
├── backend/
│   ├── server.js          # Express API server
│   ├── data.json          # Today's stop data (auto-created)
│   ├── causes.json        # Causes & locations list (auto-created)
│   └── exports/           # Daily Excel reports saved here
└── frontend/
    └── public/
        ├── index.html     # Admin dashboard
        └── worker.html    # Worker panel (simple)
```

## Setup & Run

```bash
cd backend
npm install
node server.js
```

Open browser:
- **Admin:** http://localhost:3000
- **Worker:** http://localhost:3000/worker

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

## Pages

### Admin Dashboard (`/`)
- Full-featured management interface
- Stops log in **descending order** (newest first)
- Stats cards: total stops, downtime, microstops, classified
- Active failure banner with live elapsed timer
- Edit cause/location for any pending stop directly in the table
- Manage causes & locations lists
- Export history with download
- Sensor simulator for testing
- Excel export to `.xlsx`

### Worker Panel (`/worker`)
- Ultra-simple interface for factory-floor workers
- Shows **all pending stops** as independent cards
- Each card displays: **Start, End, Duration**, Location dropdown, Cause dropdown, Submit button
- **Smart re-render:** the page only updates when something actually changes (new failure, failure classified, active stop starts/ends). Existing cards never flicker or reset while the worker is interacting
- Active failure banner with live timer while a failure is in progress
- "Machine Running" state when everything is clear

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

## Startup Behavior

When the server starts, it automatically cleans up any `active` stops left over from a previous session (e.g., if the server crashed while a failure was in progress). These stale active stops are auto-ended with the current time and converted to `pending` status.

---

## API Reference

| Method | Endpoint | Description |
|--------|----------|-------------|
| POST | `/api/sensor/failure-start` | Sensor reports failure began |
| POST | `/api/sensor/failure-end` | Sensor reports failure resolved |
| POST | `/api/simulate/start` | Simulate failure start (testing) |
| POST | `/api/simulate/end` | Simulate failure end (testing) |
| GET | `/api/stops` | Get today's stops + summary |
| PATCH | `/api/stops/:id` | Update stop's cause/location |
| POST | `/api/stops/:id/lock` | Lock a stop (optional) |
| POST | `/api/stops/:id/unlock` | Unlock a stop (optional) |
| POST | `/api/submit` | Export today to Excel & download |
| GET | `/api/causes` | Get causes & locations lists |
| PUT | `/api/causes` | Update causes & locations |
| GET | `/api/exports` | List past exports |
| GET | `/api/download/:filename` | Download an export file |

---

## Excel Export Format

Each daily export (`exports/downtime_YYYY-MM-DD.xlsx`) contains:

**Sheet 1 — Downtime Log:**
- Date, Machine, Start Time, End Time, Duration, Duration (sec), Location, Cause, Type, Status

**Sheet 2 — Summary:**
- Total stops, regular stops, microstops
- Total downtime
- Breakdown by cause

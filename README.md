# Adaptive Traffic Light System
ESP32-based 4-way traffic light controller with a web dashboard for real-time admin controls.

## Features
- 4-direction traffic cycle with independent green/yellow/red phases per road
- Configurable phase durations per direction
- Manual phase override via web dashboard
- RFID emergency preemption — scan any card to trigger all-red for 5 seconds
- LDR ambient light sensing — brightness (0–100) printed to Serial every second
- Self-hosted web dashboard over ESP32 WiFi hotspot — no router needed

## Hardware
| Component | Purpose |
|---|---|
| ESP32 | Microcontroller |
| RFID-RC522 | Emergency vehicle preemption |
| 74HC595N shift register | Drives 8 LEDs (4× red, 4× yellow) |
| 4× Green LEDs | Direct GPIO, one per road |
| Emergency red LED | Indicates active emergency state |
| LDR | Ambient light sensor, output on Serial Monitor |

Full pin mapping: [`traffic light pins.md`](traffic%20light%20pins.md)

## Project Structure
```
src/
  main.cpp          — setup, loop, RFID handling, LDR reading
  traffic.h/.cpp    — traffic state machine, LED output
  webserver.h/.cpp  — WiFi AP, HTTP server, REST API
data/
  dashboard.html    — web dashboard (edit this for UI changes)
  API.md            — REST API reference for dashboard development
```

## Building & Flashing

### Requirements
- [PlatformIO](https://platformio.org/)

### First-time setup
```bash
# Flash firmware
pio run -t upload

# Upload dashboard to ESP32 filesystem
pio run -t uploadfs
```

### After dashboard changes
```bash
pio run -t uploadfs   # filesystem only, no firmware reflash needed
```

### After firmware changes
```bash
pio run -t upload
```

## Usage

1. Power the ESP32 via USB
2. Connect to WiFi: **TrafficLight** / **12345678**
3. Open browser to **http://192.168.4.1**

## Dashboard Development

The dashboard is a single HTML file at `data/dashboard.html`. A working stub is already provided with the fetch loop and API calls pre-wired — you only need to build the UI.

See [`data/API.md`](data/API.md) for the full API reference.

**Quick API summary:**

| Method | Endpoint | Description |
|---|---|---|
| GET | `/api/status` | Live system state (poll at ~100ms) |
| POST | `/api/override` | Force a direction green, or clear override |
| POST | `/api/timing` | Update phase durations (ms) |

## License
MIT

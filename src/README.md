# src/ — ESP32 Adaptive Traffic Light Firmware

Source code for an adaptive traffic light controller running on ESP32. The firmware manages a four-road intersection with hardware LED control, RFID-triggered emergency mode, ambient light sensing, a browser-based dashboard, and adaptive greedy scheduling for traffic optimization.

## Files

| File | Role |
|---|---|
| `main.cpp` | Entry point: SPI init, RFID polling, LDR sampling, WiFi/server setup, main loop |
| `traffic.h` / `traffic.cpp` | Traffic light state machine, LED control, phase logic |
| `webserver.h` / `webserver.cpp` | WiFi AP creation, LittleFS mount, async REST API server |
| `ml.h` / `ml.cpp` | Greedy adaptive scheduling and queue simulation |

## Architecture

Single-loop firmware with no RTOS. The main loop calls `trafficUpdate()` and polls the RFID reader on every iteration; the LDR is sampled once per second. All state transitions are time-based using `millis()` — no threads or interrupt-driven scheduling beyond the RFID trigger.

## Traffic State Machine

Defined in `traffic.h` / `traffic.cpp` around the `TrafficState` struct and a `Phase` enum.

### Normal Cycle

```
TOP_GREEN -> TOP_YELLOW -> BOTTOM_GREEN -> BOTTOM_YELLOW ->
LEFT_GREEN -> LEFT_YELLOW -> RIGHT_GREEN -> RIGHT_YELLOW -> repeat
```

### Special Phases

- **OVERRIDE** — one road held green indefinitely until cleared via the web API
- **EMERGENCY** — all roads red for `5000 / simSpeed` ms, triggered by RFID card scan; 15-second cooldown between triggers

### Key `TrafficState` Fields

| Field | Type | Description |
|---|---|---|
| `phase` | `Phase` | Current phase enum value |
| `emergency` | `bool` | True during 5-second all-red |
| `overrideDir` | `int` | -1 = none, 0-3 = direction index |
| `timings` | struct | Configurable green/yellow durations (ms) |
| `phaseStart` | `unsigned long` | `millis()` timestamp of phase start |
| `ldrBrightness` | `int` | Ambient brightness 0–100% (mapped from ADC) |
| `ldrNightThreshold` | `int` | Calibrated at boot |
| `autoDimEnabled` | `bool` | Whether night mode scaling is active |
| `mlMode` | `MLMode` | `ML_NORMAL` or `ML_GREEDY` |
| `queues[4]` | `float` | Normalized queue depth per road [0.0, 1.0] |
| `intensity[4]` | `int` | Arrival rate per road: 0=low, 1=med, 2=high |
| `mlDuration` | `int` | Green phase duration from last greedy decision (ms) |
| `simSpeed` | `uint8_t` | Simulation speed multiplier: 1=×1, 2=×2, 5=×5 — scales tick rate and phase durations |

### LED Control

- **Red and yellow LEDs:** 74HC595N shift register (DATA=27, CLOCK=26, LATCH=25). Q0-Q3 drive red; Q4-Q7 drive yellow.
- **Green LEDs:** Direct GPIO — top=2, bottom=16, left=17, right=21.
- **Emergency LED:** Direct GPIO 22.

### Night Mode

When `ldrBrightness < ldrNightThreshold` and `autoDimEnabled` is true, green phase durations are scaled by 2.0x to account for reduced traffic volume at night.

## Adaptive Scheduling

Defined in `ml.h` / `ml.cpp`. After every yellow phase when `mlMode == ML_GREEDY`, `mlInfer()` runs the greedy heuristic directly in firmware — no model file required.

**Input:** current queue depths and intensity multipliers per road (8 values total)

**Algorithm:**
- Score each road: `score = queue × intensity_multiplier`
- Pick the road with the highest score
- Duration: `5 + queue[best] × (15 − 5)` seconds, clamped to [5, 15]

**Output:** which road goes green next + green duration in seconds

### Queue Simulation (`mlQueueTick()`)

Runs every `500 / simSpeed` ms, updating the `queues[4]` values:

- Green road: queue decreases by 0.03 per tick
- Red roads: queue increases by `0.015 * intensity_multiplier` per tick
- All values clamped to [0.0, 1.0]

## Web Server

Defined in `webserver.h` / `webserver.cpp`. Creates a WiFi access point and serves an async REST API plus a dashboard UI from LittleFS.

### Network

- **Mode:** Access Point
- **SSID:** `TrafficLight`
- **Password:** `12345678`
- **IP:** `192.168.4.1`

### Endpoints

| Endpoint | Method | Body | Purpose |
|---|---|---|---|
| `/` | GET | — | Serve dashboard HTML from LittleFS |
| `/api/status` | GET | — | Poll full system state (recommended interval: 100 ms) |
| `/api/override` | POST | `{"direction":"top"}` | Force one road green, or clear override |
| `/api/timing` | POST | `{"top":5000,...}` | Update per-direction green/yellow durations |
| `/api/dim` | POST | — | Toggle auto-dim night mode |
| `/api/ml` | POST | `{"mode":"normal"\|"greedy"}` | Switch inference mode |
| `/api/intensity` | POST | `{"road":"top","level":"high"}` | Set per-road arrival rate |
| `/api/speed` | POST | `{"speed":1}` | Set simulation speed multiplier (1, 2, or 5) |

### `/api/status` Response Fields

`phase`, `mode`, `emergency`, `mlMode` (string: `"normal"` / `"greedy"`), `override`, `leds`, `timings`, `queues`, `intensity`, `remaining`, `brightness`, `threshold`, `autoDim`, `nightMode`

## Build

- **Framework:** PlatformIO / Arduino for ESP32
- **Partition scheme:** `huge_app.csv` — 3 MB app partition, 960 KB LittleFS for dashboard assets

```bash
# Flash firmware
pio run -t upload

# Flash filesystem (dashboard HTML/CSS/JS)
pio run -t uploadfs
```

Both steps are required on first deployment.

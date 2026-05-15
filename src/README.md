# src/ — ESP32 Adaptive Traffic Light Firmware

Source code for an adaptive traffic light controller running on ESP32. The firmware manages a four-road intersection with hardware LED control, RFID-triggered emergency mode, ambient light sensing, a browser-based dashboard, and on-device TFLite inference for traffic optimization.

## Files

| File | Role |
|---|---|
| `main.cpp` | Entry point: SPI init, RFID polling, LDR sampling, WiFi/server setup, calls `mlInit()`, main loop |
| `traffic.h` / `traffic.cpp` | Traffic light state machine, LED control, phase logic |
| `webserver.h` / `webserver.cpp` | WiFi AP creation, LittleFS mount, async REST API server |
| `ml.h` / `ml.cpp` | TFLite inference for supervised and RL models, queue simulation |

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
- **EMERGENCY** — all roads red for 5 seconds, triggered by RFID card scan; 15-second cooldown between triggers

### Key `TrafficState` Fields

| Field | Type | Description |
|---|---|---|
| `phase` | `Phase` | Current phase enum value |
| `emergency` | `bool` | True during 5-second all-red |
| `overrideDir` | `int` | -1 = none, 0-3 = direction index |
| `timings` | struct | Configurable green/yellow durations (ms) |
| `phaseStart` | `unsigned long` | `millis()` timestamp of phase start |
| `ldrBrightness` | `int` | Raw ADC reading from LDR on GPIO 34 |
| `ldrNightThreshold` | `int` | Calibrated at boot |
| `autoDimEnabled` | `bool` | Whether night mode scaling is active |
| `mlMode` | `MLMode` | `ML_NORMAL`, `ML_GREEDY`, or `ML_RL` |
| `queues[4]` | `float` | Normalized queue depth per road [0.0, 1.0] |
| `intensity[4]` | `int` | Arrival rate per road: 0=low, 1=med, 2=high |
| `mlDuration` | `int` | Green phase duration from last ML inference (ms) |

### LED Control

- **Red and yellow LEDs:** 74HC595N shift register (DATA=27, CLOCK=26, LATCH=25). Q0-Q3 drive red; Q4-Q7 drive yellow.
- **Green LEDs:** Direct GPIO — top=2, bottom=16, left=17, right=21.
- **Emergency LED:** Direct GPIO 22.

### Night Mode

When `ldrBrightness < ldrNightThreshold` and `autoDimEnabled` is true, green phase durations are scaled by 2.0x to account for reduced traffic volume at night.

## ML Inference

Defined in `ml.h` / `ml.cpp`. Two TFLite models are loaded from LittleFS at boot via `mlInit()`.

### Models

| File | Type | Size | Fallback |
|---|---|---|---|
| `traffic_model.tflite` | Supervised / greedy | ~4.7 KB | Required — `mlInit()` returns false if missing |
| `traffic_rl_model.tflite` | Reinforcement learning (PPO) | ~4.8 KB | Optional — supervised mode still works |

### Input / Output

Both models share the same interface:

- **Input:** 8 floats — `[q_top, q_bottom, q_left, q_right, i_top, i_bottom, i_left, i_right]`
- **Output:** road index (argmax of softmax) + green duration in seconds, clamped to [5, 15]

Inference fires at every yellow-to-green transition when `mlMode` is not `ML_NORMAL`.

### Queue Simulation (`mlQueueTick()`)

Runs every 500 ms regardless of active mode, updating the `queues[4]` values:

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
| `/api/ml` | POST | `{"mode":"normal"\|"greedy"\|"rl"}` | Switch inference mode |
| `/api/intensity` | POST | `{"road":"top","level":"high"}` | Set per-road arrival rate |

### `/api/status` Response Fields

`phase`, `mode`, `emergency`, `mlMode` (string: `"normal"` / `"greedy"` / `"rl"`), `override`, `leds`, `timings`, `queues`, `intensity`, `remaining`, `brightness`, `threshold`, `autoDim`, `nightMode`

## Build

- **Framework:** PlatformIO / Arduino for ESP32
- **Partition scheme:** `huge_app.csv` — 3 MB app partition, 960 KB LittleFS for TFLite models and dashboard assets

```bash
# Flash firmware
pio run -t upload

# Flash filesystem (dashboard HTML + TFLite models)
pio run -t uploadfs
```

Both steps are required on first deployment. The firmware will fail to initialize ML inference if the models are not present on LittleFS.

# Traffic Light API Reference

## Connect

1. On your phone/laptop, connect to WiFi: **TrafficLight** / **12345678**
2. Open browser to: **http://192.168.4.1**

---

## Endpoints

### GET /api/status
Returns the current system state. Poll this every 100ms.

**Response:**
```json
{
  "phase": "top_green",
  "mode": "normal",
  "emergency": false,
  "mlMode": "normal",
  "override": null,
  "leds": {
    "top": "green",
    "bottom": "red",
    "left": "red",
    "right": "red"
  },
  "timings": {
    "top": 5000,
    "bottom": 5000,
    "left": 5000,
    "right": 5000,
    "yellow": 2000
  },
  "queues": {
    "top": 0.0,
    "bottom": 0.15,
    "left": 0.15,
    "right": 0.15
  },
  "intensity": {
    "top": "med",
    "bottom": "med",
    "left": "med",
    "right": "med"
  },
  "remaining": 2400,
  "brightness": 75,
  "threshold": 75,
  "autoDim": true,
  "nightMode": false,
  "simSpeed": 1
}
```

| Field | Values |
|---|---|
| `phase` | `top_green`, `top_yellow`, `bottom_green`, `bottom_yellow`, `left_green`, `left_yellow`, `right_green`, `right_yellow`, `override`, `emergency` |
| `mode` | `"normal"`, `"greedy"`, `"override"`, or `"emergency"` — active control mode |
| `emergency` | `true` while RFID emergency is active (auto-clears after 5s) |
| `mlMode` | Active scheduling mode: `"normal"` (round-robin) or `"greedy"` (adaptive heuristic) |
| `override` | `"top"`, `"bottom"`, `"left"`, `"right"`, or `null` |
| `leds.X` | `"green"`, `"yellow"`, or `"red"` |
| `timings` | Green phase durations in ms per direction, yellow duration shared |
| `queues.X` | Simulated queue depth per road, 0.0–1.0 (updated every 500 ms) |
| `intensity.X` | Arrival rate for each road: `"low"`, `"med"`, or `"high"` |
| `remaining` | ms left in current phase |
| `brightness` | LDR ambient brightness 0–100 (higher = brighter) |
| `threshold` | Night threshold — calibrated from the first LDR reading at boot |
| `autoDim` | `true` when auto-dim is enabled |
| `nightMode` | `true` when autoDim is on and brightness is below the threshold |
| `simSpeed` | Simulation speed multiplier: `1` (real-time), `2` (×2), or `5` (×5) |

---

### POST /api/override
Force one direction to green, or clear the override.

**Body:**
```json
{ "direction": "top" }
```

| direction | Effect |
|---|---|
| `"top"` | Top light goes green, others red |
| `"bottom"` | Bottom light goes green, others red |
| `"left"` | Left light goes green, others red |
| `"right"` | Right light goes green, others red |
| `"none"` | Clear override, resume normal cycle |

**Response:** `{ "ok": true }`

---

### POST /api/timing
Update how long each green phase lasts. All values in milliseconds. You can send just the keys you want to change.

**Body:**
```json
{ "top": 5000, "bottom": 5000, "left": 5000, "right": 5000, "yellow": 2000 }
```

**Response:** `{ "ok": true }`

---

### POST /api/dim
Toggle auto-dim on or off. When auto-dim is active and brightness drops below the boot-calibrated threshold, green phase durations are doubled to give traffic more time at night.

**Body:** _(none required)_

**Response:** `{ "ok": true, "autoDim": true }`

---

### POST /api/ml
Set the active control mode. When set to `greedy`, the adaptive scheduling logic selects which road goes green and for how long after each yellow phase, based on simulated queue depths and per-road arrival intensity. Takes effect at the next yellow-to-green transition.

**Body:**
```json
{ "mode": "greedy" }
```

| `mode` | Effect |
|---|---|
| `"normal"` | Round-robin cycle using configured durations |
| `"greedy"` | Adaptive heuristic selects road and duration |

**Response:** `{ "ok": true, "mlMode": "greedy" }`

---

### POST /api/intensity
Set the simulated arrival intensity for one road. This controls how fast that road's queue accumulates while it is red, which in turn influences Adaptive mode's road selection.

**Body:**
```json
{ "road": "top", "level": "high" }
```

| Field | Values |
|---|---|
| `road` | `"top"`, `"bottom"`, `"left"`, `"right"` |
| `level` | `"low"`, `"med"`, `"high"` |

**Response:** `{ "ok": true }`

---

### POST /api/speed
Set the simulation speed multiplier. This scales the internal tick rate and phase durations so the cycle can be observed faster than real time.

**Body:**
```json
{ "speed": 2 }
```

| Field | Values |
|---|---|
| `speed` | `1` (×1, 500 ms/tick), `2` (×2, 250 ms/tick), or `5` (×5, 100 ms/tick). Any other value is clamped to `1`. |

**Response:** `{ "ok": true }`

---

## Upload Your Dashboard Changes

After editing `dashboard.html`, upload it to the ESP32 with:

```
pio run -t uploadfs
```

You don't need to re-flash the firmware — just the filesystem upload.

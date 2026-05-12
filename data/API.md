# Traffic Light API Reference

## Connect

1. On your phone/laptop, connect to WiFi: **TrafficLight** / **12345678**
2. Open browser to: **http://192.168.4.1**

---

## Endpoints

### GET /api/status
Returns the current system state. Poll this every 500ms.

**Response:**
```json
{
  "phase": "top_green",
  "emergency": false,
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
  "remaining": 2400
}
```

| Field | Values |
|---|---|
| `phase` | `top_green`, `top_yellow`, `bottom_green`, `bottom_yellow`, `left_green`, `left_yellow`, `right_green`, `right_yellow`, `override`, `emergency` |
| `emergency` | `true` while RFID emergency is active (auto-clears after 5s) |
| `override` | `"top"`, `"bottom"`, `"left"`, `"right"`, or `null` |
| `leds.X` | `"green"`, `"yellow"`, or `"red"` |
| `timings` | Green phase durations in ms per direction, yellow duration shared |
| `remaining` | ms left in current phase |

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

## Upload Your Dashboard Changes

After editing `dashboard.html`, upload it to the ESP32 with:

```
pio run -t uploadfs
```

You don't need to re-flash the firmware — just the filesystem upload.

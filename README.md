# Adaptive Traffic Light System

ESP32-based 4-way traffic light controller with a web dashboard for real-time admin controls.

---

## How It Works

### The big picture

The ESP32 runs one infinite loop. Every iteration it does three things:

1. Check if an RFID card is present
2. Check the ambient light sensor
3. Advance the traffic light (which also updates queue simulation internally)

That's it. No operating system, no threads, no scheduler — just a tight loop running hundreds of times per second.

---

### Normal operation — the phase cycle

There are four roads: Top, Bottom, Left, Right. Only one road is green at a time. All others are red. The cycle steps through each road in order:

```
Top green → Top yellow → Bottom green → Bottom yellow →
Left green → Left yellow → Right green → Right yellow → (repeat)
```

Each green phase lasts **5 seconds** by default. Each yellow phase lasts **2 seconds**. Both are configurable in the traffic code.

**How the timer works:**
When a phase starts, the ESP32 records the current time (`millis()` — milliseconds since boot). On every loop iteration it checks:

```
time elapsed = now - phase start time
```

If that gap exceeds the phase duration, it moves to the next phase and stamps the time again. There is no hardware timer or interrupt — just a subtraction checked on every loop.

---

### Interruption 1 — Manual override (web dashboard)

From the dashboard, an operator can force any one road to stay green indefinitely. The cycle **pauses** and holds that road green until the operator clears the override. When cleared, the cycle resets back to the top (Top green).

---

### Interruption 2 — Emergency mode (RFID card scan) 

Scanning any RFID card near the RC522 reader simulates an emergency vehicle approaching. The system immediately:

1. Sets **all four roads to red**
2. Lights the dedicated **emergency LED**
3. Holds for **5 seconds**
4. Resets back to normal cycle (Top green)

A **15-second cooldown** prevents a single card from re-triggering back-to-back emergencies.

---

### The light sensor (LDR) — night mode

At boot, the ESP32 reads the ambient brightness from the LDR and saves it as the **night threshold**. From that point on:

- If current brightness drops **below** the threshold → **night mode** activates
- In night mode, all green phase durations are **doubled** (e.g. 5 s → 10 s), giving traffic more time to clear in low-visibility conditions
- Night mode can be toggled on or off from the web dashboard at any time

The LDR brightness (0–100%) is printed to Serial every second for monitoring.

---

### Interruption 3 — Adaptive mode

The dashboard's Mode Control panel lets you switch between two modes at runtime:

- **Normal** — fixed round-robin cycle (Top → Bottom → Left → Right), each road using its configured green duration. Baseline: ~30% avg cars waiting.
- **Adaptive** — greedy heuristic implemented directly in firmware. Selects the road with the highest `queue × intensity` score and sets a proportional green duration (5–15 seconds). ~24% avg cars waiting on imbalanced traffic. No model file required.

The greedy formula runs at the end of every yellow phase:
```
scores[i]  = queues[i] × intensity_multiplier[i]   → pick argmax
duration   = MIN_GREEN + queues[best] × (MAX_GREEN − MIN_GREEN)   → clamped to [5, 15] s
```

**Queue simulation** runs every 500 ms regardless of mode:
- The currently-green road drains at a fixed rate
- All red roads accumulate based on their configured arrival intensity (low / med / high)
- Queue values stay in [0, 1]

Switching modes takes effect at the next yellow phase with no cycle reset. The `mode` field in the API reports `"normal"`, `"greedy"`, `"override"`, or `"emergency"`.

**Why not a trained model?**

Two ML approaches were prototyped before settling on the direct formula:

1. **Supervised neural network (8→16→8, TFLite)** — trained to imitate the greedy formula. Achieved perfect accuracy on training data, but this just proved it had learned to replicate two lines of arithmetic. Deploying a 4.7 KB neural net to reproduce `argmax(queue × intensity)` added complexity with no behavioural difference. Replaced by the formula itself.

2. **Reinforcement learning (PPO, 8→32→16)** — trained with a delta-queue reward signal across three curriculum stages. The agent learned a degenerate policy: it found it could maximise cumulative reward by permanently starving one low-intensity road (which accumulates slowly and barely hurts the reward), rather than managing all four roads fairly. Fixing this requires more elaborate reward shaping (fairness penalties, starvation timeouts) which would have introduced as much arbitrary tuning as the heuristic it was meant to replace.

The greedy formula is simpler, fully deterministic, and performs within the same range as both models on the benchmarks that matter.

---

### The web dashboard

The ESP32 hosts its own WiFi hotspot — no router or internet connection needed. Connect to it and open a browser to reach the dashboard.

The dashboard polls the ESP32 every **100 ms** for live state and lets you:

- See the current color of each road in real time
- See how much time is left in the current phase
- Force any road green (override, keeps it green until user turns it off, then traffic returns to cycle)
- Release an override and return to normal cycle
- Change how long each road stays green or yellow
- See ambient brightness and toggle night mode
- Switch between Normal and Adaptive modes and set per-road arrival intensity

---

### LED hardware — why a shift register?

Driving 13 LEDs (4 red + 4 yellow + 4 green + 1 emergency) directly from GPIO would require 13 pins. The ESP32 doesn't have that many to spare after SPI and other functions are assigned.

The solution: a **74HC595N shift register** handles all 8 red and yellow LEDs using only **3 GPIO pins** (data, clock, latch). The 4 green LEDs and 1 emergency LED are driven directly from GPIO because they don't need to be multiplexed — only one green is ever on at a time.

---

## Hardware

| Component | Purpose |
|---|---|
| ESP32 | Microcontroller |
| RFID-RC522 | Emergency vehicle preemption |
| 74HC595N shift register | Drives 8 LEDs (4× red, 4× yellow) using 3 GPIO pins |
| 4× Green LEDs | Direct GPIO, one per road |
| Emergency red LED | Indicates active emergency state |
| LDR | Ambient light sensor for night mode |

The adaptive heuristic runs directly in firmware — no model file or additional hardware is required.

Full pin mapping: [`traffic light pins.md`](traffic%20light%20pins.md)

---

## Project Structure

```
src/
  main.cpp          — entry point; setup, loop, RFID polling, LDR sampling
  traffic.h/.cpp    — traffic state machine and LED output driver
  webserver.h/.cpp  — WiFi access point, async HTTP server, REST API
  ml.h/.cpp         — queue simulation and adaptive greedy heuristic
data/
  dashboard.html    — web dashboard served from ESP32 flash filesystem
  style.css         — dashboard styles
  script.js         — dashboard polling loop and API helpers
API.md              — REST API reference
traffic light pins.md — full hardware wiring reference
```

---

## Building & Flashing

### Requirements

- [PlatformIO](https://platformio.org/)

### First-time setup

```bash
# Flash firmware to ESP32
pio run -t upload

# Upload dashboard files to ESP32 filesystem
pio run -t uploadfs
```

Both steps are required on first flash. After that:

```bash
# Only the dashboard changed
pio run -t uploadfs

# Only firmware (src/) changed
pio run -t upload
```

---

## Usage

1. Power the ESP32 via USB
2. Connect to WiFi: **TrafficLight** / **12345678**
3. Open a browser to **http://192.168.4.1**

---

## Dashboard Development

The dashboard is split across three files in `data/`: `dashboard.html`, `style.css`, and `script.js`. The polling loop and all API helper functions are in `script.js` — add your UI logic inside `onStateUpdate(s)` in `script.js`.

The `state` object available in `onStateUpdate` has this shape:

```js
{
    phase:     "top_green" | "top_yellow" | "bottom_green" | "bottom_yellow" |
               "left_green" | "left_yellow" | "right_green" | "right_yellow" |
               "override" | "emergency",
    mode:      "normal" | "greedy" | "override" | "emergency",
    emergency: true | false,
    mlMode:    "normal" | "greedy",
    override:  "top" | "bottom" | "left" | "right" | null,
    leds: {
        top:    "green" | "yellow" | "red",
        bottom: "green" | "yellow" | "red",
        left:   "green" | "yellow" | "red",
        right:  "green" | "yellow" | "red"
    },
    timings:   { top: 5000, bottom: 5000, left: 5000, right: 5000, yellow: 2000 },
    queues: {
        top:    0.0,   // normalised queue depth 0–1
        bottom: 0.15,
        left:   0.15,
        right:  0.15
    },
    intensity: {
        top:    "low" | "med" | "high",   // arrival rate per road
        bottom: "low" | "med" | "high",
        left:   "low" | "med" | "high",
        right:  "low" | "med" | "high"
    },
    remaining: 2400,    // ms left in current phase
    brightness: 75,     // LDR ambient brightness 0–100
    threshold:  75,     // night threshold set at boot
    autoDim:    true,   // whether auto-dim is enabled
    nightMode:  false,  // true when autoDim && brightness < threshold
    simSpeed:   1       // simulation speed multiplier: 1, 2, or 5
}
```

See [`API.md`](API.md) for the full REST API reference.

---

## License

MIT

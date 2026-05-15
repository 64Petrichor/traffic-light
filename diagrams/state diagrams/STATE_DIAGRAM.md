# Traffic Light System — State Diagram

> Run `python export_diagrams.py` to regenerate the PNG images from the `.mmd` source files in `diagrams/`.

---

## 1. System Overview

![System overview](diagrams/overview.png)

The system has three top-level operating modes:

- **Cycle** — normal operation. Internally switches between Fixed Timing and ML Control via `POST /api/ml`.
- **Override** — cycle suspended; one road held green indefinitely via `POST /api/override`.
- **Emergency** — all roads red for exactly 5 seconds, triggered by an RFID scan. Highest priority; interrupts both Cycle and Override.

After Emergency ends the system always returns to Cycle (resuming whichever timing mode was active before).

---

## 2. Fixed Timing Cycle (internal)

![Fixed timing cycle](diagrams/fixed_cycle.png)

A strict round-robin: Top → Bottom → Left → Right → Top. Each green phase lasts `timings.<road>` milliseconds (configurable via `POST /api/timing`). Each yellow phase always lasts `timings.yellow` (default 2 000 ms). In **night mode** (LDR below boot-calibrated threshold and `autoDimEnabled` true), green durations are doubled by `LDR_NIGHT_MULTIPLIER` (2.0); yellow is unaffected.

---

## 3. ML Control Cycle (internal)

![ML control cycle](diagrams/ml_cycle.png)

Instead of a fixed sequence, after every yellow phase `mlInfer()` is called with an 8-element input tensor — four queue levels (0–1) and four intensity multipliers (0.3 / 0.6 / 1.0, one per road). The model outputs road probabilities (4 values) and a green duration (1 value). The road with the highest probability goes green next; the duration is clamped to 5–15 seconds. The queue values are kept live by `mlQueueTick`, which runs every 500 ms regardless of the current mode or phase.

---

## State Descriptions

### FixedTiming (inside Cycle)
The four roads cycle Top → Bottom → Left → Right in order. Green durations come from `traffic.timings`; yellow is always `timings.yellow`. Night mode doubles green durations only.

### MLControl (inside Cycle)
Same physical light sequence (one road green at a time) but the next road and its green duration are chosen by `mlInfer()` at every yellow expiry. The RL-trained `traffic_rl_model.tflite` (PPO, 8→32→16) outperforms the supervised `traffic_model.tflite` (8→16→8) on imbalanced traffic.

### Override
`trafficUpdate()` returns immediately when `overrideDir >= 0`, so the cycle is fully paused. All shift-register LEDs go all-red; the selected road's direct-drive green GPIO is held HIGH. `mlQueueTick` still runs, keeping queue values live. Persists until cleared by API. Emergency preemption still works.

### Emergency
`traffic.emergency = true`, all LEDs all-red, GPIO 22 (emergency indicator) HIGH. Evaluated first in `trafficUpdate()` — highest priority. A 15-second RFID cooldown (`RFID_COOLDOWN_MS`) prevents immediate re-trigger. Cannot be cleared via API; only the 5-second timer clears it.

---

## Transition Descriptions

### Power-on → Cycle (FixedTiming)
`trafficInit()` called from `setup()`. Configures all GPIO pins, sets `phase = TOP_GREEN`, `phaseStart = millis()`, drives Top-green LEDs. Always starts in FixedTiming regardless of prior state.

### Green → Yellow (any road, any mode)
`millis() - phaseStart >= phaseDuration(phase)` in `trafficUpdate()`. Duration is `timings.<road>` (fixed) or `mlDuration` (ML), with night-mode doubling for fixed only. `applyPhase(YELLOW)` switches LEDs.

### Yellow → next Green (FixedTiming)
Yellow timer (`timings.yellow`) expires with `mlMode = false`. `nextPhase()` returns the next road in the round-robin.

### Yellow → next Green (MLControl)
Yellow timer expires with `mlMode = true`. `mlInfer()` picks the road and duration. `roadToGreen(road)` maps the model's choice (0–3) to the matching green phase.

### Cycle → Override
`POST /api/override` with a direction field. `trafficSetOverride(idx)` sets `overrideDir`, switches phase to OVERRIDE, drives LEDs.

### Override → Cycle
`POST /api/override` with no direction (null). `trafficSetOverride(-1)` resets `overrideDir`, resumes from TOP_GREEN.

### Any → Emergency
`checkRFID()` detects a card, `emergency` is false, and the 15-second cooldown has elapsed. `trafficTriggerEmergency()` sets `emergency = true`, drives all-red, pulls GPIO 22 HIGH.

### Emergency → Cycle
`now - phaseStart >= 5000` inside `trafficUpdate()`. Clears `emergency`, drives GPIO 22 LOW, resets to TOP_GREEN.

### FixedTiming ↔ MLControl
`POST /api/ml` toggles `traffic.mlMode`. No immediate phase change; the flag is read at the next yellow expiry and inside `phaseDuration()`.

---

## Supporting Mechanisms

### ML Queue Simulation (`mlQueueTick`)
Runs every 500 ms unconditionally (including during Override and Emergency). Active green road drains by `ML_DRAIN_RATE` (0.03) per tick; all others accumulate at `ML_ACCUM_BASE` (0.015) × intensity multiplier. Values clamped to [0.0, 1.0].

### Night Mode (LDR)
`checkLDR()` samples GPIO 34 once per second. At boot, `ldrNightThreshold` is set to the ambient reading. When `autoDimEnabled` is true and brightness falls below threshold, fixed green durations are doubled. Toggle via `POST /api/dim`.

### Runtime Updates
- `POST /api/timing` — updates any green/yellow duration; takes effect at next phase transition.
- `POST /api/intensity` — sets road intensity (0/1/2); affects queue accumulation and next ML input immediately.

# State Diagrams — Adaptive Traffic Light System

## 1. Overview

The firmware implements a finite state machine (FSM) for a 4-road intersection controller running on the ESP32. The FSM has **10 distinct states** drawn from the `Phase` enum:

| Category | States |
|----------|--------|
| Normal cycle | `TOP_GREEN`, `TOP_YELLOW`, `BOTTOM_GREEN`, `BOTTOM_YELLOW`, `LEFT_GREEN`, `LEFT_YELLOW`, `RIGHT_GREEN`, `RIGHT_YELLOW` |
| Interrupt / special | `EMERGENCY`, `OVERRIDE` |

At any given moment, exactly one road has a green or yellow signal while the remaining three show red. Two special states (`EMERGENCY` and `OVERRIDE`) can interrupt the cycle from **any** of the eight normal states and bypass the regular sequencing until their exit condition is met.

The full picture is in [`scripts/overview.mmd`](scripts/overview.mmd).

---

## 2. Normal Cycle

**Diagram:** [`scripts/normal_cycle.mmd`](scripts/normal_cycle.mmd)

The controller cycles through all four roads in a fixed sequence:

```
TOP_GREEN → TOP_YELLOW → BOTTOM_GREEN → BOTTOM_YELLOW
         → LEFT_GREEN  → LEFT_YELLOW  → RIGHT_GREEN
         → RIGHT_YELLOW → TOP_GREEN (repeat)
```

### Timing

| Transition | Duration |
|------------|----------|
| Green → Yellow | Default **5 s**, configurable at runtime |
| Yellow → next Green | Fixed **2 s** |

### Night Mode Modifier

When the onboard LDR (light-dependent resistor) reading falls below the configured threshold, the controller enters night mode. Night mode **doubles the green duration** (e.g. 5 s → 10 s). This is not a separate state — the FSM stays in the same green state and simply uses a longer timer. The yellow-phase duration (2 s) is unaffected.

---

## 3. Adaptive / Greedy Mode

**Diagram:** [`scripts/adaptive_cycle.mmd`](scripts/adaptive_cycle.mmd)

When the operating mode is set to **Adaptive**, the green-phase duration is no longer fixed. Instead, `mlInfer()` is called at each green state to determine how long that road should stay green based on current queue estimates.

### mlInfer() Decision Logic

`mlInfer()` selects the road with the **highest queue value** and returns a green duration between **5 s and 15 s** proportional to that queue.

### Queue Dynamics

The queue model runs on a 500 ms tick:

| Road State | Queue Update per Tick |
|------------|----------------------|
| Currently green | `queue -= 0.03` (vehicles clearing) |
| Currently red | `queue += 0.015 × intensity` (vehicles accumulating) |

### Intensity Levels

Traffic intensity is reported by each road's sensor and mapped to one of three discrete values:

| Value | Level | Meaning |
|-------|-------|---------|
| `0` | Low | Light or no traffic |
| `1` | Medium | Moderate traffic |
| `2` | High | Heavy congestion |

Yellow-phase duration (2 s) remains fixed regardless of mode.

---

## 4. Interrupts and Special States

**Diagram:** [`scripts/overview.mmd`](scripts/overview.mmd)

### EMERGENCY

| Property | Detail |
|----------|--------|
| Trigger | RFID card scan **or** HTTP API call |
| Entry | Immediate — from any of the 8 normal states |
| Behavior | All four roads show **RED** for the full duration |
| Duration | **5 seconds** (fixed) |
| Exit | Automatically returns to `TOP_GREEN` after 5 s |
| Cooldown | **15 s cooldown** after exit — any further RFID/API trigger is ignored until the cooldown expires, preventing ambulance re-trigger or accidental double activation |

### OVERRIDE

| Property | Detail |
|----------|--------|
| Trigger | HTTP API call `setOverride(direction)` |
| Entry | Immediate — from any of the 8 normal states |
| Behavior | The specified direction is held **green indefinitely**; all other roads show red |
| Duration | **Indefinite** — no automatic timeout |
| Exit | HTTP API call `clearOverride()` — returns to `TOP_GREEN` |
| Use case | Manual control by traffic operator (e.g. clearing an accident, VIP convoy) |

---

## 5. Phase Enum Reference

| Phase Value | Description | Typical Duration |
|-------------|-------------|-----------------|
| `TOP_GREEN` | Top road: green signal | 5 s (normal) / 5–15 s (adaptive) |
| `TOP_YELLOW` | Top road: yellow / clearing | 2 s |
| `BOTTOM_GREEN` | Bottom road: green signal | 5 s (normal) / 5–15 s (adaptive) |
| `BOTTOM_YELLOW` | Bottom road: yellow / clearing | 2 s |
| `LEFT_GREEN` | Left road: green signal | 5 s (normal) / 5–15 s (adaptive) |
| `LEFT_YELLOW` | Left road: yellow / clearing | 2 s |
| `RIGHT_GREEN` | Right road: green signal | 5 s (normal) / 5–15 s (adaptive) |
| `RIGHT_YELLOW` | Right road: yellow / clearing | 2 s |
| `EMERGENCY` | All-red emergency hold | 5 s (fixed) |
| `OVERRIDE` | Manual direction hold | Indefinite (API-cleared) |

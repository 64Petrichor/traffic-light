#include "ml.h"
#include "traffic.h"

// Intensity multipliers: how fast a red road's queue grows per tick.
// 0=low (0.3×), 1=med (0.6×), 2=high (1.0×) — set per road from the dashboard.
const float ML_INTENSITY_MUL[3] = {0.3f, 0.6f, 1.0f};

// Called every loop() iteration. Updates each road's simulated queue level on a 500 ms timer
// (scaled by simSpeed). The queue is a float in [0, 1] representing how backed-up that road is.
//
// Queue dynamics (per tick):
//   Green road  → queue -= ML_DRAIN_RATE (0.03)  — cars are moving, queue shrinks
//   Red roads   → queue += ML_ACCUM_BASE (0.015) × intensity_multiplier — cars are waiting
//
// During yellow phases greenRoad == -1, so all four queues accumulate (nobody is moving).
void mlQueueTick(uint32_t now) {
    // Rate-limit updates to ML_TICK_MS intervals, scaled by simSpeed so the
    // simulation runs faster without changing the underlying tick logic.
    static uint32_t lastTick = 0;
    if (now - lastTick < (uint32_t)(ML_TICK_MS / traffic.simSpeed)) return;
    lastTick = now;

    // Determine which road (if any) is currently green.
    int greenRoad = -1;
    if (traffic.overrideDir >= 0) {
        greenRoad = traffic.overrideDir;  // manual override holds one road green
    } else {
        switch (traffic.phase) {
            case TOP_GREEN:    greenRoad = 0; break;
            case BOTTOM_GREEN: greenRoad = 1; break;
            case LEFT_GREEN:   greenRoad = 2; break;
            case RIGHT_GREEN:  greenRoad = 3; break;
            default: break;  // yellow/emergency/override: no road is actively clearing
        }
    }

    // Update all four queue levels.
    for (int i = 0; i < 4; i++) {
        if (i == greenRoad)
            // This road has a green light: drain at a fixed rate (cars clearing the junction).
            traffic.queues[i] -= ML_DRAIN_RATE;
        else
            // This road is red: accumulate traffic proportional to its arrival intensity.
            traffic.queues[i] += ML_ACCUM_BASE * ML_INTENSITY_MUL[traffic.intensity[i]];
        // Clamp to valid range so the float never goes negative or above 1.
        traffic.queues[i] = constrain(traffic.queues[i], 0.0f, 1.0f);
    }
}

// Greedy inference: picks the road with the most urgent need for a green phase.
//
// Score formula:  score[i] = queue[i]
//   — Uses current queue depth directly. Intensity already shapes how fast queues
//     grow in mlQueueTick(), so multiplying it in here would double-count it and
//     cause a high-intensity road to outrank a road with more cars.
//
// Green duration formula:  dur = MIN_GREEN + queue[best] × (MAX_GREEN - MIN_GREEN)
//   — Scales the green phase length between 5 s and 15 s proportionally to how long
//     the winning road's queue is. A nearly-empty queue gets a short green; a full
//     queue gets the maximum green time.
//
// Always returns true (model is always "loaded" — it's a simple formula, not a TFLite model).
bool mlInfer(int* road, uint32_t* durationMs) {
    // Compute urgency score for each road: simply the current queue depth.
    // (Intensity is intentionally excluded here — it already drives accumulation in mlQueueTick.)
    float scores[4];
    for (int i = 0; i < 4; i++)
        scores[i] = traffic.queues[i];

    // Find the road with the highest score.
    int best = 0;
    for (int i = 1; i < 4; i++) if (scores[i] > scores[best]) best = i;
    *road = best;

    // Compute green duration proportional to that road's queue depth.
    float dur = ML_MIN_GREEN_S + traffic.queues[best] * (ML_MAX_GREEN_S - ML_MIN_GREEN_S);
    *durationMs = (uint32_t)(constrain(dur, ML_MIN_GREEN_S, ML_MAX_GREEN_S) * 1000.0f);

    Serial.printf("[GREEDY] road=%d dur=%.1fs q=[%.2f %.2f %.2f %.2f]\n",
                  best, dur,
                  traffic.queues[0], traffic.queues[1],
                  traffic.queues[2], traffic.queues[3]);
    return true;
}

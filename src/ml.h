/*
 * ml.h — Simulated queue model and greedy scheduling for the traffic light.
 *
 * Each road has a "queue" value in [0, 1] that represents how backed-up it is.
 * mlQueueTick() updates these queues every 500 ms based on which road is green.
 * mlInfer() uses the queues to pick the most congested road and compute a
 * proportional green duration (the "greedy" algorithm).
 *
 * There is no neural network here — "ML" refers to the adaptive decision logic
 * that replaces the fixed round-robin schedule in greedy mode.
 */

#pragma once
#include <Arduino.h>

// ── Queue simulation constants ────────────────────────────────────────────────

// How much the green road's queue shrinks per 500 ms tick (cars moving through).
#define ML_DRAIN_RATE    0.03f

// Base accumulation per tick for a red road. Multiplied by the road's intensity level.
// Example: medium intensity → 0.015 × 0.6 = 0.009 queue added per tick.
#define ML_ACCUM_BASE    0.015f

// How often (ms) the queue simulation updates. Scaled by simSpeed at runtime.
#define ML_TICK_MS       500

// ── Green phase duration bounds ───────────────────────────────────────────────
// mlInfer() returns a value in this range, proportional to the winning road's queue.
#define ML_MIN_GREEN_S   5.0f    // shortest possible green phase (seconds)
#define ML_MAX_GREEN_S   15.0f   // longest possible green phase (seconds)
#define ML_MIN_GREEN_MS  5000UL  // same in milliseconds
#define ML_MAX_GREEN_MS  15000UL

// Multipliers for the three traffic intensity levels (low=0, med=1, high=2).
// A "high" road accumulates queue 1.0× base; "low" only 0.3× base.
extern const float ML_INTENSITY_MUL[3];

// ── Public API ────────────────────────────────────────────────────────────────

// Updates all four queue floats on a 500 ms timer. Always call from trafficUpdate()
// regardless of mode — keeps queues live even during override/emergency.
void mlQueueTick(uint32_t now);

// Runs one greedy inference using current queues and intensities.
// Outputs: *road (0–3 = top/bottom/left/right), *durationMs (green phase length).
// Always returns true — the "model" is a formula, not a TFLite binary.
bool mlInfer(int* road, uint32_t* durationMs);

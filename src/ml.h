#pragma once
#include <Arduino.h>

// Queue dynamics — match notebook constants exactly (per 500 ms tick)
#define ML_DRAIN_RATE    0.03f   // green road loses this per tick
#define ML_ACCUM_BASE    0.015f  // red roads gain this × intensity per tick
#define ML_TICK_MS       500

// Green phase duration bounds (seconds and ms)
#define ML_MIN_GREEN_S   5.0f
#define ML_MAX_GREEN_S   15.0f
#define ML_MIN_GREEN_MS  5000UL
#define ML_MAX_GREEN_MS  15000UL

// Intensity multipliers indexed by traffic.intensity[i]: 0=low, 1=med, 2=high
extern const float ML_INTENSITY_MUL[3];

// Loads traffic_model.tflite from LittleFS and allocates the interpreter.
// Call once from setup(), after webserverInit() (which mounts LittleFS).
bool mlInit();

// Updates all four queue floats on a 500 ms timer. Always call from trafficUpdate()
// regardless of mode — keeps queues live even during override/emergency.
void mlQueueTick(uint32_t now);

// Runs one TFLite inference using current queues and intensities.
// Sets *road (0–3) and *durationMs. Returns false if model is not loaded.
bool mlInfer(int* road, uint32_t* durationMs);

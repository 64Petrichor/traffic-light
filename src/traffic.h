/*
 * traffic.h — Public API for the traffic light state machine.
 *
 * Defines all shared types (Phase, Timings, TrafficState), pin constants,
 * and the extern TrafficState that the rest of the firmware reads and writes.
 * Implementation is in traffic.cpp.
 */

#pragma once
#include <Arduino.h>

// ── Shift register pins (74HC595N) ───────────────────────────────────────────
#define DATA_PIN   27   // Serial data in (DS / SER)
#define CLOCK_PIN  26   // Shift clock (SHCP / SRCLK)
#define LATCH_PIN  25   // Storage clock / latch (STCP / RCLK)

// ── Green LED pins (driven directly from GPIO, one per road) ─────────────────
#define GREEN_TOP    2
#define GREEN_BOTTOM 16
#define GREEN_LEFT   17
#define GREEN_RIGHT  21

// ── Emergency LED (all-red indicator, active during emergency phase) ──────────
#define EMERG_PIN 22

// ── Night mode multiplier ─────────────────────────────────────────────────────
// Green phase durations are multiplied by this value when night mode is active.
#define LDR_NIGHT_MULTIPLIER 2.0f

// ── Shift register bit positions ──────────────────────────────────────────────
// Q0–Q3 drive the four red LEDs; Q4–Q7 drive the four yellow LEDs.
#define BIT_RED_TOP    0
#define BIT_RED_BOTTOM 1
#define BIT_RED_LEFT   2
#define BIT_RED_RIGHT  3
#define BIT_YEL_TOP    4
#define BIT_YEL_BOTTOM 5
#define BIT_YEL_LEFT   6
#define BIT_YEL_RIGHT  7

// All possible states the traffic light can be in.
// The normal sequence cycles through the GREEN→YELLOW pairs in order.
// OVERRIDE holds indefinitely until cleared. EMERGENCY lasts 5 seconds.
enum Phase {
    TOP_GREEN, TOP_YELLOW,
    BOTTOM_GREEN, BOTTOM_YELLOW,
    LEFT_GREEN, LEFT_YELLOW,
    RIGHT_GREEN, RIGHT_YELLOW,
    OVERRIDE,
    EMERGENCY
};

// Configurable phase durations in milliseconds.
// All values can be updated at runtime via the web dashboard.
struct Timings {
    uint32_t top    = 5000;   // green duration for the Top road
    uint32_t bottom = 5000;   // green duration for the Bottom road
    uint32_t left   = 5000;   // green duration for the Left road
    uint32_t right  = 5000;   // green duration for the Right road
    uint32_t yellow = 2000;   // yellow duration shared by all roads
};

// Complete runtime state of the traffic light system.
// This single global struct is read by the web server to build API responses
// and written by both the traffic logic and the web server request handlers.
struct TrafficState {
    Phase    phase        = TOP_GREEN;  // current phase in the cycle
    bool     emergency    = false;      // true while the 5-second all-red is active
    int      overrideDir  = -1;         // -1 = no override; 0=top, 1=bottom, 2=left, 3=right
    Timings  timings;                   // current phase duration settings
    uint32_t phaseStart   = 0;          // millis() timestamp when the current phase began

    int      ldrBrightness     = 100;  // most recent LDR reading mapped to 0–100%
    int      ldrNightThreshold = 30;   // brightness level below which night mode activates
    bool     autoDimEnabled    = true;  // whether night mode (green duration doubling) is on

    enum MLMode { ML_NORMAL, ML_GREEDY };
    MLMode   mlMode       = ML_NORMAL;
    float    queues[4]    = {0.0f, 0.0f, 0.0f, 0.0f};  // normalised queue per road [top,bottom,left,right]
    uint8_t  intensity[4] = {1, 1, 1, 1};               // arrival rate per road: 0=low 1=med 2=high
    uint32_t mlDuration   = 5000;                        // green phase duration chosen by last ML inference (ms)
    bool     supervisedLoaded = false;  // set true by mlInit() when supervised model loads

    // Simulation speed multiplier: 1 = ×1 (500 ms/tick), 2 = ×2 (250 ms/tick), 5 = ×5 (100 ms/tick)
    // Scales both queue tick rate and all phase durations proportionally.
    uint8_t  simSpeed = 1;
};

// The single global traffic state instance, defined in traffic.cpp.
extern TrafficState traffic;

// ── Public API ────────────────────────────────────────────────────────────────

// Initialises all LED GPIO pins and starts the cycle at TOP_GREEN.
void trafficInit();

// Advances the state machine. Must be called on every loop() iteration.
void trafficUpdate();

// Forces one direction green and pauses the normal cycle. Pass -1 to clear.
void trafficSetOverride(int dir);

// Triggers an all-red emergency lasting 5 seconds. Idempotent if already active.
void trafficTriggerEmergency();

// Returns a string label for the current phase (e.g. "top_green", "emergency").
const char* phaseName();

// Returns the LED color ("green", "yellow", or "red") for a given direction.
// dir: 0 = top, 1 = bottom, 2 = left, 3 = right.
const char* ledColor(int dir);

// Returns the milliseconds remaining in the current phase.
// Returns 0 during override (indefinite hold) and counts down during emergency.
uint32_t timeRemaining();

// Returns the active control mode: "normal", "greedy", "override", or "emergency".
const char* modeName();

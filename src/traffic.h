#pragma once
#include <Arduino.h>

// Shift register pins
#define DATA_PIN   27
#define CLOCK_PIN  26
#define LATCH_PIN  25

// Green LED pins (direct GPIO)
#define GREEN_TOP    2
#define GREEN_BOTTOM 16
#define GREEN_LEFT   17
#define GREEN_RIGHT  21

// Emergency LED
#define EMERG_PIN 22

// Shift register bit positions
// Q0–Q3: Red LEDs (Top, Bottom, Left, Right)
// Q4–Q7: Yellow LEDs (Top, Bottom, Left, Right)
#define BIT_RED_TOP    0
#define BIT_RED_BOTTOM 1
#define BIT_RED_LEFT   2
#define BIT_RED_RIGHT  3
#define BIT_YEL_TOP    4
#define BIT_YEL_BOTTOM 5
#define BIT_YEL_LEFT   6
#define BIT_YEL_RIGHT  7

enum Phase {
    TOP_GREEN, TOP_YELLOW,
    BOTTOM_GREEN, BOTTOM_YELLOW,
    LEFT_GREEN, LEFT_YELLOW,
    RIGHT_GREEN, RIGHT_YELLOW,
    OVERRIDE,
    EMERGENCY
};

struct Timings {
    uint32_t top    = 5000;
    uint32_t bottom = 5000;
    uint32_t left   = 5000;
    uint32_t right  = 5000;
    uint32_t yellow = 2000;
};

struct TrafficState {
    Phase    phase        = TOP_GREEN;
    bool     emergency    = false;
    int      overrideDir  = -1;  // -1=none, 0=top, 1=bottom, 2=left, 3=right
    Timings  timings;
    uint32_t phaseStart   = 0;
};

extern TrafficState traffic;

void        trafficInit();
void        trafficUpdate();
void        trafficSetOverride(int dir);   // -1 to clear
void        trafficTriggerEmergency();
const char* phaseName();
const char* ledColor(int dir);             // 0=top, 1=bottom, 2=left, 3=right
uint32_t    timeRemaining();

/*
 * traffic.cpp — Traffic light state machine and LED output driver.
 *
 * Manages the 4-road phase cycle (green → yellow → next road → …),
 * emergency mode (all-red for 5 s triggered by RFID), and manual override
 * (one road held green until cleared from the dashboard).
 *
 * LED output strategy:
 *   - Red and Yellow LEDs are driven through a 74HC595N shift register
 *     using only 3 GPIO pins (DATA, CLOCK, LATCH).
 *   - Green LEDs are driven directly from 4 individual GPIO pins.
 */

#include "traffic.h"

TrafficState traffic;

// ── Internal helpers ──────────────────────────────────────────────────────────

enum LedColor { LED_RED, LED_YELLOW, LED_GREEN };

// Clocks one byte into the 74HC595 shift register and latches it to the outputs.
// Each bit corresponds to one red or yellow LED (see BIT_* defines in traffic.h).
static void shiftWrite(byte state) {
    digitalWrite(LATCH_PIN, LOW);
    shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, state);
    digitalWrite(LATCH_PIN, HIGH);
}

// Sets all 8 red/yellow LEDs via the shift register and all 4 green LEDs via
// direct GPIO, based on the requested color for each of the four roads.
static void applyColors(LedColor top, LedColor bottom, LedColor left, LedColor right) {
    byte sr = 0;
    if (top    == LED_RED)    sr |= (1 << BIT_RED_TOP);
    if (bottom == LED_RED)    sr |= (1 << BIT_RED_BOTTOM);
    if (left   == LED_RED)    sr |= (1 << BIT_RED_LEFT);
    if (right  == LED_RED)    sr |= (1 << BIT_RED_RIGHT);
    if (top    == LED_YELLOW) sr |= (1 << BIT_YEL_TOP);
    if (bottom == LED_YELLOW) sr |= (1 << BIT_YEL_BOTTOM);
    if (left   == LED_YELLOW) sr |= (1 << BIT_YEL_LEFT);
    if (right  == LED_YELLOW) sr |= (1 << BIT_YEL_RIGHT);

    shiftWrite(sr);
    digitalWrite(GREEN_TOP,    top    == LED_GREEN ? HIGH : LOW);
    digitalWrite(GREEN_BOTTOM, bottom == LED_GREEN ? HIGH : LOW);
    digitalWrite(GREEN_LEFT,   left   == LED_GREEN ? HIGH : LOW);
    digitalWrite(GREEN_RIGHT,  right  == LED_GREEN ? HIGH : LOW);
}

// Translates a named Phase into the correct LED color pattern and drives the hardware.
// OVERRIDE and EMERGENCY both result in all-red; green is applied separately for OVERRIDE.
static void applyPhase(Phase p) {
    switch (p) {
        case TOP_GREEN:     applyColors(LED_GREEN,  LED_RED,    LED_RED,    LED_RED);    break;
        case TOP_YELLOW:    applyColors(LED_YELLOW, LED_RED,    LED_RED,    LED_RED);    break;
        case BOTTOM_GREEN:  applyColors(LED_RED,    LED_GREEN,  LED_RED,    LED_RED);    break;
        case BOTTOM_YELLOW: applyColors(LED_RED,    LED_YELLOW, LED_RED,    LED_RED);    break;
        case LEFT_GREEN:    applyColors(LED_RED,    LED_RED,    LED_GREEN,  LED_RED);    break;
        case LEFT_YELLOW:   applyColors(LED_RED,    LED_RED,    LED_YELLOW, LED_RED);    break;
        case RIGHT_GREEN:   applyColors(LED_RED,    LED_RED,    LED_RED,    LED_GREEN);  break;
        case RIGHT_YELLOW:  applyColors(LED_RED,    LED_RED,    LED_RED,    LED_YELLOW); break;
        case OVERRIDE:
        case EMERGENCY:     applyColors(LED_RED,    LED_RED,    LED_RED,    LED_RED);    break;
    }
}

// Returns how long the given phase should last in milliseconds.
// For green phases, doubles the configured duration when night mode is active.
// All yellow phases share a single duration regardless of road.
static uint32_t phaseDuration(Phase p) {
    uint32_t base;
    switch (p) {
        case TOP_GREEN:    base = traffic.timings.top;    break;
        case BOTTOM_GREEN: base = traffic.timings.bottom; break;
        case LEFT_GREEN:   base = traffic.timings.left;   break;
        case RIGHT_GREEN:  base = traffic.timings.right;  break;
        default:           return traffic.timings.yellow;
    }
    // Auto-dim: extend green phases at night so traffic gets more time to clear
    if (traffic.autoDimEnabled && traffic.ldrBrightness < traffic.ldrNightThreshold)
        return (uint32_t)(base * LDR_NIGHT_MULTIPLIER);
    return base;
}

// Returns the phase that follows the given one in the fixed rotation sequence:
// TOP_GREEN → TOP_YELLOW → BOTTOM_GREEN → … → RIGHT_YELLOW → TOP_GREEN.
static Phase nextPhase(Phase p) {
    switch (p) {
        case TOP_GREEN:     return TOP_YELLOW;
        case TOP_YELLOW:    return BOTTOM_GREEN;
        case BOTTOM_GREEN:  return BOTTOM_YELLOW;
        case BOTTOM_YELLOW: return LEFT_GREEN;
        case LEFT_GREEN:    return LEFT_YELLOW;
        case LEFT_YELLOW:   return RIGHT_GREEN;
        case RIGHT_GREEN:   return RIGHT_YELLOW;
        default:            return TOP_GREEN;
    }
}

// ── Public API ────────────────────────────────────────────────────────────────

// Sets all LED-related pins to OUTPUT mode, resets the state to TOP_GREEN,
// and drives the initial LED pattern. Called once from setup().
void trafficInit() {
    pinMode(DATA_PIN,    OUTPUT);
    pinMode(CLOCK_PIN,   OUTPUT);
    pinMode(LATCH_PIN,   OUTPUT);
    pinMode(GREEN_TOP,    OUTPUT);
    pinMode(GREEN_BOTTOM, OUTPUT);
    pinMode(GREEN_LEFT,   OUTPUT);
    pinMode(GREEN_RIGHT,  OUTPUT);
    pinMode(EMERG_PIN,   OUTPUT);

    traffic.phase      = TOP_GREEN;
    traffic.phaseStart = millis();
    applyPhase(TOP_GREEN);
}

// Advances the traffic state machine. Called on every loop() iteration.
// During emergency: waits 5 s then resets to normal cycle.
// During override: does nothing (held until cleared via trafficSetOverride(-1)).
// Normal operation: moves to the next phase when the current phase duration expires.
void trafficUpdate() {
    uint32_t now = millis();

    if (traffic.emergency) {
        if (now - traffic.phaseStart >= 5000) {
            traffic.emergency = false;
            digitalWrite(EMERG_PIN, LOW);
            traffic.phase      = TOP_GREEN;
            traffic.phaseStart = now;
            applyPhase(TOP_GREEN);
        }
        return;
    }

    if (traffic.overrideDir >= 0) return;  // held in override until cleared

    if (now - traffic.phaseStart >= phaseDuration(traffic.phase)) {
        traffic.phase      = nextPhase(traffic.phase);
        traffic.phaseStart = now;
        applyPhase(traffic.phase);
    }
}

// Forces the given direction green and pauses the normal cycle.
// Pass dir = -1 to clear the override and restart the cycle from TOP_GREEN.
// dir: 0 = top, 1 = bottom, 2 = left, 3 = right.
void trafficSetOverride(int dir) {
    traffic.overrideDir = dir;

    if (dir < 0) {
        traffic.phase      = TOP_GREEN;
        traffic.phaseStart = millis();
        applyPhase(TOP_GREEN);
        return;
    }

    traffic.phase = OVERRIDE;
    applyColors(LED_RED, LED_RED, LED_RED, LED_RED);

    const byte greenPins[] = { GREEN_TOP, GREEN_BOTTOM, GREEN_LEFT, GREEN_RIGHT };
    digitalWrite(greenPins[dir], HIGH);
}

// Immediately sets all roads to red, lights the emergency LED, and starts
// a 5-second countdown after which the normal cycle resumes.
// Idempotent — does nothing if an emergency is already in progress.
void trafficTriggerEmergency() {
    if (traffic.emergency) return;
    traffic.emergency  = true;
    traffic.phase      = EMERGENCY;
    traffic.phaseStart = millis();
    applyPhase(EMERGENCY);
    digitalWrite(EMERG_PIN, HIGH);
}

// Returns a short string label for the current phase, used in the JSON API response.
const char* phaseName() {
    if (traffic.emergency) return "emergency";
    switch (traffic.phase) {
        case TOP_GREEN:     return "top_green";
        case TOP_YELLOW:    return "top_yellow";
        case BOTTOM_GREEN:  return "bottom_green";
        case BOTTOM_YELLOW: return "bottom_yellow";
        case LEFT_GREEN:    return "left_green";
        case LEFT_YELLOW:   return "left_yellow";
        case RIGHT_GREEN:   return "right_green";
        case RIGHT_YELLOW:  return "right_yellow";
        case OVERRIDE:      return "override";
        default:            return "emergency";
    }
}

// Returns the current LED color ("green", "yellow", or "red") for the given direction.
// Accounts for emergency (all red), override (one green, rest red), and normal cycle.
// dir: 0 = top, 1 = bottom, 2 = left, 3 = right.
const char* ledColor(int dir) {
    if (traffic.emergency) return "red";

    if (traffic.overrideDir >= 0) {
        return (traffic.overrideDir == dir) ? "green" : "red";
    }

    const Phase greenPhases[]  = { TOP_GREEN,  BOTTOM_GREEN,  LEFT_GREEN,  RIGHT_GREEN  };
    const Phase yellowPhases[] = { TOP_YELLOW, BOTTOM_YELLOW, LEFT_YELLOW, RIGHT_YELLOW };

    if (traffic.phase == greenPhases[dir])  return "green";
    if (traffic.phase == yellowPhases[dir]) return "yellow";
    return "red";
}

// Returns the number of milliseconds remaining in the current phase.
// Returns 0 when an override is active (no defined end time).
// During an emergency, counts down from 5000 ms to 0.
uint32_t timeRemaining() {
    if (traffic.emergency) {
        uint32_t elapsed = millis() - traffic.phaseStart;
        return elapsed >= 5000 ? 0 : 5000 - elapsed;
    }
    if (traffic.overrideDir >= 0) return 0;

    uint32_t dur     = phaseDuration(traffic.phase);
    uint32_t elapsed = millis() - traffic.phaseStart;
    return elapsed >= dur ? 0 : dur - elapsed;
}

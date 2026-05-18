#include "traffic.h"
#include "ml.h"

// The single global instance of all runtime state — shared by traffic, ml, and webserver modules.
TrafficState traffic;

// ── Internal helpers ──────────────────────────────────────────────────────────

enum LedColor { LED_RED, LED_YELLOW, LED_GREEN };

// Sends one byte to the 74HC595 shift register.
// Protocol: pull latch LOW → clock in 8 bits MSB-first → pull latch HIGH to latch outputs.
// Each bit position maps to a red or yellow LED as defined in traffic.h (BIT_RED_*, BIT_YEL_*).
static void shiftWrite(byte state) {
    digitalWrite(LATCH_PIN, LOW);           // open the shift register for new data
    shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, state); // push 8 bits, MSB first
    digitalWrite(LATCH_PIN, HIGH);          // latch: transfer shift register → output pins
}

// Sets all eight red/yellow LEDs (via the shift register) and all four green LEDs (direct GPIO).
// Red and yellow are handled by the 74HC595 because there are more than the ESP32 can comfortably
// drive directly; green LEDs are fewer and controlled straight from GPIO pins.
static void applyColors(LedColor top, LedColor bottom, LedColor left, LedColor right) {
    // Build the byte for the shift register: each bit turns a red or yellow LED on.
    byte sr = 0;
    if (top    == LED_RED)    sr |= (1 << BIT_RED_TOP);
    if (bottom == LED_RED)    sr |= (1 << BIT_RED_BOTTOM);
    if (left   == LED_RED)    sr |= (1 << BIT_RED_LEFT);
    if (right  == LED_RED)    sr |= (1 << BIT_RED_RIGHT);
    if (top    == LED_YELLOW) sr |= (1 << BIT_YEL_TOP);
    if (bottom == LED_YELLOW) sr |= (1 << BIT_YEL_BOTTOM);
    if (left   == LED_YELLOW) sr |= (1 << BIT_YEL_LEFT);
    if (right  == LED_YELLOW) sr |= (1 << BIT_YEL_RIGHT);

    shiftWrite(sr);  // send all 8 red/yellow bits at once

    // Green LEDs are driven directly from individual GPIO pins.
    digitalWrite(GREEN_TOP,    top    == LED_GREEN ? HIGH : LOW);
    digitalWrite(GREEN_BOTTOM, bottom == LED_GREEN ? HIGH : LOW);
    digitalWrite(GREEN_LEFT,   left   == LED_GREEN ? HIGH : LOW);
    digitalWrite(GREEN_RIGHT,  right  == LED_GREEN ? HIGH : LOW);
}

// Translates an abstract Phase value into the correct LED colours for all four roads.
// Exactly one road is active (green or yellow) at a time; the other three are red.
// OVERRIDE and EMERGENCY both show all-red (the overriding direction's green is set separately).
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

// Returns how long (ms) the given phase should last, accounting for three modifiers:
//   1. ML (greedy) mode — uses traffic.mlDuration chosen by the last mlInfer() call.
//   2. Night mode     — doubles green duration when the LDR reading is below the threshold.
//   3. Sim speed      — divides all durations so the simulation can run faster than real time.
static uint32_t phaseDuration(Phase p) {
    // In ML/greedy mode the model decides green duration; yellow still uses the configured value.
    if (traffic.mlMode != TrafficState::ML_NORMAL) {
        switch (p) {
            case TOP_GREEN:
            case BOTTOM_GREEN:
            case LEFT_GREEN:
            case RIGHT_GREEN: return traffic.mlDuration / traffic.simSpeed;
            default: break;
        }
    }
    // Normal mode: look up the per-road configured green duration.
    uint32_t base;
    switch (p) {
        case TOP_GREEN:    base = traffic.timings.top;    break;
        case BOTTOM_GREEN: base = traffic.timings.bottom; break;
        case LEFT_GREEN:   base = traffic.timings.left;   break;
        case RIGHT_GREEN:  base = traffic.timings.right;  break;
        default:           return traffic.timings.yellow / traffic.simSpeed; // yellow phases
    }
    // Apply night-mode multiplier: low ambient light → longer green so fewer cars run reds.
    if (traffic.autoDimEnabled && traffic.ldrBrightness < traffic.ldrNightThreshold)
        return (uint32_t)(base * LDR_NIGHT_MULTIPLIER) / traffic.simSpeed;
    return base / traffic.simSpeed;
}

// Returns the phase that follows p in the fixed round-robin cycle:
//   TOP_GREEN → TOP_YELLOW → BOTTOM_GREEN → BOTTOM_YELLOW → LEFT_GREEN → LEFT_YELLOW → RIGHT_GREEN → RIGHT_YELLOW → TOP_GREEN …
// OVERRIDE and EMERGENCY are not part of the cycle; after their special handling, the system
// always restarts at TOP_GREEN (handled in trafficUpdate/trafficSetOverride).
static Phase nextPhase(Phase p) {
    switch (p) {
        case TOP_GREEN:     return TOP_YELLOW;
        case TOP_YELLOW:    return BOTTOM_GREEN;
        case BOTTOM_GREEN:  return BOTTOM_YELLOW;
        case BOTTOM_YELLOW: return LEFT_GREEN;
        case LEFT_GREEN:    return LEFT_YELLOW;
        case LEFT_YELLOW:   return RIGHT_GREEN;
        case RIGHT_GREEN:   return RIGHT_YELLOW;
        default:            return TOP_GREEN;   // RIGHT_YELLOW wraps back to start
    }
}

// Returns true if p is one of the four yellow transition phases.
// Used in trafficUpdate to decide when to run ML inference (we ask which road goes green NEXT
// while the current road is still showing yellow, so the decision is ready on time).
static bool isYellow(Phase p) {
    return p == TOP_YELLOW || p == BOTTOM_YELLOW ||
           p == LEFT_YELLOW || p == RIGHT_YELLOW;
}

// Converts a road index (0=top, 1=bottom, 2=left, 3=right) to its GREEN phase enum value.
static Phase roadToGreen(int road) {
    const Phase greens[] = { TOP_GREEN, BOTTOM_GREEN, LEFT_GREEN, RIGHT_GREEN };
    return greens[road];
}

// ── Public API ────────────────────────────────────────────────────────────────

void trafficInit() {
    // Configure all LED control pins as outputs.
    pinMode(DATA_PIN,    OUTPUT);
    pinMode(CLOCK_PIN,   OUTPUT);
    pinMode(LATCH_PIN,   OUTPUT);
    pinMode(GREEN_TOP,    OUTPUT);
    pinMode(GREEN_BOTTOM, OUTPUT);
    pinMode(GREEN_LEFT,   OUTPUT);
    pinMode(GREEN_RIGHT,  OUTPUT);
    pinMode(EMERG_PIN,   OUTPUT);

    // Start the cycle with Top road green.
    traffic.phase      = TOP_GREEN;
    traffic.phaseStart = millis();
    applyPhase(TOP_GREEN);
}

void trafficUpdate() {
    uint32_t now = millis();
    // Always tick the ML queue simulation, even during override or emergency,
    // so queue levels stay meaningful when normal/greedy mode resumes.
    mlQueueTick(now);

    // ── Emergency: hold all-red for 5 s (scaled by simSpeed), then restart cycle ──
    if (traffic.emergency) {
        if (now - traffic.phaseStart >= 5000 / traffic.simSpeed) {
            traffic.emergency = false;
            digitalWrite(EMERG_PIN, LOW);  // turn off the emergency indicator LED
            traffic.phase      = TOP_GREEN;
            traffic.phaseStart = now;
            applyPhase(TOP_GREEN);
        }
        return;  // don't advance the normal cycle while emergency is active
    }

    // ── Override: a direction is held green indefinitely — do nothing until cleared ──
    if (traffic.overrideDir >= 0) return;

    // ── Normal / greedy mode: advance the phase when the current duration expires ──
    if (now - traffic.phaseStart >= phaseDuration(traffic.phase)) {
        Phase next;
        // In greedy mode, run ML inference at the END of each yellow phase so we know
        // which road has the longest queue and should get the next green.
        if (traffic.mlMode != TrafficState::ML_NORMAL && isYellow(traffic.phase)) {
            int road;
            uint32_t dur;
            bool ok = mlInfer(&road, &dur);
            if (ok) {
                next = roadToGreen(road);   // jump to the highest-priority road
                traffic.mlDuration = dur;   // store the model's recommended green duration
            } else {
                next = nextPhase(traffic.phase); // fallback: round-robin
            }
        } else {
            next = nextPhase(traffic.phase); // normal round-robin step
        }
        traffic.phase      = next;
        traffic.phaseStart = now;
        applyPhase(traffic.phase);
    }
}

void trafficSetOverride(int dir) {
    traffic.overrideDir = dir;

    if (dir < 0) {
        // Clear override: restart the normal cycle from TOP_GREEN.
        traffic.phase      = TOP_GREEN;
        traffic.phaseStart = millis();
        applyPhase(TOP_GREEN);
        return;
    }

    // Set override: all shift-register outputs go red, then the chosen green pin goes HIGH.
    // applyColors sets the shift register; then we manually drive the correct green GPIO.
    traffic.phase = OVERRIDE;
    applyColors(LED_RED, LED_RED, LED_RED, LED_RED);

    const byte greenPins[] = { GREEN_TOP, GREEN_BOTTOM, GREEN_LEFT, GREEN_RIGHT };
    digitalWrite(greenPins[dir], HIGH);
}

void trafficTriggerEmergency() {
    if (traffic.emergency) return;  // already in emergency — idempotent
    traffic.emergency  = true;
    traffic.phase      = EMERGENCY;
    traffic.phaseStart = millis();
    applyPhase(EMERGENCY);            // all-red via shift register
    digitalWrite(EMERG_PIN, HIGH);    // turn on the separate emergency indicator LED
}

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

const char* modeName() {
    if (traffic.emergency)        return "emergency";
    if (traffic.overrideDir >= 0) return "override";
    if (traffic.mlMode == TrafficState::ML_GREEDY) return "greedy";
return "normal";
}

const char* ledColor(int dir) {
    if (traffic.emergency) return "red";

    if (traffic.overrideDir >= 0)
        return (traffic.overrideDir == dir) ? "green" : "red";

    const Phase greenPhases[]  = { TOP_GREEN,  BOTTOM_GREEN,  LEFT_GREEN,  RIGHT_GREEN  };
    const Phase yellowPhases[] = { TOP_YELLOW, BOTTOM_YELLOW, LEFT_YELLOW, RIGHT_YELLOW };

    if (traffic.phase == greenPhases[dir])  return "green";
    if (traffic.phase == yellowPhases[dir]) return "yellow";
    return "red";
}

uint32_t timeRemaining() {
    if (traffic.emergency) {
        uint32_t dur     = 5000 / traffic.simSpeed;
        uint32_t elapsed = millis() - traffic.phaseStart;
        return elapsed >= dur ? 0 : dur - elapsed;
    }
    if (traffic.overrideDir >= 0) return 0;

    uint32_t dur     = phaseDuration(traffic.phase);
    uint32_t elapsed = millis() - traffic.phaseStart;
    return elapsed >= dur ? 0 : dur - elapsed;
}

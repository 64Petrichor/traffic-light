#include "traffic.h"
#include "ml.h"

TrafficState traffic;

// ── Internal helpers ──────────────────────────────────────────────────────────

enum LedColor { LED_RED, LED_YELLOW, LED_GREEN };

static void shiftWrite(byte state) {
    digitalWrite(LATCH_PIN, LOW);
    shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, state);
    digitalWrite(LATCH_PIN, HIGH);
}

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

static uint32_t phaseDuration(Phase p) {
    // In ML mode the model decides green duration; yellow is always the configured value
    if (traffic.mlMode != TrafficState::ML_NORMAL) {
        switch (p) {
            case TOP_GREEN:
            case BOTTOM_GREEN:
            case LEFT_GREEN:
            case RIGHT_GREEN: return traffic.mlDuration / traffic.simSpeed;
            default: break;
        }
    }
    uint32_t base;
    switch (p) {
        case TOP_GREEN:    base = traffic.timings.top;    break;
        case BOTTOM_GREEN: base = traffic.timings.bottom; break;
        case LEFT_GREEN:   base = traffic.timings.left;   break;
        case RIGHT_GREEN:  base = traffic.timings.right;  break;
        default:           return traffic.timings.yellow / traffic.simSpeed;
    }
    if (traffic.autoDimEnabled && traffic.ldrBrightness < traffic.ldrNightThreshold)
        return (uint32_t)(base * LDR_NIGHT_MULTIPLIER) / traffic.simSpeed;
    return base / traffic.simSpeed;
}

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

static bool isYellow(Phase p) {
    return p == TOP_YELLOW || p == BOTTOM_YELLOW ||
           p == LEFT_YELLOW || p == RIGHT_YELLOW;
}

static Phase roadToGreen(int road) {
    const Phase greens[] = { TOP_GREEN, BOTTOM_GREEN, LEFT_GREEN, RIGHT_GREEN };
    return greens[road];
}

// ── Public API ────────────────────────────────────────────────────────────────

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

void trafficUpdate() {
    uint32_t now = millis();
    mlQueueTick(now);  // always update queues, even during override/emergency

    if (traffic.emergency) {
        if (now - traffic.phaseStart >= 5000 / traffic.simSpeed) {
            traffic.emergency = false;
            digitalWrite(EMERG_PIN, LOW);
            traffic.phase      = TOP_GREEN;
            traffic.phaseStart = now;
            applyPhase(TOP_GREEN);
        }
        return;
    }

    if (traffic.overrideDir >= 0) return;

    if (now - traffic.phaseStart >= phaseDuration(traffic.phase)) {
        Phase next;
        if (traffic.mlMode != TrafficState::ML_NORMAL && isYellow(traffic.phase)) {
            int road;
            uint32_t dur;
            bool ok = (traffic.mlMode == TrafficState::ML_RL)
                      ? mlRLInfer(&road, &dur)
                      : mlInfer(&road, &dur);
            if (ok) {
                next = roadToGreen(road);
                traffic.mlDuration = dur;
            } else {
                next = nextPhase(traffic.phase);
            }
        } else {
            next = nextPhase(traffic.phase);
        }
        traffic.phase      = next;
        traffic.phaseStart = now;
        applyPhase(traffic.phase);
    }
}

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

void trafficTriggerEmergency() {
    if (traffic.emergency) return;
    traffic.emergency  = true;
    traffic.phase      = EMERGENCY;
    traffic.phaseStart = millis();
    applyPhase(EMERGENCY);
    digitalWrite(EMERG_PIN, HIGH);
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
    if (traffic.mlMode == TrafficState::ML_RL)     return "rl";
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

#include "ml.h"
#include "traffic.h"

const float ML_INTENSITY_MUL[3] = {0.3f, 0.6f, 1.0f};

void mlQueueTick(uint32_t now) {
    static uint32_t lastTick = 0;
    if (now - lastTick < (uint32_t)(ML_TICK_MS / traffic.simSpeed)) return;
    lastTick = now;

    int greenRoad = -1;
    if (traffic.overrideDir >= 0) {
        greenRoad = traffic.overrideDir;
    } else {
        switch (traffic.phase) {
            case TOP_GREEN:    greenRoad = 0; break;
            case BOTTOM_GREEN: greenRoad = 1; break;
            case LEFT_GREEN:   greenRoad = 2; break;
            case RIGHT_GREEN:  greenRoad = 3; break;
            default: break;
        }
    }

    for (int i = 0; i < 4; i++) {
        if (i == greenRoad)
            traffic.queues[i] -= ML_DRAIN_RATE;
        else
            traffic.queues[i] += ML_ACCUM_BASE * ML_INTENSITY_MUL[traffic.intensity[i]];
        traffic.queues[i] = constrain(traffic.queues[i], 0.0f, 1.0f);
    }
}

bool mlInfer(int* road, uint32_t* durationMs) {
    float scores[4];
    for (int i = 0; i < 4; i++)
        scores[i] = traffic.queues[i] * ML_INTENSITY_MUL[traffic.intensity[i]];
    int best = 0;
    for (int i = 1; i < 4; i++) if (scores[i] > scores[best]) best = i;
    *road = best;
    float dur = ML_MIN_GREEN_S + traffic.queues[best] * (ML_MAX_GREEN_S - ML_MIN_GREEN_S);
    *durationMs = (uint32_t)(constrain(dur, ML_MIN_GREEN_S, ML_MAX_GREEN_S) * 1000.0f);
    Serial.printf("[GREEDY] road=%d dur=%.1fs q=[%.2f %.2f %.2f %.2f]\n",
                  best, dur,
                  traffic.queues[0], traffic.queues[1],
                  traffic.queues[2], traffic.queues[3]);
    return true;
}

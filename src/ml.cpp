#include "ml.h"
#include "traffic.h"
#include "ml_weights.h"
#include <math.h>

const float ML_INTENSITY_MUL[3] = {0.3f, 0.6f, 1.0f};

static void relu(float* v, int n) {
    for (int i = 0; i < n; i++) if (v[i] < 0.0f) v[i] = 0.0f;
}

static void matmul_bias(const float* W, const float* b,
                        const float* in, float* out, int in_n, int out_n) {
    for (int j = 0; j < out_n; j++) {
        float s = b[j];
        for (int i = 0; i < in_n; i++) s += in[i] * W[i * out_n + j];
        out[j] = s;
    }
}

static void softmax(float* v, int n) {
    float mx = v[0];
    for (int i = 1; i < n; i++) if (v[i] > mx) mx = v[i];
    float s = 0.0f;
    for (int i = 0; i < n; i++) { v[i] = expf(v[i] - mx); s += v[i]; }
    for (int i = 0; i < n; i++) v[i] /= s;
}

bool mlInit() {
    traffic.supervisedLoaded = true;
    traffic.rlLoaded         = true;
    Serial.println("[ML] Weights compiled-in — supervised + RL ready");
    return true;
}

void mlQueueTick(uint32_t now) {
    static uint32_t lastTick = 0;
    if (now - lastTick < ML_TICK_MS) return;
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
    float inp[8], h1[16], h2[8], road_out[4], dur_out[1];

    for (int i = 0; i < 4; i++) {
        inp[i]   = traffic.queues[i];
        inp[i+4] = ML_INTENSITY_MUL[traffic.intensity[i]];
    }

    matmul_bias(SUP_L1_W,   SUP_L1_B,   inp, h1, 8,  16); relu(h1, 16);
    matmul_bias(SUP_L2_W,   SUP_L2_B,   h1,  h2, 16, 8);  relu(h2, 8);
    matmul_bias(SUP_ROAD_W, SUP_ROAD_B, h2, road_out, 8, 4); softmax(road_out, 4);
    matmul_bias(SUP_DUR_W,  SUP_DUR_B,  h2, dur_out,  8, 1);

    int best = 0;
    for (int i = 1; i < 4; i++) if (road_out[i] > road_out[best]) best = i;
    *road = best;

    float dur = fmaxf(ML_MIN_GREEN_S, fminf(ML_MAX_GREEN_S, dur_out[0]));
    *durationMs = (uint32_t)(dur * 1000.0f);

    Serial.printf("[ML] road=%d dur=%.1fs q=[%.2f %.2f %.2f %.2f]\n",
                  best, dur,
                  traffic.queues[0], traffic.queues[1],
                  traffic.queues[2], traffic.queues[3]);
    return true;
}

bool mlRLInfer(int* road, uint32_t* durationMs) {
    float inp[8], h1[32], h2[16], road_out[4], dur_out[1];

    for (int i = 0; i < 4; i++) {
        inp[i]   = traffic.queues[i];
        inp[i+4] = ML_INTENSITY_MUL[traffic.intensity[i]];
    }

    matmul_bias(RL_L1_W,   RL_L1_B,   inp, h1, 8,  32); relu(h1, 32);
    matmul_bias(RL_L2_W,   RL_L2_B,   h1,  h2, 32, 16); relu(h2, 16);
    matmul_bias(RL_ROAD_W, RL_ROAD_B, h2, road_out, 16, 4); softmax(road_out, 4);
    matmul_bias(RL_DUR_W,  RL_DUR_B,  h2, dur_out,  16, 1);

    int best = 0;
    for (int i = 1; i < 4; i++) if (road_out[i] > road_out[best]) best = i;
    *road = best;

    float dur = fmaxf(ML_MIN_GREEN_S, fminf(ML_MAX_GREEN_S, dur_out[0]));
    *durationMs = (uint32_t)(dur * 1000.0f);

    Serial.printf("[RL] road=%d dur=%.1fs q=[%.2f %.2f %.2f %.2f]\n",
                  best, dur,
                  traffic.queues[0], traffic.queues[1],
                  traffic.queues[2], traffic.queues[3]);
    return true;
}

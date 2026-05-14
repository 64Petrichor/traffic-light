#include "ml.h"
#include "traffic.h"
#include <LittleFS.h>
#include <TensorFlowLite_ESP32.h>
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

const float ML_INTENSITY_MUL[3] = {0.3f, 0.6f, 1.0f};

static uint8_t* s_modelBuf = nullptr;
static tflite::MicroErrorReporter s_errReporter;
static tflite::AllOpsResolver s_resolver;
static tflite::MicroInterpreter* s_interp = nullptr;
static constexpr size_t kArena = 16 * 1024;
static uint8_t s_arena[kArena];

bool mlInit() {
    File f = LittleFS.open("/traffic_model.tflite", "r");
    if (!f) {
        Serial.println("[ML] traffic_model.tflite not found in LittleFS");
        return false;
    }
    size_t sz = f.size();
    s_modelBuf = (uint8_t*)malloc(sz);
    if (!s_modelBuf) {
        Serial.println("[ML] malloc failed");
        f.close();
        return false;
    }
    f.read(s_modelBuf, sz);
    f.close();

    const tflite::Model* model = tflite::GetModel(s_modelBuf);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        Serial.println("[ML] TFLite schema version mismatch");
        free(s_modelBuf);
        s_modelBuf = nullptr;
        return false;
    }

    s_interp = new tflite::MicroInterpreter(
        model, s_resolver, s_arena, kArena, &s_errReporter);
    if (s_interp->AllocateTensors() != kTfLiteOk) {
        Serial.println("[ML] AllocateTensors failed");
        return false;
    }

    Serial.printf("[ML] Ready — model %u B, arena used %u B\n",
                  (unsigned)sz, (unsigned)s_interp->arena_used_bytes());
    return true;
}

void mlQueueTick(uint32_t now) {
    static uint32_t lastTick = 0;
    if (now - lastTick < ML_TICK_MS) return;
    lastTick = now;

    // Determine which road is currently green (for drain)
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
    if (!s_interp) return false;

    TfLiteTensor* inp = s_interp->input(0);
    for (int i = 0; i < 4; i++) {
        inp->data.f[i]   = traffic.queues[i];
        inp->data.f[i+4] = ML_INTENSITY_MUL[traffic.intensity[i]];
    }

    if (s_interp->Invoke() != kTfLiteOk) return false;

    // Model may produce two separate output tensors [road:1×4, duration:1×1]
    // or a single flat tensor [5] depending on the TFLite conversion path.
    float roadProbs[4];
    float durS;

    if (s_interp->outputs_size() >= 2) {
        TfLiteTensor* rOut = s_interp->output(0);
        TfLiteTensor* dOut = s_interp->output(1);
        for (int i = 0; i < 4; i++) roadProbs[i] = rOut->data.f[i];
        durS = dOut->data.f[0];
    } else {
        TfLiteTensor* out = s_interp->output(0);
        for (int i = 0; i < 4; i++) roadProbs[i] = out->data.f[i];
        durS = out->data.f[4];
    }

    int best = 0;
    for (int i = 1; i < 4; i++)
        if (roadProbs[i] > roadProbs[best]) best = i;
    *road = best;

    durS = constrain(durS, ML_MIN_GREEN_S, ML_MAX_GREEN_S);
    *durationMs = (uint32_t)(durS * 1000.0f);

    Serial.printf("[ML] road=%d dur=%.1fs q=[%.2f %.2f %.2f %.2f]\n",
                  best, durS,
                  traffic.queues[0], traffic.queues[1],
                  traffic.queues[2], traffic.queues[3]);
    return true;
}

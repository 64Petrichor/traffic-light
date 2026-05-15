#include "webserver.h"
#include "traffic.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

static AsyncWebServer server(80);

static const char* SSID = "TrafficLight";
static const char* PASS = "12345678";

static const char* dirName(int dir) {
    switch (dir) {
        case 0: return "top";
        case 1: return "bottom";
        case 2: return "left";
        case 3: return "right";
        default: return "none";
    }
}

static int dirIndex(const char* name) {
    if (strcmp(name, "top")    == 0) return 0;
    if (strcmp(name, "bottom") == 0) return 1;
    if (strcmp(name, "left")   == 0) return 2;
    if (strcmp(name, "right")  == 0) return 3;
    return -1;
}

static const char* intensityName(uint8_t lvl) {
    if (lvl == 0) return "low";
    if (lvl == 2) return "high";
    return "med";
}

static uint8_t intensityLevel(const char* s) {
    if (strcmp(s, "low")  == 0) return 0;
    if (strcmp(s, "high") == 0) return 2;
    return 1;
}

void webserverInit() {
    if (!LittleFS.begin()) {
        Serial.println("LittleFS mount failed — run 'pio run -t uploadfs' then reflash");
    }

    WiFi.mode(WIFI_AP);
    WiFi.softAP(SSID, PASS);
    delay(500);

    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());
    Serial.println("Starting server...");

    server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(LittleFS, "/dashboard.html", "text/html");
    });

    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* req) {
        StaticJsonDocument<960> doc;
        doc["phase"]     = phaseName();
        doc["mode"]      = modeName();
        doc["emergency"] = traffic.emergency;
        const char* mlModeStr = "normal";
        if (traffic.mlMode == TrafficState::ML_GREEDY) mlModeStr = "greedy";
        else if (traffic.mlMode == TrafficState::ML_RL) mlModeStr = "rl";
        doc["mlMode"] = mlModeStr;

        if (traffic.overrideDir >= 0)
            doc["override"] = dirName(traffic.overrideDir);
        else
            doc["override"] = nullptr;

        JsonObject leds = doc.createNestedObject("leds");
        leds["top"]    = ledColor(0);
        leds["bottom"] = ledColor(1);
        leds["left"]   = ledColor(2);
        leds["right"]  = ledColor(3);

        JsonObject timings = doc.createNestedObject("timings");
        timings["top"]    = traffic.timings.top;
        timings["bottom"] = traffic.timings.bottom;
        timings["left"]   = traffic.timings.left;
        timings["right"]  = traffic.timings.right;
        timings["yellow"] = traffic.timings.yellow;

        JsonObject queues = doc.createNestedObject("queues");
        queues["top"]    = traffic.queues[0];
        queues["bottom"] = traffic.queues[1];
        queues["left"]   = traffic.queues[2];
        queues["right"]  = traffic.queues[3];

        JsonObject intensity = doc.createNestedObject("intensity");
        intensity["top"]    = intensityName(traffic.intensity[0]);
        intensity["bottom"] = intensityName(traffic.intensity[1]);
        intensity["left"]   = intensityName(traffic.intensity[2]);
        intensity["right"]  = intensityName(traffic.intensity[3]);

        doc["remaining"]  = timeRemaining();
        doc["brightness"] = traffic.ldrBrightness;
        doc["threshold"]  = traffic.ldrNightThreshold;
        doc["autoDim"]    = traffic.autoDimEnabled;
        doc["nightMode"]  = traffic.autoDimEnabled && traffic.ldrBrightness < traffic.ldrNightThreshold;
        doc["supervisedLoaded"] = traffic.supervisedLoaded;
        doc["rlLoaded"]         = traffic.rlLoaded;
        doc["simSpeed"]         = traffic.simSpeed;

        String body;
        serializeJson(doc, body);
        req->send(200, "application/json", body);
    });

    server.on("/api/override", HTTP_POST,
        [](AsyncWebServerRequest* req) {},
        nullptr,
        [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
            StaticJsonDocument<64> doc;
            if (deserializeJson(doc, data, len)) {
                req->send(400, "application/json", "{\"error\":\"bad json\"}");
                return;
            }
            const char* dir = doc["direction"];
            int idx = dir ? dirIndex(dir) : -1;
            trafficSetOverride(idx);
            req->send(200, "application/json", "{\"ok\":true}");
        }
    );

    server.on("/api/timing", HTTP_POST,
        [](AsyncWebServerRequest* req) {},
        nullptr,
        [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
            StaticJsonDocument<128> doc;
            if (deserializeJson(doc, data, len)) {
                req->send(400, "application/json", "{\"error\":\"bad json\"}");
                return;
            }
            if (doc.containsKey("top"))    traffic.timings.top    = doc["top"];
            if (doc.containsKey("bottom")) traffic.timings.bottom = doc["bottom"];
            if (doc.containsKey("left"))   traffic.timings.left   = doc["left"];
            if (doc.containsKey("right"))  traffic.timings.right  = doc["right"];
            if (doc.containsKey("yellow")) traffic.timings.yellow = doc["yellow"];
            req->send(200, "application/json", "{\"ok\":true}");
        }
    );

    server.on("/api/dim", HTTP_POST, [](AsyncWebServerRequest* req) {
        traffic.autoDimEnabled = !traffic.autoDimEnabled;
        Serial.printf("[LDR] Auto-dim %s\n", traffic.autoDimEnabled ? "ENABLED" : "DISABLED");
        req->send(200, "application/json",
                  traffic.autoDimEnabled ? "{\"ok\":true,\"autoDim\":true}"
                                         : "{\"ok\":true,\"autoDim\":false}");
    });

    // POST /api/ml  { "mode": "normal"|"greedy"|"rl" }
    server.on("/api/ml", HTTP_POST,
        [](AsyncWebServerRequest* req) {},
        nullptr,
        [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
            StaticJsonDocument<64> doc;
            if (deserializeJson(doc, data, len)) {
                req->send(400, "application/json", "{\"error\":\"bad json\"}");
                return;
            }
            const char* mode = doc["mode"] | "normal";
            if (strcmp(mode, "greedy") == 0)      traffic.mlMode = TrafficState::ML_GREEDY;
            else if (strcmp(mode, "rl") == 0)     traffic.mlMode = TrafficState::ML_RL;
            else                                   traffic.mlMode = TrafficState::ML_NORMAL;
            Serial.printf("[ML] Mode set to %s\n", mode);
            String body = "{\"ok\":true,\"mlMode\":\"";
            body += mode;
            body += "\"}";
            req->send(200, "application/json", body);
        }
    );

    // POST /api/speed  { "speed": 1|2|5 }  — 1=×1 (500ms/tick), 2=×2 (250ms/tick), 5=×5 (100ms/tick)
    server.on("/api/speed", HTTP_POST,
        [](AsyncWebServerRequest* req) {},
        nullptr,
        [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
            StaticJsonDocument<32> doc;
            if (deserializeJson(doc, data, len)) {
                req->send(400, "application/json", "{\"error\":\"bad json\"}");
                return;
            }
            uint8_t spd = doc["speed"] | 1;
            if (spd != 1 && spd != 2 && spd != 5) spd = 1;
            traffic.simSpeed = spd;
            Serial.printf("[SIM] Speed set to x%d (%d ms/tick)\n", spd, 500 / spd);
            String body = "{\"ok\":true,\"simSpeed\":";
            body += spd;
            body += "}";
            req->send(200, "application/json", body);
        }
    );

    // POST /api/intensity  { "road": "top"|"bottom"|"left"|"right", "level": "low"|"med"|"high" }
    server.on("/api/intensity", HTTP_POST,
        [](AsyncWebServerRequest* req) {},
        nullptr,
        [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
            StaticJsonDocument<96> doc;
            if (deserializeJson(doc, data, len)) {
                req->send(400, "application/json", "{\"error\":\"bad json\"}");
                return;
            }
            const char* road  = doc["road"];
            const char* level = doc["level"];
            if (!road || !level) {
                req->send(400, "application/json", "{\"error\":\"missing fields\"}");
                return;
            }
            int idx = dirIndex(road);
            if (idx < 0) {
                req->send(400, "application/json", "{\"error\":\"unknown road\"}");
                return;
            }
            traffic.intensity[idx] = intensityLevel(level);
            req->send(200, "application/json", "{\"ok\":true}");
        }
    );

    server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(LittleFS, "/style.css", "text/css");
    });
    server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(LittleFS, "/script.js", "application/javascript");
    });
    server.begin();
    Serial.println("Server started");
}

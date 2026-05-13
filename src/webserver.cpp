/*
 * webserver.cpp — WiFi access point, async HTTP server, and REST API.
 *
 * The ESP32 acts as its own WiFi hotspot (no external router needed).
 * Clients connect to the hotspot and open http://192.168.4.1 to reach the
 * dashboard, which is served from the LittleFS flash filesystem.
 *
 * REST API endpoints:
 *   GET  /api/status   — returns full system state as JSON (poll at ~100 ms)
 *   POST /api/override — force a direction green or clear override
 *   POST /api/timing   — update per-road green and yellow durations
 *   POST /api/dim      — toggle night mode (auto-dim) on/off
 */

#include "webserver.h"
#include "traffic.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

static AsyncWebServer server(80);

static const char* SSID = "TrafficLight";
static const char* PASS = "12345678";

// Converts a direction index to its string name for use in JSON responses.
// dir: 0 = "top", 1 = "bottom", 2 = "left", 3 = "right", anything else = "none".
static const char* dirName(int dir) {
    switch (dir) {
        case 0: return "top";
        case 1: return "bottom";
        case 2: return "left";
        case 3: return "right";
        default: return "none";
    }
}

// Initialises the WiFi access point, mounts LittleFS, registers all HTTP
// routes, and starts the async web server on port 80.
void webserverInit() {
    if (!LittleFS.begin()) {
        Serial.println("LittleFS mount failed — run 'pio run -t uploadfs' then reflash");
    }

    WiFi.mode(WIFI_AP);
    WiFi.softAP(SSID, PASS);
    delay(500);  // give AP time to initialise before starting server

    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());
    Serial.println("Starting server...");

    // Serves the web dashboard HTML file from the LittleFS flash filesystem.
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(LittleFS, "/dashboard.html", "text/html");
    });

    // Returns the full system state as a JSON object.
    // Includes current phase, LED colors, timings, time remaining, and LDR data.
    // The dashboard polls this endpoint every 100 ms.
    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* req) {
        StaticJsonDocument<384> doc;
        doc["phase"]     = phaseName();
        doc["emergency"] = traffic.emergency;

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

        doc["remaining"]  = timeRemaining();
        doc["brightness"]  = traffic.ldrBrightness;
        doc["threshold"]   = traffic.ldrNightThreshold;
        doc["autoDim"]     = traffic.autoDimEnabled;
        doc["nightMode"]   = traffic.autoDimEnabled && traffic.ldrBrightness < traffic.ldrNightThreshold;

        String body;
        serializeJson(doc, body);
        req->send(200, "application/json", body);
    });

    // Forces one road green (pausing the normal cycle) or clears an active override.
    // Expected body: {"direction": "top" | "bottom" | "left" | "right" | "none"}
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
            if      (strcmp(dir, "top")    == 0) trafficSetOverride(0);
            else if (strcmp(dir, "bottom") == 0) trafficSetOverride(1);
            else if (strcmp(dir, "left")   == 0) trafficSetOverride(2);
            else if (strcmp(dir, "right")  == 0) trafficSetOverride(3);
            else                                 trafficSetOverride(-1);

            req->send(200, "application/json", "{\"ok\":true}");
        }
    );

    // Updates one or more phase durations. Only keys present in the body are changed;
    // omitted keys leave the existing timing unchanged.
    // Expected body: {"top":5000, "bottom":5000, "left":5000, "right":5000, "yellow":2000}
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

    // Toggles auto-dim (night mode) on or off.
    // When enabled, green phases are doubled in duration while brightness is below threshold.
    server.on("/api/dim", HTTP_POST, [](AsyncWebServerRequest* req) {
        traffic.autoDimEnabled = !traffic.autoDimEnabled;
        Serial.printf("[LDR] Auto-dim %s\n", traffic.autoDimEnabled ? "ENABLED" : "DISABLED");
        req->send(200, "application/json",
                  traffic.autoDimEnabled ? "{\"ok\":true,\"autoDim\":true}"
                                         : "{\"ok\":true,\"autoDim\":false}");
    });

    server.begin();
    Serial.println("Server started");
}

#include "webserver.h"
#include "traffic.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

// Async HTTP server listening on port 80.
// "Async" means the ESP32 handles requests via callbacks without blocking loop().
static AsyncWebServer server(80);

// WiFi access-point credentials. Connect to this network to reach the dashboard.
static const char* SSID = "TrafficLight";
static const char* PASS = "12345678";

// ── Utility converters ────────────────────────────────────────────────────────

// Converts a direction index (0–3) to a human-readable string for JSON responses.
static const char* dirName(int dir) {
    switch (dir) {
        case 0: return "top";
        case 1: return "bottom";
        case 2: return "left";
        case 3: return "right";
        default: return "none";
    }
}

// Parses a direction string from an incoming JSON request into an index (0–3), or -1 if unknown.
static int dirIndex(const char* name) {
    if (strcmp(name, "top")    == 0) return 0;
    if (strcmp(name, "bottom") == 0) return 1;
    if (strcmp(name, "left")   == 0) return 2;
    if (strcmp(name, "right")  == 0) return 3;
    return -1;
}

// Converts the internal intensity level (0/1/2) to a JSON-friendly string.
static const char* intensityName(uint8_t lvl) {
    if (lvl == 0) return "low";
    if (lvl == 2) return "high";
    return "med";
}

// Parses an intensity string from an incoming JSON request into an internal level (0/1/2).
static uint8_t intensityLevel(const char* s) {
    if (strcmp(s, "low")  == 0) return 0;
    if (strcmp(s, "high") == 0) return 2;
    return 1;  // default to medium
}

// ── Server initialisation ─────────────────────────────────────────────────────

void webserverInit() {
    // Mount the LittleFS flash filesystem where dashboard.html, style.css, script.js live.
    // If this fails, the web UI won't load but the traffic light still works.
    if (!LittleFS.begin()) {
        Serial.println("LittleFS mount failed — run 'pio run -t uploadfs' then reflash");
    }

    // Create a WiFi access point. Clients connect to "TrafficLight" to reach the dashboard.
    WiFi.mode(WIFI_AP);
    WiFi.softAP(SSID, PASS);
    delay(500);  // small delay to let the AP come up before printing the IP

    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());  // typically 192.168.4.1
    Serial.println("Starting server...");

    // ── GET /  →  serve the main dashboard page from LittleFS ────────────────
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(LittleFS, "/dashboard.html", "text/html");
    });

    // ── GET /api/status  →  full system snapshot as JSON ─────────────────────
    // The dashboard polls this endpoint (typically every second) to update all its widgets.
    // StaticJsonDocument<960> allocates the JSON buffer on the stack — no heap fragmentation.
    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* req) {
        StaticJsonDocument<960> doc;
        doc["phase"]     = phaseName();   // e.g. "top_green", "left_yellow"
        doc["mode"]      = modeName();    // "normal", "greedy", "override", or "emergency"
        doc["emergency"] = traffic.emergency;
        const char* mlModeStr = (traffic.mlMode == TrafficState::ML_GREEDY) ? "greedy" : "normal";
        doc["mlMode"] = mlModeStr;

        // "override" is the currently held direction, or null if no override is active.
        if (traffic.overrideDir >= 0)
            doc["override"] = dirName(traffic.overrideDir);
        else
            doc["override"] = nullptr;

        // Per-road LED colours ("green", "yellow", or "red").
        JsonObject leds = doc.createNestedObject("leds");
        leds["top"]    = ledColor(0);
        leds["bottom"] = ledColor(1);
        leds["left"]   = ledColor(2);
        leds["right"]  = ledColor(3);

        // Currently configured phase durations in milliseconds.
        JsonObject timings = doc.createNestedObject("timings");
        timings["top"]    = traffic.timings.top;
        timings["bottom"] = traffic.timings.bottom;
        timings["left"]   = traffic.timings.left;
        timings["right"]  = traffic.timings.right;
        timings["yellow"] = traffic.timings.yellow;

        // Simulated queue levels (0.0–1.0) for each road — used by greedy ML mode.
        JsonObject queues = doc.createNestedObject("queues");
        queues["top"]    = traffic.queues[0];
        queues["bottom"] = traffic.queues[1];
        queues["left"]   = traffic.queues[2];
        queues["right"]  = traffic.queues[3];

        // Traffic arrival intensity settings per road ("low", "med", "high").
        JsonObject intensity = doc.createNestedObject("intensity");
        intensity["top"]    = intensityName(traffic.intensity[0]);
        intensity["bottom"] = intensityName(traffic.intensity[1]);
        intensity["left"]   = intensityName(traffic.intensity[2]);
        intensity["right"]  = intensityName(traffic.intensity[3]);

        doc["remaining"]  = timeRemaining();        // ms left in the current phase
        doc["brightness"] = traffic.ldrBrightness;  // 0–100% from the LDR
        doc["threshold"]  = traffic.ldrNightThreshold;
        doc["autoDim"]    = traffic.autoDimEnabled;
        // nightMode is true when auto-dim is on AND it's actually dark enough to trigger it.
        doc["nightMode"]  = traffic.autoDimEnabled && traffic.ldrBrightness < traffic.ldrNightThreshold;
        doc["simSpeed"]   = traffic.simSpeed;

        String body;
        serializeJson(doc, body);
        req->send(200, "application/json", body);
    });

    // ── POST /api/override  { "direction": "top"|"bottom"|"left"|"right"|null }  ──
    // Holds one direction green indefinitely. Omit "direction" (or send null) to clear.
    // ESPAsyncWebServer requires three callbacks for body-bearing POST requests:
    //   1st = headers-only handler (left empty), 2nd = file upload (nullptr), 3rd = body handler.
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
            int idx = dir ? dirIndex(dir) : -1;  // -1 clears the override
            trafficSetOverride(idx);
            req->send(200, "application/json", "{\"ok\":true}");
        }
    );

    // ── POST /api/timing  { "top": ms, "bottom": ms, "left": ms, "right": ms, "yellow": ms }  ──
    // Updates green/yellow phase durations at runtime. Only keys present in the JSON are changed.
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

    // ── POST /api/dim  (no body)  →  toggles auto-dim (night mode) on/off ────
    server.on("/api/dim", HTTP_POST, [](AsyncWebServerRequest* req) {
        traffic.autoDimEnabled = !traffic.autoDimEnabled;
        Serial.printf("[LDR] Auto-dim %s\n", traffic.autoDimEnabled ? "ENABLED" : "DISABLED");
        req->send(200, "application/json",
                  traffic.autoDimEnabled ? "{\"ok\":true,\"autoDim\":true}"
                                         : "{\"ok\":true,\"autoDim\":false}");
    });

    // ── POST /api/ml  { "mode": "normal"|"greedy" }  ─────────────────────────
    // Switches between fixed round-robin timing (normal) and adaptive greedy scheduling.
    server.on("/api/ml", HTTP_POST,
        [](AsyncWebServerRequest* req) {},
        nullptr,
        [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
            StaticJsonDocument<64> doc;
            if (deserializeJson(doc, data, len)) {
                req->send(400, "application/json", "{\"error\":\"bad json\"}");
                return;
            }
            const char* mode = doc["mode"] | "normal";  // default to "normal" if key missing
            if (strcmp(mode, "greedy") == 0) traffic.mlMode = TrafficState::ML_GREEDY;
            else                             traffic.mlMode = TrafficState::ML_NORMAL;
            Serial.printf("[ML] Mode set to %s\n", mode);
            String body = "{\"ok\":true,\"mlMode\":\"";
            body += mode;
            body += "\"}";
            req->send(200, "application/json", body);
        }
    );

    // ── POST /api/speed  { "speed": 1|2|5 }  ────────────────────────────────
    // Sets the simulation speed multiplier. Higher values compress time so queue changes
    // and phase transitions happen faster (useful for demos).
    // Only 1 (real-time), 2 (2×), and 5 (5×) are accepted; anything else resets to 1.
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
            if (spd != 1 && spd != 2 && spd != 5) spd = 1;  // reject invalid values
            traffic.simSpeed = spd;
            Serial.printf("[SIM] Speed set to x%d (%d ms/tick)\n", spd, 500 / spd);
            String body = "{\"ok\":true,\"simSpeed\":";
            body += spd;
            body += "}";
            req->send(200, "application/json", body);
        }
    );

    // ── POST /api/intensity  { "road": "top"|…, "level": "low"|"med"|"high" }  ──
    // Sets the simulated traffic arrival rate for one road. Affects how fast that road's
    // queue grows when it is red, which in turn influences greedy mode decisions.
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

    // ── Serve static assets from LittleFS ────────────────────────────────────
    server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(LittleFS, "/style.css", "text/css");
    });
    server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(LittleFS, "/script.js", "application/javascript");
    });

    server.begin();
    Serial.println("Server started");
}

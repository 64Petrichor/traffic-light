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

    // Serve dashboard
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(LittleFS, "/dashboard.html", "text/html");
    });

    // GET /api/status
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

    // POST /api/override  body: {"direction":"top"} or {"direction":"none"}
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

    // POST /api/timing  body: {"top":5000,"bottom":5000,"left":5000,"right":5000,"yellow":2000}
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

    // POST /api/dim  — toggle auto-dim on/off
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

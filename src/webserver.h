/*
 * webserver.h — Public interface for the web server module.
 *
 * The ESP32 runs a WiFi access point (SSID: TrafficLight) and hosts an
 * async HTTP server on port 80. The dashboard HTML is served from LittleFS
 * and a REST API exposes live state and control endpoints.
 * Implementation is in webserver.cpp.
 */

#pragma once

// Initialises the WiFi access point, mounts the LittleFS filesystem,
// registers all HTTP routes, and starts the async web server.
// Must be called once from setup().
void webserverInit();

/*
 * main.cpp — Entry point for the Adaptive Traffic Light System.
 *
 * Responsibilities:
 *   - Arduino setup() / loop() entry points
 *   - Polling the RFID reader for emergency triggers
 *   - Sampling the LDR ambient light sensor
 *   - Delegating traffic state updates to traffic.cpp
 *
 * All traffic logic lives in traffic.cpp/.h.
 * All web/API logic lives in webserver.cpp/.h.
 */

#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "traffic.h"
#include "webserver.h"
#include "ml.h"

#define SS_PIN  5
#define RST_PIN 4
#define LDR_PIN 34

#define RFID_COOLDOWN_MS 15000   // ignore re-scans for this long after an emergency

MFRC522 rfid(SS_PIN, RST_PIN);

// Reads ambient brightness from the LDR on GPIO 34 once per second.
// Maps the raw ADC value (0–4095) to a 0–100% scale and stores it in
// traffic.ldrBrightness. Prints to Serial; tags the line with [NIGHT MODE]
// when night mode is active.
static void checkLDR() {
    static uint32_t lastPrint = 0;
    if (millis() - lastPrint < 1000) return;
    lastPrint = millis();
    traffic.ldrBrightness = map(analogRead(LDR_PIN), 0, 4095, 0, 100);
    Serial.printf("[LDR] Brightness: %d%%%s\n", traffic.ldrBrightness,
                  (traffic.autoDimEnabled && traffic.ldrBrightness < traffic.ldrNightThreshold) ? " [NIGHT MODE]" : "");
}

// Polls the RFID reader for a new card on every loop iteration.
// If a card is detected and the cooldown has expired, triggers an emergency
// (all-red for 5 seconds) and starts the 15-second cooldown timer.
// Does nothing if an emergency is already active.
static void checkRFID() {
    static uint32_t lastRfidScanMs = 0;
    uint32_t now = millis();

    if (traffic.emergency) return;
    if (lastRfidScanMs && (now - lastRfidScanMs) < RFID_COOLDOWN_MS) return;

    if (!rfid.PICC_IsNewCardPresent()) return;
    if (!rfid.PICC_ReadCardSerial()) {
        rfid.PICC_HaltA();   // reset card state so next poll sees it as new
        return;
    }

    Serial.print("Emergency triggered. UID:");
    for (byte i = 0; i < rfid.uid.size; i++) {
        Serial.print(rfid.uid.uidByte[i] < 0x10 ? " 0" : " ");
        Serial.print(rfid.uid.uidByte[i], HEX);
    }
    Serial.println();

    lastRfidScanMs = now;
    trafficTriggerEmergency();

    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
}

// Runs once at power-on.
// Initialises SPI, the RFID reader, the traffic state machine, and the web server.
// Calibrates the LDR night threshold to the ambient brightness at boot time.
void setup() {
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);  // prevent boot-loop on external supply
    Serial.begin(115200);
    SPI.begin();
    rfid.PCD_Init();
    trafficInit();
    webserverInit();
    mlInit();

    // Calibrate night threshold to ambient brightness at boot
    traffic.ldrBrightness      = map(analogRead(LDR_PIN), 0, 4095, 0, 100);
    traffic.ldrNightThreshold  = traffic.ldrBrightness;
    Serial.printf("[LDR] Night threshold calibrated to %d%%\n", traffic.ldrNightThreshold);

    Serial.println("Ready");
}

// Called repeatedly by the Arduino runtime — the main program loop.
// Polls the RFID reader, samples the LDR, then advances the traffic state machine.
void loop() {
    checkRFID();
    checkLDR();
    trafficUpdate();
}

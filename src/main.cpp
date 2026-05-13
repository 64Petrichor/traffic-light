#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include "traffic.h"
#include "webserver.h"

#define SS_PIN  5
#define RST_PIN 4
#define LDR_PIN 34

#define RFID_COOLDOWN_MS 15000   // ignore re-scans for this long after an emergency

MFRC522 rfid(SS_PIN, RST_PIN);

static void checkLDR() {
    static uint32_t lastPrint = 0;
    if (millis() - lastPrint < 1000) return;
    lastPrint = millis();
    traffic.ldrBrightness = map(analogRead(LDR_PIN), 0, 4095, 0, 100);
    Serial.printf("[LDR] Brightness: %d%%%s\n", traffic.ldrBrightness,
                  (traffic.autoDimEnabled && traffic.ldrBrightness < traffic.ldrNightThreshold) ? " [NIGHT MODE]" : "");
}

static void checkRFID() {
    static uint32_t lastRfidScanMs = 0;
    uint32_t now = millis();

    if (traffic.emergency) return;
    if (lastRfidScanMs && (now - lastRfidScanMs) < RFID_COOLDOWN_MS) return;

    if (!rfid.PICC_IsNewCardPresent()) return;
    if (!rfid.PICC_ReadCardSerial())   return;

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

void setup() {
    Serial.begin(115200);
    SPI.begin();
    rfid.PCD_Init();
    trafficInit();
    webserverInit();

    // Calibrate night threshold to ambient brightness at boot
    traffic.ldrBrightness      = map(analogRead(LDR_PIN), 0, 4095, 0, 100);
    traffic.ldrNightThreshold  = traffic.ldrBrightness;
    Serial.printf("[LDR] Night threshold calibrated to %d%%\n", traffic.ldrNightThreshold);

    Serial.println("Ready");
}

void loop() {
    checkRFID();
    checkLDR();
    trafficUpdate();
}

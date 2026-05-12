#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include "traffic.h"
#include "webserver.h"

#define SS_PIN  5
#define RST_PIN 4

MFRC522 rfid(SS_PIN, RST_PIN);

static void checkRFID() {
    if (!rfid.PICC_IsNewCardPresent()) return;
    if (!rfid.PICC_ReadCardSerial())   return;

    Serial.print("Emergency triggered. UID:");
    for (byte i = 0; i < rfid.uid.size; i++) {
        Serial.print(rfid.uid.uidByte[i] < 0x10 ? " 0" : " ");
        Serial.print(rfid.uid.uidByte[i], HEX);
    }
    Serial.println();

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
    Serial.println("Ready");
}

void loop() {
    checkRFID();
    trafficUpdate();
}

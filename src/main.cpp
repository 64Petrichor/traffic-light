#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>

#define DATA_PIN   27
#define CLOCK_PIN  26
#define LATCH_PIN  25
#define SS_PIN     5
#define RST_PIN    4
#define EMERG_PIN  22  // Emergency red LED

const byte GREEN_PINS[] = { 2, 16, 17, 21 };  // Top, Bottom, Left, Right

MFRC522 rfid(SS_PIN, RST_PIN);

void setLEDs(byte state) {
    digitalWrite(LATCH_PIN, LOW);
    shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, state);
    digitalWrite(LATCH_PIN, HIGH);
}

void checkRFID() {
    if (!rfid.PICC_IsNewCardPresent()) return;
    if (!rfid.PICC_ReadCardSerial()) return;

    Serial.print("UID:");
    for (byte i = 0; i < rfid.uid.size; i++) {
        Serial.print(rfid.uid.uidByte[i] < 0x10 ? " 0" : " ");
        Serial.print(rfid.uid.uidByte[i], HEX);
    }
    Serial.println();

    digitalWrite(EMERG_PIN, HIGH);
    delay(300);
    digitalWrite(EMERG_PIN, LOW);

    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
}

void setup() {
    Serial.begin(115200);
    SPI.begin();
    rfid.PCD_Init();

    pinMode(DATA_PIN,  OUTPUT);
    pinMode(CLOCK_PIN, OUTPUT);
    pinMode(LATCH_PIN, OUTPUT);
    pinMode(EMERG_PIN, OUTPUT);

    for (byte pin : GREEN_PINS) pinMode(pin, OUTPUT);

    Serial.println("Ready");
}

void loop() {
    checkRFID();

    // Shift register LEDs one at a time (reds + yellows)
    for (int i = 0; i < 8; i++) {
        checkRFID();
        setLEDs(1 << i);
        delay(300);
    }
    setLEDs(0x00);

    // Green LEDs one at a time
    for (byte pin : GREEN_PINS) {
        checkRFID();
        digitalWrite(pin, HIGH);
        delay(300);
        digitalWrite(pin, LOW);
    }

    // All on
    setLEDs(0xFF);
    for (byte pin : GREEN_PINS) digitalWrite(pin, HIGH);
    delay(500);

    // All off
    setLEDs(0x00);
    for (byte pin : GREEN_PINS) digitalWrite(pin, LOW);
    delay(500);
}

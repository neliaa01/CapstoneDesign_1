#include <Arduino.h>
#include "Sensors.h"
#include "LED.h"
#include "Comm.h"

// ===== 핀 =====
#define PIN_BUTTON 0
#define PIN_IR_LED 10

void captureAndSend();

bool lastButtonState = HIGH;
bool currentButtonState = HIGH;

void setup() {
    Serial.begin(115200);

    pinMode(PIN_BUTTON, INPUT_PULLUP);
    pinMode(PIN_IR_LED, OUTPUT);
    digitalWrite(PIN_IR_LED, LOW);

    Sensors::begin();
    LED::begin();
    Comm::begin();

    LED::off();
}

void loop() {
    currentButtonState = digitalRead(PIN_BUTTON);

    if (lastButtonState == HIGH && currentButtonState == LOW) {
        captureAndSend();
    }

    lastButtonState = currentButtonState;

    Comm::handleClient(); // 웹 접속 처리
}


// ===== 핵심 동작 =====
void captureAndSend() {
    LED::scanning();

    digitalWrite(PIN_IR_LED, HIGH);
    delay(200);

    camera_fb_t* frame = Sensors::captureImage();

    digitalWrite(PIN_IR_LED, LOW);

    if (frame == NULL) {
        LED::error();
        return;
    }

    
    Comm::setFrame(frame);  // 전송용 저장

    LED::success();
    delay(1500);
    LED::off();
}
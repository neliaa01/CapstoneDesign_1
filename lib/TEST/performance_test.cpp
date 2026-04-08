#include <Arduino.h>
#include <Wire.h>
#include "performance_test.h"

// ===== 핀 설정 =====
#define BUTTON_PIN 2
#define LED_R 3
#define LED_G 4
#define LED_B 5
#define IR_PIN 6

#define SDA_PIN 8
#define SCL_PIN 9
#define MLX_ADDR 0x5A

bool pressed = false;

// ===== LED =====
void setLED(bool r, bool g, bool b) {
    digitalWrite(LED_R, r);
    digitalWrite(LED_G, g);
    digitalWrite(LED_B, b);
}

// ===== 온도 =====
float readTemp() {
    Wire.beginTransmission(MLX_ADDR);
    Wire.write(0x07);
    Wire.endTransmission(false);
    Wire.requestFrom(MLX_ADDR, 3);

    if (Wire.available() < 3) return -999;

    uint16_t data = Wire.read();
    data |= Wire.read() << 8;
    Wire.read();

    return data * 0.02 - 273.15;
}

// ===== IR =====
void testIR() {
    setLED(1,0,0);
    digitalWrite(IR_PIN, HIGH);
    delay(300);
    digitalWrite(IR_PIN, LOW);
}

// ===== LED 테스트 =====
void testLED() {
    setLED(1,0,0); delay(200);
    setLED(0,1,0); delay(200);
    setLED(0,0,1); delay(200);
    setLED(0,0,0);
}

// ===== 온도 테스트 =====
bool testTemp() {
    float t = readTemp();

    if (t == -999) return false;

    if (t > 30) {
        setLED(1,0,1);
    } else {
        setLED(0,1,0);
    }

    delay(500);
    return true;
}

// ===== 🔥 기존 setup → 변경 =====
void performanceSetup() {
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    pinMode(LED_R, OUTPUT);
    pinMode(LED_G, OUTPUT);
    pinMode(LED_B, OUTPUT);

    pinMode(IR_PIN, OUTPUT);

    Wire.begin(SDA_PIN, SCL_PIN);

    // 시작 표시
    setLED(0,0,1);
    delay(1000);
    setLED(0,0,0);
}

// ===== 🔥 기존 loop → 변경 =====
void performanceLoop() {

    if (digitalRead(BUTTON_PIN) == LOW && !pressed) {
        pressed = true;

        setLED(0,0,1);
        delay(300);

        testLED();
        testIR();

        bool ok = testTemp();

        if (!ok) {
            setLED(1,0,1);
            delay(1000);
        }

        setLED(0,0,0);
        delay(1000);
    }

    if (digitalRead(BUTTON_PIN) == HIGH) {
        pressed = false;
    }
}
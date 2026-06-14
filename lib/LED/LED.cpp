#include <Arduino.h>
#include "LED.h"

#define R_PIN 8
#define G_PIN 7
#define B_PIN 44

void LED::begin() {
    pinMode(R_PIN, OUTPUT);
    pinMode(G_PIN, OUTPUT);
    pinMode(B_PIN, OUTPUT);
}

void LED::off() { digitalWrite(R_PIN,LOW); digitalWrite(G_PIN,LOW); digitalWrite(B_PIN,LOW); }
void LED::scanning() { digitalWrite(B_PIN,HIGH); }   // 파랑
void LED::success() { digitalWrite(G_PIN,HIGH); }    // 초록
void LED::error() { digitalWrite(R_PIN,HIGH); }      // 빨강
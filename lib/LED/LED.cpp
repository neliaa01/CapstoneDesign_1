#include <Arduino.h>
#include "LED.h"

#define R_PIN 44
#define G_PIN 7
#define B_PIN 8

void LED::begin() {
    pinMode(R_PIN, OUTPUT);
    pinMode(G_PIN, OUTPUT);
    pinMode(B_PIN, OUTPUT);
}

void LED::off() { analogWrite(R_PIN,0); analogWrite(G_PIN,0); analogWrite(B_PIN,0); }
void LED::scanning() { analogWrite(B_PIN,255); }   // 파랑
void LED::success() { analogWrite(G_PIN,255); }    // 초록
void LED::error() { analogWrite(R_PIN,255); }      // 빨강
#include "performance_test.h"
#include "camera_test.h"

void setup() {
    performanceSetup();
}

void loop() {
    performanceLoop();
}

// #include <Arduino.h>


// #include "Sensors.h"
// #include "First_Detection.h"
// #include "Second_Detection.h"
// #include "Comm.h"
// #include "LED.h"

// // 핀 정의
// #define BUTTON_PIN 2

// // 상태 정의
// enum State {
//     IDLE,
//     SCANNING,
//     ANALYZING,
//     RESULT,
//     PRECISION_SCAN
// };

// State currentState = IDLE;

// // 결과 변수
// float detectionScore = 0.0;
// bool suspiciousDetected = false;

// // 버튼 상태
// bool lastButtonState = LOW;

// void setup() {
//     Serial.begin(115200);

//     pinMode(BUTTON_PIN, INPUT);

//     initSensors();
//     initLED();
//     initComm();
//     setLEDIdle();
// }

// void loop() {
// }
#include <Arduino.h>
#include "Sensors.h"
#include "LED.h"
#include "Comm.h"

// ===== 핀 =====
#define PIN_BUTTON 1
#define PIN_IR_LED 9   // GPIO10은 카메라 XCLK라 사용하면 안 됨

#define THERMAL_WIDTH   32
#define THERMAL_HEIGHT  24
#define THERMAL_PIXELS  (THERMAL_WIDTH * THERMAL_HEIGHT)

void captureAndSend();

bool lastButtonState = HIGH;
bool currentButtonState = HIGH;

static uint8_t thermalIndexFrame[THERMAL_PIXELS];

void setup() {
  Serial.begin(115200);
  delay(10000);

  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_IR_LED, OUTPUT);

  digitalWrite(PIN_IR_LED, LOW);

  Serial.println("[MAIN] Setup start");

  Serial.println("[MAIN] BLE start");
  Comm::begin();
  Serial.println("[MAIN] BLE done");

  Serial.println("[MAIN] Camera + Thermal sensor start");
  Sensors::begin();
  Serial.println("[MAIN] Camera + Thermal sensor done");

  Serial.println("[MAIN] LED start");
  LED::begin();
  Serial.println("[MAIN] LED done");

  LED::off();

  Serial.println("[MAIN] Setup done");
  Serial.println("[MAIN] Press button to capture image + thermal frame");
}

void loop() {
  currentButtonState = digitalRead(PIN_BUTTON);

  // 버튼은 INPUT_PULLUP이라 평소 HIGH, 누르면 LOW
  if (lastButtonState == HIGH && currentButtonState == LOW) {
    Serial.println("[BUTTON] Button pressed");

    delay(50);  // 디바운싱

    if (digitalRead(PIN_BUTTON) == LOW) {
      Serial.println("[BUTTON] Button confirmed");
      captureAndSend();
    } else {
      Serial.println("[BUTTON] Noise ignored");
    }
  }

  lastButtonState = currentButtonState;

  // BLE 전송 처리
  Comm::handleClient();
}

void captureAndSend() {
  Serial.println("[CAPTURE] Capture function started");

  LED::scanning();
  Serial.println("[CAPTURE] LED scanning mode");

  Serial.println("[CAPTURE] IR LED ON");
  digitalWrite(PIN_IR_LED, HIGH);
  delay(200);

  Serial.println("[CAPTURE] Taking picture...");
  camera_fb_t* frame = Sensors::captureImage();

  digitalWrite(PIN_IR_LED, LOW);
  Serial.println("[CAPTURE] IR LED OFF");

  if (frame == nullptr) {
    Serial.println("[CAPTURE] Failed: camera frame is NULL");
    LED::error();
    return;
  }

  Serial.print("[CAPTURE] Picture taken successfully. Image size: ");
  Serial.print(frame->len);
  Serial.println(" bytes");

  Serial.println("[CAPTURE] Saving image frame for BLE transfer...");
  Comm::setFrame(frame);
  Serial.println("[CAPTURE] Image frame is ready to send by BLE");

  float thermalMin = 0.0f;
  float thermalMax = 0.0f;

  Serial.println("[CAPTURE] Taking thermal frame...");

  bool thermalOk = Sensors::captureThermalIndex(
    thermalIndexFrame,
    &thermalMin,
    &thermalMax
  );

  if (thermalOk) {
    Serial.println("[CAPTURE] Thermal frame captured");

    Comm::setThermalFrame(
      thermalIndexFrame,
      THERMAL_PIXELS,
      thermalMin,
      thermalMax
    );

    Serial.println("[CAPTURE] Thermal frame is ready to send by BLE");
    LED::success();
  } else {
    Serial.println("[CAPTURE] Thermal capture failed");
    LED::error();
  }

  delay(1500);
  LED::off();

  Serial.println("[CAPTURE] Capture process done");
}
#include <Arduino.h>
#include "Sensors.h"
#include "LED.h"
#include "Comm.h"

<<<<<<< HEAD
// ===============================
// 핀 설정
// ===============================
#define PIN_BUTTON 1

// IR LED는 실제 납땜한 핀에 맞춰야 함
#define PIN_IR_LED 9

// ===============================
// 열화상 프레임 설정
// ===============================
=======
// ===== 핀 =====
#define PIN_BUTTON 1
#define PIN_IR_LED 9   // GPIO10은 카메라 XCLK라 사용하면 안 됨

>>>>>>> d3372bbc361414834b197fe1af66e11e71c61c57
#define THERMAL_WIDTH   32
#define THERMAL_HEIGHT  24
#define THERMAL_PIXELS  (THERMAL_WIDTH * THERMAL_HEIGHT)

<<<<<<< HEAD
// 버튼 상태
bool lastButtonState = HIGH;
bool currentButtonState = HIGH;

// 열화상 0~255 인덱스 프레임 버퍼
static uint8_t thermalIndexFrame[THERMAL_PIXELS];

void captureAndSend();

void setup() {
  Serial.begin(115200);
  delay(3000);

  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_IR_LED, OUTPUT);
  digitalWrite(PIN_IR_LED, LOW);

  Serial.println();
  Serial.println("=================================");
  Serial.println("[MAIN] OwlGuard setup start");
  Serial.println("=================================");

  Serial.println("[MAIN] BLE start");
  Comm::begin();
  Serial.println("[MAIN] BLE done");

  Serial.println("[MAIN] Sensors start");
  Sensors::begin();
  Serial.println("[MAIN] Sensors done");

  Serial.println("[MAIN] LED start");
  LED::begin();
  LED::off();
  Serial.println("[MAIN] LED done");

=======
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

>>>>>>> d3372bbc361414834b197fe1af66e11e71c61c57
  Serial.println("[MAIN] Setup done");
  Serial.println("[MAIN] Press button to capture image + thermal frame");
}

void loop() {
  currentButtonState = digitalRead(PIN_BUTTON);

<<<<<<< HEAD
  // 버튼은 INPUT_PULLUP 기준
  // 평소 HIGH, 누르면 LOW
=======
  // 버튼은 INPUT_PULLUP이라 평소 HIGH, 누르면 LOW
>>>>>>> d3372bbc361414834b197fe1af66e11e71c61c57
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
<<<<<<< HEAD
  // setFrame(), setThermalFrame()에서 저장해둔 데이터를
  // 여기서 조금씩 Notify로 전송함
=======
>>>>>>> d3372bbc361414834b197fe1af66e11e71c61c57
  Comm::handleClient();
}

void captureAndSend() {
<<<<<<< HEAD
  Serial.println();
  Serial.println("=================================");
  Serial.println("[CAPTURE] Capture process start");
  Serial.println("=================================");

  LED::scanning();

  // ===============================
  // 1. 일반 카메라 촬영
  // ===============================
=======
  Serial.println("[CAPTURE] Capture function started");

  LED::scanning();
  Serial.println("[CAPTURE] LED scanning mode");

>>>>>>> d3372bbc361414834b197fe1af66e11e71c61c57
  Serial.println("[CAPTURE] IR LED ON");
  digitalWrite(PIN_IR_LED, HIGH);
  delay(200);

<<<<<<< HEAD
  Serial.println("[CAPTURE] Taking camera image...");
=======
  Serial.println("[CAPTURE] Taking picture...");
>>>>>>> d3372bbc361414834b197fe1af66e11e71c61c57
  camera_fb_t* frame = Sensors::captureImage();

  digitalWrite(PIN_IR_LED, LOW);
  Serial.println("[CAPTURE] IR LED OFF");

  if (frame == nullptr) {
    Serial.println("[CAPTURE] Failed: camera frame is NULL");
    LED::error();
<<<<<<< HEAD
    delay(1000);
    LED::off();
    return;
  }

  Serial.print("[CAPTURE] Camera image captured. Size: ");
  Serial.print(frame->len);
  Serial.println(" bytes");

  // Comm::setFrame() 내부에서 frame 내용을 복사하고
  // esp_camera_fb_return(fb)까지 처리함
  Serial.println("[CAPTURE] Store image frame for BLE");
  Comm::setFrame(frame);

  // ===============================
  // 2. MLX90640 열화상 캡처
  // ===============================
=======
    return;
  }

  Serial.print("[CAPTURE] Picture taken successfully. Image size: ");
  Serial.print(frame->len);
  Serial.println(" bytes");

  Serial.println("[CAPTURE] Saving image frame for BLE transfer...");
  Comm::setFrame(frame);
  Serial.println("[CAPTURE] Image frame is ready to send by BLE");

>>>>>>> d3372bbc361414834b197fe1af66e11e71c61c57
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

<<<<<<< HEAD
    Serial.print("[CAPTURE] Thermal min: ");
    Serial.print(thermalMin, 1);
    Serial.print(" C, max: ");
    Serial.print(thermalMax, 1);
    Serial.println(" C");

    // Comm.cpp 기준 함수:
    // void setThermalFrame(const uint8_t* data, size_t len, float minTemp, float maxTemp)
=======
>>>>>>> d3372bbc361414834b197fe1af66e11e71c61c57
    Comm::setThermalFrame(
      thermalIndexFrame,
      THERMAL_PIXELS,
      thermalMin,
      thermalMax
    );

<<<<<<< HEAD
    Serial.println("[CAPTURE] Thermal frame stored for BLE");
=======
    Serial.println("[CAPTURE] Thermal frame is ready to send by BLE");
>>>>>>> d3372bbc361414834b197fe1af66e11e71c61c57
    LED::success();
  } else {
    Serial.println("[CAPTURE] Thermal capture failed");
    LED::error();
  }

  delay(1500);
  LED::off();

  Serial.println("[CAPTURE] Capture process done");
}
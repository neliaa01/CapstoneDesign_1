#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MLX90640.h>

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

// =========================
// 핀 설정
// =========================
#define SDA_PIN 5   // XIAO ESP32S3 D4
#define SCL_PIN 6   // XIAO ESP32S3 D5

// =========================
// MLX90640 설정
// =========================
#define THERMAL_WIDTH   32
#define THERMAL_HEIGHT  24
#define PIXEL_COUNT     (THERMAL_WIDTH * THERMAL_HEIGHT)

Adafruit_MLX90640 mlx;
float tempFrame[PIXEL_COUNT];
uint8_t indexFrame[PIXEL_COUNT];

// =========================
// BLE 설정
// =========================
#define DEVICE_NAME        "XIAO-MLX90640"
#define SERVICE_UUID       "12345678-1234-1234-1234-1234567890ab"
#define DATA_CHAR_UUID     "12345678-1234-1234-1234-1234567890ac"

// 청크 payload 크기
// 180 정도면 보통 괜찮음.
// 휴대폰에서 문제 있으면 100 또는 20으로 낮춰보기.
#define CHUNK_PAYLOAD_SIZE 180

BLEServer* pServer = nullptr;
BLECharacteristic* pDataChar = nullptr;

bool deviceConnected = false;
bool oldDeviceConnected = false;

// =========================
// 표시용 온도 범위 설정
// =========================
// 자동 범위 사용
bool useAutoRange = true;

// 자동 범위 smoothing
bool firstRange = true;
float displayMinTemp = 20.0f;
float displayMaxTemp = 35.0f;

// 고정 범위를 쓰고 싶으면 아래 값 참고
float fixedMinTemp = 20.0f;
float fixedMaxTemp = 40.0f;

// 프레임 ID
uint16_t frameId = 0;

// =========================
// BLE 콜백
// =========================
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) override {
    deviceConnected = true;
    Serial.println("[BLE] Client connected");
  }

  void onDisconnect(BLEServer* pServer) override {
    deviceConnected = false;
    Serial.println("[BLE] Client disconnected");
  }
};

// =========================
// 유틸 함수
// =========================
uint8_t tempToIndex(float temp, float tMin, float tMax) {
  if (tMax <= tMin) return 0;

  if (temp < tMin) temp = tMin;
  if (temp > tMax) temp = tMax;

  float ratio = (temp - tMin) / (tMax - tMin); // 0.0 ~ 1.0
  int value = (int)(ratio * 255.0f + 0.5f);

  if (value < 0) value = 0;
  if (value > 255) value = 255;

  return (uint8_t)value;
}

void updateAutoRange(float* frame, float& outMin, float& outMax) {
  float rawMin = 1000.0f;
  float rawMax = -1000.0f;

  for (int i = 0; i < PIXEL_COUNT; i++) {
    if (frame[i] < rawMin) rawMin = frame[i];
    if (frame[i] > rawMax) rawMax = frame[i];
  }

  // 약간 여유를 둬서 색이 꽉 차지 않게
  float targetMin = rawMin - 1.0f;
  float targetMax = rawMax + 1.0f;

  // 범위가 너무 좁으면 최소 span 보장
  float minSpan = 8.0f;
  float span = targetMax - targetMin;
  if (span < minSpan) {
    float center = (targetMin + targetMax) * 0.5f;
    targetMin = center - minSpan * 0.5f;
    targetMax = center + minSpan * 0.5f;
  }

  if (firstRange) {
    outMin = targetMin;
    outMax = targetMax;
    firstRange = false;
  } else {
    // 부드럽게 변화하도록 smoothing
    outMin = outMin * 0.85f + targetMin * 0.15f;
    outMax = outMax * 0.85f + targetMax * 0.15f;
  }
}

void convertFrameToIndex(float* src, uint8_t* dst, float tMin, float tMax) {
  for (int i = 0; i < PIXEL_COUNT; i++) {
    dst[i] = tempToIndex(src[i], tMin, tMax);
  }
}

// =========================
// BLE 패킷 전송
// =========================
// 패킷 구조:
// [0]  0xAA
// [1]  0x55
// [2]  frameId low
// [3]  frameId high
// [4]  chunkId
// [5]  totalChunks
// [6]  width   (=32)
// [7]  height  (=24)
// [8]  tMin x10 low   (signed int16)
// [9]  tMin x10 high
// [10] tMax x10 low   (signed int16)
// [11] tMax x10 high
// [12] payloadLen
// [13...] payload (0~255 index data)

void sendIndexFrameBLE(uint8_t* frameData, uint16_t currentFrameId, float tMin, float tMax) {
  const uint16_t totalSize = PIXEL_COUNT; // 768 bytes
  const uint8_t totalChunks = (totalSize + CHUNK_PAYLOAD_SIZE - 1) / CHUNK_PAYLOAD_SIZE;

  int16_t tMin10 = (int16_t)(tMin * 10.0f);
  int16_t tMax10 = (int16_t)(tMax * 10.0f);

  uint8_t packet[13 + CHUNK_PAYLOAD_SIZE];

  for (uint8_t chunkId = 0; chunkId < totalChunks; chunkId++) {
    if (!deviceConnected) return;

    uint16_t offset = chunkId * CHUNK_PAYLOAD_SIZE;
    uint8_t payloadLen = (uint8_t)min((int)CHUNK_PAYLOAD_SIZE, (int)(totalSize - offset));

    packet[0]  = 0xAA;
    packet[1]  = 0x55;
    packet[2]  = lowByte(currentFrameId);
    packet[3]  = highByte(currentFrameId);
    packet[4]  = chunkId;
    packet[5]  = totalChunks;
    packet[6]  = THERMAL_WIDTH;
    packet[7]  = THERMAL_HEIGHT;
    packet[8]  = lowByte(tMin10);
    packet[9]  = highByte(tMin10);
    packet[10] = lowByte(tMax10);
    packet[11] = highByte(tMax10);
    packet[12] = payloadLen;

    memcpy(&packet[13], &frameData[offset], payloadLen);

    pDataChar->setValue(packet, 13 + payloadLen);
    pDataChar->notify();

    // 휴대폰이 놓치지 않도록 아주 짧은 간격
    delay(5);
  }
}

// =========================
// BLE 초기화
// =========================
void setupBLE() {
  BLEDevice::init(DEVICE_NAME);

  // MTU 크게 시도
  BLEDevice::setMTU(247);

  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService* pService = pServer->createService(SERVICE_UUID);

  pDataChar = pService->createCharacteristic(
    DATA_CHAR_UUID,
    BLECharacteristic::PROPERTY_NOTIFY
  );

  pDataChar->addDescriptor(new BLE2902());

  pService->start();

  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->start();

  Serial.println("[BLE] Advertising started");
}

// =========================
// MLX90640 초기화
// =========================
void setupMLX90640() {
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);

  if (!mlx.begin(MLX90640_I2CADDR_DEFAULT, &Wire)) {
    Serial.println("[MLX90640] Sensor not found!");
    while (1) {
      delay(1000);
    }
  }

  Serial.println("[MLX90640] Sensor found");

  mlx.setMode(MLX90640_CHESS);
  mlx.setResolution(MLX90640_ADC_18BIT);

  // BLE 전송 안정성을 위해 일단 2Hz 추천
  mlx.setRefreshRate(MLX90640_2_HZ);

  Serial.println("[MLX90640] Ready");
}

// =========================
// setup
// =========================
void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("=== XIAO ESP32S3 + MLX90640 BLE Thermal Sender ===");

  setupMLX90640();
  setupBLE();
}

// =========================
// loop
// =========================
void loop() {
  // 연결 상태 처리
  if (!deviceConnected && oldDeviceConnected) {
    delay(500);
    pServer->startAdvertising();
    Serial.println("[BLE] Restart advertising");
    oldDeviceConnected = deviceConnected;
  }

  if (deviceConnected && !oldDeviceConnected) {
    oldDeviceConnected = deviceConnected;
  }

  // 프레임 읽기
  if (mlx.getFrame(tempFrame) != 0) {
    Serial.println("[MLX90640] Frame read failed");
    delay(100);
    return;
  }

  // 온도 범위 결정
  if (useAutoRange) {
    updateAutoRange(tempFrame, displayMinTemp, displayMaxTemp);
  } else {
    displayMinTemp = fixedMinTemp;
    displayMaxTemp = fixedMaxTemp;
  }

  // 0~255 인덱스로 변환
  convertFrameToIndex(tempFrame, indexFrame, displayMinTemp, displayMaxTemp);

  // BLE로 전송
  if (deviceConnected) {
    sendIndexFrameBLE(indexFrame, frameId, displayMinTemp, displayMaxTemp);

    Serial.print("[SEND] frameId=");
    Serial.print(frameId);
    Serial.print("  tMin=");
    Serial.print(displayMinTemp, 1);
    Serial.print("  tMax=");
    Serial.println(displayMaxTemp, 1);

    frameId++;
  } else {
    static unsigned long lastMsg = 0;
    if (millis() - lastMsg > 2000) {
      Serial.println("[BLE] Waiting for client...");
      lastMsg = millis();
    }
  }

  // 2Hz니까 너무 짧게 돌릴 필요 없음
  delay(20);
}
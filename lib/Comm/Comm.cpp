#include "Comm.h"

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include "esp_camera.h"

// ===============================
// OwlGuard BLE UUID
// Android 앱 코드와 반드시 동일해야 함
// ===============================
#define OWL_SERVICE_UUID        "d10045a3-16e8-41cb-a720-19f9f20dce98"

#define STATUS_CHAR_UUID        "19758804-2826-41ab-ae18-63d9e11807e1"

#define IMAGE_META_CHAR_UUID    "6c6869ee-9392-4425-bd02-c5d2aeb026ee"
#define IMAGE_DATA_CHAR_UUID    "50c63f63-af08-4ebb-8738-7fc09fc8d713"

// 새로 추가한 열화상 BLE Characteristic UUID
#define THERMAL_META_CHAR_UUID  "f9ee6d11-9076-49a6-9976-008123c87bd3"
#define THERMAL_DATA_CHAR_UUID  "79dc4891-2ce8-4e9a-af50-77550f0f2274"

static const size_t CHUNK_SIZE = 180;
static const unsigned long SEND_INTERVAL_MS = 20;

// BLE 객체
static BLEServer* bleServer = nullptr;
static BLECharacteristic* statusChar = nullptr;

static BLECharacteristic* imageMetaChar = nullptr;
static BLECharacteristic* imageDataChar = nullptr;

static BLECharacteristic* thermalMetaChar = nullptr;
static BLECharacteristic* thermalDataChar = nullptr;

// 연결 상태
static bool deviceConnected = false;

// 이미지 전송 버퍼
static uint8_t* frameBuffer = nullptr;
static size_t frameSize = 0;
static size_t imageSendOffset = 0;
static size_t imageTotalChunks = 0;
static bool imageSending = false;
static bool imageBeginSent = false;
static bool imageEndSent = false;

// 열화상 전송 버퍼
static uint8_t* thermalBuffer = nullptr;
static size_t thermalSize = 0;
static size_t thermalSendOffset = 0;
static size_t thermalTotalChunks = 0;
static size_t thermalChunkIndex = 0;
static bool thermalSending = false;
static bool thermalBeginSent = false;
static bool thermalEndSent = false;
static float thermalMinTemp = 0.0f;
static float thermalMaxTemp = 0.0f;
static uint16_t thermalFrameId = 0;

static unsigned long lastSendTime = 0;

// ===============================
// BLE 연결/해제 콜백
// ===============================
class OwlServerCallbacks : public BLEServerCallbacks {
public:
  void onConnect(BLEServer* server) override {
    deviceConnected = true;
    Serial.println("[BLE] Client connected");
  }

  void onDisconnect(BLEServer* server) override {
    deviceConnected = false;
    Serial.println("[BLE] Client disconnected");
    BLEDevice::startAdvertising();
  }
};

namespace Comm {

void begin() {
  Serial.println("[Comm] BLE begin");

  BLEDevice::init("OwlGuard");
  BLEDevice::setMTU(200);

  bleServer = BLEDevice::createServer();
  bleServer->setCallbacks(new OwlServerCallbacks());

  BLEService* service = bleServer->createService(OWL_SERVICE_UUID);

  statusChar = service->createCharacteristic(
    STATUS_CHAR_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );
  statusChar->addDescriptor(new BLE2902());
  statusChar->setValue("READY");

  imageMetaChar = service->createCharacteristic(
    IMAGE_META_CHAR_UUID,
    BLECharacteristic::PROPERTY_NOTIFY
  );
  imageMetaChar->addDescriptor(new BLE2902());

  imageDataChar = service->createCharacteristic(
    IMAGE_DATA_CHAR_UUID,
    BLECharacteristic::PROPERTY_NOTIFY
  );
  imageDataChar->addDescriptor(new BLE2902());

  thermalMetaChar = service->createCharacteristic(
    THERMAL_META_CHAR_UUID,
    BLECharacteristic::PROPERTY_NOTIFY
  );
  thermalMetaChar->addDescriptor(new BLE2902());

  thermalDataChar = service->createCharacteristic(
    THERMAL_DATA_CHAR_UUID,
    BLECharacteristic::PROPERTY_NOTIFY
  );
  thermalDataChar->addDescriptor(new BLE2902());

  service->start();

  BLEAdvertising* advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(OWL_SERVICE_UUID);
  advertising->setScanResponse(true);
  advertising->start();

  Serial.println("[Comm] BLE advertising started: OwlGuard");
  delay(500);
  Serial.println("[Comm] BLE begin done");
}

bool isConnected() {
  return deviceConnected;
}

void setFrame(camera_fb_t* fb) {
  if (fb == nullptr || fb->buf == nullptr || fb->len == 0) {
    Serial.println("[Comm] setFrame failed: invalid frame");
    return;
  }

  if (frameBuffer != nullptr) {
    free(frameBuffer);
    frameBuffer = nullptr;
  }

  frameSize = fb->len;
  frameBuffer = (uint8_t*)malloc(frameSize);

  if (frameBuffer == nullptr) {
    Serial.println("[Comm] setFrame failed: malloc failed");
    esp_camera_fb_return(fb);
    return;
  }

  memcpy(frameBuffer, fb->buf, frameSize);
  esp_camera_fb_return(fb);

  imageSendOffset = 0;
  imageTotalChunks = (frameSize + CHUNK_SIZE - 1) / CHUNK_SIZE;
  imageSending = true;
  imageBeginSent = false;
  imageEndSent = false;
  lastSendTime = 0;

  Serial.print("[Comm] Image frame stored. Size: ");
  Serial.print(frameSize);
  Serial.print(" bytes, chunks: ");
  Serial.println(imageTotalChunks);
}

void setThermalFrame(const uint8_t* data, size_t len, float minTemp, float maxTemp) {
  if (data == nullptr || len == 0) {
    Serial.println("[Comm] setThermalFrame failed: invalid data");
    return;
  }

  if (thermalBuffer != nullptr) {
    free(thermalBuffer);
    thermalBuffer = nullptr;
  }

  thermalBuffer = (uint8_t*)malloc(len);

  if (thermalBuffer == nullptr) {
    Serial.println("[Comm] setThermalFrame failed: malloc failed");
    thermalSize = 0;
    return;
  }

  memcpy(thermalBuffer, data, len);

  thermalSize = len;
  thermalMinTemp = minTemp;
  thermalMaxTemp = maxTemp;

  thermalSendOffset = 0;
  thermalTotalChunks = (thermalSize + CHUNK_SIZE - 1) / CHUNK_SIZE;
  thermalChunkIndex = 0;
  thermalSending = true;
  thermalBeginSent = false;
  thermalEndSent = false;
  lastSendTime = 0;

  Serial.print("[Comm] Thermal frame stored. Size: ");
  Serial.print(thermalSize);
  Serial.print(" bytes, chunks: ");
  Serial.println(thermalTotalChunks);
}

static void handleImageSend() {
  if (!imageSending || frameBuffer == nullptr || frameSize == 0) {
    return;
  }

  if (!imageBeginSent) {
    String meta = "IMG_BEGIN,";
    meta += String(frameSize);
    meta += ",";
    meta += String(imageTotalChunks);

    imageMetaChar->setValue(meta.c_str());
    imageMetaChar->notify();

    statusChar->setValue("IMG_SENDING");
    statusChar->notify();

    imageBeginSent = true;

    Serial.print("[Comm] ");
    Serial.println(meta);
    return;
  }

  if (imageSendOffset < frameSize) {
    size_t remain = frameSize - imageSendOffset;
    size_t chunkLen = remain > CHUNK_SIZE ? CHUNK_SIZE : remain;

    imageDataChar->setValue(frameBuffer + imageSendOffset, chunkLen);
    imageDataChar->notify();

    imageSendOffset += chunkLen;

    Serial.print("[Comm] IMG Sent ");
    Serial.print(imageSendOffset);
    Serial.print(" / ");
    Serial.println(frameSize);
    return;
  }

  if (!imageEndSent) {
    imageMetaChar->setValue("IMG_END");
    imageMetaChar->notify();

    statusChar->setValue("IMG_DONE");
    statusChar->notify();

    imageEndSent = true;
    Serial.println("[Comm] IMG_END");
    return;
  }

  free(frameBuffer);
  frameBuffer = nullptr;
  frameSize = 0;
  imageSendOffset = 0;
  imageTotalChunks = 0;
  imageSending = false;
  imageBeginSent = false;
  imageEndSent = false;

  Serial.println("[Comm] Image transfer complete");
}

static void handleThermalSend() {
  if (!thermalSending || thermalBuffer == nullptr || thermalSize == 0) {
    return;
  }

  if (!thermalBeginSent) {
    String meta = "THM_BEGIN,";
    meta += String(thermalFrameId);
    meta += ",";
    meta += String(32);
    meta += ",";
    meta += String(24);
    meta += ",";
    meta += String(thermalSize);
    meta += ",";
    meta += String(thermalTotalChunks);
    meta += ",";
    meta += String(thermalMinTemp, 1);
    meta += ",";
    meta += String(thermalMaxTemp, 1);

    thermalMetaChar->setValue(meta.c_str());
    thermalMetaChar->notify();

    statusChar->setValue("THM_SENDING");
    statusChar->notify();

    thermalBeginSent = true;

    Serial.print("[Comm] ");
    Serial.println(meta);
    return;
  }

  if (thermalSendOffset < thermalSize) {
  size_t remain = thermalSize - thermalSendOffset;
  size_t chunkLen = remain > CHUNK_SIZE ? CHUNK_SIZE : remain;

  /*
    Thermal data packet 구조:

    [0] frameId low byte
    [1] frameId high byte
    [2] chunkId
    [3] totalChunks
    [4] payloadLen
    [5...] thermal index payload

    payload는 0~255로 변환된 MLX90640 데이터
  */

  uint8_t packet[5 + CHUNK_SIZE];

  packet[0] = lowByte(thermalFrameId);
  packet[1] = highByte(thermalFrameId);
  packet[2] = (uint8_t)thermalChunkIndex;
  packet[3] = (uint8_t)thermalTotalChunks;
  packet[4] = (uint8_t)chunkLen;

  memcpy(&packet[5], thermalBuffer + thermalSendOffset, chunkLen);

  thermalDataChar->setValue(packet, 5 + chunkLen);
  thermalDataChar->notify();

  thermalSendOffset += chunkLen;

  Serial.print("[Comm] THM Sent frameId=");
  Serial.print(thermalFrameId);
  Serial.print(" chunk=");
  Serial.print(thermalChunkIndex);
  Serial.print("/");
  Serial.print(thermalTotalChunks);
  Serial.print(" bytes=");
  Serial.print(thermalSendOffset);
  Serial.print("/");
  Serial.println(thermalSize);

  thermalChunkIndex++;

  return;
}

  if (!thermalEndSent) {
    String endMsg = "THM_END,";
    endMsg += String(thermalFrameId);

    thermalMetaChar->setValue(endMsg.c_str());
    thermalMetaChar->notify();

    statusChar->setValue("THM_DONE");
    statusChar->notify();

    thermalEndSent = true;
    Serial.println("[Comm] THM_END");
    return;
  }

  free(thermalBuffer);
  thermalBuffer = nullptr;
  thermalSize = 0;
  thermalSendOffset = 0;
  thermalTotalChunks = 0;
  thermalChunkIndex = 0;
  thermalSending = false;
  thermalBeginSent = false;
  thermalEndSent = false;
  thermalFrameId++;

  Serial.println("[Comm] Thermal transfer complete");
}

void handleClient() {
  if (!deviceConnected) {
    return;
  }

  unsigned long now = millis();

  if (now - lastSendTime < SEND_INTERVAL_MS) {
    return;
  }

  lastSendTime = now;

  // 이미지 전송이 있으면 먼저 전송하고, 그다음 열화상 전송
  if (imageSending) {
    handleImageSend();
    return;
  }

  if (thermalSending) {
    handleThermalSend();
    return;
  }

  statusChar->setValue("READY");
}

}
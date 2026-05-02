#include "Comm.h"

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// ===============================
// OwlGuard BLE UUID
// Android 앱 코드와 반드시 동일해야 함
// ===============================
#define OWL_SERVICE_UUID        "8b7f0001-2f3a-4a6d-9b7a-0a1b2c3d0001"
#define STATUS_CHAR_UUID        "8b7f0003-2f3a-4a6d-9b7a-0a1b2c3d0003"
#define IMAGE_META_CHAR_UUID    "8b7f0004-2f3a-4a6d-9b7a-0a1b2c3d0004"
#define IMAGE_DATA_CHAR_UUID    "8b7f0005-2f3a-4a6d-9b7a-0a1b2c3d0005"

// BLE 전송 chunk 크기
// Android 앱에서 MTU 517 요청을 전제로 180 bytes 사용
static const size_t CHUNK_SIZE = 180;
static const unsigned long SEND_INTERVAL_MS = 10;

// BLE 객체
static BLEServer* bleServer = nullptr;
static BLECharacteristic* statusChar = nullptr;
static BLECharacteristic* imageMetaChar = nullptr;
static BLECharacteristic* imageDataChar = nullptr;

// 연결 상태
static bool deviceConnected = false;

// 이미지 전송 버퍼
static uint8_t* frameBuffer = nullptr;
static size_t frameSize = 0;
static size_t sendOffset = 0;
static size_t totalChunks = 0;
static bool sending = false;
static bool beginSent = false;
static bool endSent = false;
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

        // 재연결 가능하게 광고 재시작
        BLEDevice::startAdvertising();
    }
};

namespace Comm {

void begin() {
    Serial.println("[Comm] BLE begin");

    BLEDevice::init("OwlGuard");

    bleServer = BLEDevice::createServer();
    bleServer->setCallbacks(new OwlServerCallbacks());

    BLEService* service = bleServer->createService(OWL_SERVICE_UUID);

    // 상태 전송용 characteristic
    statusChar = service->createCharacteristic(
        STATUS_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_NOTIFY
    );
    statusChar->addDescriptor(new BLE2902());
    statusChar->setValue("READY");

    // 이미지 메타데이터 전송용 characteristic
    imageMetaChar = service->createCharacteristic(
        IMAGE_META_CHAR_UUID,
        BLECharacteristic::PROPERTY_NOTIFY
    );
    imageMetaChar->addDescriptor(new BLE2902());

    // 이미지 데이터 chunk 전송용 characteristic
    imageDataChar = service->createCharacteristic(
        IMAGE_DATA_CHAR_UUID,
        BLECharacteristic::PROPERTY_NOTIFY
    );
    imageDataChar->addDescriptor(new BLE2902());

    service->start();

    BLEAdvertising* advertising = BLEDevice::getAdvertising();
    advertising->addServiceUUID(OWL_SERVICE_UUID);
    advertising->setScanResponse(true);

    BLEDevice::startAdvertising();

    Serial.println("[Comm] BLE advertising started: OwlGuard");
}

bool isConnected() {
    return deviceConnected;
}

void setFrame(camera_fb_t* fb) {
    if (fb == nullptr || fb->buf == nullptr || fb->len == 0) {
        Serial.println("[Comm] setFrame failed: invalid frame");
        return;
    }

    // 이전 전송 버퍼가 있으면 제거
    if (frameBuffer != nullptr) {
        free(frameBuffer);
        frameBuffer = nullptr;
    }

    frameSize = fb->len;
    frameBuffer = (uint8_t*)malloc(frameSize);

    if (frameBuffer == nullptr) {
        Serial.println("[Comm] setFrame failed: malloc failed");

        // 카메라 프레임 반환
        esp_camera_fb_return(fb);
        return;
    }

    memcpy(frameBuffer, fb->buf, frameSize);

    // 카메라 프레임은 바로 반환해야 다음 촬영이 가능함
    esp_camera_fb_return(fb);

    sendOffset = 0;
    totalChunks = (frameSize + CHUNK_SIZE - 1) / CHUNK_SIZE;
    sending = true;
    beginSent = false;
    endSent = false;
    lastSendTime = 0;

    Serial.print("[Comm] Frame stored. Size: ");
    Serial.print(frameSize);
    Serial.print(" bytes, chunks: ");
    Serial.println(totalChunks);
}

void handleClient() {
    if (!deviceConnected) {
        return;
    }

    if (!sending || frameBuffer == nullptr || frameSize == 0) {
        return;
    }

    unsigned long now = millis();

    if (now - lastSendTime < SEND_INTERVAL_MS) {
        return;
    }

    lastSendTime = now;

    // 1. IMG_BEGIN 전송
    if (!beginSent) {
        String meta = "IMG_BEGIN,";
        meta += String(frameSize);
        meta += ",";
        meta += String(totalChunks);

        imageMetaChar->setValue(meta.c_str());
        imageMetaChar->notify();

        statusChar->setValue("SENDING");
        statusChar->notify();

        beginSent = true;

        Serial.print("[Comm] ");
        Serial.println(meta);
        return;
    }

    // 2. JPEG 데이터 chunk 전송
    if (sendOffset < frameSize) {
        size_t remain = frameSize - sendOffset;
        size_t chunkLen = remain > CHUNK_SIZE ? CHUNK_SIZE : remain;

        imageDataChar->setValue(frameBuffer + sendOffset, chunkLen);
        imageDataChar->notify();

        sendOffset += chunkLen;

        Serial.print("[Comm] Sent ");
        Serial.print(sendOffset);
        Serial.print(" / ");
        Serial.println(frameSize);

        return;
    }

    // 3. IMG_END 전송
    if (!endSent) {
        imageMetaChar->setValue("IMG_END");
        imageMetaChar->notify();

        statusChar->setValue("DONE");
        statusChar->notify();

        endSent = true;

        Serial.println("[Comm] IMG_END");
        return;
    }

    // 4. 전송 완료 후 버퍼 정리
    free(frameBuffer);
    frameBuffer = nullptr;

    frameSize = 0;
    sendOffset = 0;
    totalChunks = 0;
    sending = false;
    beginSent = false;
    endSent = false;

    Serial.println("[Comm] Transfer complete");
}

}

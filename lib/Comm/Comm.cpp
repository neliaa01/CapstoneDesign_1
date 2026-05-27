#include "Comm.h"

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

namespace Comm {

#define OWL_SERVICE_UUID        "d10045a3-16e8-41cb-a720-19f9f20dce98"
#define STATUS_CHAR_UUID        "19758804-2826-41ab-ae18-63d9e11807e1"
#define IMAGE_META_CHAR_UUID    "6c6869ee-9392-4425-bd02-c5d2aeb026ee"
#define IMAGE_DATA_CHAR_UUID    "50c63f63-af08-4ebb-8738-7fc09fc8d713"

#define THERMAL_META_CHAR_UUID  "f9ee6d11-9076-49a6-9976-008123c87bd3"
#define THERMAL_DATA_CHAR_UUID  "79dc4891-2ce8-4e9a-af50-77550f0f2274"

static BLEServer* bleServer = nullptr;
static BLEService* owlService = nullptr;

static BLECharacteristic* statusChar = nullptr;
static BLECharacteristic* imageMetaChar = nullptr;
static BLECharacteristic* imageDataChar = nullptr;

static BLECharacteristic* thermalMetaChar = nullptr;
static BLECharacteristic* thermalDataChar = nullptr;

static bool deviceConnected = false;

static const size_t IMAGE_CHUNK_SIZE = 180;
static const size_t THERMAL_CHUNK_SIZE = 180;

class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* server) override {
        deviceConnected = true;
        Serial.println("[Comm] BLE client connected");

        if (statusChar != nullptr) {
            statusChar->setValue("READY");
            statusChar->notify();
        }
    }

    void onDisconnect(BLEServer* server) override {
        deviceConnected = false;
        Serial.println("[Comm] BLE client disconnected");

        delay(300);
        BLEDevice::startAdvertising();
        Serial.println("[Comm] BLE advertising restarted");
    }
};

static void notifyText(BLECharacteristic* characteristic, const String& text) {
    if (!deviceConnected || characteristic == nullptr) {
        return;
    }

    characteristic->setValue(text.c_str());
    characteristic->notify();

    delay(20);
}

static void notifyBytes(
    BLECharacteristic* characteristic,
    const uint8_t* data,
    size_t length
) {
    if (!deviceConnected || characteristic == nullptr || data == nullptr || length == 0) {
        return;
    }

    characteristic->setValue((uint8_t*)data, length);
    characteristic->notify();

    delay(10);
}

static void sendStatus(const String& status) {
    notifyText(statusChar, status);
    Serial.println("[Comm] Status: " + status);
}

void begin() {
    Serial.println("[Comm] BLE begin");

    BLEDevice::init("OwlGuard");

    bleServer = BLEDevice::createServer();
    bleServer->setCallbacks(new ServerCallbacks());

    owlService = bleServer->createService(OWL_SERVICE_UUID);

    statusChar = owlService->createCharacteristic(
        STATUS_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_NOTIFY
    );
    statusChar->addDescriptor(new BLE2902());

    imageMetaChar = owlService->createCharacteristic(
        IMAGE_META_CHAR_UUID,
        BLECharacteristic::PROPERTY_NOTIFY
    );
    imageMetaChar->addDescriptor(new BLE2902());

    imageDataChar = owlService->createCharacteristic(
        IMAGE_DATA_CHAR_UUID,
        BLECharacteristic::PROPERTY_NOTIFY
    );
    imageDataChar->addDescriptor(new BLE2902());

    thermalMetaChar = owlService->createCharacteristic(
        THERMAL_META_CHAR_UUID,
        BLECharacteristic::PROPERTY_NOTIFY
    );
    thermalMetaChar->addDescriptor(new BLE2902());

    // 중요:
    // 이 descriptor가 없으면 앱에서
    // "CCCD descriptor 없음: 79dc4891-2ce8-4e9a-af50-77550f0f2274"
    // 로그가 뜨고 열화상 chunk Notify를 못 받음.
    thermalDataChar = owlService->createCharacteristic(
        THERMAL_DATA_CHAR_UUID,
        BLECharacteristic::PROPERTY_NOTIFY
    );
    thermalDataChar->addDescriptor(new BLE2902());

    owlService->start();

    BLEAdvertising* advertising = BLEDevice::getAdvertising();
    advertising->addServiceUUID(OWL_SERVICE_UUID);
    advertising->setScanResponse(true);
    advertising->setMinPreferred(0x06);
    advertising->setMinPreferred(0x12);

    BLEDevice::startAdvertising();

    Serial.println("[Comm] BLE advertising started");
}

void handleClient() {
    // 현재 구조에서는 별도 반복 처리 필요 없음.
    // main loop에서 기존 코드 호환용으로 호출해도 문제 없음.
}

bool isConnected() {
    return deviceConnected;
}

void setFrame(camera_fb_t* fb) {
    if (fb == nullptr) {
        Serial.println("[Comm] camera frame is null");
        return;
    }

    if (!deviceConnected) {
        Serial.println("[Comm] No BLE client connected");
        return;
    }

    if (imageMetaChar == nullptr || imageDataChar == nullptr) {
        Serial.println("[Comm] image characteristics are null");
        return;
    }

    const uint8_t* imageData = fb->buf;
    const size_t imageSize = fb->len;

    if (imageData == nullptr || imageSize == 0) {
        Serial.println("[Comm] image data is empty");
        return;
    }

    const size_t totalChunks =
        (imageSize + IMAGE_CHUNK_SIZE - 1) / IMAGE_CHUNK_SIZE;

    Serial.printf(
        "[Comm] JPEG send start: %u bytes, %u chunks\n",
        (unsigned int)imageSize,
        (unsigned int)totalChunks
    );

    sendStatus("IMG_SENDING");

    String beginMeta = "IMG_BEGIN," + String(imageSize) + "," + String(totalChunks);
    notifyText(imageMetaChar, beginMeta);

    size_t offset = 0;
    size_t chunkIndex = 0;

    while (offset < imageSize && deviceConnected) {
        size_t remaining = imageSize - offset;
        size_t chunkSize = remaining > IMAGE_CHUNK_SIZE
            ? IMAGE_CHUNK_SIZE
            : remaining;

        notifyBytes(
            imageDataChar,
            imageData + offset,
            chunkSize
        );

        offset += chunkSize;
        chunkIndex++;

        if (chunkIndex % 20 == 0) {
            Serial.printf(
                "[Comm] JPEG chunk sent: %u / %u\n",
                (unsigned int)chunkIndex,
                (unsigned int)totalChunks
            );
        }
    }

    notifyText(imageMetaChar, "IMG_END");
    sendStatus("IMG_DONE");

    Serial.println("[Comm] JPEG send done");
}

void sendThermalFrame(
    const uint8_t* thermalIndexFrame,
    size_t dataSize,
    int frameId,
    int width,
    int height,
    float minTemp,
    float maxTemp
) {
    if (!deviceConnected) {
        Serial.println("[Comm] No BLE client connected for thermal frame");
        return;
    }

    if (thermalMetaChar == nullptr || thermalDataChar == nullptr) {
        Serial.println("[Comm] thermal characteristics are null");
        return;
    }

    if (thermalIndexFrame == nullptr || dataSize == 0) {
        Serial.println("[Comm] thermal data is empty");
        return;
    }

    const size_t expectedSize = (size_t)width * (size_t)height;

    if (dataSize < expectedSize) {
        Serial.printf(
            "[Comm] thermal data size too small: %u / %u\n",
            (unsigned int)dataSize,
            (unsigned int)expectedSize
        );
        return;
    }

    const size_t totalChunks =
        (dataSize + THERMAL_CHUNK_SIZE - 1) / THERMAL_CHUNK_SIZE;

    Serial.printf(
        "[Comm] Thermal send start: frameId=%d, %dx%d, %u bytes, %u chunks, min=%.2f, max=%.2f\n",
        frameId,
        width,
        height,
        (unsigned int)dataSize,
        (unsigned int)totalChunks,
        minTemp,
        maxTemp
    );

    sendStatus("THM_SENDING");

    String beginMeta =
        "THM_BEGIN," +
        String(frameId) + "," +
        String(width) + "," +
        String(height) + "," +
        String(dataSize) + "," +
        String(totalChunks) + "," +
        String(minTemp, 1) + "," +
        String(maxTemp, 1);

    notifyText(thermalMetaChar, beginMeta);

    size_t offset = 0;
    size_t chunkIndex = 0;

    while (offset < dataSize && deviceConnected) {
        size_t remaining = dataSize - offset;
        size_t chunkSize = remaining > THERMAL_CHUNK_SIZE
            ? THERMAL_CHUNK_SIZE
            : remaining;

        notifyBytes(
            thermalDataChar,
            thermalIndexFrame + offset,
            chunkSize
        );

        offset += chunkSize;
        chunkIndex++;

        Serial.printf(
            "[Comm] Thermal chunk sent: %u / %u, size=%u\n",
            (unsigned int)chunkIndex,
            (unsigned int)totalChunks,
            (unsigned int)chunkSize
        );
    }

    String endMeta = "THM_END," + String(frameId);
    notifyText(thermalMetaChar, endMeta);

    sendStatus("THM_DONE");

    Serial.println("[Comm] Thermal send done");
}

}

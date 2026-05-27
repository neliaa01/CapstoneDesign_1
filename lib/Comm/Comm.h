#pragma once

#include <Arduino.h>
#include "esp_camera.h"

namespace Comm {
    void begin();
    void handleClient();

    bool isConnected();

    // 일반 카메라 JPEG 프레임 전송
    void setFrame(camera_fb_t* fb);

    // MLX90640 열화상 데이터 전송
    // thermalIndexFrame: 0~255로 변환된 uint8_t[768]
    void sendThermalFrame(
        const uint8_t* thermalIndexFrame,
        size_t dataSize,
        int frameId,
        int width,
        int height,
        float minTemp,
        float maxTemp
    );
}

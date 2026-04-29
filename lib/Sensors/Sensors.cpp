#include "Sensors.h"

void Sensors::begin() {
    camera_config_t config;

    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;

    // 반드시 보드에 맞게 핀 설정 필요
    config.frame_size = FRAMESIZE_SVGA;  // 800x600
    config.pixel_format = PIXFORMAT_JPEG;

    config.fb_count = 1;

    esp_camera_init(&config);
}

camera_fb_t* Sensors::captureImage() {
    return esp_camera_fb_get();
}
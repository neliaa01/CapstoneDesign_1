#include "camera_test.h"
#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"
#include <Arduino.h>

// ===== 와이파이 =====
const char* ssid = "iPhone (4)";
const char* password = "01290129";

// ===== 카메라 핀 =====
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     10
#define SIOD_GPIO_NUM     40
#define SIOC_GPIO_NUM     39

#define Y9_GPIO_NUM       48
#define Y8_GPIO_NUM       11
#define Y7_GPIO_NUM       12
#define Y6_GPIO_NUM       14
#define Y5_GPIO_NUM       16
#define Y4_GPIO_NUM       18
#define Y3_GPIO_NUM       17
#define Y2_GPIO_NUM       15
#define VSYNC_GPIO_NUM    38
#define HREF_GPIO_NUM     47
#define PCLK_GPIO_NUM     13

// ===== 내부 함수 =====
void startCameraServer();

// ===== 🔥 setup → 변경 =====
void cameraSetup() {

    Serial.begin(115200);

    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;

    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;

    if (esp_camera_init(&config) != ESP_OK) {
        Serial.println("Camera init failed");
        return;
    }

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
    }

    Serial.println("WiFi connected");

    startCameraServer();

    Serial.print("Camera URL: http://");
    Serial.println(WiFi.localIP());
}

// ===== 🔥 loop → 변경 =====
void cameraLoop() {
    // 지금은 스트리밍 서버가 알아서 돌아감
}

// ===== 스트리밍 서버 =====
static esp_err_t stream_handler(httpd_req_t *req){
    camera_fb_t * fb = NULL;

    httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=frame");

    while(true){
        fb = esp_camera_fb_get();
        if (!fb) continue;

        httpd_resp_send_chunk(req, "--frame\r\n", strlen("--frame\r\n"));
        httpd_resp_send_chunk(req, "Content-Type: image/jpeg\r\n\r\n",
                              strlen("Content-Type: image/jpeg\r\n\r\n"));
        httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
        httpd_resp_send_chunk(req, "\r\n", strlen("\r\n"));

        esp_camera_fb_return(fb);
    }

    return ESP_OK;
}

void startCameraServer(){
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    httpd_start(&server, &config);

    httpd_uri_t uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = stream_handler,
        .user_ctx = NULL
    };

    httpd_register_uri_handler(server, &uri);
}
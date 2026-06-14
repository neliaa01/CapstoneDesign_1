#include "Sensors.h"

#include <Arduino.h>
#include <Wire.h>
#include "esp_camera.h"
#include <Adafruit_MLX90640.h>

// ===============================
// XIAO ESP32S3 Sense camera pin map
// ===============================
#define PWDN_GPIO_NUM   -1
#define RESET_GPIO_NUM  -1
#define XCLK_GPIO_NUM   10
#define SIOD_GPIO_NUM   40
#define SIOC_GPIO_NUM   39

#define Y9_GPIO_NUM     48
#define Y8_GPIO_NUM     11
#define Y7_GPIO_NUM     12
#define Y6_GPIO_NUM     14
#define Y5_GPIO_NUM     16
#define Y4_GPIO_NUM     18
#define Y3_GPIO_NUM     17
#define Y2_GPIO_NUM     15
#define VSYNC_GPIO_NUM  38
#define HREF_GPIO_NUM   47
#define PCLK_GPIO_NUM   13

// ===============================
// MLX90640 I2C pin
// XIAO ESP32S3: D4=GPIO5 SDA, D5=GPIO6 SCL
// ===============================
#define MLX_SDA_PIN 5
#define MLX_SCL_PIN 6

#define THERMAL_WIDTH   32
#define THERMAL_HEIGHT  24
#define THERMAL_PIXELS  (THERMAL_WIDTH * THERMAL_HEIGHT)

static Adafruit_MLX90640 mlx;
static bool cameraReady = false;
static bool thermalReady = false;
static bool firstRange = true;

static float thermalFrame[THERMAL_PIXELS];
static float displayMinTemp = 20.0f;
static float displayMaxTemp = 35.0f;

static void printMemoryStatus(const char* label) {
  Serial.print("[Sensors] ");
  Serial.print(label);
  Serial.print(" free heap=");
  Serial.print(ESP.getFreeHeap());
  Serial.print(", free PSRAM=");
  Serial.println(ESP.getFreePsram());
}

static camera_config_t makeCameraConfig() {
  camera_config_t config = {};

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

  config.xclk_freq_hz = 10000000;
  config.frame_size = FRAMESIZE_QVGA;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  return config;
}

static bool beginCameraOnly() {
  Serial.println("[Sensors] Camera init start");

  camera_config_t config = makeCameraConfig();
  esp_err_t err = esp_camera_init(&config);

  if (err != ESP_OK) {
    cameraReady = false;
    Serial.print("[Sensors] Camera init failed: 0x");
    Serial.println(err, HEX);
    return false;
  }

  cameraReady = true;
  Serial.println("[Sensors] Camera init success");
  return true;
}

static uint8_t tempToIndex(float temp, float tMin, float tMax) {
  if (tMax <= tMin) return 0;

  if (temp < tMin) temp = tMin;
  if (temp > tMax) temp = tMax;

  float ratio = (temp - tMin) / (tMax - tMin);
  int value = (int)(ratio * 255.0f + 0.5f);

  if (value < 0) value = 0;
  if (value > 255) value = 255;

  return (uint8_t)value;
}

static void updateAutoRange(const float* frame, float& outMin, float& outMax) {
  float rawMin = 1000.0f;
  float rawMax = -1000.0f;

  for (int i = 0; i < THERMAL_PIXELS; i++) {
    if (frame[i] < rawMin) rawMin = frame[i];
    if (frame[i] > rawMax) rawMax = frame[i];
  }

  float targetMin = rawMin - 1.0f;
  float targetMax = rawMax + 1.0f;

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
    outMin = outMin * 0.85f + targetMin * 0.15f;
    outMax = outMax * 0.85f + targetMax * 0.15f;
  }
}

void Sensors::begin() {
  beginCameraOnly();
  beginThermal();
}

bool Sensors::restartCamera() {
  Serial.println("[Sensors] Camera restart start");
  printMemoryStatus("before restart");

  if (cameraReady) {
    esp_err_t err = esp_camera_deinit();
    if (err != ESP_OK) {
      Serial.print("[Sensors] Camera deinit warning: 0x");
      Serial.println(err, HEX);
    }
    cameraReady = false;
    delay(300);
  }

  bool ok = beginCameraOnly();
  delay(300);
  printMemoryStatus("after restart");

  if (ok) {
    Serial.println("[Sensors] Camera restart success");
  } else {
    Serial.println("[Sensors] Camera restart failed");
  }

  return ok;
}

bool Sensors::beginThermal() {
  Serial.println("[Sensors] MLX90640 init start");

  Wire.begin(MLX_SDA_PIN, MLX_SCL_PIN);
  Wire.setClock(100000);

  if (!mlx.begin(MLX90640_I2CADDR_DEFAULT, &Wire)) {
    Serial.println("[Sensors] MLX90640 not found");
    thermalReady = false;
    return false;
  }

  mlx.setMode(MLX90640_CHESS);
  mlx.setResolution(MLX90640_ADC_18BIT);
  mlx.setRefreshRate(MLX90640_2_HZ);

  thermalReady = true;
  firstRange = true;

  Serial.println("[Sensors] MLX90640 init success");
  return true;
}

camera_fb_t* Sensors::captureImage() {
  if (!cameraReady) {
    Serial.println("[Sensors] Camera is not ready, restarting");
    if (!restartCamera()) {
      return nullptr;
    }
  }

  camera_fb_t* fb = esp_camera_fb_get();

  if (fb == nullptr) {
    Serial.println("[Sensors] Camera capture failed, restarting camera");
    if (!restartCamera()) {
      return nullptr;
    }

    Serial.println("[Sensors] Retrying camera capture");
    fb = esp_camera_fb_get();

    if (fb == nullptr) {
      Serial.println("[Sensors] Camera capture failed after restart");
      return nullptr;
    }
  }

  Serial.print("[Sensors] Camera capture success, size: ");
  Serial.println(fb->len);

  return fb;
}

bool Sensors::captureThermalIndex(uint8_t* outIndexFrame, float* outMinTemp, float* outMaxTemp) {
  if (!thermalReady) {
    Serial.println("[Sensors] Thermal not ready, retry init");
    if (!beginThermal()) {
      return false;
    }
  }

  if (outIndexFrame == nullptr || outMinTemp == nullptr || outMaxTemp == nullptr) {
    Serial.println("[Sensors] Thermal output buffer invalid");
    return false;
  }

  if (mlx.getFrame(thermalFrame) != 0) {
    Serial.println("[Sensors] MLX90640 frame read failed");
    return false;
  }

  updateAutoRange(thermalFrame, displayMinTemp, displayMaxTemp);

  for (int i = 0; i < THERMAL_PIXELS; i++) {
    outIndexFrame[i] = tempToIndex(thermalFrame[i], displayMinTemp, displayMaxTemp);
  }

  *outMinTemp = displayMinTemp;
  *outMaxTemp = displayMaxTemp;

  Serial.print("[Sensors] Thermal captured. tMin=");
  Serial.print(displayMinTemp, 1);
  Serial.print(", tMax=");
  Serial.println(displayMaxTemp, 1);

  return true;
}

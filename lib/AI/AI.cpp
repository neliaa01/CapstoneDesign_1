#include "AI.h"

#include <Arduino.h>
#include "esp_camera.h"
#include "img_converters.h"

// 원본 카메라 해상도
// 현재 Sensors.cpp에서 FRAMESIZE_QVGA를 쓰고 있다면 320x240
#define SRC_WIDTH   320
#define SRC_HEIGHT  240

namespace AI {

// JPEG frame → RGB888 전체 이미지로 디코딩
static bool decodeJpegToRGB888(camera_fb_t* fb, uint8_t** outRgb) {
  if (fb == nullptr || fb->buf == nullptr || fb->len == 0) {
    Serial.println("[AI] Invalid camera frame");
    return false;
  }

  // QVGA 기준 320 * 240 * 3 = 230400 bytes
  size_t rgbSize = SRC_WIDTH * SRC_HEIGHT * 3;

  uint8_t* rgb = (uint8_t*)ps_malloc(rgbSize);
  if (rgb == nullptr) {
    Serial.println("[AI] RGB buffer allocation failed");
    return false;
  }

  bool ok = fmt2rgb888(
    fb->buf,
    fb->len,
    fb->format,
    rgb
  );

  if (!ok) {
    Serial.println("[AI] JPEG to RGB888 conversion failed");
    free(rgb);
    return false;
  }

  *outRgb = rgb;
  return true;
}

// JPEG → resize → grayscale uint8
bool makeGrayscaleInput(
  camera_fb_t* fb,
  uint8_t* outInput,
  int inputW,
  int inputH
) {
  if (outInput == nullptr) {
    Serial.println("[AI] Grayscale output buffer is null");
    return false;
  }

  uint8_t* rgb = nullptr;

  if (!decodeJpegToRGB888(fb, &rgb)) {
    return false;
  }

  for (int y = 0; y < inputH; y++) {
    int srcY = y * SRC_HEIGHT / inputH;

    for (int x = 0; x < inputW; x++) {
      int srcX = x * SRC_WIDTH / inputW;

      int srcIndex = (srcY * SRC_WIDTH + srcX) * 3;

      uint8_t r = rgb[srcIndex + 0];
      uint8_t g = rgb[srcIndex + 1];
      uint8_t b = rgb[srcIndex + 2];

      // 일반적인 grayscale 변환
      uint8_t gray = (uint8_t)(
        0.299f * r +
        0.587f * g +
        0.114f * b
      );

      outInput[y * inputW + x] = gray;
    }
  }

  free(rgb);

  Serial.println("[AI] Grayscale input created");
  return true;
}

// JPEG → resize → RGB uint8
bool makeRGBInput(
  camera_fb_t* fb,
  uint8_t* outInput,
  int inputW,
  int inputH
) {
  if (outInput == nullptr) {
    Serial.println("[AI] RGB output buffer is null");
    return false;
  }

  uint8_t* rgb = nullptr;

  if (!decodeJpegToRGB888(fb, &rgb)) {
    return false;
  }

  for (int y = 0; y < inputH; y++) {
    int srcY = y * SRC_HEIGHT / inputH;

    for (int x = 0; x < inputW; x++) {
      int srcX = x * SRC_WIDTH / inputW;

      int srcIndex = (srcY * SRC_WIDTH + srcX) * 3;
      int dstIndex = (y * inputW + x) * 3;

      outInput[dstIndex + 0] = rgb[srcIndex + 0]; // R
      outInput[dstIndex + 1] = rgb[srcIndex + 1]; // G
      outInput[dstIndex + 2] = rgb[srcIndex + 2]; // B
    }
  }

  free(rgb);

  Serial.println("[AI] RGB input created");
  return true;
}

}
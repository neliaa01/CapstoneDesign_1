#pragma once

#include "esp_camera.h"
#include <Arduino.h>

namespace Sensors {
  void begin();

  camera_fb_t* captureImage();

  bool beginThermal();
  bool captureThermalIndex(uint8_t* outIndexFrame, float* outMinTemp, float* outMaxTemp);
}
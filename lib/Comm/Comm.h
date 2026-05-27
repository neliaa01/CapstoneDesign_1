#pragma once

#include "esp_camera.h"
#include <Arduino.h>

namespace Comm {
  void begin();
  void handleClient();

  bool isConnected();

  void setFrame(camera_fb_t* fb);

  void setThermalFrame(const uint8_t* data, size_t len, float minTemp, float maxTemp);
}
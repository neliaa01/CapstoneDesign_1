#pragma once

#include <Arduino.h>
#include "esp_camera.h"

namespace AI {
  bool makeGrayscaleInput(
    camera_fb_t* fb,
    uint8_t* outInput,
    int inputW,
    int inputH
  );

  bool makeRGBInput(
    camera_fb_t* fb,
    uint8_t* outInput,
    int inputW,
    int inputH
  );
}
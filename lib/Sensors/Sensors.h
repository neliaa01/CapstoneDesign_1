#pragma once
#include "esp_camera.h"

namespace Sensors {
    void begin();
    camera_fb_t* captureImage();
}
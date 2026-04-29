#pragma once
#include "esp_camera.h"

namespace First_Detection {
    float detect(camera_fb_t* image);
}
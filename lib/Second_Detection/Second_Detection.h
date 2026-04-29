#pragma once
#include "esp_camera.h"

namespace Second_Detection {
    float refine(camera_fb_t* image, float thermal);
}
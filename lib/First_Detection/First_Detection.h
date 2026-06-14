#pragma once

#include "esp_camera.h"

namespace FirstDetection {

    struct Result {
        bool detected;
        float probability;
    };

    bool begin();

    Result run(camera_fb_t* fb);

}
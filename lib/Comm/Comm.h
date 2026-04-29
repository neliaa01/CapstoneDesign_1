#pragma once
#include "esp_camera.h"

namespace Comm {
    void begin();
    void handleClient();

    void setFrame(camera_fb_t* fb);
}
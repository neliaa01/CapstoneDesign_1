#include "First_Detection.h"

#include <Arduino.h>
#include <vector>
#include <algorithm>
#include <math.h>

#include "img_converters.h"

namespace FirstDetection {

// ================================
// Logistic Regression Parameters
// ================================

static const float MEAN[8] = {
    244.21428571428572f,
    208.69151992394188f,
    52.98961278521827f,
    39.3205321330092f,
    1366.2698412698412f,
    1170.031746031746f,
    1.0269589319448578f,
    0.6882805736131904f
};

static const float SCALE[8] = {
    27.49923726008814f,
    47.24697864352277f,
    27.761547898076618f,
    12.77454553116549f,
    1859.0093065363774f,
    1780.8480270423863f,
    0.8643726004470302f,
    0.3163768233775306f
};

static const float COEF[8] = {
    2.9713123628213385f,
   -1.068590509128137f,
   -1.6658326237805072f,
    1.3813962099567423f,
   -0.2484661697336458f,
    0.04743146897053318f,
   -0.47506698363465577f,
   -0.8339609235037054f
};

static const float BIAS =
   -0.9706228607052986f;


// ================================
// Sigmoid
// ================================

static float sigmoid(float x)
{
    return 1.0f / (1.0f + expf(-x));
}

// ================================
// Main
// ================================

bool begin()
{
    return true;
}

Result run(camera_fb_t* fb)
{
    Result result;
    result.detected = false;
    result.probability = 0.0f;

    if (!fb)
        return result;

    const int width = 320;
    const int height = 240;

    size_t rgbSize = width * height * 3;

    uint8_t* rgb888 =
        (uint8_t*)malloc(rgbSize);

    if (!rgb888)
        return result;

    bool ok = fmt2rgb888(
        fb->buf,
        fb->len,
        PIXFORMAT_JPEG,
        rgb888
    );

    if (!ok) {
        free(rgb888);
        return result;
    }

    // ============================
    // Feature Extraction
    // ============================

    uint8_t maxIntensity = 0;

    double sum = 0.0;
    double sumSq = 0.0;

    uint32_t brightPixelCount = 0;

    std::vector<uint8_t> grayPixels;
    grayPixels.reserve(width * height);

    int minX = width;
    int minY = height;

    int maxX = 0;
    int maxY = 0;

    uint32_t largestBlobArea = 0;

    for (int y = 0; y < height; y++) {

        for (int x = 0; x < width; x++) {

            int idx = (y * width + x) * 3;

            uint8_t r = rgb888[idx + 0];
            uint8_t g = rgb888[idx + 1];
            uint8_t b = rgb888[idx + 2];

            uint8_t gray =
                (uint8_t)(
                    0.299f * r +
                    0.587f * g +
                    0.114f * b
                );

            grayPixels.push_back(gray);

            if (gray > maxIntensity)
                maxIntensity = gray;

            sum += gray;
            sumSq += gray * gray;

            if (gray > 220) {

                brightPixelCount++;

                if (x < minX) minX = x;
                if (y < minY) minY = y;

                if (x > maxX) maxX = x;
                if (y > maxY) maxY = y;
            }
        }
    }

    const int totalPixels =
        width * height;

    float meanIntensity =
        sum / totalPixels;

    float variance =
        (sumSq / totalPixels) -
        (meanIntensity * meanIntensity);

    if (variance < 0)
        variance = 0;

    float stdIntensity =
        sqrtf(variance);

    // ============================
    // Top 1%
    // ============================

    std::sort(
        grayPixels.begin(),
        grayPixels.end()
    );

    int start =
        grayPixels.size() * 0.99f;

    double topSum = 0;

    int count = 0;

    for (size_t i = start;
         i < grayPixels.size();
         i++) {

        topSum += grayPixels[i];
        count++;
    }

    float top1Mean =
        count > 0 ?
        topSum / count :
        0;

    // ============================
    // Bounding Box 기반
    // ============================

    float aspectRatio = 0;

    if (brightPixelCount > 0) {

        int w =
            maxX - minX + 1;

        int h =
            maxY - minY + 1;

        largestBlobArea =
            w * h;

        if (h > 0)
            aspectRatio =
                (float)w / h;
    }

    float concentration = 0;

    if (brightPixelCount > 0) {

        concentration =
            (float)largestBlobArea /
            brightPixelCount;
    }

    free(rgb888);

    // ============================
    // Feature Vector
    // ============================

    float feature[8] = {

        (float)maxIntensity,
        top1Mean,
        meanIntensity,
        stdIntensity,

        (float)brightPixelCount,
        (float)largestBlobArea,

        aspectRatio,
        concentration
    };

    // ============================
    // Logistic Regression
    // ============================

    float z = BIAS;

    for (int i = 0; i < 8; i++) {

        float norm =
            (feature[i] - MEAN[i]) /
            SCALE[i];

        z += norm * COEF[i];
    }

    float probability =
        sigmoid(z);

    result.probability =
        probability;

    result.detected =
        (probability >= 0.5f);

    Serial.println("----- AI -----");

    for (int i = 0; i < 8; i++) {
        Serial.print("F");
        Serial.print(i);
        Serial.print(": ");
        Serial.println(feature[i]);
    }

    Serial.print("Probability: ");
    Serial.println(probability);

    Serial.print("Detected: ");
    Serial.println(
        result.detected ?
        "YES" : "NO"
    );

    return result;
}

}
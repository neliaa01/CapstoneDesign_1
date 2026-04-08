#include "Second_Detection.h"
#include <math.h>

#define CENTER_RADIUS_RATIO 0.2
#define RING_RADIUS_RATIO 0.4

struct Features2 {
    float center_darkness;
    float ring_brightness;
    float center_ring_ratio;

    float local_diff_intensity;

    float radial_symmetry;
    float highlight_peak;
    float edge_strength;

    float blob_roundness;

    float temperature_diff;
};

// ===== Feature Extraction =====
Features2 extractFeatures2(uint8_t* on, uint8_t* off, int w, int h, float temp) {
    Features2 f = {0};

    int size = w * h;

    float cx = w / 2.0;
    float cy = h / 2.0;

    float center_r = w * CENTER_RADIUS_RATIO;
    float ring_r = w * RING_RADIUS_RATIO;

    float center_sum = 0, ring_sum = 0;
    int center_cnt = 0, ring_cnt = 0;

    float diff_sum = 0;

    float mean = 0;
    int max_val = 0;

    // ===== 1차 루프 =====
    for (int i = 0; i < size; i++) {
        int x = i % w;
        int y = i / w;

        float dx = x - cx;
        float dy = y - cy;
        float dist = sqrt(dx*dx + dy*dy);

        int val = on[i];
        int diff = on[i] - off[i];
        if (diff < 0) diff = 0;

        mean += val;
        diff_sum += diff;

        if (val > max_val) max_val = val;

        if (dist < center_r) {
            center_sum += val;
            center_cnt++;
        }
        else if (dist < ring_r) {
            ring_sum += val;
            ring_cnt++;
        }
    }

    mean /= size;

    f.center_darkness = center_cnt ? center_sum / center_cnt : 0;
    f.ring_brightness = ring_cnt ? ring_sum / ring_cnt : 0;

    f.center_ring_ratio = (f.ring_brightness + 1) / (f.center_darkness + 1);

    f.local_diff_intensity = diff_sum / size;

    f.highlight_peak = max_val - mean;

    // ===== symmetry =====
    float sym = 0;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w / 2; x++) {
            int i1 = y * w + x;
            int i2 = y * w + (w - x - 1);

            sym += abs(on[i1] - on[i2]);
        }
    }
    f.radial_symmetry = sym / size;

    // ===== edge =====
    float edge = 0;
    for (int i = 0; i < size - 1; i++) {
        edge += abs(on[i] - on[i + 1]);
    }
    f.edge_strength = edge / size;

    // ===== roundness (간단 근사) =====
    int min_x = w, max_x = 0, min_y = h, max_y = 0;

    for (int i = 0; i < size; i++) {
        if (on[i] > mean) {
            int x = i % w;
            int y = i / w;

            if (x < min_x) min_x = x;
            if (x > max_x) max_x = x;
            if (y < min_y) min_y = y;
            if (y > max_y) max_y = y;
        }
    }

    int bw = max_x - min_x + 1;
    int bh = max_y - min_y + 1;

    if (bh != 0)
        f.blob_roundness = (float)bw / bh;
    else
        f.blob_roundness = 0;

    f.temperature_diff = temp;

    return f;
}

// ===== Logistic Regression =====
float runLogistic2(Features2 f) {
    float z =
        -1.5
        + 1.2 * f.center_ring_ratio
        + 0.8 * f.local_diff_intensity
        - 0.7 * f.radial_symmetry
        + 0.9 * f.highlight_peak
        + 0.6 * f.edge_strength
        - 0.5 * abs(f.blob_roundness - 1.0);

    return 1.0 / (1.0 + exp(-z));
}

// ===== 외부 API =====
float analyzeDataPrecision(uint8_t* img_on, uint8_t* img_off, int w, int h, float temp) {
    Features2 f = extractFeatures2(img_on, img_off, w, h, temp);
    return runLogistic2(f);
}
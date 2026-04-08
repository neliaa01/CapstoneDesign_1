#include "First_Detection.h"
#include <math.h>

// ===== 설정값 =====
#define DIFF_THRESHOLD 40
#define BRIGHT_THRESHOLD 200
#define MAX_PIXELS 19200   // 160x120 기준
#define TOP_K 20

// ===== 내부 구조체 =====
struct Features {
    int diff_high_pixel_count;
    int diff_max_intensity;
    int diff_top_k_sum;

    int small_blob_count;
    int smallest_blob_size;

    float spatial_variance;
    float cluster_density;

    float diff_ratio;
    float local_ratio;

    float center_proximity;

    float temperature_diff;
};

// ===== 유틸 =====
static inline int abs_int(int x) {
    return x > 0 ? x : -x;
}

// ===== 핵심: Feature 추출 =====
Features extractFeatures(uint8_t* img_on, uint8_t* img_off, int width, int height, float temp) {
    Features f = {0};

    int size = width * height;

    int sum_on = 0;
    int sum_off = 0;
    int sum_diff = 0;

    int topk[TOP_K] = {0};

    int bright_x_sum = 0;
    int bright_y_sum = 0;

    int bright_count = 0;

    // ===== 1차 루프 =====
    for (int i = 0; i < size; i++) {
        int diff = img_on[i] - img_off[i];
        if (diff < 0) diff = 0;

        sum_on += img_on[i];
        sum_off += img_off[i];
        sum_diff += diff;

        // diff high count
        if (diff > DIFF_THRESHOLD) {
            f.diff_high_pixel_count++;

            // 좌표 계산
            int x = i % width;
            int y = i / width;

            bright_x_sum += x;
            bright_y_sum += y;
            bright_count++;
        }

        // max diff
        if (diff > f.diff_max_intensity)
            f.diff_max_intensity = diff;

        // top-k 유지 (단순 삽입)
        for (int k = 0; k < TOP_K; k++) {
            if (diff > topk[k]) {
                int temp = topk[k];
                topk[k] = diff;
                diff = temp;
            }
        }
    }

    // ===== top-k sum =====
    for (int i = 0; i < TOP_K; i++)
        f.diff_top_k_sum += topk[i];

    // ===== ratio =====
    f.diff_ratio = (float)(sum_on - sum_off) / (float)(sum_off + 1);

    // ===== 중심 계산 =====
    float cx = width / 2.0;
    float cy = height / 2.0;

    float avg_x = bright_count ? (float)bright_x_sum / bright_count : cx;
    float avg_y = bright_count ? (float)bright_y_sum / bright_count : cy;

    float dist = sqrt((avg_x - cx)*(avg_x - cx) + (avg_y - cy)*(avg_y - cy));
    f.center_proximity = 1.0 / (1.0 + dist);

    // ===== spatial variance =====
    float var = 0;
    if (bright_count > 0) {
        for (int i = 0; i < size; i++) {
            int diff = img_on[i] - img_off[i];
            if (diff > DIFF_THRESHOLD) {
                int x = i % width;
                int y = i / width;

                float dx = x - avg_x;
                float dy = y - avg_y;

                var += dx*dx + dy*dy;
            }
        }
        var /= bright_count;
    }
    f.spatial_variance = var;

    // ===== cluster density =====
    f.cluster_density = (float)bright_count / (float)(size + 1);

    // ===== local ratio =====
    float local_on = 0;
    float local_off = 0;

    for (int i = 0; i < size; i++) {
        int diff = img_on[i] - img_off[i];
        if (diff > DIFF_THRESHOLD) {
            local_on += img_on[i];
            local_off += img_off[i];
        }
    }

    f.local_ratio = (local_on + 1) / (local_off + 1);

    // ===== 온도 =====
    f.temperature_diff = temp; // 필요하면 baseline 빼기

    // ===== blob (초간단 근사) =====
    int current_blob = 0;
    int min_blob = 999999;

    for (int i = 0; i < size; i++) {
        int diff = img_on[i] - img_off[i];

        if (diff > DIFF_THRESHOLD) {
            current_blob++;
        } else {
            if (current_blob > 0) {
                if (current_blob < 50) { // 작은 blob만
                    f.small_blob_count++;
                    if (current_blob < min_blob)
                        min_blob = current_blob;
                }
                current_blob = 0;
            }
        }
    }

    f.smallest_blob_size = (min_blob == 999999) ? 0 : min_blob;

    return f;
}

// ===== Logistic Regression (예시) =====
float runLogistic(Features f) {
    float z =
        -2.0
        + 0.8 * f.diff_high_pixel_count
        + 1.2 * f.diff_max_intensity
        + 0.5 * f.diff_top_k_sum
        + 0.7 * f.small_blob_count
        - 0.3 * f.spatial_variance
        + 0.9 * f.local_ratio;

    float prob = 1.0 / (1.0 + exp(-z));
    return prob;
}

// ===== 외부 함수 =====

// 1차 탐지
float analyzeData(uint8_t* img_on, uint8_t* img_off, int w, int h, float temp) {
    Features f = extractFeatures(img_on, img_off, w, h, temp);
    return runLogistic(f);
}


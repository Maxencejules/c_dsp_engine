#include "dsp.h"
#include <math.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static float rand_normal() {
    // Box Muller
    float u1 = (rand() + 1.0f) / (RAND_MAX + 2.0f);
    float u2 = (rand() + 1.0f) / (RAND_MAX + 2.0f);
    float r = sqrtf(-2.0f * logf(u1));
    float theta = 2.0f * (float)M_PI * u2;
    return r * cosf(theta);
}

void generate_signal(float *buffer, size_t length, float fs, float freq, float noise_std) {
    for (size_t n = 0; n < length; ++n) {
        float t = (float)n / fs;
        float s = sinf(2.0f * (float)M_PI * freq * t);
        float noise = noise_std * rand_normal();
        buffer[n] = s + noise;
    }
}

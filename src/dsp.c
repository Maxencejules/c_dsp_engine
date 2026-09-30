#include "dsp.h"
#include <errno.h>
#include <math.h>

#define DSP_PI 3.14159265358979323846

static double uniform(uint32_t *state) {
    *state = UINT32_C(1664525) * *state + UINT32_C(1013904223);
    return ((double)*state + 0.5) / 4294967296.0;
}

int generate_signal(float *buffer, size_t length, float fs, float freq,
                    float noise_std, uint32_t seed) {
    if (!isfinite(fs) || fs <= 0 || !isfinite(freq) || freq < 0 || freq > fs / 2 ||
        !isfinite(noise_std) || noise_std < 0 || (length && !buffer)) return EINVAL;
    if (length > SIZE_MAX / sizeof(float)) return EOVERFLOW;
    uint32_t state = seed;
    for (size_t index = 0; index < length; ++index) {
        double noise = 0;
        if (noise_std > 0) {
            double first = uniform(&state), second = uniform(&state);
            noise = (double)noise_std * sqrt(-2 * log(first)) * cos(2 * DSP_PI * second);
        }
        double phase = 2 * DSP_PI * (double)freq * (double)index / (double)fs;
        buffer[index] = (float)(sin(phase) + noise);
    }
    return 0;
}

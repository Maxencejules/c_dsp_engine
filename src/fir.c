#include "fir.h"
#include <stdlib.h>
#include <string.h>

void fir_init(FirFilter *f, const float *coeffs, size_t num_taps) {
    f->coeffs = coeffs;
    f->num_taps = num_taps;
    if (num_taps > 1) {
        f->state = (float *)calloc(num_taps - 1, sizeof(float));
    } else {
        f->state = NULL;
    }
}

void fir_reset(FirFilter *f) {
    if (f->state && f->num_taps > 1) {
        memset(f->state, 0, (f->num_taps - 1) * sizeof(float));
    }
}

float fir_process_sample(FirFilter *f, float x) {
    size_t i;
    float y = 0.0f;

    // For simplicity, maintain a small circular buffer concept by shifting.
    if (f->num_taps > 1) {
        // Shift state right
        for (i = f->num_taps - 2; i > 0; --i) {
            f->state[i] = f->state[i - 1];
        }
        if (f->num_taps > 1) {
            f->state[0] = x;
        }
    }

    // Convolution: y = sum_{k=0..num_taps-1} h[k] * x[n-k]
    // x[n] is x, x[n-1] is state[0], x[n-2] is state[1], etc.
    y += f->coeffs[0] * x;
    for (i = 1; i < f->num_taps; ++i) {
        float s = (i - 1 < f->num_taps - 1) ? f->state[i - 1] : 0.0f;
        y += f->coeffs[i] * s;
    }

    return y;
}

void fir_process_block(FirFilter *f,
                       const float *input,
                       float *output,
                       size_t length) {
    for (size_t n = 0; n < length; ++n) {
        output[n] = fir_process_sample(f, input[n]);
    }
}

void fir_free(FirFilter *f) {
    if (f->state) {
        free(f->state);
        f->state = NULL;
    }
}

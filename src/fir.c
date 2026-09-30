#include "fir.h"
#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int valid(const FirFilter *filter) {
    return filter && filter->coeffs && filter->num_taps > 0 &&
           (filter->num_taps == 1 || filter->state);
}

int fir_init(FirFilter *filter, const float *coeffs, size_t num_taps) {
    if (!filter) return EINVAL;
    if (filter->coeffs || filter->state || filter->num_taps) return EBUSY;
    if (!coeffs || !num_taps) return EINVAL;
    if (num_taps - 1 > SIZE_MAX / sizeof(float)) return EOVERFLOW;
    float *state = num_taps > 1 ? calloc(num_taps - 1, sizeof(float)) : NULL;
    if (num_taps > 1 && !state) return ENOMEM;
    filter->coeffs = coeffs;
    filter->num_taps = num_taps;
    filter->state = state;
    return 0;
}

int fir_reset(FirFilter *filter) {
    if (!valid(filter)) return EINVAL;
    if (filter->state) memset(filter->state, 0, (filter->num_taps - 1) * sizeof(float));
    return 0;
}

float fir_process_sample(FirFilter *filter, float sample) {
    if (!valid(filter)) return NAN;
    float result = filter->coeffs[0] * sample;
    for (size_t tap = 1; tap < filter->num_taps; ++tap)
        result += filter->coeffs[tap] * filter->state[tap - 1];
    if (filter->num_taps > 1) {
        for (size_t index = filter->num_taps - 2; index > 0; --index)
            filter->state[index] = filter->state[index - 1];
        filter->state[0] = sample;
    }
    return result;
}

int fir_buffers_overlap(const float *input, const float *output, size_t length) {
    uintptr_t first = (uintptr_t)input, second = (uintptr_t)output;
    size_t bytes = length * sizeof(float); /* caller checks multiplication */
    return first <= second ? second - first < bytes : first - second < bytes;
}

int fir_process_block(FirFilter *filter, const float *input, float *output, size_t length) {
    if (!valid(filter)) return EINVAL;
    if (!length) return 0;
    if (!input || !output) return EINVAL;
    if (length > SIZE_MAX / sizeof(float)) return EOVERFLOW;
    if (input != output && fir_buffers_overlap(input, output, length)) return EINVAL;
    for (size_t index = 0; index < length; ++index)
        output[index] = fir_process_sample(filter, input[index]);
    return 0;
}

void fir_free(FirFilter *filter) {
    if (!filter) return;
    free(filter->state);
    *filter = (FirFilter){0};
}

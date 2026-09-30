#include "test_helpers.h"
#include "dsp.h"
#include <errno.h>
#include <stdint.h>
#include <string.h>
void test_dsp(void) {
    float signal[8], second[8];
    const float expected[] = {0, 1, 0, -1, 0, 1, 0, -1};
    CHECK(generate_signal(signal, 8, 8, 2, 0, 42) == 0);
    for (size_t index = 0; index < 8; ++index) NEAR(signal[index], expected[index], 1e-6);
    CHECK(generate_signal(signal, 8, 48000, 1000, 0.3f, 2026) == 0);
    CHECK(generate_signal(second, 8, 48000, 1000, 0.3f, 2026) == 0);
    CHECK(memcmp(signal, second, sizeof(signal)) == 0);
    CHECK(generate_signal(second, 8, 48000, 1000, 0.3f, 2027) == 0);
    CHECK(memcmp(signal, second, sizeof(signal)) != 0);
    CHECK(generate_signal(NULL, 0, 48000, 0, 0, 0) == 0);
    CHECK(generate_signal(NULL, 1, 48000, 0, 0, 0) == EINVAL);
    CHECK(generate_signal(signal, 8, 0, 0, 0, 0) == EINVAL);
    CHECK(generate_signal(signal, 8, NAN, 0, 0, 0) == EINVAL);
    CHECK(generate_signal(signal, 8, 48000, INFINITY, 0, 0) == EINVAL);
    CHECK(generate_signal(signal, 8, 48000, 24001, 0, 0) == EINVAL);
    CHECK(generate_signal(signal, 8, 48000, -1, 0, 0) == EINVAL);
    CHECK(generate_signal(signal, 8, 48000, 0, -1, 0) == EINVAL);
    CHECK(generate_signal(signal, 8, 48000, 0, NAN, 0) == EINVAL);
    CHECK(generate_signal(signal, SIZE_MAX / sizeof(float) + 1, 48000, 0, 0, 0) == EOVERFLOW);
    const size_t count = 100000;
    float *noise = malloc(count * sizeof(*noise));
    CHECK(noise != NULL);
    if (!noise) return;
    CHECK(generate_signal(noise, count, 48000, 0, 1, 42) == 0);
    double sum = 0, squares = 0;
    for (size_t index = 0; index < count; ++index) {
        CHECK(isfinite(noise[index]));
        sum += noise[index]; squares += (double)noise[index] * noise[index];
    }
    double mean = sum / (double)count;
    NEAR(mean, 0, 0.02);
    NEAR(sqrt(squares / (double)count - mean * mean), 1, 0.02);
    free(noise);
}
#include "test_helpers.h"
#include "../src/dsp.h"
#include <stdlib.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Pure sine with zero noise should match expected sine values. */
static int test_generate_pure_sine(void) {
    const size_t N = 480;
    const float fs = 48000.0f;
    const float freq = 1000.0f;

    float *buf = (float *)malloc(N * sizeof(float));
    ASSERT_TRUE(buf != NULL);

    generate_signal(buf, N, fs, freq, 0.0f);

    for (size_t i = 0; i < N; i++) {
        float t = (float)i / fs;
        float expected = sinf(2.0f * (float)M_PI * freq * t);
        ASSERT_FLOAT_NEAR(buf[i], expected, 1e-5f);
    }

    free(buf);
    return 0;
}

/* Zero length should not crash. */
static int test_generate_zero_length(void) {
    float dummy;
    generate_signal(&dummy, 0, 48000.0f, 1000.0f, 0.5f);
    return 0;
}

/* DC signal (freq=0): pure sine of 0 Hz is 0, so output is just noise. */
static int test_generate_dc_signal(void) {
    const size_t N = 1000;
    float *buf = (float *)malloc(N * sizeof(float));
    ASSERT_TRUE(buf != NULL);

    generate_signal(buf, N, 48000.0f, 0.0f, 0.0f);

    /* sin(0) = 0 for all samples, noise = 0 */
    for (size_t i = 0; i < N; i++) {
        ASSERT_FLOAT_NEAR(buf[i], 0.0f, 1e-6f);
    }

    free(buf);
    return 0;
}

/* Noisy signal: verify the mean is near 0 and std is reasonable. */
static int test_generate_noisy_signal(void) {
    const size_t N = 100000;
    float *buf = (float *)malloc(N * sizeof(float));
    ASSERT_TRUE(buf != NULL);

    srand(42);
    generate_signal(buf, N, 48000.0f, 0.0f, 1.0f);

    /* freq=0 means sine=0, so output is pure noise with std=1. */
    double sum = 0.0, sum_sq = 0.0;
    for (size_t i = 0; i < N; i++) {
        sum += buf[i];
        sum_sq += (double)buf[i] * (double)buf[i];
    }
    double mean = sum / N;
    double variance = sum_sq / N - mean * mean;
    double std = sqrt(variance);

    /* Mean should be near 0, std near 1. Allow generous tolerance. */
    ASSERT_DOUBLE_NEAR(mean, 0.0, 0.05);
    ASSERT_DOUBLE_NEAR(std, 1.0, 0.1);

    free(buf);
    return 0;
}

void run_dsp_tests(void) {
    TEST_SUITE("DSP Signal Generation");
    RUN_TEST(test_generate_pure_sine);
    RUN_TEST(test_generate_zero_length);
    RUN_TEST(test_generate_dc_signal);
    RUN_TEST(test_generate_noisy_signal);
}

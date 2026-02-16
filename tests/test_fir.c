#include "test_helpers.h"
#include "../src/fir.h"
#include <stdlib.h>
#include <string.h>

/* Impulse response: feeding [1, 0, 0, ...] should produce the coefficients. */
static int test_fir_impulse_response(void) {
    const float coeffs[] = {0.25f, 0.5f, 0.25f};
    const size_t num_taps = 3;

    FirFilter f;
    fir_init(&f, coeffs, num_taps);
    fir_reset(&f);

    float input[6] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float output[6];
    fir_process_block(&f, input, output, 6);

    ASSERT_FLOAT_NEAR(output[0], 0.25f, 1e-6f);
    ASSERT_FLOAT_NEAR(output[1], 0.5f, 1e-6f);
    ASSERT_FLOAT_NEAR(output[2], 0.25f, 1e-6f);
    ASSERT_FLOAT_NEAR(output[3], 0.0f, 1e-6f);
    ASSERT_FLOAT_NEAR(output[4], 0.0f, 1e-6f);
    ASSERT_FLOAT_NEAR(output[5], 0.0f, 1e-6f);

    fir_free(&f);
    return 0;
}

/* Single tap: output = coeff * input. */
static int test_fir_single_tap(void) {
    const float coeffs[] = {2.0f};

    FirFilter f;
    fir_init(&f, coeffs, 1);
    fir_reset(&f);

    float input[] = {1.0f, 3.0f, -0.5f};
    float output[3];
    fir_process_block(&f, input, output, 3);

    ASSERT_FLOAT_NEAR(output[0], 2.0f, 1e-6f);
    ASSERT_FLOAT_NEAR(output[1], 6.0f, 1e-6f);
    ASSERT_FLOAT_NEAR(output[2], -1.0f, 1e-6f);

    fir_free(&f);
    return 0;
}

/* DC gain: constant input should converge to sum(coeffs) * input. */
static int test_fir_dc_gain(void) {
    const float coeffs[] = {0.2f, 0.3f, 0.3f, 0.2f};
    const size_t num_taps = 4;
    const float dc_input = 5.0f;
    const float expected_dc = (0.2f + 0.3f + 0.3f + 0.2f) * dc_input;

    FirFilter f;
    fir_init(&f, coeffs, num_taps);
    fir_reset(&f);

    float input[20];
    float output[20];
    for (int i = 0; i < 20; i++) input[i] = dc_input;

    fir_process_block(&f, input, output, 20);

    /* After num_taps-1 samples, output should be at steady state. */
    for (int i = (int)(num_taps - 1); i < 20; i++) {
        ASSERT_FLOAT_NEAR(output[i], expected_dc, 1e-5f);
    }

    fir_free(&f);
    return 0;
}

/* State persistence: two consecutive blocks should produce the same result
   as one large block. */
static int test_fir_state_persistence(void) {
    const float coeffs[] = {0.25f, 0.5f, 0.25f};
    const size_t N = 20;

    float input[20];
    for (size_t i = 0; i < N; i++) input[i] = (float)i;

    /* Single block */
    float out_single[20];
    {
        FirFilter f;
        fir_init(&f, coeffs, 3);
        fir_reset(&f);
        fir_process_block(&f, input, out_single, N);
        fir_free(&f);
    }

    /* Two blocks without reset */
    float out_split[20];
    {
        FirFilter f;
        fir_init(&f, coeffs, 3);
        fir_reset(&f);
        fir_process_block(&f, input, out_split, 10);
        fir_process_block(&f, input + 10, out_split + 10, 10);
        fir_free(&f);
    }

    for (size_t i = 0; i < N; i++) {
        ASSERT_FLOAT_NEAR(out_single[i], out_split[i], 1e-6f);
    }

    return 0;
}

/* Reset: after reset, processing should match a fresh filter. */
static int test_fir_reset(void) {
    const float coeffs[] = {0.3f, 0.4f, 0.3f};
    float input[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    float out1[5], out2[5];

    FirFilter f;
    fir_init(&f, coeffs, 3);

    /* First pass */
    fir_reset(&f);
    fir_process_block(&f, input, out1, 5);

    /* Second pass after reset */
    fir_reset(&f);
    fir_process_block(&f, input, out2, 5);

    for (int i = 0; i < 5; i++) {
        ASSERT_FLOAT_NEAR(out1[i], out2[i], 1e-6f);
    }

    fir_free(&f);
    return 0;
}

/* Empty block: length 0 should not crash. */
static int test_fir_empty_block(void) {
    const float coeffs[] = {1.0f};
    FirFilter f;
    fir_init(&f, coeffs, 1);
    fir_reset(&f);

    fir_process_block(&f, NULL, NULL, 0);

    fir_free(&f);
    return 0;
}

void run_fir_tests(void) {
    TEST_SUITE("FIR Filter");
    RUN_TEST(test_fir_impulse_response);
    RUN_TEST(test_fir_single_tap);
    RUN_TEST(test_fir_dc_gain);
    RUN_TEST(test_fir_state_persistence);
    RUN_TEST(test_fir_reset);
    RUN_TEST(test_fir_empty_block);
}

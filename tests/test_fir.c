#include "test_helpers.h"
#include "fir.h"
#include "fault_hooks.h"
#include <errno.h>
#include <stdint.h>
#include <string.h>

void test_fir(void) {
    const size_t tap_counts[] = {1, 2, 5, 17, 31};
    const size_t lengths[] = {1, 4, 73, 103};
    const size_t chunks[] = {1, 2, 7, 257};
    float coefficients[31], input[140], output[140];
    for (size_t tap = 0; tap < 31; ++tap)
        coefficients[tap] = (float)((int)(tap % 7) - 3) / 17.0f;
    coefficients[0] = 0.35f;
    for (size_t index = 0; index < 140; ++index)
        input[index] = (float)sin((double)index * 0.37) + (float)(index % 5) / 9.0f;
    /* Fixed/irregular partitions, interleaved empty calls, zero-padded tails. */
    for (size_t ti = 0; ti < 5; ++ti) {
        size_t taps = tap_counts[ti];
        for (size_t ni = 0; ni < 4; ++ni) {
            size_t length = lengths[ni];
            for (size_t ci = 0; ci < 5; ++ci) {
                FirFilter filter = {0};
                CHECK(fir_init(&filter, coefficients, taps) == 0);
                size_t position = 0;
                while (position < length) {
                    size_t chunk = ci < 4 ? chunks[ci] : (position * 3 + 5) % 11 + 1;
                    if (chunk > length - position) chunk = length - position;
                    CHECK(fir_process_block(&filter, NULL, NULL, 0) == 0);
                    CHECK(fir_process_block(&filter, input + position, output + position, chunk) == 0);
                    position += chunk;
                }
                for (size_t index = 0; index < length; ++index)
                    NEAR(output[index], reference_sample(input, index, coefficients, taps), 2e-6);
                for (size_t tail = 0; tail + 1 < taps; ++tail) {
                    double expected = 0;
                    size_t index = length + tail;
                    for (size_t tap = 0; tap < taps; ++tap)
                        if (tap <= index && index - tap < length)
                            expected += (double)coefficients[tap] * (double)input[index - tap];
                    NEAR(fir_process_sample(&filter, 0), expected, 2e-6);
                }
                CHECK(fir_reset(&filter) == 0);
                NEAR(fir_process_sample(&filter, input[0]),
                     (double)coefficients[0] * input[0], 1e-7);
                fir_free(&filter);
                CHECK(!filter.coeffs && !filter.state && !filter.num_taps);
                fir_free(&filter);
            }
        }
    }
    const float taps[] = {0.25f, -0.5f, 0.75f};
    float impulse[] = {1, 0, 0, 0}, impulse_out[4];
    FirFilter filter = {0};
    CHECK(fir_init(&filter, taps, 3) == 0);
    CHECK(fir_process_block(&filter, impulse, impulse_out, 4) == 0);
    NEAR(impulse_out[0], 0.25, 0); NEAR(impulse_out[1], -0.5, 0);
    NEAR(impulse_out[2], 0.75, 0); NEAR(impulse_out[3], 0, 0);
    CHECK(fir_reset(&filter) == 0);
    NEAR(fir_process_sample(&filter, 1), 0.25, 0);
    NEAR(fir_process_sample(&filter, 1), -0.25, 0);
    NEAR(fir_process_sample(&filter, 1), 0.5, 0);
    CHECK(fir_reset(&filter) == 0);
    float inplace[73]; memcpy(inplace, input, sizeof(inplace));
    CHECK(fir_process_block(&filter, inplace, inplace, 73) == 0);
    for (size_t index = 0; index < 73; ++index)
        NEAR(inplace[index], reference_sample(input, index, taps, 3), 2e-6);
    CHECK(fir_reset(&filter) == 0);
    float overlap[] = {1, 2, 3, 4};
    CHECK(fir_process_block(&filter, overlap, overlap + 1, 3) == EINVAL);
    CHECK(overlap[0] == 1 && overlap[1] == 2 && overlap[2] == 3 && overlap[3] == 4);
    NEAR(fir_process_sample(&filter, 1), 0.25, 0); /* rejected block did not advance */
    CHECK(fir_init(&filter, taps, 3) == EBUSY);
    CHECK(fir_process_block(&filter, NULL, output, 1) == EINVAL);
    CHECK(fir_process_block(&filter, input, output, SIZE_MAX / sizeof(float) + 1) == EOVERFLOW);
    fir_free(&filter);
    CHECK(fir_init(NULL, taps, 3) == EINVAL);
    CHECK(fir_init(&filter, NULL, 3) == EINVAL);
    CHECK(fir_init(&filter, taps, 0) == EINVAL);
    CHECK(fir_init(&filter, taps, SIZE_MAX) == EOVERFLOW);
    CHECK(fir_reset(&filter) == EINVAL);
    CHECK(fir_process_block(&filter, NULL, NULL, 0) == EINVAL);
    CHECK(isnan(fir_process_sample(&filter, 1)));
    fir_free(NULL);
    fault_reset(); fault_fail_allocation(1);
    CHECK(fir_init(&filter, taps, 3) == ENOMEM);
    CHECK(!filter.coeffs && !filter.state && !filter.num_taps);
    fault_reset();
    CHECK(fir_init(&filter, taps, 3) == 0);
    fir_free(&filter);
}
#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
extern int tests_run, tests_failed;
#define CHECK(condition) do { \
    ++tests_run; \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
        ++tests_failed; \
    } \
} while (0)
#define NEAR(actual, expected, tolerance) do { \
    double test_actual = (double)(actual), test_expected = (double)(expected); \
    CHECK(isfinite(test_actual) && isfinite(test_expected) && \
          fabs(test_actual - test_expected) <= (double)(tolerance)); \
} while (0)
/* Directly index the original full input; use no production FIR/state code. */
static inline double reference_sample(const float *input, size_t index,
                                      const float *coeffs, size_t taps) {
    double result = 0;
    for (size_t tap = 0; tap < taps && tap <= index; ++tap)
        result += (double)coeffs[tap] * (double)input[index - tap];
    return result;
}
void test_fir(void);
void test_dsp(void);
void test_worker(void);
#endif
#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

#include <stdio.h>
#include <math.h>

extern int tests_run;
extern int tests_passed;
extern int tests_failed;

#define ASSERT_TRUE(expr)                                                     \
    do {                                                                      \
        if (!(expr)) {                                                        \
            printf("  FAIL: %s:%d: %s\n", __FILE__, __LINE__, #expr);         \
            return 1;                                                         \
        }                                                                     \
    } while (0)

#define ASSERT_FLOAT_NEAR(a, b, eps)                                          \
    do {                                                                      \
        float _a = (a), _b = (b), _eps = (eps);                               \
        if (fabsf(_a - _b) > _eps) {                                          \
            printf("  FAIL: %s:%d: %.8f != %.8f (eps=%.8f)\n",                \
                   __FILE__, __LINE__, (double)_a, (double)_b, (double)_eps); \
            return 1;                                                         \
        }                                                                     \
    } while (0)

#define ASSERT_DOUBLE_NEAR(a, b, eps)                                         \
    do {                                                                      \
        double _a = (a), _b = (b), _eps = (eps);                              \
        if (fabs(_a - _b) > _eps) {                                           \
            printf("  FAIL: %s:%d: %.12f != %.12f (eps=%.12f)\n",             \
                   __FILE__, __LINE__, _a, _b, _eps);                         \
            return 1;                                                         \
        }                                                                     \
    } while (0)

#define RUN_TEST(fn)                                                          \
    do {                                                                      \
        tests_run++;                                                          \
        printf("  %-50s ", #fn);                                              \
        if (fn() == 0) {                                                      \
            tests_passed++;                                                   \
            printf("PASS\n");                                                 \
        } else {                                                              \
            tests_failed++;                                                   \
        }                                                                     \
    } while (0)

#define TEST_SUITE(name)                                                      \
    printf("\n[%s]\n", name)

static inline void print_test_summary(void) {
    printf("\n========================================\n");
    printf("Tests run: %d  Passed: %d  Failed: %d\n",
           tests_run, tests_passed, tests_failed);
    printf("========================================\n");
}

#endif

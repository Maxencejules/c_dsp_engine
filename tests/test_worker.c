#include "test_helpers.h"
#include "../src/fir.h"
#include "../src/worker.h"
#include <stdlib.h>
#include <string.h>

static const float TEST_COEFFS[] = {0.1f, 0.2f, 0.4f, 0.2f, 0.1f};
static const size_t TEST_NUM_TAPS = 5;

/* Run single-threaded FIR on the full signal for reference output. */
static void reference_fir(const float *input, float *output, size_t N,
                          const float *coeffs, size_t num_taps) {
    FirFilter f;
    fir_init(&f, coeffs, num_taps);
    fir_reset(&f);
    fir_process_block(&f, input, output, N);
    fir_free(&f);
}

/* Run multithreaded FIR with the given thread count. */
static int run_multithreaded(const float *input, float *output, size_t N,
                             const float *coeffs, size_t num_taps,
                             int num_threads) {
    Worker *workers = (Worker *)malloc(num_threads * sizeof(Worker));
    if (!workers) return -1;

    size_t chunk = N / num_threads;

    for (int i = 0; i < num_threads; i++) {
        workers[i].job.input = input;
        workers[i].job.output = output;
        workers[i].job.start = i * chunk;
        workers[i].job.length = (i == num_threads - 1)
                                ? (N - i * chunk)
                                : chunk;
        workers[i].job.coeffs = coeffs;
        workers[i].job.num_taps = num_taps;

        if (worker_start(&workers[i]) != 0) {
            free(workers);
            return -1;
        }
    }

    for (int i = 0; i < num_threads; i++) {
        worker_join(&workers[i]);
    }

    free(workers);
    return 0;
}

static double compute_mse(const float *a, const float *b, size_t N) {
    double mse = 0.0;
    for (size_t i = 0; i < N; i++) {
        double diff = (double)a[i] - (double)b[i];
        mse += diff * diff;
    }
    return mse / (double)N;
}

/* Single vs multi equivalence with 2 threads. */
static int test_worker_2_threads(void) {
    const size_t N = 10000;
    float *input = (float *)malloc(N * sizeof(float));
    float *ref = (float *)malloc(N * sizeof(float));
    float *out = (float *)malloc(N * sizeof(float));
    ASSERT_TRUE(input && ref && out);

    srand(123);
    for (size_t i = 0; i < N; i++) input[i] = (float)(i % 100) * 0.01f;

    reference_fir(input, ref, N, TEST_COEFFS, TEST_NUM_TAPS);
    ASSERT_TRUE(run_multithreaded(input, out, N, TEST_COEFFS, TEST_NUM_TAPS, 2) == 0);

    double mse = compute_mse(ref, out, N);
    ASSERT_DOUBLE_NEAR(mse, 0.0, 1e-12);

    free(input); free(ref); free(out);
    return 0;
}

/* Single vs multi equivalence with 4 threads. */
static int test_worker_4_threads(void) {
    const size_t N = 10000;
    float *input = (float *)malloc(N * sizeof(float));
    float *ref = (float *)malloc(N * sizeof(float));
    float *out = (float *)malloc(N * sizeof(float));
    ASSERT_TRUE(input && ref && out);

    for (size_t i = 0; i < N; i++) input[i] = (float)(i % 100) * 0.01f;

    reference_fir(input, ref, N, TEST_COEFFS, TEST_NUM_TAPS);
    ASSERT_TRUE(run_multithreaded(input, out, N, TEST_COEFFS, TEST_NUM_TAPS, 4) == 0);

    double mse = compute_mse(ref, out, N);
    ASSERT_DOUBLE_NEAR(mse, 0.0, 1e-12);

    free(input); free(ref); free(out);
    return 0;
}

/* Single vs multi equivalence with 8 threads. */
static int test_worker_8_threads(void) {
    const size_t N = 10000;
    float *input = (float *)malloc(N * sizeof(float));
    float *ref = (float *)malloc(N * sizeof(float));
    float *out = (float *)malloc(N * sizeof(float));
    ASSERT_TRUE(input && ref && out);

    for (size_t i = 0; i < N; i++) input[i] = (float)(i % 100) * 0.01f;

    reference_fir(input, ref, N, TEST_COEFFS, TEST_NUM_TAPS);
    ASSERT_TRUE(run_multithreaded(input, out, N, TEST_COEFFS, TEST_NUM_TAPS, 8) == 0);

    double mse = compute_mse(ref, out, N);
    ASSERT_DOUBLE_NEAR(mse, 0.0, 1e-12);

    free(input); free(ref); free(out);
    return 0;
}

/* Single worker covering the full signal should match reference exactly. */
static int test_worker_single_thread(void) {
    const size_t N = 5000;
    float *input = (float *)malloc(N * sizeof(float));
    float *ref = (float *)malloc(N * sizeof(float));
    float *out = (float *)malloc(N * sizeof(float));
    ASSERT_TRUE(input && ref && out);

    for (size_t i = 0; i < N; i++) input[i] = (float)i * 0.001f;

    reference_fir(input, ref, N, TEST_COEFFS, TEST_NUM_TAPS);
    ASSERT_TRUE(run_multithreaded(input, out, N, TEST_COEFFS, TEST_NUM_TAPS, 1) == 0);

    double mse = compute_mse(ref, out, N);
    ASSERT_DOUBLE_NEAR(mse, 0.0, 1e-12);

    free(input); free(ref); free(out);
    return 0;
}

/* Small signal shorter than num_taps, split across 2 threads. */
static int test_worker_small_signal(void) {
    const size_t N = 4; /* smaller than TEST_NUM_TAPS (5) */
    float input[] = {1.0f, 2.0f, 3.0f, 4.0f};
    float ref[4], out[4];

    reference_fir(input, ref, N, TEST_COEFFS, TEST_NUM_TAPS);
    ASSERT_TRUE(run_multithreaded(input, out, N, TEST_COEFFS, TEST_NUM_TAPS, 2) == 0);

    double mse = compute_mse(ref, out, N);
    ASSERT_DOUBLE_NEAR(mse, 0.0, 1e-12);

    return 0;
}

/* Uneven chunks: signal length not divisible by thread count. */
static int test_worker_uneven_chunks(void) {
    const size_t N = 1003; /* prime number, not divisible by 4 */
    float *input = (float *)malloc(N * sizeof(float));
    float *ref = (float *)malloc(N * sizeof(float));
    float *out = (float *)malloc(N * sizeof(float));
    ASSERT_TRUE(input && ref && out);

    for (size_t i = 0; i < N; i++) input[i] = (float)(i * 7 % 31) * 0.1f;

    reference_fir(input, ref, N, TEST_COEFFS, TEST_NUM_TAPS);
    ASSERT_TRUE(run_multithreaded(input, out, N, TEST_COEFFS, TEST_NUM_TAPS, 4) == 0);

    double mse = compute_mse(ref, out, N);
    ASSERT_DOUBLE_NEAR(mse, 0.0, 1e-12);

    free(input); free(ref); free(out);
    return 0;
}

void run_worker_tests(void) {
    TEST_SUITE("Multithreaded Worker");
    RUN_TEST(test_worker_2_threads);
    RUN_TEST(test_worker_4_threads);
    RUN_TEST(test_worker_8_threads);
    RUN_TEST(test_worker_single_thread);
    RUN_TEST(test_worker_small_signal);
    RUN_TEST(test_worker_uneven_chunks);
}

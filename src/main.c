#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <pthread.h>

#include "dsp.h"
#include "fir.h"
#include "worker.h"

#define SAMPLE_RATE 48000.0f
#define SIGNAL_FREQ 1000.0f
#define NOISE_STD   0.3f

// Simple lowpass FIR coefficients (for example, 21 taps, windowed sinc).
// For a real project you would design these offline.
static const float LP_COEFFS[21] = {
    -0.0041f, -0.0082f, -0.0055f,  0.0052f,  0.0217f,
     0.0397f,  0.0514f,  0.0500f,  0.0330f,  0.0038f,
    -0.0276f, -0.0535f, -0.0663f, -0.0610f, -0.0364f,
     0.0023f,  0.0461f,  0.0838f,  0.1048f,  0.1023f,
     0.0751f
};

static double now_seconds() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

int main(void) {
    const size_t N = 480000;   // 10 seconds at 48kHz
    const int NUM_THREADS = 4;

    srand((unsigned int)time(NULL));

    float *input = (float *)malloc(N * sizeof(float));
    float *output_single = (float *)malloc(N * sizeof(float));
    float *output_multi = (float *)malloc(N * sizeof(float));

    if (!input || !output_single || !output_multi) {
        fprintf(stderr, "Allocation failure\n");
        return 1;
    }

    printf("Generating signal...\n");
    generate_signal(input, N, SAMPLE_RATE, SIGNAL_FREQ, NOISE_STD);

    // Single threaded
    {
        FirFilter f;
        fir_init(&f, LP_COEFFS, sizeof(LP_COEFFS) / sizeof(LP_COEFFS[0]));
        fir_reset(&f);

        double t0 = now_seconds();
        fir_process_block(&f, input, output_single, N);
        double t1 = now_seconds();

        fir_free(&f);

        printf("Single threaded FIR time: %.6f s\n", t1 - t0);
    }

    // Multithreaded
    {
        Worker workers[NUM_THREADS];
        size_t chunk = N / NUM_THREADS;

        double t0 = now_seconds();

        for (int i = 0; i < NUM_THREADS; ++i) {
            workers[i].job.input = input;
            workers[i].job.output = output_multi;
            workers[i].job.start = i * chunk;
            workers[i].job.length = (i == NUM_THREADS - 1)
                                    ? (N - i * chunk)
                                    : chunk;
            workers[i].job.coeffs = LP_COEFFS;
            workers[i].job.num_taps = sizeof(LP_COEFFS) / sizeof(LP_COEFFS[0]);

            if (worker_start(&workers[i]) != 0) {
                fprintf(stderr, "Failed to start thread %d\n", i);
                return 1;
            }
        }

        for (int i = 0; i < NUM_THREADS; ++i) {
            worker_join(&workers[i]);
        }

        double t1 = now_seconds();
        printf("Multithreaded FIR time: %.6f s\n", t1 - t0);
    }

    // Basic correctness check: outputs should be similar
    double mse = 0.0;
    for (size_t n = 0; n < N; ++n) {
        double diff = (double)output_single[n] - (double)output_multi[n];
        mse += diff * diff;
    }
    mse /= (double)N;
    printf("MSE between single and multithreaded outputs: %.8e\n", mse);

    free(input);
    free(output_single);
    free(output_multi);

    return 0;
}

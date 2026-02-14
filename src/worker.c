#include "worker.h"
#include <stdlib.h>

static void *worker_run(void *arg) {
    WorkerJob *job = (WorkerJob *)arg;

    fir_init(&job->filter, job->coeffs, job->num_taps);
    fir_reset(&job->filter);

    // Warm up the filter with samples before this chunk so state is correct
    // at the boundary. Process up to num_taps-1 preceding samples, discarding output.
    size_t warmup = 0;
    if (job->num_taps > 1 && job->start > 0) {
        warmup = (job->start < job->num_taps - 1) ? job->start : job->num_taps - 1;
        for (size_t i = 0; i < warmup; ++i) {
            fir_process_sample(&job->filter, job->input[job->start - warmup + i]);
        }
    }

    const float *in = job->input + job->start;
    float *out = job->output + job->start;

    fir_process_block(&job->filter, in, out, job->length);

    fir_free(&job->filter);
    return NULL;
}

int worker_start(Worker *w) {
    return pthread_create(&w->thread, NULL, worker_run, &w->job);
}

int worker_join(Worker *w) {
    return pthread_join(w->thread, NULL);
}

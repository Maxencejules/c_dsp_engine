#include "worker.h"
#include <stdlib.h>

static void *worker_run(void *arg) {
    WorkerJob *job = (WorkerJob *)arg;

    fir_init(&job->filter, job->coeffs, job->num_taps);
    fir_reset(&job->filter);

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

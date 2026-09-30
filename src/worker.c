#include "worker.h"
#include <errno.h>
#include <stdint.h>

static void *worker_run(void *argument) {
    WorkerJob *job = argument;
    size_t warmup = job->start < job->num_taps - 1 ? job->start : job->num_taps - 1;
    for (size_t index = job->start - warmup; index < job->start; ++index)
        (void)fir_process_sample(&job->filter, job->input[index]);
    job->result = job->length ? fir_process_block(&job->filter, job->input + job->start,
                                                 job->output + job->start, job->length) : 0;
    return NULL;
}

int worker_start(Worker *worker) {
    if (!worker) return EINVAL;
    if (worker->started) return EBUSY;
    WorkerJob *job = &worker->job;
    if (job->start > job->input_length || job->length > job->input_length - job->start)
        return EINVAL;
    if (job->input_length > SIZE_MAX / sizeof(float)) return EOVERFLOW;
    if (job->length && (!job->input || !job->output)) return EINVAL;
    if (job->length && fir_buffers_overlap(job->input, job->output, job->input_length)) return EINVAL;
    if (!job->length && job->start) return EINVAL;
    int status = fir_init(&job->filter, job->coeffs, job->num_taps);
    if (status) return status;
    job->result = 0;
    status = pthread_create(&worker->thread, NULL, worker_run, job);
    if (status) fir_free(&job->filter);
    else worker->started = 1;
    return status;
}

int worker_join(Worker *worker) {
    if (!worker || !worker->started) return EINVAL;
    int status = pthread_join(worker->thread, NULL);
    if (status) return status;
    worker->started = 0;
    status = worker->job.result;
    fir_free(&worker->job.filter);
    return status;
}

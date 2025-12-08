#ifndef WORKER_H
#define WORKER_H

#include <stddef.h>
#include <pthread.h>
#include "fir.h"

typedef struct {
    const float *input;
    float *output;
    size_t start;
    size_t length;
    FirFilter filter;
    const float *coeffs;
    size_t num_taps;
} WorkerJob;

typedef struct {
    pthread_t thread;
    WorkerJob job;
} Worker;

int worker_start(Worker *w);
int worker_join(Worker *w);

#endif

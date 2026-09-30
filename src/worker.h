#ifndef WORKER_H
#define WORKER_H
#include <pthread.h>
#include <stddef.h>
#include "fir.h"

typedef struct {
    const float *input;
    float *output;
    size_t input_length;
    size_t start;
    size_t length;
    FirFilter filter;
    const float *coeffs;
    size_t num_taps;
    int result;
} WorkerJob;

/* Zero-initialize; never move/free a live worker. Input and coefficients must
   remain immutable through join. Jobs need disjoint output ranges and output
   must not overlap input, including exact in-place buffers. */
typedef struct {
    pthread_t thread;
    WorkerJob job;
    int started;
} Worker;

/* Allocate state before spawning: allocation failure starts no thread.
   input_length is the capacity of each full buffer. Return errno/pthread status. */
int worker_start(Worker *worker);
/* Join once. Errors preserve the live handle; release buffers/Worker storage
   only after successful join. */
int worker_join(Worker *worker);
#endif

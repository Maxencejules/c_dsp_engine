#ifndef FIR_H
#define FIR_H

#include <stddef.h>

typedef struct {
    const float *coeffs;
    size_t num_taps;
    float *state;     // state buffer of length num_taps - 1
} FirFilter;

// Initialize FIR filter
void fir_init(FirFilter *f, const float *coeffs, size_t num_taps);

// Reset internal state
void fir_reset(FirFilter *f);

// Process one sample
float fir_process_sample(FirFilter *f, float x);

// Process a block of samples
void fir_process_block(FirFilter *f,
                       const float *input,
                       float *output,
                       size_t length);

void fir_free(FirFilter *f);

#endif

#ifndef DSP_H
#define DSP_H

#include <stddef.h>

void generate_signal(float *buffer, size_t length, float fs, float freq, float noise_std);

#endif

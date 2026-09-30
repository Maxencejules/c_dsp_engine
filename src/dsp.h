#ifndef DSP_H
#define DSP_H
#include <stddef.h>
#include <stdint.h>
/* Local-seed sine + Gaussian noise. Return 0 or errno. Zero length accepts NULL;
   fs>0, 0<=freq<=fs/2, std>=0. Each call starts at sample/time zero. */
int generate_signal(float *buffer, size_t length, float fs, float freq,
                    float noise_std, uint32_t seed);
#endif

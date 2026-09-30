#ifndef FIR_H
#define FIR_H
#include <stddef.h>

/* Zero-initialize before init. Coefficients are borrowed and must remain alive
   and unchanged until free; a filter has one owner and is not thread-safe. */
typedef struct {
    const float *coeffs;
    size_t num_taps;
    float *state; /* newest previous sample first; length num_taps - 1 */
} FirFilter;

/* Return 0 or an errno code. Failed init of an empty owner leaves it empty. Reinitializing
   a live filter returns EBUSY; free it first. */
int fir_init(FirFilter *filter, const float *coeffs, size_t num_taps);
int fir_reset(FirFilter *filter);
/* Invalid/empty filter returns NAN. Otherwise compute causal float convolution. */
float fir_process_sample(FirFilter *filter, float sample);
/* Empty blocks accept NULL buffers. Exact in-place processing is supported;
   partial overlap is rejected. Invalid geometry leaves state/output unchanged.
   Buffers must contain length floats; C cannot validate their actual capacity. */
int fir_process_block(FirFilter *filter, const float *input, float *output, size_t length);
/* Free is idempotent and clears all fields; NULL is accepted. */
void fir_free(FirFilter *filter);
/* Overlap utility: caller must ensure length <= SIZE_MAX / sizeof(float). */
int fir_buffers_overlap(const float *input, const float *output, size_t length);
#endif

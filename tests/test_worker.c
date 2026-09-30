#include "test_helpers.h"
#include "worker.h"
#include "fault_hooks.h"
#include <errno.h>
#include <stdint.h>
/* Join every successful start before releasing its storage on partial failure. */
static int run_workers(const float *input, float *output, size_t length,
                       const float *coeffs, size_t taps, size_t thread_count) {
    if (!length || !thread_count) return EINVAL;
    size_t count = thread_count < length ? thread_count : length;
    Worker *workers = calloc(count, sizeof(*workers));
    if (!workers) return ENOMEM;
    int status = 0;
    size_t started = 0, position = 0;
    for (size_t index = 0; index < count; ++index) {
        size_t chunk = length / count + (index < length % count ? 1 : 0);
        workers[index].job = (WorkerJob){.input = input, .output = output,
            .input_length = length, .start = position, .length = chunk,
            .coeffs = coeffs, .num_taps = taps};
        status = worker_start(&workers[index]);
        if (status) break;
        ++started; position += chunk;
    }
    for (size_t index = 0; index < started; ++index) {
        int joined = worker_join(&workers[index]);
        CHECK(joined == 0);
        if (joined) abort();
    }
    free(workers);
    return status;
}
void test_worker(void) {
    float input[1003], output[1003], coefficients[31];
    for (size_t index = 0; index < 1003; ++index)
        input[index] = (float)cos((double)index * 0.19) + (float)(index % 11) / 7.0f;
    for (size_t tap = 0; tap < 31; ++tap)
        coefficients[tap] = (float)((int)(tap % 5) - 2) / 11.0f;
    coefficients[0] = 0.2f;
    const size_t lengths[] = {1, 4, 73, 1003};
    const size_t tap_counts[] = {1, 2, 5, 31};
    const size_t threads[] = {1, 2, 4, 8};
    for (size_t ni = 0; ni < 4; ++ni)
        for (size_t ti = 0; ti < 4; ++ti)
            for (size_t ki = 0; ki < 4; ++ki) {
                fault_reset();
                CHECK(run_workers(input, output, lengths[ni], coefficients,
                                  tap_counts[ti], threads[ki]) == 0);
                for (size_t index = 0; index < lengths[ni]; ++index)
                    NEAR(output[index], reference_sample(input, index, coefficients, tap_counts[ti]), 3e-6);
                CHECK(fault_starts() == fault_joins());
            }
    for (size_t index = 0; index < 1003; ++index) output[index] = -999;
    Worker worker = {0};
    worker.job = (WorkerJob){.input = input, .output = output, .input_length = 1003,
        .start = 11, .length = 7, .coeffs = coefficients, .num_taps = 31};
    CHECK(worker_join(&worker) == EINVAL);
    CHECK(worker_start(&worker) == 0);
    CHECK(worker_start(&worker) == EBUSY);
    CHECK(worker_join(&worker) == 0);
    CHECK(worker_join(&worker) == EINVAL);
    for (size_t index = 0; index < 1003; ++index) {
        if (index >= 11 && index < 18)
            NEAR(output[index], reference_sample(input, index, coefficients, 31), 3e-6);
        else CHECK(output[index] == -999);
    }
    CHECK(!worker.started && !worker.job.filter.state && !worker.job.filter.coeffs);
    worker.job.start = 1004; CHECK(worker_start(&worker) == EINVAL);
    worker.job.start = 1000; worker.job.length = 4; CHECK(worker_start(&worker) == EINVAL);
    worker.job.start = 0; worker.job.length = 4; worker.job.output = input;
    CHECK(worker_start(&worker) == EINVAL);
    worker.job.output = input + 1; CHECK(worker_start(&worker) == EINVAL);
    worker.job.output = output; worker.job.input = NULL; CHECK(worker_start(&worker) == EINVAL);
    worker.job.input = input; worker.job.input_length = SIZE_MAX;
    CHECK(worker_start(&worker) == EOVERFLOW);
    CHECK(worker_start(NULL) == EINVAL); CHECK(worker_join(NULL) == EINVAL);
    worker = (Worker){0};
    worker.job.coeffs = coefficients; worker.job.num_taps = 31;
    CHECK(worker_start(&worker) == 0); CHECK(worker_join(&worker) == 0);
    fault_reset(); fault_fail_allocation(1);
    worker.job = (WorkerJob){.input = input, .output = output, .input_length = 73,
        .length = 73, .coeffs = coefficients, .num_taps = 31};
    CHECK(worker_start(&worker) == ENOMEM);
    CHECK(!worker.started && !worker.job.filter.state && !worker.job.filter.coeffs);
    fault_reset(); fault_fail_creation(1);
    CHECK(worker_start(&worker) == EAGAIN);
    CHECK(!worker.started && !worker.job.filter.state && !worker.job.filter.coeffs);
    fault_reset(); CHECK(worker_start(&worker) == 0);
    float *preserved_state = worker.job.filter.state;
    fault_fail_join(1); /* fail before reaping: the handle must stay usable */
    CHECK(worker_join(&worker) == EINVAL);
    CHECK(worker.started && worker.job.filter.state == preserved_state);
    CHECK(fault_joins() == 0);
    CHECK(worker_join(&worker) == 0);
    CHECK(!worker.started && !worker.job.filter.state && fault_joins() == 1);
    fault_reset(); fault_fail_creation(2);
    CHECK(run_workers(input, output, 73, coefficients, 31, 4) == EAGAIN);
    CHECK(fault_starts() == 1 && fault_joins() == 1);
    fault_reset(); fault_fail_allocation(3);
    CHECK(run_workers(input, output, 73, coefficients, 31, 4) == ENOMEM);
    CHECK(fault_starts() == 1 && fault_joins() == 1);
    fault_reset();
}
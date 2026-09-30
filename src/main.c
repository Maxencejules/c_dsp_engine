#define _POSIX_C_SOURCE 200809L
#define WIN32_LEAN_AND_MEAN
#if defined(_WIN32) && !defined(_WIN32_WINNT)
#define _WIN32_WINNT 0x0600
#endif
#ifdef __APPLE__
#define _DARWIN_C_SOURCE
#endif
#include "dsp.h"
#include "fir.h"
#include "worker.h"
#include "build_config.h"
#include <errno.h>
#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#include <unistd.h>
#include <sys/utsname.h>
#ifdef __APPLE__
#include <sys/types.h>
#include <sys/sysctl.h>
#endif
#endif

#define MAX_TRIALS 21
#define PI 3.14159265358979323846
typedef struct {
    size_t samples, taps, threads, trials;
    uint32_t seed;
    const char *csv;
} Options;
typedef struct { double seconds[3], errors[3]; } Trial;
static const char *names[] = {"block", "stream", "threads"};

static double wall_time(void) {
#ifdef _WIN32
    LARGE_INTEGER count, frequency;
    if (!QueryPerformanceFrequency(&frequency) || !QueryPerformanceCounter(&count)) {
        fprintf(stderr, "Monotonic clock unavailable\n"); exit(EXIT_FAILURE);
    }
    return (double)count.QuadPart / (double)frequency.QuadPart;
#else
    struct timespec value;
    if (clock_gettime(CLOCK_MONOTONIC, &value)) {
        perror("clock_gettime"); exit(EXIT_FAILURE);
    }
    return (double)value.tv_sec + (double)value.tv_nsec / 1e9;
#endif
}
static int number(const char *text, uint64_t *result) {
    if (!text || text[0] < '0' || text[0] > '9') return EINVAL;
    char *end = NULL;
    errno = 0;
    unsigned long long value = strtoull(text, &end, 10);
    if (errno || !end || *end) return EINVAL;
    *result = (uint64_t)value;
    return 0;
}
static void usage(const char *program) {
    printf("Usage: %s [--samples N] [--taps odd-T] [--threads K] [--trials R]\n"
           "          [--seed uint32] [--csv PATH]\n"
           "Defaults: N=480000 T=63 K=4 R=7 seed=2026; streaming chunk=257.\n"
           "Bounds: N=1..2000000, odd T=1..255, K=1..64, R=3..21, N*T*R<=500000000.\n",
           program);
}
static int options(int argc, char **argv, Options *settings) {
    *settings = (Options){480000, 63, 4, 7, 2026, NULL};
    for (int index = 1; index < argc; index += 2) {
        if (index + 1 >= argc) return EINVAL;
        if (!strcmp(argv[index], "--csv")) {
            settings->csv = argv[index + 1]; continue;
        }
        uint64_t value;
        if (number(argv[index + 1], &value)) return EINVAL;
        if (!strcmp(argv[index], "--samples") && value <= 2000000) settings->samples = (size_t)value;
        else if (!strcmp(argv[index], "--taps") && value <= 255) settings->taps = (size_t)value;
        else if (!strcmp(argv[index], "--threads") && value <= 64) settings->threads = (size_t)value;
        else if (!strcmp(argv[index], "--trials") && value <= MAX_TRIALS) settings->trials = (size_t)value;
        else if (!strcmp(argv[index], "--seed") && value <= UINT32_MAX) settings->seed = (uint32_t)value;
        else return EINVAL;
    }
    if (!settings->samples || !settings->taps || !(settings->taps % 2) ||
        !settings->threads || settings->trials < 3 ||
        (uint64_t)settings->samples * settings->taps * settings->trials > UINT64_C(500000000)) return EINVAL;
    if (settings->threads > settings->samples) settings->threads = settings->samples;
    return 0;
}
/* Hamming-windowed sinc at 0.1 cycles/sample, then normalize DC gain.
   A one-tap request is explicitly the identity filter. */
static void lowpass(float *coeffs, size_t taps) {
    if (taps == 1) { coeffs[0] = 1; return; }
    double sum = 0;
    size_t middle = (taps - 1) / 2;
    for (size_t tap = 0; tap <= middle; ++tap) {
        double offset = (double)tap - (double)middle;
        double sinc = offset == 0 ? 0.2 : sin(2 * PI * 0.1 * offset) / (PI * offset);
        double window = 0.54 - 0.46 * cos(2 * PI * (double)tap / (double)(taps - 1));
        coeffs[tap] = (float)(sinc * window);
        coeffs[taps - 1 - tap] = coeffs[tap];
        sum += (double)coeffs[tap] * (tap == middle ? 1 : 2);
    }
    for (size_t tap = 0; tap < taps; ++tap) coeffs[tap] = (float)((double)coeffs[tap] / sum);
}
/* Independent double-precision convolution and per-sample absolute-product sum. */
static void reference(const float *input, const float *coeffs, const Options *settings,
                      double *expected, double *bounds) {
    for (size_t index = 0; index < settings->samples; ++index) {
        double sum = 0, magnitude = 0;
        for (size_t tap = 0; tap < settings->taps && tap <= index; ++tap) {
            double product = (double)coeffs[tap] * (double)input[index - tap];
            sum += product; magnitude += fabs(product);
        }
        expected[index] = sum;
        bounds[index] = 4 * (double)settings->taps * FLT_EPSILON * magnitude + 1e-7;
    }
}
static int verify(const float *output, const double *expected, const double *bounds,
                  size_t length, double *maximum) {
    *maximum = 0;
    for (size_t index = 0; index < length; ++index) {
        double error = fabs((double)output[index] - expected[index]);
        if (!isfinite(output[index]) || error > bounds[index]) {
            fprintf(stderr, "Reference mismatch at sample %zu: %.9g vs %.17g (bound %.9g)\n",
                    index, (double)output[index], expected[index], bounds[index]);
            return EDOM;
        }
        if (error > *maximum) *maximum = error;
    }
    return 0;
}
static int threaded(const float *input, float *output, const float *coeffs, const Options *settings) {
    Worker *workers = calloc(settings->threads, sizeof(*workers));
    if (!workers) return ENOMEM;
    size_t started = 0, position = 0;
    int status = 0;
    for (size_t index = 0; index < settings->threads; ++index) {
        size_t length = settings->samples / settings->threads +
                        (index < settings->samples % settings->threads ? 1 : 0);
        workers[index].job = (WorkerJob){.input = input, .output = output,
            .input_length = settings->samples, .start = position, .length = length,
            .coeffs = coeffs, .num_taps = settings->taps};
        status = worker_start(&workers[index]);
        if (status) break;
        ++started; position += length;
    }
    for (size_t index = 0; index < started; ++index) {
        int joined = worker_join(&workers[index]);
        if (joined) {
            /* A join error leaves potentially-live storage. Terminate the CLI
               without freeing it rather than returning dangling buffers. */
            fprintf(stderr, "Cannot join worker %zu: %s\n", index, strerror(joined));
            exit(EXIT_FAILURE);
        }
    }
    free(workers);
    return status;
}
static int process(size_t method, const float *input, float *output,
                   const float *coeffs, const Options *settings) {
    if (method == 2) return threaded(input, output, coeffs, settings);
    FirFilter filter = {0};
    int status = fir_init(&filter, coeffs, settings->taps);
    if (status) return status;
    size_t position = 0;
    while (position < settings->samples) {
        size_t length = method == 0 ? settings->samples : 257;
        if (length > settings->samples - position) length = settings->samples - position;
        status = fir_process_block(&filter, input + position, output + position, length);
        if (status) break;
        position += length;
    }
    fir_free(&filter);
    return status;
}
static int compare_double(const void *left, const void *right) {
    double first = *(const double *)left, second = *(const double *)right;
    return (first > second) - (first < second);
}
static double median(double *values, size_t count) {
    qsort(values, count, sizeof(*values), compare_double);
    return count % 2 ? values[count / 2] : (values[count / 2 - 1] + values[count / 2]) / 2;
}
static void host(char *cpu, size_t cpu_size, char *os, size_t os_size, unsigned long *logical) {
#ifdef _WIN32
    SYSTEM_INFO info; GetSystemInfo(&info); *logical = info.dwNumberOfProcessors;
    DWORD bytes = (DWORD)cpu_size;
    if (RegGetValueA(HKEY_LOCAL_MACHINE,
                    "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                    "ProcessorNameString", RRF_RT_REG_SZ, NULL, cpu, &bytes) != ERROR_SUCCESS) {
        const char *identifier = getenv("PROCESSOR_IDENTIFIER");
        snprintf(cpu, cpu_size, "%s", identifier ? identifier : "unavailable");
    }
    snprintf(os, os_size, "Windows");
#else
    long count = sysconf(_SC_NPROCESSORS_ONLN); *logical = count > 0 ? (unsigned long)count : 0;
    struct utsname info = {0};
    if (!uname(&info)) snprintf(os, os_size, "%s %s %s", info.sysname, info.release, info.machine);
    else snprintf(os, os_size, "unavailable");
    snprintf(cpu, cpu_size, "unavailable");
#if defined(__linux__)
    FILE *file = fopen("/proc/cpuinfo", "r");
    if (file) {
        char line[512];
        while (fgets(line, sizeof(line), file)) {
            if (!strncmp(line, "model name", 10)) {
                char *value = strchr(line, ':');
                if (value) {
                    ++value; while (*value == ' ') ++value;
                    value[strcspn(value, "\r\n")] = 0;
                    snprintf(cpu, cpu_size, "%s", value); break;
                }
            }
        }
        fclose(file);
    }
#elif defined(__APPLE__)
    size_t bytes = cpu_size;
    if (sysctlbyname("machdep.cpu.brand_string", cpu, &bytes, NULL, 0))
        snprintf(cpu, cpu_size, "unavailable (architecture %s)",
                 info.machine[0] ? info.machine : "unavailable");
    cpu[cpu_size - 1] = 0;
#endif
#endif
}
static void csv_string(FILE *file, const char *value) {
    fputc('"', file);
    for (; *value; ++value) { if (*value == '"') fputc('"', file); fputc(*value, file); }
    fputc('"', file);
}
int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "--help")) { usage(argv[0]); return EXIT_SUCCESS; }
    Options settings;
    if (options(argc, argv, &settings)) { usage(argv[0]); return EXIT_FAILURE; }
    float *input = malloc(settings.samples * sizeof(*input));
    float *output = malloc(settings.samples * sizeof(*output));
    float *coeffs = malloc(settings.taps * sizeof(*coeffs));
    double *expected = malloc(settings.samples * sizeof(*expected));
    double *bounds = malloc(settings.samples * sizeof(*bounds));
    int result = EXIT_FAILURE;
    FILE *csv = NULL;
    if (!input || !output || !coeffs || !expected || !bounds) {
        fprintf(stderr, "Allocation failed\n"); goto cleanup;
    }
    lowpass(coeffs, settings.taps);
    if (generate_signal(input, settings.samples, 48000, 1000, 0.3f, settings.seed)) goto cleanup;
    reference(input, coeffs, &settings, expected, bounds);
    Trial trials[MAX_TRIALS] = {0};
    /* One untimed warmup of each method; every measured run is also verified. */
    for (size_t method = 0; method < 3; ++method) {
        double error;
        if (process(method, input, output, coeffs, &settings) ||
            verify(output, expected, bounds, settings.samples, &error)) goto cleanup;
    }
    for (size_t trial = 0; trial < settings.trials; ++trial) {
        for (size_t offset = 0; offset < 3; ++offset) {
            size_t method = (trial + offset) % 3;
            double start = wall_time();
            int status = process(method, input, output, coeffs, &settings);
            trials[trial].seconds[method] = wall_time() - start;
            if (status) { fprintf(stderr, "Processing failed: %s\n", strerror(status)); goto cleanup; }
            if (!isfinite(trials[trial].seconds[method]) || trials[trial].seconds[method] <= 0 ||
                verify(output, expected, bounds, settings.samples, &trials[trial].errors[method])) goto cleanup;
        }
    }
    char cpu[512], os[512]; unsigned long logical;
    host(cpu, sizeof(cpu), os, sizeof(os), &logical);
    printf("Compiler: %s %s; build: %s; flags: %s\n", DSP_COMPILER, DSP_COMPILER_VERSION,
           DSP_BUILD_TYPE, DSP_BUILD_FLAGS);
    printf("CPU: %s; logical processors: %lu; OS: %s\n", cpu, logical, os);
    printf("N=%zu taps=%zu active_threads=%zu trials=%zu seed=%" PRIu32
           "; Fs=48000 Hz; cutoff=0.1 cycles/sample; stream_chunk=257\n",
           settings.samples, settings.taps, settings.threads, settings.trials, settings.seed);
    printf("Monotonic wall time, initialization/teardown included; one excluded warmup per method.\n");
    double medians[3];
    for (size_t method = 0; method < 3; ++method) {
        double times[MAX_TRIALS], max_error = 0;
        for (size_t trial = 0; trial < settings.trials; ++trial) {
            times[trial] = trials[trial].seconds[method];
            if (trials[trial].errors[method] > max_error) max_error = trials[trial].errors[method];
        }
        medians[method] = median(times, settings.trials);
        printf("%-7s median=%.9f s min=%.9f s max=%.9f s throughput=%.3f Msamples/s max_abs_error=%.9g\n",
               names[method], medians[method], times[0], times[settings.trials - 1],
               (double)settings.samples / medians[method] / 1e6, max_error);
    }
    printf("Observed block/threads median ratio: %.3fx (workload/machine specific)\n",
           medians[0] / medians[2]);
    if (settings.csv) {
        csv = fopen(settings.csv, "w");
        if (!csv) { perror(settings.csv); goto cleanup; }
        fputs("compiler,compiler_version,build_type,flags,cpu,logical_processors,os,samples,taps,threads,trials,seed,trial,first_method,block_seconds,stream_seconds,threads_seconds,block_max_abs_error,stream_max_abs_error,threads_max_abs_error\n", csv);
        for (size_t trial = 0; trial < settings.trials; ++trial) {
            csv_string(csv, DSP_COMPILER); fputc(',', csv); csv_string(csv, DSP_COMPILER_VERSION);
            fputc(',', csv); csv_string(csv, DSP_BUILD_TYPE); fputc(',', csv);
            csv_string(csv, DSP_BUILD_FLAGS); fputc(',', csv); csv_string(csv, cpu);
            fprintf(csv, ",%lu,", logical); csv_string(csv, os);
            fprintf(csv, ",%zu,%zu,%zu,%zu,%" PRIu32 ",%zu,%s",
                    settings.samples, settings.taps, settings.threads, settings.trials,
                    settings.seed, trial + 1, names[trial % 3]);
            for (size_t method = 0; method < 3; ++method) fprintf(csv, ",%.17g", trials[trial].seconds[method]);
            for (size_t method = 0; method < 3; ++method) fprintf(csv, ",%.17g", trials[trial].errors[method]);
            fputc('\n', csv);
        }
        if (ferror(csv)) { fprintf(stderr, "CSV write failed\n"); goto cleanup; }
        if (fclose(csv)) { csv = NULL; fprintf(stderr, "CSV close failed\n"); goto cleanup; }
        csv = NULL;
    }
    result = EXIT_SUCCESS;
cleanup:
    if (csv) fclose(csv);
    free(bounds); free(expected); free(coeffs); free(output); free(input);
    return result;
}
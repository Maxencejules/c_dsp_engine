# C DSP engine

A small C11 project for understanding causal FIR filtering, persistent streaming state,
and pthread chunk processing. The benchmark verifies every run against independent
double-precision convolution before reporting timings. It makes no general speedup
or real-time guarantee.

## Build and test

Use [CMake 3.20+](https://cmake.org/cmake/help/latest/release/3.20.html), GCC or Clang, a single-config generator (Ninja/Makefiles),
and a POSIX pthread implementation. Linux and macOS:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/dsp_engine --samples 48000 --taps 31 --threads 4 --trials 5 --csv build/benchmark.csv
```

Windows uses **MinGW UCRT64 GCC with POSIX threads**, rather than MSVC. From
PowerShell with GCC and mingw32-make on PATH:

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
.\build\dsp_engine.exe --samples 48000 --taps 31 --threads 4 --trials 5 --csv build/benchmark.csv
```

The optional Makefile is a CMake front end for a POSIX shell. Builds use
`-Wall -Wextra -Wpedantic -Werror -Wshadow -ffp-contract=off` and do not enable
fast-math. GitHub Actions builds/tests Linux, macOS, and Windows/MinGW, runs a
bounded benchmark on each, and uploads raw CSV/text results. A separate Linux
Debug job runs AddressSanitizer and UndefinedBehaviorSanitizer:

```sh
cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug -DDSP_SANITIZERS=ON
cmake --build build-sanitize --parallel
ctest --test-dir build-sanitize --output-on-failure
```

## Numerical model and streaming contract

For `T` taps, `y[n] = sum(h[k] * x[n-k], k=0..T-1)`, with zero input
before the start. `h[0]` multiplies the current sample. Accumulation and state
are `float`; the test/benchmark oracle sums products of the same supplied
float inputs/coefficients in `double`.

A block emits exactly one output per input and preserves the last `T-1`
inputs for the next call. Chunk boundaries do not reset history. To obtain the
full linear-convolution tail, explicitly feed `T-1` zeros; reset starts a new
zero-history stream. Exact in-place sequential filtering is supported.
Partially overlapping input/output buffers are rejected before changing output
or history. A valid empty block accepts NULL buffers and leaves history intact.

```c
#include "fir.h"

float h[] = {0.25f, -0.5f, 0.75f};
float x[] = {1, 0, 0, 0}, y[4];
FirFilter filter = {0};
int status = fir_init(&filter, h, 3);
if (status == 0) {
    status = fir_process_block(&filter, x, y, 4);
    /* y = {0.25, -0.5, 0.75, 0}; check status before using outputs. */
}
fir_free(&filter);
```

Lifecycle functions return zero on success or an errno/pthread error code.
Initialize filters and workers with `{0}`; do not copy live owners.
Initialization failure leaves an empty filter, including on allocation failure.
Initializing a live filter returns `EBUSY`; free it first. Free is idempotent
and clears all fields. An invalid filter passed to the sample function returns
NAN; the block/reset functions return `EINVAL`.

Coefficients are borrowed, so keep them alive and unchanged until the filter is
freed. A filter is owned by one caller and is not thread-safe. Callers supply
valid buffer capacities; C cannot check an allocation's actual size. Use finite,
reasonably scaled input/coefficient values: arithmetic overflow and nonfinite
input propagate through the float calculation.

This revision changes the earlier unchecked APIs: FIR init/reset/block now
return status, WorkerJob carries full-buffer `input_length`, and the signal
generator takes an explicit seed and returns status.

## Threaded chunks and failure handling

Each Worker owns a fresh filter. Before its assigned output interval it feeds
up to `T-1` original preceding input samples, discarding those warmup outputs.
That reconstructs the causal history without relying on another worker's output,
including when chunks are shorter than the filter.

Set `input_length` to the capacity of **both full input and output buffers**;
`start` and `length` select the interval to write. Configure jobs before starting them; do not modify Worker/WorkerJob fields or
read a job's output before joining. Input and coefficients
remain immutable until all jobs are joined. Output intervals must be disjoint;
threaded processing rejects input/output overlap, including exact in-place use.

`worker_start` allocates state before creating the thread and reports either
failure synchronously. A failed start frees its state and can be retried.
`worker_join` frees state only after successful joining. A join error retains
the live handle/state: do not modify/free the Worker, input, coefficients, or
output while a thread might still run. The benchmark joins all successful starts
after a partial startup failure; on an unexpected join error it terminates
without freeing potentially-live storage. Outputs from a failed overall batch
must not be treated as a complete result.

## What the tests verify

The regression suite uses direct indexed double convolution independent of the
production filter/state code. It covers asymmetric mixed-sign coefficients,
1/2/5/17/31 taps, signals shorter than the filter, fixed and irregular chunk
sizes, empty calls between chunks, zero-padded tails, reset, exact in-place
filtering, and hand-computed impulse/DC responses.

Worker sweeps compare every sample for short/uneven chunks and one or multiple
workers, including more requested workers than samples. Sentinel checks ensure
a single job writes only its interval. Invalid geometry, overlap, live-owner
reinitialization, allocation failure, creation failure, and partial startup
cleanup are checked. Test-only allocator/pthread wrappers inject failures into
the same library sources; the production library has no test indirection.
Near assertions reject NaN/infinity instead of accidentally passing them.

The seeded signal generator uses a local 32-bit LCG and Box-Muller transform,
with no global rand/srand state. Tests check a known sinusoid, seed repeatability,
parameter rejection, and broad noise-moment sanity bounds. This is a demo PRNG,
not a randomness-quality certification. libm/compiler differences can change
the final bits across platforms.

## Repeatable benchmark

```sh
./build/dsp_engine --csv build/benchmark.csv
./build/dsp_engine --samples 4099 --taps 31 --threads 4 --trials 3 --seed 2026
./build/dsp_engine --help
```

Defaults are 480,000 samples, 63 taps, 4 threads, 7 trials, seed 2026.
The input is a 1 kHz unit-amplitude sine plus Gaussian noise (standard deviation
0.3), sampled at 48 kHz. Taps form a symmetric Hamming-windowed sinc lowpass
with nominal cutoff 0.1 cycles/sample (4.8 kHz), normalized to unit DC gain.
A one-tap request is identity. For the default 63 taps the linear-phase group
delay is 31 samples, approximately 0.646 ms. The nominal cutoff does not imply
a specified passband ripple or stopband attenuation.

Three paths process identical input: one block, streaming blocks of 257 samples,
and independent threaded chunks. Each path gets one excluded warmup. Trial order
rotates between paths. Timing uses monotonic wall time (QueryPerformanceCounter
on Windows; CLOCK_MONOTONIC on Unix) and includes filter/job allocation,
initialization, warmup history, thread creation/join, and teardown. Signal
generation, shared input/output buffer allocation, reference computation, and
verification are outside each timed interval. This measures complete invocations,
not a persistent worker pool.

Every warmup and measured run must have finite output and pass **every sample**:
`abs(float_output - double_reference) <= 4*T*FLT_EPSILON*sum(abs(h[k]*x[n-k])) + 1e-7`.
The magnitude term avoids a misleading relative-error check near cancellation;
the conservative allowance covers float product/sum rounding for these bounded
workloads. Failures exit nonzero and are never reported as valid performance.

CSV includes all raw trials, first method/order, compiler version, build type and
flags, CPU identifier, logical processor count, OS, workload, seed, and each
path's maximum absolute error. Output reports median/min/max and throughput.
The speed ratio is an observation for that machine/workload, not a portable
claim. Scheduling, CPU frequency, cache state, and other processes affect it.

CLI limits bound runs: samples 1..2,000,000; odd taps 1..255; threads 1..64;
trials 3..21; samples*taps*trials <= 500,000,000. Threads are clamped to the
sample count. Invalid arguments return failure. Build/benchmark artifacts stay
under ignored build directories.

## Local measurement

Recorded on 2026-09-29 on an Intel Core i7-7700 @ 3.60 GHz (8 logical processors),
Windows NT 10.0.26100.0, MinGW GCC 15.2.0 (UCRT/POSIX threads), CMake 4.2.1,
Release `-O3 -DNDEBUG` plus the warning/C11/rounding flags above. This is one
sandboxed workstation run with uncontrolled scheduling; use it as a reproducible
workload example, not a hardware performance promise.

Default workload: N=480,000, T=63, 4 active threads, 7 measured trials, seed 2026.

| Path | Median ms | Min–max ms | Msamples/s | Max absolute error |
| --- | ---: | ---: | ---: | ---: |
| block | 52.081 | 47.703–67.849 | 9.216 | 7.8e-7 |
| stream | 51.064 | 40.969–61.068 | 9.400 | 7.8e-7 |
| threads | 37.843 | 23.395–102.014 | 12.684 | 7.8e-7 |

The observed block/threads median ratio was 1.376x, with substantial timing
variation. [Raw seven-trial CSV](benchmarks/windows-i7-7700.csv) and
[the complete run log](benchmarks/windows-i7-7700.txt) preserve the measurements.
Their medians, extrema, throughput, finite values, trial order, and metadata
were independently checked using Python's CSV/statistics standard libraries.

## Scope and limits

The FIR uses a straightforward dot product and history shift: O(N*T) work and
O(T) state per filter. Worker warmup adds work, and thread overhead can make
parallel execution slower on small workloads. There is no SIMD, FFT convolution,
persistent thread pool, audio-device I/O, hard real-time scheduling, or guarantee
that it scales with core count. The small implementation and checked contracts
are intended for learning and further experiments.
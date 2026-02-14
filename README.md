# C DSP Engine — Multithreaded FIR Filtering in C

A high-performance DSP engine written in **pure C** demonstrating:

- FIR filtering  
- Multithreading using pthreads  
- Real-time–style DSP workload  
- Benchmarking + performance comparison  
- Numerical accuracy validation  

This project compares **single-threaded vs multithreaded** FIR execution on a large signal buffer, showing real performance gains while maintaining accuracy.

## Features

### Realistic DSP Pipeline
- Generate a test signal  
- Apply an FIR filter  
- Compare outputs across implementations  

### Multithreading with pthreads
- Worker threads each process a chunk  
- Thread-safe buffer stitching  
- Shared coefficient memory  

### Performance Benchmarking
Example output:

```
Generating signal...
Single threaded FIR time: 0.106091 s
Multithreaded FIR time: 0.064802 s
MSE between single and multithreaded outputs: 7.38933028e-06
```

### Numerical Accuracy Validation
Computes Mean Squared Error (MSE).

Each worker thread starts with a zeroed filter state, so the first few samples at each chunk boundary differ from the single-threaded output. The MSE is low because only a small number of boundary samples are affected relative to the total signal length.

## Project Structure

```
c-dsp-engine/
├── src/
│   ├── main.c
│   ├── dsp.c
│   ├── dsp.h
│   ├── fir.c
│   ├── fir.h
│   ├── worker.c
│   ├── worker.h
├── CMakeLists.txt
├── Makefile
└── README.md
```

## Build & Run

### Requirements
- GCC or Clang  
- CMake ≥ 3.10  
- pthreads  

### Build
```
mkdir build
cd build
cmake ..
cmake --build .
```

### Run
```
./c_dsp_engine
```

## Example Output

```
Generating signal...
Single threaded FIR time: 0.106091 s
Multithreaded FIR time: 0.064802 s
MSE between single and multithreaded outputs: 7.38933028e-06
```

Speedup: ~1.6× faster.

## How It Works

### 1. Generate Input Signal
A noisy sine wave.

### 2. Apply FIR Filter (Single Thread)
Standard convolution.

### 3. Multithreaded FIR
- Splits signal into blocks
- Each thread processes one block with its own FIR filter state
- Combines results into a shared output buffer  

### 4. Accuracy Measurement
MSE:

```
mean( (y_single[i] - y_multi[i])^2 )
```

## Performance Discussion

Multithreading provides:
- Higher CPU utilization  
- Parallel FIR computation  
- Low overhead  

Possible future enhancements:
- SIMD/AVX  
- Thread pool  
- FFT-based convolution  

## Why This Project Matters

Demonstrates:
- Low-level C skills  
- Pointers & memory  
- Multithreading  
- DSP fundamentals  
- Benchmarking & correctness testing  

Useful for:
- Embedded  
- Telecom  
- Signal processing  
- Systems programming  
- Robotics  

## License

MIT License

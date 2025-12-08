cmake_minimum_required(VERSION 3.10)
project(c_dsp_engine C)

set(CMAKE_C_STANDARD 11)

add_executable(c_dsp_engine
        src/main.c
        src/dsp.c
        src/fir.c
        src/worker.c
)

# Link math library (m) and pthread
target_link_libraries(c_dsp_engine
        m
        pthread
)

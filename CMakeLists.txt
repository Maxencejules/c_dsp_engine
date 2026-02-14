cmake_minimum_required(VERSION 3.10)
project(c_dsp_engine C)

set(CMAKE_C_STANDARD 11)

# Shared library for DSP sources
add_library(dsp_lib STATIC
        src/dsp.c
        src/fir.c
        src/worker.c
)
target_link_libraries(dsp_lib m pthread)

# Main executable
add_executable(c_dsp_engine src/main.c)
target_link_libraries(c_dsp_engine dsp_lib)

# Tests
enable_testing()
add_executable(dsp_tests
        tests/test_main.c
        tests/test_fir.c
        tests/test_dsp.c
        tests/test_worker.c
)
target_link_libraries(dsp_tests dsp_lib)
add_test(NAME dsp_tests COMMAND dsp_tests)

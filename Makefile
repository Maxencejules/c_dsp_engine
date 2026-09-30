# Optional convenience front end; CMake is the supported cross-platform build.
.PHONY: all test bench clean
all:
	cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
	cmake --build build --parallel
test: all
	ctest --test-dir build --output-on-failure
bench: all
	./build/dsp_engine --csv build/benchmark.csv
clean:
	cmake -E remove_directory build
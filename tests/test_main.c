#include "test_helpers.h"

int tests_run = 0;
int tests_passed = 0;
int tests_failed = 0;

extern void run_fir_tests(void);
extern void run_dsp_tests(void);
extern void run_worker_tests(void);

int main(void) {
    run_fir_tests();
    run_dsp_tests();
    run_worker_tests();

    print_test_summary();
    return tests_failed > 0 ? 1 : 0;
}

#include "test_helpers.h"
int tests_run = 0, tests_failed = 0;
int main(void) {
    test_fir(); test_dsp(); test_worker();
    printf("%d assertions, %d failures\n", tests_run, tests_failed);
    return tests_failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
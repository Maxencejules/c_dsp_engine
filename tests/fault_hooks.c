#include "fault_hooks.h"
#include <errno.h>
#undef calloc
#undef pthread_create
#undef pthread_join
static size_t allocations, creations, join_calls, starts, joins, fail_allocation, fail_creation, fail_join;
void fault_reset(void) {
    allocations = creations = join_calls = starts = joins = fail_allocation = fail_creation = fail_join = 0;
}
void fault_fail_allocation(size_t call) { fail_allocation = call; }
void fault_fail_creation(size_t call) { fail_creation = call; }
void fault_fail_join(size_t call) { fail_join = call; }
size_t fault_starts(void) { return starts; }
size_t fault_joins(void) { return joins; }
void *fault_calloc(size_t count, size_t size) {
    ++allocations;
    if (fail_allocation && allocations == fail_allocation) return NULL;
    return calloc(count, size);
}
int fault_pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                         void *(*entry)(void *), void *argument) {
    ++creations;
    if (fail_creation && creations == fail_creation) return EAGAIN;
    int status = pthread_create(thread, attr, entry, argument);
    if (!status) ++starts;
    return status;
}
int fault_pthread_join(pthread_t thread, void **result) {
    ++join_calls;
    if (fail_join && join_calls == fail_join) return EINVAL;
    int status = pthread_join(thread, result);
    if (!status) ++joins;
    return status;
}
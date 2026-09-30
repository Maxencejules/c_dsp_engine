#ifndef FAULT_HOOKS_H
#define FAULT_HOOKS_H
#include <stddef.h>
#include <stdlib.h>
#include <pthread.h>
/* Test-only indirection. All counters are accessed by the caller thread. */
void fault_reset(void);
void fault_fail_allocation(size_t call);
void fault_fail_creation(size_t call);
void fault_fail_join(size_t call);
size_t fault_starts(void);
size_t fault_joins(void);
void *fault_calloc(size_t count, size_t size);
int fault_pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                         void *(*entry)(void *), void *argument);
int fault_pthread_join(pthread_t thread, void **result);
#define calloc fault_calloc
#define pthread_create fault_pthread_create
#define pthread_join fault_pthread_join
#endif
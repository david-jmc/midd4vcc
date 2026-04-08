#ifndef JOB_CATALOG_H
#define JOB_CATALOG_H

#include <stddef.h>

typedef int (*job_fn_t)(const int *args, size_t argc);

typedef struct {
    const char *service;
    const char *function;
    job_fn_t fn;
} job_entry_t;

/* funções concretas */
int job_add(const int *args, size_t argc);
int job_mul(const int *args, size_t argc);
int job_factorial(const int *args, size_t argc);
int job_fibonacci(const int *args, size_t argc);

/* lookup por service + function */
job_fn_t job_catalog_lookup(const char *service, const char *function);

#endif

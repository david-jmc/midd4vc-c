#ifndef JOB_CATALOG_H
#define JOB_CATALOG_H

typedef int (*job_fn_t)(int a, int b);

typedef struct {
    const char *service;
    const char *function;
    job_fn_t fn;
} job_entry_t;

/* funções concretas */
int job_add(int a, int b);
int job_mul(int a, int b);

/* lookup por service + function */
job_fn_t job_catalog_lookup(
    const char *service,
    const char *function);

#endif

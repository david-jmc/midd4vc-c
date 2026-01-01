#include "job_catalog.h"
#include <string.h>

int job_add(int a, int b) {
    return a + b;
}

int job_mul(int a, int b) {
    return a * b;
}

static job_entry_t catalog[] = {
    { "math", "add", job_add },
    { "math", "multiply", job_mul },
};

job_fn_t job_catalog_lookup(
    const char *service,
    const char *function)
{
    for (unsigned i = 0; i < sizeof(catalog)/sizeof(catalog[0]); i++) {
        if (strcmp(catalog[i].service, service) == 0 &&
            strcmp(catalog[i].function, function) == 0)
            return catalog[i].fn;
    }
    return NULL;
}

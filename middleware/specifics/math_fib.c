#include <stdio.h>

// O nome 'run_job' é o padrão que o dlsym vai buscar no job_catalog.c
int run_job(const int *args, int argc) {
    if (argc < 1) return 0;
    int n = args[0];
    if (n <= 1) return n;
    
    int a = 0, b = 1, temp;
    for (int i = 2; i <= n; i++) {
        temp = a + b;
        a = b;
        b = temp;
    }
    return b;
}
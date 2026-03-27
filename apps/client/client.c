#define _POSIX_C_SOURCE 199309L
#include "../../middleware/distribution/midd4vc_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

// Estrutura para rastrear o tempo de cada Job individualmente
typedef struct {
    struct timespec start;
    int n_val;
} job_context_t;


job_context_t tracker;
char current_job_id[64];
char client_name[32];

void on_result_received(const char* job_id, midd4vc_job_status_t status, int result) {
    if (status == JOB_DONE) {
        struct timespec end;
        clock_gettime(CLOCK_MONOTONIC, &end);

        // Cálculo do RTT com precisão de microssegundos
        double rtt = (end.tv_sec - tracker.start.tv_sec) * 1000.0 +
                     (end.tv_nsec - tracker.start.tv_nsec) / 1000000.0;

        // LOG FORMATADO PARA O ARTIGO
        // CSV: TIMESTAMP;CLIENT_ID;JOB_ID;ARG;RESULT;RTT_MS
        printf("DATA_POINT;%ld;%s;%s;%d;%d;%.4f\n", 
                time(NULL), client_name, job_id, tracker.n_val, result, rtt);
        fflush(stdout); 
    }
}

int main(int argc, char *argv[]) {

    // Seed para o random ser diferente em cada processo cliente
    srand(time(NULL) + getpid());

    midd4vc_client_t *client = midd4vc_create(client_name, ROLE_CLIENT);
    midd4vc_set_job_result_handler(client, on_result_received);
    midd4vc_start(client);

    while (1) {
        // Gera um número aleatório entre 5 e 25 para calcular Fibonacci
        int n_fib = (rand() % 21) + 5; 
        int vals[] = { n_fib };
        char jid[32];
        snprintf(jid, sizeof(jid), "fib_%d_%d", getpid(), rand() % 100);

        printf("[%s] Solicitando Fibonacci de %d na posição (%.4f, %.4f)...\n", 
                client_name, n_fib, 0.0, 0.0);
        
        // Submete job para o serviço "math" função "fib" (conforme seu catálogo)
        //midd4vc_submit_job(client, jid, "math", "fib", 0.0, 0.0, vals, 1);
        midd4vc_submit_job(client, jid, "math", "fib", 0.0, 0.0, vals, 1);

        sleep(rand() % 4 + 3); // Intervalo entre 3 e 7 segundos
    }
    return 0;
}
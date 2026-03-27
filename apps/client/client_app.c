#include "../../middleware/distribution/midd4vc_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

void on_result_received(const char* job_id, midd4vc_job_status_t status, int result) {
    if (status == JOB_DONE) {
        printf("[CLIENT] Resposta do Job %s: Fibonacci = %d\n", job_id, result);
    }
}

int main(int argc, char *argv[]) {

    if (argc < 4) {
        printf("Uso: %s <ID> <LAT> <LON>\n", argv[0]);
        return 1;
    }

    char *id = (argc > 1) ? argv[1] : "client_default";
    double target_lat = atof(argv[2]);
    double target_lon = atof(argv[3]);
    srand(time(NULL) + getpid());

    midd4vc_client_t *client = midd4vc_create(id, ROLE_CLIENT);
    midd4vc_set_job_result_handler(client, on_result_received);
    midd4vc_start(client);

    while (1) {
        // Gera um número aleatório entre 5 e 25 para calcular Fibonacci
        int n_fib = (rand() % 21) + 5; 
        int vals[] = { n_fib };
        char jid[32];
        snprintf(jid, sizeof(jid), "fib_%d_%d", getpid(), rand() % 100);

        printf("[%s] Solicitando Fibonacci de %d na posição (%.4f, %.4f)...\n", 
                id, n_fib, target_lat, target_lon);
        
        // Submete job para o serviço "math" função "fib" (conforme seu catálogo)
        //midd4vc_submit_job(client, jid, "math", "fib", 0.0, 0.0, vals, 1);
        midd4vc_submit_job(client, jid, "math", "fib", target_lat, target_lon, vals, 1);

        sleep(rand() % 4 + 3); // Intervalo entre 3 e 7 segundos
    }
    return 0;
}
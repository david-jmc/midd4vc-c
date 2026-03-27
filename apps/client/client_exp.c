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

// Como teremos vários jobs "em voo", o ideal para o artigo é um array simples
// ou usar o próprio job_id para mapear. Para 100 jobs sequenciais (5s de intervalo),
// um rastreador simples resolve.
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
    if (argc < 4) {
        printf("Uso: %s <ID_CLIENTE> <LAT> <LON>\n", argv[0]);
        return 1;
    }

    strncpy(client_name, argv[1], sizeof(client_name));
    double target_lat = atof(argv[2]);
    double target_lon = atof(argv[3]);
    
    // Seed para o random ser diferente em cada processo cliente
    srand(time(NULL) + getpid());

    midd4vc_client_t *client = midd4vc_create(client_name, ROLE_CLIENT);
    midd4vc_set_job_result_handler(client, on_result_received);
    midd4vc_start(client);

    // Conforme o Artigo 1: 100 jobs por experimento
    for (int i = 0; i < 100; i++) {
        // Replica o random.randint(1, 10) do seu código Python
        tracker.n_val = (rand() % 10) + 1; 
        int vals[] = { tracker.n_val };
        
        snprintf(current_job_id, sizeof(current_job_id), "job_%s_%d", client_name, i);
        
        // Marca o tempo de saída
        clock_gettime(CLOCK_MONOTONIC, &tracker.start);

        // Submete para math.factorial conforme a aplicação original
        // Note: Verifique se no seu catálogo em C o serviço está como "math" e função "factorial"
        midd4vc_submit_job(client, current_job_id, "math", "factorial", target_lat, target_lon, vals, 1);

        // Intervalo de 5 segundos entre as publicações (metodologia do Artigo 1)
        sleep(5); 
    }

    // Aguarda um pouco o último resultado antes de fechar
    sleep(10);
    return 0;
}
#include "../../middleware/midd4vc_client.h"
#include "job_catalog.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

typedef struct {
    char id[64];
    double lat;
    double lon;
} app_state_t;

static app_state_t my_app;

void on_job_received(midd4vc_client_t *c, const midd4vc_job_t *job) {
    // Log identificando qual instância de veículo recebeu o trabalho
    printf("[%s] Job recebido: %s.%s (ID: %s)\n", 
           my_app.id, job->service, job->function, job->job_id);
    
    job_fn_t worker = job_catalog_lookup(job->service, job->function);
    
    if (worker) {
        // Simula esforço computacional
        usleep(200000); 

        int result = worker(job->args, job->argc);
        
        printf("[%s] Job %s concluído. Resultado: %d\n", my_app.id, job->job_id, result);
        
        midd4vc_send_job_success(c, job->client_id, job->job_id, result);
    } else {
        printf("[%s] Erro: Função não suportada.\n", my_app.id);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        printf("Uso: %s <ID> <LAT> <LON>\n", argv[0]);
        return 1;
    }

    // Identidade única vinda do script de inicialização
    strncpy(my_app.id, argv[1], 63);
    my_app.lat = atof(argv[2]);
    my_app.lon = atof(argv[3]);

    srand(time(NULL) + getpid());

    midd4vc_client_t *v_node = midd4vc_create(my_app.id, ROLE_VEHICLE);
    midd4vc_set_job_handler(v_node, on_job_received);
    midd4vc_start(v_node);

    printf("[%s] Estacionado em (%.4f, %.4f). Pronto.\n", my_app.id, my_app.lat, my_app.lon);

    while (1) {
        char status[128];
        // Envia posição fixa (Static Cloud)
        snprintf(status, sizeof(status), "{\"latitude\":%.6f,\"longitude\":%.6f}", 
                 my_app.lat, my_app.lon);
        
        midd4vc_register(v_node, status);
        sleep(1); // Beaconing menos frequente para não poluir o log da simulação
    }
    
    return 0;
}
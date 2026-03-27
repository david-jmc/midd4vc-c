#include "../../middleware/distribution/midd4vc_client.h"
#include "job_catalog.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

/* Estrutura para manter o estado do veículo móvel */
typedef struct {
    char id[64];
    double lat;
    double lon;
    double v_lat; // Velocidade/Direção em Latitude
    double v_lon; // Velocidade/Direção em Longitude
} mobile_state_t;

static mobile_state_t my_app;

void on_job_received(midd4vc_client_t *c, const midd4vc_job_t *job) {
    // Log detalhado: mostra a posição exata onde o carro estava quando o job chegou
    printf("\n>>> [%s] MOBILITY ALERT: Job recebido em (%.6f, %.6f)\n", 
            my_app.id, my_app.lat, my_app.lon);
    printf("    Job: %s.%s (ID: %s)\n", job->service, job->function, job->job_id);
    
    job_fn_t worker = job_catalog_lookup(job->service, job->function);
    
    if (worker) {
        // Simula o tempo de processamento
        usleep(200000); 

        int result = worker(job->args, job->argc);
        printf("<<< [%s] Job %s concluído. Enviando resposta...\n", my_app.id, job->job_id);
        
        midd4vc_send_job_success(c, job->client_id, job->job_id, result);
    } else {
        printf("[%s] Erro: Função '%s' não encontrada.\n", my_app.id, job->function);
    }
}

/**
 * Converte velocidade de km/h para deslocamento em graus de latitude
 * @param kmh Velocidade em km/h
 * @param update_interval_ms Intervalo de atualização em milissegundos
 * @return O delta (deslocamento) a ser somado à coordenada
 */
double kmh_to_lat_delta(double kmh, int update_interval_ms) {
    double metros_por_segundo = kmh / 3.6;
    double intervalo_segundos = update_interval_ms / 1000.0;
    // 111111.0 é a média de metros por grau de latitude
    return (metros_por_segundo * intervalo_segundos) / 111111.0;
}

int main(int argc, char *argv[]) {
    if (argc < 5) {
        printf("Uso: %s <ID> <LAT> <LON> <KMH>\n", argv[0]);
        printf("Exemplo: %s V1 41.001 -8.001 50\n", argv[0]);
        return 1;
    }

    // Inicialização básica
    strncpy(my_app.id, argv[1], 63);
    my_app.lat = atof(argv[2]);
    my_app.lon = atof(argv[3]);
    double velocidade_kmh = atof(argv[4]);

    // Configuramos o intervalo de atualização
    int update_ms = 500;

    // Calculamos o deslocamento fixo para o movimento (ex: sempre para o Norte)
    my_app.v_lat = kmh_to_lat_delta(velocidade_kmh, update_ms);
    my_app.v_lon = 0.0; // Mantemos longitude fixa para simplificar o teste de trajetória

    midd4vc_client_t *v_node = midd4vc_create(my_app.id, ROLE_VEHICLE);
    midd4vc_set_job_handler(v_node, on_job_received);
    midd4vc_start(v_node);

    printf("[%s] Movendo a %.1f km/h. Delta por update: %.8f\n", 
            my_app.id, velocidade_kmh, my_app.v_lat);

    while (1) {
        my_app.lat += my_app.v_lat;
        my_app.lon += my_app.v_lon;

        char status[128];
        snprintf(status, sizeof(status), "{\"latitude\":%.6f,\"longitude\":%.6f}", 
                 my_app.lat, my_app.lon);
        
        midd4vc_register(v_node, status);
        usleep(update_ms * 1000); 
    }
    
    return 0;
}
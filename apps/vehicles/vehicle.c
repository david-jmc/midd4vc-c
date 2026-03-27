#include "../../middleware/distribution/midd4vc_client.h"
#include "job_catalog.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

/* Estrutura para manter o estado interno do veículo */
typedef struct {
    char id[64];
    double lat;
    double lon;
} app_state_t;

static app_state_t application;

/**
 * Handler disparado quando o middleware recebe um Job destinado a este veículo.
 */
void on_job_received(midd4vc_client_t *c, const midd4vc_job_t *job) {
    // Log identificando o processamento (importante para debug, pode ser silenciado no stress test)
    printf("[%s] Job Recebido: %s.%s (ID: %s)\n", 
           application.id, job->service, job->function, job->job_id);
    
    // Busca a implementação real no catálogo em C
    job_fn_t worker = job_catalog_lookup(job->service, job->function);
    
    if (worker) {
  
        int result = worker(job->args, job->argc);
        
        printf("[%s] Job %s concluído. Resultado: %d\n", application.id, job->job_id, result);
        
        midd4vc_send_job_success(c, job->client_id, job->job_id, result);
    } else {
        printf("[%s] Erro: Função %s.%s não suportada no catálogo.\n", 
               application.id, job->service, job->function);
    }
}

int main(int argc, char *argv[]) {
    
    // Seed única por processo para evitar padrões idênticos em centenas de veículos
    srand(time(NULL) + getpid());

    midd4vc_client_t *v_node = midd4vc_create(application.id, ROLE_VEHICLE);
    
    // Configura o handler para processar as tarefas (math.fib, math.factorial, etc)
    midd4vc_set_job_handler(v_node, on_job_received);
    
    // Conecta ao Broker e inicia threads de escuta
    midd4vc_start(v_node);

    printf("[%s] Online em (%.6f, %.6f). Aguardando tarefas...\n", 
           application.id, application.lat, application.lon);

    /* Loop Principal: Beaconing de Localização */
    while (1) {
        char status_json[128];
        
        /* * Prepara o payload JSON com a localização. 
         * Fundamental para os Cenários de Proximidade e Dynamic Cloud.
         */
        snprintf(status_json, sizeof(status_json), 
                 "{\"latitude\":%.6f,\"longitude\":%.6f}", 
                 application.lat, application.lon);
        
        // Registra a posição atual no Servidor/Broker
        midd4vc_register(v_node, status_json);

        /* * Frequência de atualização: 1 segundo.
         * Em produção pode ser maior, mas para experimentos de proximidade 
         * com veículos em movimento (Cenários 3 e 4), 1s é o ideal.
         */
        sleep(1); 
    }
    
    return 0;
}
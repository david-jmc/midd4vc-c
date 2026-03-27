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

static app_state_t my_app;

/**
 * Handler disparado quando o middleware recebe um Job destinado a este veículo.
 */
void on_job_received(midd4vc_client_t *c, const midd4vc_job_t *job) {
    // Log identificando o processamento (importante para debug, pode ser silenciado no stress test)
    printf("[%s] Job Recebido: %s.%s (ID: %s)\n", 
           my_app.id, job->service, job->function, job->job_id);
    
    // Busca a implementação real no catálogo em C
    job_fn_t worker = job_catalog_lookup(job->service, job->function);
    
    if (worker) {
        /* * REMOVIDO usleep(200000) para comparação justa com Python.
         * O processamento em C será quase instantâneo para Fatorial(1..10).
         */
        int result = worker(job->args, job->argc);
        
        printf("[%s] Job %s concluído. Resultado: %d\n", my_app.id, job->job_id, result);
        
        // Envia o resultado de volta para o cliente solicitante via Middleware
        midd4vc_send_job_success(c, job->client_id, job->job_id, result);
    } else {
        printf("[%s] Erro: Função %s.%s não suportada no catálogo.\n", 
               my_app.id, job->service, job->function);
        // Opcional: midd4vc_send_job_error(c, job->client_id, job->job_id, "Not Found");
    }
}

int main(int argc, char *argv[]) {
    // Validação de argumentos para o script de orquestração
    if (argc < 4) {
        printf("Uso: %s <ID_VEICULO> <LATITUDE> <LONGITUDE>\n", argv[0]);
        return 1;
    }

    // Inicialização do estado baseado nos argumentos de linha de comando
    strncpy(my_app.id, argv[1], 63);
    my_app.lat = atof(argv[2]);
    my_app.lon = atof(argv[3]);

    // Seed única por processo para evitar padrões idênticos em centenas de veículos
    srand(time(NULL) + getpid());

    // Criação da instância do cliente Midd4VC com papel de VEÍCULO
    midd4vc_client_t *v_node = midd4vc_create(my_app.id, ROLE_VEHICLE);
    
    // Configura o handler para processar as tarefas (math.fib, math.factorial, etc)
    midd4vc_set_job_handler(v_node, on_job_received);
    
    // Conecta ao Broker e inicia threads de escuta
    midd4vc_start(v_node);

    printf("[%s] Online em (%.6f, %.6f). Aguardando tarefas...\n", 
           my_app.id, my_app.lat, my_app.lon);

    /* Loop Principal: Beaconing de Localização */
    while (1) {
        char status_json[128];
        
        /* * Prepara o payload JSON com a localização. 
         * Fundamental para os Cenários de Proximidade e Dynamic Cloud.
         */
        snprintf(status_json, sizeof(status_json), 
                 "{\"latitude\":%.6f,\"longitude\":%.6f}", 
                 my_app.lat, my_app.lon);
        
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
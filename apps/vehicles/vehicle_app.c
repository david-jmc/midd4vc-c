#include "../../middleware/midd4vc_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

// Estrutura simples de estado local
typedef struct {
    char id[64];
    double lat;
    double lon;
    double speed; 
} app_state_t;

static app_state_t my_app;

// Callback de Job: Transparente para a aplicação
void on_job_received(midd4vc_client_t *c, const midd4vc_job_t *job) {
    printf("[APP-VEHICLE] Executando Job %s: %d + %d\n", job->job_id, job->args[0], job->args[1]);
    
    // Simula processamento
    usleep(800000); 
    int result = job->args[0] + job->args[1];
    
    // Resposta via API do Middleware
    midd4vc_send_job_success(c, job->client_id, job->job_id, result);
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        printf("Uso: %s <ID> <LAT> <LON> [SPEED]\n", argv[0]);
        return 1;
    }

    strncpy(my_app.id, argv[1], 63);
    my_app.lat = atof(argv[2]);
    my_app.lon = atof(argv[3]);
    my_app.speed = (argc > 4) ? atof(argv[4]) : 0.0;

    srand(time(NULL) + getpid());

    midd4vc_client_t *v_node = midd4vc_create(my_app.id, ROLE_VEHICLE);
    midd4vc_set_job_handler(v_node, on_job_received);
    midd4vc_start(v_node);

    printf("[VEHICLE %s] Online em (%.4f, %.4f)\n", my_app.id, my_app.lat, my_app.lon);

    while (1) {
        // Simula movimento (Mobilidade)
        if (my_app.speed > 0) {
            my_app.lat += ((rand() % 10 - 5) * 0.0001) * my_app.speed;
            my_app.lon += ((rand() % 10 - 5) * 0.0001) * my_app.speed;
        }

        // Beaconing via Middleware
        char status[128];
        snprintf(status, sizeof(status), "{\"latitude\":%.6f,\"longitude\":%.6f}", my_app.lat, my_app.lon);
        midd4vc_register(v_node, status);

        sleep(3); // Envia posição a cada 3s
    }
    return 0;
}
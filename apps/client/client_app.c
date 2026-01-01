#include "../../middleware/midd4vc_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

// Callback de Resultado: Transparente
void on_result_received(const char* job_id, midd4vc_job_status_t status, int result) {
    if (status == JOB_DONE) {
        printf("[APP-CLIENT] Job %s FINALIZADO! Resultado: %d\n", job_id, result);
    } else {
        printf("[APP-CLIENT] Job %s FALHOU.\n", job_id);
    }
}

int main(int argc, char *argv[]) {
    char *id = (argc > 1) ? argv[1] : "user_1";
    srand(time(NULL));

    midd4vc_client_t *client = midd4vc_create(id, ROLE_CLIENT);
    midd4vc_set_job_result_handler(client, on_result_received);
    midd4vc_start(client);

    printf("[CLIENT %s] Conectado à Cloud...\n", id);

    while (1) {
        int vals[] = {rand() % 50, rand() % 50};
        char jid[32];
        snprintf(jid, sizeof(jid), "job_%d", rand() % 1000);

        double my_lat = 41.1234;
        double my_lon = -8.5678;

        printf("[APP-CLIENT] Solicitando: %d + %d (ID: %s)\n", vals[0], vals[1], jid);
        
        midd4vc_submit_job(client, jid, "math", "sum", my_lat, my_lon, vals, 2);

        sleep(7); // Nova requisição a cada 7 segundos
    }
    return 0;
}
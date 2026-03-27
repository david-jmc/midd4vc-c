#include "../../middleware/distribution/midd4vc_client.h"
#include "../../middleware/distribution/midd4vc_protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h> // Para round() se necessário para imprimir

// Constantes de Recife ajustadas para minimizar o oceano (bounding box mais "interna")
#define RECIFE_MIN_LAT -8.060  // Mais ao norte (longe da orla sul)
#define RECIFE_MAX_LAT -8.048  // Mais ao sul (longe da orla norte)
#define RECIFE_MIN_LON -34.895 // Mais a leste (longe do porto e rio)
#define RECIFE_MAX_LON -34.875 // Mais a oeste (longe do mar)

// Tópicos de eventos sincronizados com dados_recife.py
#define TOPIC_ITS_LOCATION "vc/event/car_location"
#define TOPIC_ITS_FLOODING "vc/event/flooding_alert"

// Estrutura para manter o estado do veículo
typedef struct {
    char id[64];
    double lat;
    double lon;
    double speed;
} vehicle_state_t;

// Função para gerar coordenadas aleatórias dentro da bounding box ajustada
void generate_recife_coords(double *lat, double *lon) {
    *lat = RECIFE_MIN_LAT + ((double)rand() / RAND_MAX) * (RECIFE_MAX_LAT - RECIFE_MIN_LAT);
    *lon = RECIFE_MIN_LON + ((double)rand() / RAND_MAX) * (RECIFE_MAX_LON - RECIFE_MIN_LON);
}

// Função para simular o movimento (agora com sorteio dentro da bounding box)
void move_vehicle(vehicle_state_t *v) {
    // Para simplificar, cada "movimento" reseta para uma nova posição aleatória
    // dentro da área segura. Isso simula o carro se deslocando na cidade.
    generate_recife_coords(&v->lat, &v->lon);
    v->speed = 10.0 + (rand() % 51); // Velocidade entre 10 e 60 km/h
}

int main(int argc, char *argv[]) {
    if (argc < 2) { // Apenas o ID é necessário, lat/lon inicial serão sorteados
        printf("Uso: %s <id_do_carro>\n", argv[0]);
        return 1;
    }

    // Inicializa o gerador de números aleatórios
    // Usamos o PID do processo para garantir que cada carro tenha uma semente diferente
    srand(time(NULL) ^ getpid()); 

    vehicle_state_t my_car;
    strncpy(my_car.id, argv[1], sizeof(my_car.id) - 1);
    my_car.id[sizeof(my_car.id) - 1] = '\0'; // Garante null termination

    // Gera a posição inicial dentro da área de Recife (evitando o oceano)
    generate_recife_coords(&my_car.lat, &my_car.lon);
    my_car.speed = 0.0; // Inicia parado ou com velocidade inicial

    // 1. Inicializa o Middleware
    midd4vc_client_t *v_node = midd4vc_create(my_car.id, ROLE_VEHICLE);
    midd4vc_start(v_node); // Este já subscreve em "vc/event/#" automaticamente

    printf("[%s] Carro %s iniciado em: %.4f, %.4f\n", my_car.id, my_car.id, my_car.lat, my_car.lon);

    // 2. Loop Principal de Simulação
    while (1) {
        move_vehicle(&my_car); // Obtém nova posição e velocidade

        char payload[512];
        long now = (long)time(NULL); // Timestamp para o Python

        // --- 2.1 Publicar Evento de Localização (TOPIC_ITS_LOCATION) ---
        snprintf(payload, sizeof(payload),
            "{\"event_type\":\"car_location\",\"source_id\":\"%s\",\"timestamp\":%ld,"
            "\"location\":{\"latitude\":%.6f,\"longitude\":%.6f},\"speed_kph\":%.1f}",
            my_car.id, now, my_car.lat, my_car.lon, my_car.speed);
        
        midd4vc_publish(v_node, TOPIC_ITS_LOCATION, payload);
        printf("[%s] Localização atualizada: %.4f, %.4f (Vel: %.1f km/h)\n", my_car.id, my_car.lat, my_car.lon, my_car.speed);

        // --- 2.2 Simular Alagamento (TOPIC_ITS_FLOODING) com 15% de chance ---
        if ((rand() % 100) < 15) { // 15% de chance de reportar alagamento
            const char *severities[] = {"minor", "moderate", "severe"};
            const char *severity = severities[rand() % 3]; // Sorteia severidade

            snprintf(payload, sizeof(payload),
                "{\"event_type\":\"flooding_alert\",\"source_id\":\"%s\",\"timestamp\":%ld,"
                "\"location\":{\"latitude\":%.6f,\"longitude\":%.6f},"
                "\"severity\":\"%s\",\"description\":\"Alerta de alagamento via C-Node\"}",
                my_car.id, now, my_car.lat, my_car.lon, severity);
            
            midd4vc_publish(v_node, TOPIC_ITS_FLOODING, payload);
            printf("[%s] ⚠️ ALAGAMENTO REPORTADO em %.4f, %.4f (Severidade: %s)\n", my_car.id, my_car.lat, my_car.lon, severity);
        }

        // --- 2.3 Atualizar Registro do Veículo (para o Servidor Orchestrator) ---
        // Isso é importante para o balanceamento de carga do servidor
        char reg_payload[128];
        snprintf(reg_payload, sizeof(reg_payload), "{\"latitude\":%.6f,\"longitude\":%.6f}", my_car.lat, my_car.lon);
        midd4vc_register(v_node, reg_payload);

        sleep(5); // Atualiza a cada 5 segundos (location_update_interval do Python)
    }

    midd4vc_stop(v_node); // Chama o stop do middleware
    return 0;
}
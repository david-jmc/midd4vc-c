#define _POSIX_C_SOURCE 199309L
#define _XOPEN_SOURCE 500
#include "../../middleware/distribution/midd4vc_client.h"
#include "../../middleware/distribution/midd4vc_protocol.h"
#include "../../middleware/services/midd4vc_scheduler.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#define MAX_VEHICLES 1000
#define MAX_JOBS     1024
#define JOB_TIMEOUT  2 //5  
#define MAX_RETRIES  3
#define VEHICLE_TIMEOUT 10 //120 
#define MAX_LOAD 10 //5         

static vehicle_t vehicles[MAX_VEHICLES];
static int vehicle_count = 0;
static job_ctx_t jobs[MAX_JOBS];

// experiments
typedef struct {
    char client_id[64];
    int jobs_sent;
    double rtt_sum;
    int vehicle_usage[MAX_VEHICLES]; // Índice mapeado para o array 'vehicles'
    int ready_to_log;
} client_stats_t;

static client_stats_t stats[100]; // Suporta até 100 clientes simultâneos
static int stats_count = 0;

// Função para formatar a data atual (YYYY-MM-DD HH:MM:SS)
void get_timestamp(char *buf, size_t len) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buf, len, "%Y-%m-%d %H:%M:%S", t);
}

int header_impresso = 0;
void imprimir_linha_csv(client_stats_t *cs) {
    char ts[32];
    get_timestamp(ts, sizeof(ts));

    // Formato: timestamp,client_id,jobs_sent,avg_rtt,messages_per_vehicle
    printf("EXPERIMENT_LOG,%s,%s,%d,%.4f,", 
           ts, cs->client_id, cs->jobs_sent, (cs->rtt_sum / cs->jobs_sent));

    // Monta a lista de veículos: veh337:1; veh175:1; ...
    int first = 1;
    for (int i = 0; i < vehicle_count; i++) {
        if (cs->vehicle_usage[i] > 0) {
            if (!first) printf("; ");
            printf("%s:%d", vehicles[i].vehicle_id, cs->vehicle_usage[i]);
            first = 0;
        }
    }
    printf("\n");
    fflush(stdout); // Garante que a linha vá para o arquivo CSV imediatamente
}


static scheduler_ctx_t sched_ctx = {
    .rr_idx = 0,
    .prox_rr_idx = 0,
    .max_load = MAX_LOAD,
    //.reach_distance = 500.0
    .proximity_threshold = 500
};

static balancing_strategy_fn current_policy = strategy_least_loaded;

/* --- Helpers --- */

static vehicle_t *get_vehicle(const char *id) {
    for (int i = 0; i < vehicle_count; i++) {
        if (strcmp(vehicles[i].vehicle_id, id) == 0) {
            return &vehicles[i];
        }
    }
    
   if (vehicle_count < MAX_VEHICLES) {
        vehicle_t *v = &vehicles[vehicle_count++];
        memset(v, 0, sizeof(vehicle_t));
        strncpy(v->vehicle_id, id, sizeof(v->vehicle_id) - 1);
        v->is_active = 1;
        //v->max_load = 5;
        v->last_seen = time(NULL); // Inicializa com o tempo atual
        return v;
    }
    return NULL;
}

/* --- Handlers --- */

static void assign_job(midd4vc_client_t *c, job_ctx_t *j) {
    if (!current_policy) current_policy = strategy_least_loaded;
    
    vehicle_t *v = current_policy(vehicles, vehicle_count,j->req_lat, j->req_lon, &sched_ctx);
    
    if (!v) {
        printf("[SERVER] Cloud Error: No nodes available for task %s\n", j->job_id);
        return;
    }

    v->active_jobs++;
    strncpy(j->assigned_vehicle, v->vehicle_id, sizeof(j->assigned_vehicle) - 1);
    j->assigned = 1;
    j->sent_at = time(NULL);
    clock_gettime(CLOCK_MONOTONIC, &j->sent_at_spec);

    char assign_topic[128];
    snprintf(assign_topic, sizeof(assign_topic), TOPIC_JOB_ASSIGN, v->vehicle_id);
    midd4vc_publish(c, assign_topic, j->payload);
    
    printf("[SERVER] Job %s -> %s (Load: %d)\n", j->job_id, v->vehicle_id, v->active_jobs);
}

static void on_register(midd4vc_client_t *c, const char *topic, const char *payload) {
    char vehicle_id[64];
    double lat = 0, lon = 0;
    sscanf(topic, "vc/vehicle/%63[^/]/register/request", vehicle_id);
    
    char *lat_ptr = strstr(payload, "\"latitude\":");
    char *lon_ptr = strstr(payload, "\"longitude\":");
    if (lat_ptr) sscanf(lat_ptr, "\"latitude\":%lf", &lat);
    if (lon_ptr) sscanf(lon_ptr, "\"longitude\":%lf", &lon);

    vehicle_t *v = get_vehicle(vehicle_id);
    if (v) {
        v->last_seen = time(NULL);
        v->latitude = lat;
        v->longitude = lon;
        v->is_active = 1;
        printf("[SERVER] Node %s update at (%.4f, %.4f)\n", vehicle_id, lat, lon);
    }
}

static void on_job_submit(midd4vc_client_t *c, const char *topic, const char *payload) {
    int slot = -1;
    for (int i = 0; i < MAX_JOBS; i++) {
        if (!jobs[i].in_use || jobs[i].completed) { slot = i; break; }
    }
    if (slot == -1) return;

    // 1. Usa o Codec para extrair tudo de uma vez
    midd4vc_job_t parsed;
    if (!midd4vc_parse_job(payload, &parsed)) {
        printf("[SERVER] Erro: Payload de Job inválido.\n");
        return;
    }

    job_ctx_t *j = &jobs[slot];
    memset(j, 0, sizeof(job_ctx_t));
    j->in_use = 1;
    
    // 2. Copia os dados parseados para o contexto do servidor
    strncpy(j->job_id, parsed.job_id, sizeof(j->job_id) - 1);
    strncpy(j->client_id, parsed.client_id, sizeof(j->client_id) - 1);
    
    // Se o client_id não estava no payload, tenta pegar do tópico (fallback)
    if (strlen(j->client_id) == 0) {
        sscanf(topic, "vc/client/%63[^/]", j->client_id);
    }

    j->req_lat = parsed.lat;
    j->req_lon = parsed.lon;
    strncpy(j->payload, payload, sizeof(j->payload) - 1);

    // 3. Log inteligente baseado na presença de GPS
    if (j->req_lat == GPS_INVALID) {
        printf("[SERVER] Novo Job %s (Agnóstico a posição). Selecionando...\n", j->job_id);
    } else {
        printf("[SERVER] Novo Job %s em (%.4f, %.4f). Selecionando...\n", 
               j->job_id, j->req_lat, j->req_lon);
    }

    assign_job(c, j);
}

/*
static void on_job_submit(midd4vc_client_t *c, const char *topic, const char *payload) {
    int slot = -1;
    for (int i = 0; i < MAX_JOBS; i++) {
        if (!jobs[i].in_use || jobs[i].completed) { slot = i; break; }
    }
    if (slot == -1) return;

    job_ctx_t *j = &jobs[slot];
    memset(j, 0, sizeof(job_ctx_t));
    j->in_use = 1;
    strncpy(j->payload, payload, sizeof(j->payload) - 1);
    
    // Extrai client_id do tópico ou do payload
    if (sscanf(topic, "vc/client/%63[^/]", j->client_id) != 1) {
        char *c_ptr = strstr(payload, "\"client_id\":\"");
        if (c_ptr) sscanf(c_ptr, "\"client_id\":\"%63[^\"]\"", j->client_id);
    }
    
    char *id_ptr = strstr(payload, "\"job_id\":\"");
    if (id_ptr) sscanf(id_ptr, "\"job_id\":\"%63[^\"]\"", j->job_id);

    char *lat_p = strstr(payload, "\"lat\":");
    char *lon_p = strstr(payload, "\"lon\":");
    if (lat_p) sscanf(lat_p, "\"lat\":%lf", &j->req_lat);
    if (lon_p) sscanf(lon_p, "\"lon\":%lf", &j->req_lon);

    printf("[Midd4VC SERVER] Novo Job %s em (%.4f, %.4f). Selecionando veículo...\n", 
           j->job_id, j->req_lat, j->req_lon);

    assign_job(c, j);
}
    */

static void on_job_result(midd4vc_client_t *c, const char *topic, const char *payload) {
    char job_id[64] = {0}, target_client[64] = {0};
    int result_val = 0;
    char *p;
    if ((p = strstr(payload, "\"job_id\":\""))) sscanf(p, "\"job_id\":\"%63[^\"]\"", job_id);
    if ((p = strstr(payload, "\"client_id\":\""))) sscanf(p, "\"client_id\":\"%63[^\"]\"", target_client);
    if ((p = strstr(payload, "\"result\":"))) sscanf(p, "\"result\":%d", &result_val);
    
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].in_use && strcmp(jobs[i].job_id, job_id) == 0 && !jobs[i].completed) {
            jobs[i].completed = 1;
            
            struct timespec now;
            clock_gettime(CLOCK_MONOTONIC, &now);
            double elapsed = (now.tv_sec - jobs[i].sent_at_spec.tv_sec) + 
                             (now.tv_nsec - jobs[i].sent_at_spec.tv_nsec) / 1e9;

            //vehicle_t *v = get_vehicle(jobs[i].assigned_vehicle);
            //if (v && v->active_jobs > 0) v->active_jobs--;

            char final_json[512];
            snprintf(final_json, sizeof(final_json),
                "{\"job_id\":\"%s\",\"status\":\"DONE\",\"result\":%d,\"latency\":%.4f}",
                job_id, result_val, elapsed);

            char client_topic[128];
            snprintf(client_topic, sizeof(client_topic), "vc/client/%s/job/result", target_client);
            midd4vc_publish(c, client_topic, final_json);

            // --- LÓGICA DE ATUALIZAÇÃO DE STATS ---
            client_stats_t *cs = NULL;
            // Busca o cliente na tabela de estatísticas
            for (int s = 0; s < stats_count; s++) {
                if (strcmp(stats[s].client_id, target_client) == 0) {
                    cs = &stats[s];
                    break;
                }
            }
            // Se for um novo cliente, registra ele
            if (!cs && stats_count < 100) {
                cs = &stats[stats_count++];
                memset(cs, 0, sizeof(client_stats_t));
                strcpy(cs->client_id, target_client);
            }

            if (cs) {
                cs->jobs_sent++;
                cs->rtt_sum += elapsed;
                
                // Identifica qual veículo processou e incrementa o contador dele
                for (int v = 0; v < vehicle_count; v++) {
                    if (strcmp(vehicles[v].vehicle_id, jobs[i].assigned_vehicle) == 0) {
                        cs->vehicle_usage[v]++;
                        if (vehicles[v].active_jobs > 0) vehicles[v].active_jobs--;
                        break;
                    }
                }

                // Quando chegar em 10 jobs, gera a linha no CSV
                if (cs->jobs_sent % 10 == 0) {
                    imprimir_linha_csv(cs);
                }
            }
            
            printf("[PERF] Job %s DONE in %.4fs (Node: %s)\n", job_id, elapsed, jobs[i].assigned_vehicle);
            return;
        }
    }
}

static void on_config_policy(midd4vc_client_t *c, const char *topic, const char *payload) {
    if (strstr(payload, "RR")) current_policy = strategy_round_robin;
    else if (strstr(payload, "LOAD")) current_policy = strategy_least_loaded;
    else if (strstr(payload, "PROXIMITY")) current_policy = strategy_proximity_rr; // Ajustado
    else if (strstr(payload, "HYBRID")) current_policy = strategy_hybrid_pro;    // Adicionado
    printf("[SERVER] Policy Updated\n");
}

/*
static void on_config_policy(midd4vc_client_t *c, const char *topic, const char *payload) {
    if (strstr(payload, "RR")) current_policy = strategy_round_robin;
    else if (strstr(payload, "LOAD")) current_policy = strategy_least_loaded;
    else if (strstr(payload, "PROXIMITY")) current_policy = strategy_proximity;
    printf("[SERVER] Policy Updated\n");
}
*/

static void on_vehicle_status_change(midd4vc_client_t *c, const char *topic, const char *payload) {
    char v_id[64];
    sscanf(topic, "vc/vehicle/%63[^/]/status", v_id);

    vehicle_t *v = get_vehicle(v_id);

    if (v && strstr(payload, "offline_lwt")) {
        v->is_active = 0;
        v->active_jobs = 0; // new
        v->last_seen = time(NULL);
        printf("[SERVER] LWT DETECTADO: Veículo %s desconectou abruptamente!\n", v_id);
    } else if (v && strstr(payload, "online")) {
        v->is_active = 1;
        v->last_seen = time(NULL);
        printf("[SERVER] Veículo %s está Online via LWT\n", v_id);
    }
}

static void maintenance_loop(midd4vc_client_t *c) {
    time_t now = time(NULL);

    // 1. Monitoramento de Inatividade dos Veículos (Purge)
    int active_now = 0;
    for (int i = 0; i < vehicle_count; i++) {
        if (vehicles[i].is_active) {
            long diff = (long)(now - vehicles[i].last_seen);
            
            // Se o veículo não atualiza posição/status há mais de VEHICLE_TIMEOUT (60s)
            if (diff > VEHICLE_TIMEOUT) {
                vehicles[i].is_active = 0;
                // Importante: zera os jobs ativos pois o nó sumiu
                vehicles[i].active_jobs = 0; 
                printf("[SERVER] Vehicle %s OFFLINE (Timeout: %lds)\n", vehicles[i].vehicle_id, diff);
            } else {
                active_now++;
            }
        }
    } 
    
    // 2. Monitoramento de Timeouts de Jobs (Retry Logic)
    for (int i = 0; i < MAX_JOBS; i++) {
        job_ctx_t *j = &jobs[i];
        if (j->in_use && !j->completed && j->assigned && (now - j->sent_at >= JOB_TIMEOUT)) {
            vehicle_t *v = get_vehicle(j->assigned_vehicle);
            if (v && v->active_jobs > 0) v->active_jobs--;
            
            if (j->retries < MAX_RETRIES) {
                j->retries++; 
                j->assigned = 0;
                printf("[SERVER] Retry %d for Task %s\n", j->retries, j->job_id);
                assign_job(c, j);
            } else {
                j->completed = 1;
                j->in_use = 0;
                printf("[SERVER] Task %s FAILED\n", j->job_id);
            }
        }
    }
}

int main(void) {
    srand(time(NULL));
    midd4vc_client_t *srv = midd4vc_create("server_cloud_ctrl", ROLE_DASHBOARD);
    midd4vc_start(srv);

    midd4vc_subscribe(srv, "vc/+/+/register/request", on_register);
    midd4vc_subscribe(srv, "vc/client/+/job/submit", on_job_submit);
    midd4vc_subscribe(srv, TOPIC_SERVER_JOB_RESULT, on_job_result);
    midd4vc_subscribe(srv, "vc/server/config/policy", on_config_policy);
    midd4vc_subscribe(srv, "vc/vehicle/+/status", on_vehicle_status_change);

    printf("[SERVER] Cloud Orchestrator Ready\n");

    while (1) {
        maintenance_loop(srv);
        //sleep(1);
        usleep(100000);
    }
}
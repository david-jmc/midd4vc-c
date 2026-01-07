#define _POSIX_C_SOURCE 199309L
#include "../../middleware/midd4vc_client.h"
#include "../../middleware/midd4vc_protocol.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#define MAX_VEHICLES 128
#define MAX_JOBS     512
#define JOB_TIMEOUT  5  
#define MAX_RETRIES  3
#define VEHICLE_TIMEOUT 60 
#define MAX_LOAD 5         

static vehicle_t vehicles[MAX_VEHICLES];
static int vehicle_count = 0;
static job_ctx_t jobs[MAX_JOBS];

/* --- Estratégias de Balanceamento --- */

typedef vehicle_t* (*balancing_strategy_fn)(double lat, double lon);

static vehicle_t* strategy_proximity(double lat, double lon) {
    vehicle_t *best = NULL;
    double min_dist = 1e18;
    for (int i = 0; i < vehicle_count; i++) {
        if (!vehicles[i].is_active || vehicles[i].active_jobs >= MAX_LOAD) continue;
        double d = sqrt(pow(lat - vehicles[i].latitude, 2) + pow(lon - vehicles[i].longitude, 2));
        if (d < min_dist) { min_dist = d; best = &vehicles[i]; }
    }
    return (best) ? best : NULL;
}

static vehicle_t* strategy_round_robin(double lat, double lon) {
    static int rr_idx = 0;
    for (int i = 0; i < vehicle_count; i++) {
        int idx = (rr_idx++) % vehicle_count;
        if (vehicles[idx].is_active && vehicles[idx].active_jobs < MAX_LOAD) return &vehicles[idx];
    }
    return NULL;
}

static vehicle_t* strategy_least_loaded(double lat, double lon) {
    vehicle_t *best = NULL;
    for (int i = 0; i < vehicle_count; i++) {
        if (!vehicles[i].is_active || vehicles[i].active_jobs >= MAX_LOAD) continue;
        if (!best || vehicles[i].active_jobs < best->active_jobs) best = &vehicles[i];
    }
    return best;
}

static balancing_strategy_fn current_policy = strategy_round_robin;

/* --- Helpers --- */

static vehicle_t *get_vehicle(const char *id) {
    for (int i = 0; i < vehicle_count; i++)
        if (strcmp(vehicles[i].vehicle_id, id) == 0) return &vehicles[i];
    
    if (vehicle_count >= MAX_VEHICLES) return NULL;
    vehicle_t *v = &vehicles[vehicle_count++];
    memset(v, 0, sizeof(vehicle_t));
    strcpy(v->vehicle_id, id);
    v->is_active = 1;
    return v;
}

/* --- Handlers --- */

static void assign_job(midd4vc_client_t *c, job_ctx_t *j) {
    if (!current_policy) current_policy = strategy_proximity;
    
    vehicle_t *v = current_policy(j->req_lat, j->req_lon);
    
    if (!v) {
        printf("[SERVER] Cloud Error: No nodes available for job %s\n", j->job_id);
        return;
    }

    v->active_jobs++;
    strcpy(j->assigned_vehicle, v->vehicle_id);
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

            vehicle_t *v = get_vehicle(jobs[i].assigned_vehicle);
            if (v && v->active_jobs > 0) v->active_jobs--;

            char final_json[512];
            snprintf(final_json, sizeof(final_json),
                "{\"job_id\":\"%s\",\"status\":\"DONE\",\"result\":%d,\"latency\":%.4f}",
                job_id, result_val, elapsed);

            char client_topic[128];
            snprintf(client_topic, sizeof(client_topic), "vc/client/%s/job/result", target_client);
            midd4vc_publish(c, client_topic, final_json);
            
            printf("[PERF] Job %s DONE in %.4fs (Node: %s)\n", job_id, elapsed, jobs[i].assigned_vehicle);
            return;
        }
    }
}

static void on_config_policy(midd4vc_client_t *c, const char *topic, const char *payload) {
    if (strstr(payload, "RR")) current_policy = strategy_round_robin;
    else if (strstr(payload, "LOAD")) current_policy = strategy_least_loaded;
    else if (strstr(payload, "PROXIMITY")) current_policy = strategy_proximity;
    printf("[SERVER] Policy Updated\n");
}

static void on_vehicle_status_change(midd4vc_client_t *c, const char *topic, const char *payload) {
    char v_id[64];
    sscanf(topic, "vc/vehicle/%63[^/]/status", v_id);

    vehicle_t *v = get_vehicle(v_id);
    if (v && strstr(payload, "offline_lwt")) {
        v->is_active = 0;
        printf("[SERVER] LWT DETECTADO: Veículo %s desconectou abruptamente!\n", v_id);
    } else if (v && strstr(payload, "online")) {
        v->is_active = 1;
        printf("[SERVER] Veículo %s está Online via LWT\n", v_id);
    }
}

static void maintenance_loop(midd4vc_client_t *c) {
    time_t now = time(NULL);
    
    for (int i = 0; i < MAX_JOBS; i++) {
        job_ctx_t *j = &jobs[i];
        if (j->in_use && !j->completed && j->assigned && (now - j->sent_at >= JOB_TIMEOUT)) {
            vehicle_t *v = get_vehicle(j->assigned_vehicle);
            if (v && v->active_jobs > 0) v->active_jobs--;
            
            if (j->retries < MAX_RETRIES) {
                j->retries++; 
                j->assigned = 0;
                printf("[SERVER] Retry %d for Job %s\n", j->retries, j->job_id);
                assign_job(c, j);
            } else {
                j->completed = 1;
                printf("[SERVER] Job %s FAILED\n", j->job_id);
            }
        }
    }
}

/*
static void maintenance_loop(midd4vc_client_t *c) {
    time_t now = time(NULL);
    for (int i = 0; i < vehicle_count; i++) {
        printf("DEBUG: Node %s last seen %ld seconds ago\n", vehicles[i].vehicle_id, now - vehicles[i].last_seen);
        if (vehicles[i].is_active && (now - vehicles[i].last_seen > VEHICLE_TIMEOUT)) {
            vehicles[i].is_active = 0;
            printf("[SERVER] Vehicle %s OFFLINE (Mobility)\n", vehicles[i].vehicle_id);
        }
    }
    for (int i = 0; i < MAX_JOBS; i++) {
        job_ctx_t *j = &jobs[i];
        if (j->in_use && !j->completed && j->assigned && (now - j->sent_at >= JOB_TIMEOUT)) {
            vehicle_t *v = get_vehicle(j->assigned_vehicle);
            if (v && v->active_jobs > 0) v->active_jobs--;
            
            if (j->retries < MAX_RETRIES) {
                j->retries++; j->assigned = 0;
                printf("[SERVER] Retry %d for Job %s\n", j->retries, j->job_id);
                assign_job(c, j);
            } else {
                j->completed = 1;
                printf("[SERVER] Job %s FAILED\n", j->job_id);
            }
        }
    }
}
*/

int main(void) {
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
        sleep(1);
    }
}
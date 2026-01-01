#include "midd4vc_client.h"
#include "midd4vc_protocol.h"
#include "midd4vc_job_codec.h"
#include "../platform/posix/mqtt_adapter.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward declaration */
static void handle_job_result_raw(midd4vc_client_t *c, const char *payload);

#define MAX_SUBSCRIPTIONS 100

typedef struct {
    char topic[128];
    midd4vc_sub_cb_t cb;
} midd4vc_subscription_t;

struct midd4vc_client {
    char client_id[64];
    midd4vc_role_t role;
    midd4vc_state_t state;

    midd4vc_job_cb_t job_cb;
    midd4vc_event_cb_t event_cb;
    midd4vc_job_result_cb_t job_result_cb;

    midd4vc_subscription_t subs[MAX_SUBSCRIPTIONS];
    int sub_count;
};

/* ---------- util ---------- */

static int mqtt_topic_match(const char *sub, const char *topic) {
    if (!sub || !topic) return 0;
    while (*sub && *topic) {
        if (*sub == '#') return 1;
        if (*sub == '+') {
            while (*topic && *topic != '/') topic++;
            sub++;
            if (*sub == '/') { sub++; if(*topic == '/') topic++; }
            continue;
        }
        if (*sub != *topic) return 0;
        sub++; topic++;
    }
    return (*sub == '\0' && *topic == '\0');
}

/* ---------- router ---------- */

static void mqtt_message_router(void *userdata, const char *topic, const char *payload) {
    midd4vc_client_t *c = userdata;
    if (!c || c->state != MIDD4VC_RUNNING) return;

    /* 1. Subscrições customizadas (callbacks diretos) */
    for (int i = 0; i < c->sub_count; i++) {
        if (mqtt_topic_match(c->subs[i].topic, topic)) {
            c->subs[i].cb(c, topic, payload);
        }
    }

    /* 2. Atribuição de Jobs (Papel de Veículo/RSU) */
    if (strstr(topic, "/job/assign")) {
        midd4vc_job_t job;
        if (midd4vc_parse_job(payload, &job)) {
            if (c->job_cb) c->job_cb(c, &job);
        } else {
            printf("[Midd4VC] Erro: Falha ao processar JSON do job atribuído\n");
        }
        return;
    }

    /* 3. Resultado de Jobs (Papel de Cliente) */
    if (strstr(topic, "/job/result")) {
        handle_job_result_raw(c, payload);
        return;
    }

    /* 4. Eventos do Sistema */
    if (strstr(topic, "vc/event/")) {
        if (c->event_cb) c->event_cb(c, topic, payload);
        return;
    }
}

/* ---------- API de Criação e Configuração ---------- */

midd4vc_client_t *midd4vc_create(const char *client_id, midd4vc_role_t role) {
    midd4vc_client_t *c = calloc(1, sizeof(*c));
    if (!c) return NULL;
    strncpy(c->client_id, client_id, sizeof(c->client_id) - 1);
    c->role = role;
    c->state = MIDD4VC_CREATED;
    return c;
}

void midd4vc_set_job_handler(midd4vc_client_t *c, midd4vc_job_cb_t cb) {
    if (c) c->job_cb = cb;
}

void midd4vc_set_event_handler(midd4vc_client_t *c, midd4vc_event_cb_t cb) {
    if (c) c->event_cb = cb;
}

void midd4vc_set_job_result_handler(midd4vc_client_t *c, midd4vc_job_result_cb_t cb) {
    if (c) c->job_result_cb = cb;
}

/* ---------- Ciclo de Vida ---------- */

void midd4vc_start(midd4vc_client_t *c) {
    if (!c) return;

    mqtt_init(c->client_id);
    mqtt_connect("localhost", 1883);

    char topic[128];

    // Se for um nó de processamento (Veículo), ouve o que o servidor envia
    if (c->role == ROLE_VEHICLE || c->role == ROLE_RSU) {
        snprintf(topic, sizeof(topic), TOPIC_JOB_ASSIGN, c->client_id);
        mqtt_subscribe(topic, mqtt_message_router, c);
    }

    // SE FOR UM CLIENTE: Ouve os resultados vindos da nuvem (CORREÇÃO CRÍTICA)
    if (c->role == ROLE_CLIENT) {
        snprintf(topic, sizeof(topic), TOPIC_JOB_RESULT, c->client_id);
        mqtt_subscribe(topic, mqtt_message_router, c);
    }

    // Todos ouvem eventos globais
    mqtt_subscribe("vc/event/#", mqtt_message_router, c);

    c->state = MIDD4VC_RUNNING;
    printf("[Midd4VC] Cliente '%s' iniciado (Role: %d)\n", c->client_id, c->role);
}

void midd4vc_stop(midd4vc_client_t *c) {
    if (c) {
        mqtt_disconnect();
        c->state = MIDD4VC_STOPPED;
    }
}

/* ---------- Operações de Mensagens ---------- */

void midd4vc_subscribe(midd4vc_client_t *c, const char *topic, midd4vc_sub_cb_t cb) {
    if (!c || c->sub_count >= MAX_SUBSCRIPTIONS) return;
    
    strncpy(c->subs[c->sub_count].topic, topic, sizeof(c->subs[c->sub_count].topic) - 1);
    c->subs[c->sub_count].cb = cb;
    c->sub_count++;

    mqtt_subscribe(topic, mqtt_message_router, c);
}

void midd4vc_publish(midd4vc_client_t *c, const char *topic, const char *payload) {
    if (c && c->state == MIDD4VC_RUNNING) {
        mqtt_publish(topic, payload);
    }
}

void midd4vc_submit_job(midd4vc_client_t *c, const char *job_id, const char *service, 
                        const char *function, double lat, double lon, const int *args, int argc) {
    if (!c || c->state != MIDD4VC_RUNNING) return;

    char payload[512];
    char args_buf[256] = "[";
    
    for (int i = 0; i < argc; i++) {
        char tmp[16];
        snprintf(tmp, sizeof(tmp), "%d%s", args[i], (i < argc - 1) ? "," : "");
        strcat(args_buf, tmp);
    }
    strcat(args_buf, "]");

    snprintf(payload, sizeof(payload),
        "{\"job_id\":\"%s\",\"service\":\"%s\",\"function\":\"%s\",\"args\":%s,\"client_id\":\"%s\",\"lat\":%.6f,\"lon\":%.6f}",
        job_id, service, function, args_buf, c->client_id, lat, lon);

    char topic[128];
    snprintf(topic, sizeof(topic), TOPIC_JOB_SUBMIT, c->client_id);
    
    mqtt_publish(topic, payload);
    printf("[Midd4VC] Job %s submetido com posição (%.4f, %.4f)\n", job_id, lat, lon);
}

void midd4vc_register(midd4vc_client_t *c, const char *json_payload) {
    if (!c || c->state != MIDD4VC_RUNNING) return;

    char topic[128];
    if (c->role == ROLE_VEHICLE) {
        snprintf(topic, sizeof(topic), TOPIC_VEHICLE_REGISTER, c->client_id);
    } else if (c->role == ROLE_RSU) {
        snprintf(topic, sizeof(topic), TOPIC_RSU_REGISTER, c->client_id);
    } else return;

    mqtt_publish(topic, json_payload);
}

/* ---------- Tratamento de Resultados ---------- */

static void handle_job_result_raw(midd4vc_client_t *c, const char *payload) {
    if (!c || !payload || !c->job_result_cb) return;

    char job_id[64] = {0};
    char status_str[16] = {0};
    int result = 0;

    // Parser robusto via busca de chaves
    char *p;
    if ((p = strstr(payload, "\"job_id\":\""))) sscanf(p, "\"job_id\":\"%63[^\"]\"", job_id);
    if ((p = strstr(payload, "\"status\":\""))) sscanf(p, "\"status\":\"%15[^\"]\"", status_str);
    if ((p = strstr(payload, "\"result\":")))  sscanf(p, "\"result\":%d", &result);

    midd4vc_job_status_t status = (strcmp(status_str, "DONE") == 0) ? JOB_DONE : JOB_ERROR;

    // Chama o callback da aplicação (client_app)
    c->job_result_cb(job_id, status, result);
}

void midd4vc_send_job_success(midd4vc_client_t *c, const char *client_id, const char *job_id, int result) {
    char payload[512];
    snprintf(payload, sizeof(payload),
        "{\"job_id\":\"%s\",\"client_id\":\"%s\",\"vehicle_id\":\"%s\",\"status\":\"DONE\",\"result\":%d}",
        job_id, client_id, c->client_id, result);

    char topic[128];
    snprintf(topic, sizeof(topic), TOPIC_SERVER_JOB_RESULT);
    mqtt_publish(topic, payload);
}

const char *midd4vc_get_id(midd4vc_client_t *c) {
    return c ? c->client_id : NULL;
}
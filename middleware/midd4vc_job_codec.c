#include "midd4vc_job_codec.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Helper para extrair valores de strings JSON de forma flexível */
static int get_json_string(const char *json, const char *key, char *dest, size_t dest_sz) {
    char search_key[128];
    snprintf(search_key, sizeof(search_key), "\"%s\":\"", key);
    char *start = strstr(json, search_key);
    if (!start) return 0;
    
    start += strlen(search_key);
    char *end = strchr(start, '\"');
    if (!end) return 0;

    size_t len = end - start;
    if (len >= dest_sz) len = dest_sz - 1;
    
    strncpy(dest, start, len);
    dest[len] = '\0';
    return 1;
}

/* Helper para extrair o array de argumentos [arg1, arg2] */
static int get_json_args(const char *json, int *arg1, int *arg2) {
    char *start = strstr(json, "\"args\":[");
    if (!start) return 0;
    start += 8; // Pula "args":[
    
    if (sscanf(start, "%d,%d", arg1, arg2) == 2) return 1;
    return 0;
}

int midd4vc_parse_job(const char *json, midd4vc_job_t *job) {
    if (!json || !job) return 0;

    // Inicializa a estrutura para evitar lixo
    memset(job, 0, sizeof(midd4vc_job_t));

    int success = 1;

    // Busca os campos de forma independente da ordem no JSON
    success &= get_json_string(json, "job_id", job->job_id, sizeof(job->job_id));
    success &= get_json_string(json, "service", job->service, sizeof(job->service));
    success &= get_json_string(json, "function", job->function, sizeof(job->function));
    success &= get_json_string(json, "client_id", job->client_id, sizeof(job->client_id));
    
    int a, b;
    if (get_json_args(json, &a, &b)) {
        job->args[0] = a;
        job->args[1] = b;
        job->argc = 2;
    } else {
        success = 0;
    }

    return success;
}
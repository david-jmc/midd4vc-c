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
static int get_json_args(const char *json, int *args, size_t *argc) {
    char *start = strstr(json, "\"args\":[");
    if (!start) return 0;
    
    start += 8; // Pula "args":[
    *argc = 0;
    char *current = start;

    // Loop que extrai números até encontrar o fim do array ']' ou atingir o limite do protocolo (16)
    while (*current != ']' && *current != '\0' && *argc < 16) {
        // Pula caracteres que não são números (vírgulas, espaços, etc)
        while (*current && (*current < '0' || *current > '9') && *current != '-' && *current != ']') {
            current++;
        }
        
        if (*current == ']' || *current == '\0') break;

        // Converte e armazena
        args[*argc] = atoi(current);
        (*argc)++;

        // Avança o ponteiro para o próximo separador
        while (*current && *current != ',' && *current != ']') {
            current++;
        }
    }
    return (*argc > 0);
}

int midd4vc_parse_job(const char *json, midd4vc_job_t *job) {
    if (!json || !job) return 0;

    memset(job, 0, sizeof(midd4vc_job_t));

    // Campos de texto (Independentes e robustos)
    get_json_string(json, "job_id", job->job_id, sizeof(job->job_id));
    get_json_string(json, "service", job->service, sizeof(job->service));
    get_json_string(json, "function", job->function, sizeof(job->function));
    get_json_string(json, "client_id", job->client_id, sizeof(job->client_id));
    
    // CHAMADA CORRIGIDA: Usa o array e o contador da struct
    if (get_json_args(json, job->args, &job->argc)) {
        return 1; // Sucesso genérico (pode ter 1 ou 10 argumentos)
    }

    return 0;
}
#include "midd4vc_job_codec.h"
#include "midd4vc_protocol.h"
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

static int get_json_double(const char *json, const char *key, double *dest) {
    char search_key[128];
    snprintf(search_key, sizeof(search_key), "\"%s\":", key);
    char *start = strstr(json, search_key);
    if (!start) return 0;
    
    start += strlen(search_key);
    *dest = atof(start);
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

/*
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
*/

int midd4vc_parse_job(const char *json, midd4vc_job_t *job) {
    if (!json || !job) return 0;

    memset(job, 0, sizeof(midd4vc_job_t));

    job->lat = GPS_INVALID;
    job->lon = GPS_INVALID;

    // 1. Extração de Strings (ID, Serviço, Função, Cliente, Status)
    // Note o uso das macros do seu protocol.h para garantir que o nome da chave mude no .h e reflita aqui
    get_json_string(json, FIELD_JOB_ID,    job->job_id,    sizeof(job->job_id));
    get_json_string(json, FIELD_SERVICE,   job->service,   sizeof(job->service));
    get_json_string(json, FIELD_FUNCTION,  job->function,  sizeof(job->function));
    get_json_string(json, FIELD_CLIENT_ID, job->client_id, sizeof(job->client_id));
    get_json_string(json, FIELD_STATUS,    job->status,    sizeof(job->status));

    // 2. Extração do "result" numérico (específico para respostas)
    char *res_ptr = strstr(json, "\"" FIELD_RESULT "\":");
    if (res_ptr) {
        // Pula a chave e o ":" para pegar o valor
        res_ptr = strchr(res_ptr, ':');
        if (res_ptr) job->result = atoi(res_ptr + 1);
    }

    // 3. Extração do array de argumentos (específico para submissão/assign)
    // Se não houver args, get_json_args apenas retorna 0, mas o parse continua válido
    get_json_args(json, job->args, &job->argc);

    get_json_double(json, "lat", &job->lat);
    get_json_double(json, "lon", &job->lon);

    // O parse é considerado um sucesso se pelo menos tivermos um job_id
    return (strlen(job->job_id) > 0);
}

/*
int midd4vc_encode_job(char *dest, size_t dest_sz, const char *job_id, const char *service, 
                       const char *function, const char *client_id, 
                       double lat, double lon, const int *args, int argc) {
    
    char args_buf[256] = "[";
    for (int i = 0; i < argc; i++) {
        char tmp[16];
        // Garante que não estoure o buffer do array de args
        snprintf(tmp, sizeof(tmp), "%d%s", args[i], (i < argc - 1) ? "," : "");
        strcat(args_buf, tmp);
    }
    strcat(args_buf, "]");

    return snprintf(dest, dest_sz,
        "{\"job_id\":\"%s\",\"service\":\"%s\",\"function\":\"%s\",\"args\":%s,\"client_id\":\"%s\",\"lat\":%.6f,\"lon\":%.6f}",
        job_id, service, function, args_buf, client_id, lat, lon);
}

*/

int midd4vc_encode_job(char *dest, size_t dest_sz, const char *job_id, const char *service, 
                       const char *function, const char *client_id, 
                       double lat, double lon, const int *args, int argc) {
    
    // 1. Montagem do array de argumentos [1, 2, 3]
    char args_buf[256] = "[";
    for (int i = 0; i < argc; i++) {
        char tmp[16];
        snprintf(tmp, sizeof(tmp), "%d%s", args[i], (i < argc - 1) ? "," : "");
        strcat(args_buf, tmp);
    }
    strcat(args_buf, "]");

    // 2. Montagem do JSON usando as macros do FIELD_... definidas no protocol.h
    // Isso evita strings "hardcoded" e garante conformidade total com o servidor
    return snprintf(dest, dest_sz,
        "{\"" FIELD_JOB_ID "\":\"%s\","
        "\"" FIELD_SERVICE "\":\"%s\","
        "\"" FIELD_FUNCTION "\":\"%s\","
        "\"" FIELD_ARGS "\":%s,"
        "\"" FIELD_CLIENT_ID "\":\"%s\","
        "\"lat\":%.6f,\"lon\":%.6f}",
        job_id, service, function, args_buf, client_id, lat, lon);
}


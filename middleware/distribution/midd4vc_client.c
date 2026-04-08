#include "midd4vc_client.h"
#include "midd4vc_protocol.h"
#include "midd4vc_job_codec.h"
#include "../infrastructure/mqtt_adapter.h"
#include "../specifics/job_catalog.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward declaration */
static void handle_job_result_raw(midd4vc_client_t *c, const char *payload);

#define MAX_SUBSCRIPTIONS 1000

typedef struct {
    char topic[128];
    midd4vc_sub_cb_t cb;
} midd4vc_subscription_t;

struct midd4vc_client {
    char client_id[64];
    midd4vc_role_t role;
    midd4vc_state_t state;

    double lat;
    double lon;
    int battery_level;

    midd4vc_job_cb_t job_cb;
    midd4vc_event_cb_t event_cb;
    midd4vc_job_result_cb_t job_result_cb;

    midd4vc_subscription_t subs[MAX_SUBSCRIPTIONS];
    int sub_count;

    // Data management
    midd4vc_interceptor_fn services[10];
    int services_count;
};

/* Encapsulamento de Getters/Setters para a aplicação */
void midd4vc_set_position(midd4vc_client_t *c, double lat, double lon) {
    if (c) { c->lat = lat; c->lon = lon; }
}

double midd4vc_get_lat(midd4vc_client_t *c) { return c ? c->lat : 0.0; }
double midd4vc_get_lon(midd4vc_client_t *c) { return c ? c->lon : 0.0; }

/* Refatoração do Registro Automático */
void midd4vc_register_auto(midd4vc_client_t *c) {
    if (!c || c->state != MIDD4VC_RUNNING) return;

    char payload[128];
    // Aqui o aluno de mestrado poderá atuar no futuro, 
    // alterando como esse JSON é gerado (Data Management)
    snprintf(payload, sizeof(payload), "{\"latitude\":%.6f,\"longitude\":%.6f}", c->lat, c->lon);
    
    midd4vc_register(c, payload);
}

/* Data Management */
void midd4vc_add_service(midd4vc_client_t *c, midd4vc_interceptor_fn fn) {
    if (c && c->services_count < 10) {
        c->services[c->services_count++] = fn;
    }
}

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

    char *working_payload = strdup(payload);

    // Executa o Pipeline de Data Management (Interceptores)
    for (int i = 0; i < c->services_count; i++) {
        if (c->services[i](c, topic, &working_payload) == MIDD4VC_DROP) {
            // Se o serviço retornar DROP, a mensagem é descartada (ex: filtragem ou consenso pendente)
            free(working_payload);
            return; 
        }
    }

    /* 1. Verifying custom subscritions via API (direct callbacks) */
    for (int i = 0; i < c->sub_count; i++) {
        if (mqtt_topic_match(c->subs[i].topic, topic)) {
            c->subs[i].cb(c, topic, working_payload);
        }
    }

    /* 2. Tasks Atribution (Vehicle/RSU) */
    if (strstr(topic, "/job/assign")) {
        midd4vc_job_t job;
        if (midd4vc_parse_job(working_payload, &job)) {
            if (c->job_cb) c->job_cb(c, &job);
        } else {
            printf("[Midd4VC] Erro: Failed to parser the Task\n");
        }
        free(working_payload);
        return;
    }

    /* 3. Tasks Results (Clients) */
    if (strstr(topic, "/job/result")) {
        handle_job_result_raw(c, working_payload);
        free(working_payload);
        return;
    }

    /* 4. Events */
    if (strstr(topic, "vc/event/")) {
        if (c->event_cb) c->event_cb(c, topic, working_payload);
        free(working_payload);
        return;
    }
    free(working_payload);
}

/* ---------- API de Criação e Configuração ---------- */

midd4vc_client_t *midd4vc_create(const char *client_id, midd4vc_role_t role) {
    midd4vc_client_t *c = calloc(1, sizeof(*c));
    if (!c) return NULL;

    if (client_id == NULL || strlen(client_id) == 0) {
        snprintf(c->client_id, sizeof(c->client_id), "node_%04X", rand() % 0xFFFF); 
    } else {        
        strncpy(c->client_id, client_id, sizeof(c->client_id) - 1);
    }
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

    char lwt_topic[128] = {0};
    char *lwt_payload = "{\"status\":\"offline_lwt\"}";
    
    mqtt_init(c->client_id);

    // Se for um nó de infraestrutura/veículo, configuramos o LWT
    if (c->role == ROLE_VEHICLE || c->role == ROLE_RSU) {
        // Tópico onde o servidor monitora a vida do nó
        snprintf(lwt_topic, sizeof(lwt_topic), "vc/vehicle/%s/status", c->client_id);
        
        // Conecta passando as informações do Testamento
        // Assumindo que seu mqtt_adapter agora aceita LWT
        mqtt_connect("localhost", 1883, lwt_topic, lwt_payload);
        
        // Inscrição para Jobs
        char job_topic[128];
        snprintf(job_topic, sizeof(job_topic), TOPIC_JOB_ASSIGN, c->client_id);
        mqtt_subscribe(job_topic, mqtt_message_router, c);
        
        // Publica que está ONLINE imediatamente após conectar
        midd4vc_publish(c, lwt_topic, "{\"status\":\"online\"}");
        
        // Registro formal de metadados
        midd4vc_register(c, "{\"status\":\"active\"}");
    } else {
        // Clientes e Dashboard conectam sem LWT (ou LWT genérico)
        mqtt_connect("localhost", 1883, NULL, NULL);
    }

    // Configurações específicas de Cliente
    if (c->role == ROLE_CLIENT) {
        char res_topic[128];
        snprintf(res_topic, sizeof(res_topic), TOPIC_JOB_RESULT, c->client_id);
        mqtt_subscribe(res_topic, mqtt_message_router, c);
    }

    // Todos ouvem eventos globais
    mqtt_subscribe("vc/event/#", mqtt_message_router, c);

    c->state = MIDD4VC_RUNNING;
    printf("[Midd4VC] Cliente '%s' iniciado (Role: %d) com LWT ativado\n", c->client_id, c->role);
}

void midd4vc_stop(midd4vc_client_t *c) {
    if (c && c->state == MIDD4VC_RUNNING) {
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

    /*
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
    
    */

    midd4vc_encode_job(payload, sizeof(payload), job_id, service, function, 
                       c->client_id, lat, lon, args, argc);

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

static void handle_job_result_raw(midd4vc_client_t *c, const char *payload) {
    if (!c || !payload || !c->job_result_cb) return;

    midd4vc_job_t job;
    if (midd4vc_parse_job(payload, &job)) {
        // Converte a string de status do codec para o enum do handler
        midd4vc_job_status_t status = (strcmp(job.status, JOB_STATUS_DONE) == 0) ? JOB_DONE : JOB_ERROR;
        
        // Dispara o callback com os dados prontos
        c->job_result_cb(job.job_id, status, job.result);
    }
}

/* ---------- Tratamento de Resultados ---------- */

/*
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
*/

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

/* ---------- Execução Dinâmica de Jobs (Cloud-Native Logic) ---------- */

int midd4vc_execute_job_internal(midd4vc_client_t *c, const midd4vc_job_t *job) {
    if (!c || !job) return -1;

    // 1. O Middleware consulta o catálogo interno
    // Isso resolve o problema de o vehicle.c precisar do .h
    job_fn_t worker = job_catalog_lookup(job->service, job->function);

    if (worker) {
        // Sucesso: Função encontrada (estática ou .so carregado)
        return worker(job->args, job->argc);
    }

    // 2. FALHA (Cache Miss): O código não existe no veículo
    printf("[Midd4VC] SERVIÇO NÃO ENCONTRADO: %s.%s. Iniciando busca na nuvem...\n", 
           job->service, job->function);

    // 3. Solicitação via MQTT para o Servidor (Dynamic Pull)
    char pull_topic[128];
    char pull_payload[256];
    
    // Tópico para o servidor saber quem está pedindo o que
    snprintf(pull_topic, sizeof(pull_topic), "vc/vehicle/%s/service/request", c->client_id);
    snprintf(pull_payload, sizeof(pull_payload), 
             "{\"service\":\"%s\",\"function\":\"%s\",\"node_id\":\"%s\"}", 
             job->service, job->function, c->client_id);
    
    midd4vc_publish(c, pull_topic, pull_payload);

    // Retornamos um código especial (ex: -404) para o vehicle.c saber que deve aguardar
    return -404; 
}
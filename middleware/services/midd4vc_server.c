#define _POSIX_C_SOURCE 199309L
#define _XOPEN_SOURCE 500
#include "../distribution/midd4vc_client.h"
#include "../distribution/midd4vc_protocol.h"
#include "midd4vc_scheduler.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#define MAX_VEHICLES 1000
#define MAX_JOBS     1024
#define JOB_TIMEOUT  2   
#define MAX_RETRIES  3
#define VEHICLE_TIMEOUT 10  
#define MAX_LOAD 10          

static vehicle_t vehicles[MAX_VEHICLES];
static int vehicle_count = 0;
static job_ctx_t jobs[MAX_JOBS];

static scheduler_ctx_t sched_ctx = {
    .rr_idx = 0,
    .prox_rr_idx = 0,
    .max_load = MAX_LOAD,
    .proximity_threshold = 500
};

static balancing_strategy_fn current_policy = strategy_hybrid_pro;

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
    if (!current_policy) current_policy = strategy_hybrid_pro;
    
    vehicle_t *v = current_policy(vehicles, vehicle_count,j->req_lat, j->req_lon, &sched_ctx);
    
    if (!v) {
        printf("[Midd4VCServer] Cloud Error: No nodes available for task %s\n", j->job_id);
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
    
    printf("[Midd4VCServer] Job %s -> %s (Load: %d)\n", j->job_id, v->vehicle_id, v->active_jobs);
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
        printf("[Midd4VCServer] Node %s update at (%.4f, %.4f)\n", vehicle_id, lat, lon);
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
        printf("[Midd4VCServer] Erro: Payload de Job inválido.\n");
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
            
            vehicle_t *v = get_vehicle(jobs[i].assigned_vehicle);
            if (v && v->active_jobs > 0) v->active_jobs--;

            char final_json[512];
            snprintf(final_json, sizeof(final_json),
                "{\"job_id\":\"%s\",\"status\":\"DONE\",\"result\":%d}",
                job_id, result_val);

            char client_topic[128];
            snprintf(client_topic, sizeof(client_topic), "vc/client/%s/job/result", target_client);
            midd4vc_publish(c, client_topic, final_json);
            
            printf("[Midd4VCServer] Job %s DONE (Node: %s)\n", job_id, jobs[i].assigned_vehicle);
            return;
        }
    }
}

static void on_config_policy(midd4vc_client_t *c, const char *topic, const char *payload) {
    if (strstr(payload, "RR")) current_policy = strategy_round_robin;
    else if (strstr(payload, "LOAD")) current_policy = strategy_least_loaded;
    else if (strstr(payload, "PROXIMITY")) current_policy = strategy_proximity_rr; // Ajustado
    else if (strstr(payload, "HYBRID")) current_policy = strategy_hybrid_pro;    // Adicionado
    printf("[Midd4VCServer] Policy Updated\n");
}

static void on_vehicle_status_change(midd4vc_client_t *c, const char *topic, const char *payload) {
    char v_id[64];
    sscanf(topic, "vc/vehicle/%63[^/]/status", v_id);

    vehicle_t *v = get_vehicle(v_id);

    if (v && strstr(payload, "offline_lwt")) {
        v->is_active = 0;
        v->active_jobs = 0; // new
        v->last_seen = time(NULL);
        printf("[Midd4VCServer] LWT DETECTADO: Veículo %s desconectou abruptamente!\n", v_id);
    } else if (v && strstr(payload, "online")) {
        v->is_active = 1;
        v->last_seen = time(NULL);
        printf("[Midd4VCServer] Veículo %s está Online via LWT\n", v_id);
    }
}

static void maintenance_loop(midd4vc_client_t *c) {
    time_t now = time(NULL);

    // 1. Monitoramento de Inatividade dos Veículos (Purge)
    for (int i = 0; i < vehicle_count; i++) {
        if (vehicles[i].is_active) {
            if ((long)(now - vehicles[i].last_seen) > VEHICLE_TIMEOUT) {
                vehicles[i].is_active = 0;
                vehicles[i].active_jobs = 0; 
                printf("[Midd4VCServer] Vehicle %s OFFLINE (Timeout)\n", vehicles[i].vehicle_id);
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
                printf("[Midd4VCServer] Retry %d for Task %s\n", j->retries, j->job_id);
                assign_job(c, j);
            } else {
                j->completed = 1;
                j->in_use = 0;
                printf("[Midd4VCServer] Task %s FAILED\n", j->job_id);
            }
        }
    }
}

int main(void) {
    srand(time(NULL));
    midd4vc_client_t *srv = midd4vc_create("Midd4VC Server", ROLE_DASHBOARD);
    midd4vc_start(srv);

    midd4vc_subscribe(srv, TOPIC_FILTER_VEHICLE_REGISTER, on_register);
    midd4vc_subscribe(srv, TOPIC_FILTER_JOB_SUBMIT, on_job_submit);
    midd4vc_subscribe(srv, TOPIC_SERVER_JOB_RESULT, on_job_result);
    midd4vc_subscribe(srv, TOPIC_CONFIG_POLICY, on_config_policy);
    midd4vc_subscribe(srv, TOPIC_FILTER_VEHICLE_STATUS, on_vehicle_status_change);

    printf("[Midd4VCServer] Cloud Orchestrator Ready\n");

    while (1) {
        maintenance_loop(srv);
        //sleep(1);
        usleep(100000);
    }
}
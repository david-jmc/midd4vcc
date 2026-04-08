#include "midd4vc_scheduler.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

#define MAX_LOAD 5  

// Converte graus para radianos
static double to_rad(double degree) {
    return degree * (M_PI / 180.0);
}

static double haversine(double lat1, double lon1, double lat2, double lon2) {
    double dLat = to_rad(lat2 - lat1);
    double dLon = to_rad(lon2 - lon1);

    double a = sin(dLat / 2) * sin(dLat / 2) +
               cos(to_rad(lat1)) * cos(to_rad(lat2)) *
               sin(dLon / 2) * sin(dLon / 2);
    
    double c = 2 * atan2(sqrt(a), sqrt(1 - a));
    
    return 6371000.0 * c; // Retorna a distância em METROS (raio da Terra = 6371km)
}

#define MAX_REACH_DISTANCE 500.0 // Raio de 500 metros para considerar "nuvem local"

vehicle_t* strategy_proximity_rr(vehicle_t* vehicles, int vehicle_count, double lat, double lon, scheduler_ctx_t* ctx) {
    if (vehicle_count <= 0 || !ctx || !vehicles) return NULL;

    // 1. Tenta primeiro o Round Robin apenas entre quem está dentro do threshold
    int start_idx = ctx->prox_rr_idx % vehicle_count;
    
    for (int i = 0; i < vehicle_count; i++) {
        int idx = (start_idx + i) % vehicle_count;
        
        if (vehicles[idx].is_active && vehicles[idx].active_jobs < ctx->max_load) {
            double dist = haversine(lat, lon, vehicles[idx].latitude, vehicles[idx].longitude);
            
            if (dist <= ctx->proximity_threshold) {
                ctx->prox_rr_idx = (idx + 1) % vehicle_count;
                return &vehicles[idx];
            }
        }
    }

    // 2. GLOBAL FALLBACK: Se ninguém no raio está disponível, faz o RR nos demais
    // Isso torna a estratégia "Glocal" (Global + Local)
    for (int i = 0; i < vehicle_count; i++) {
        int idx = (start_idx + i) % vehicle_count;
        
        if (vehicles[idx].is_active && vehicles[idx].active_jobs < ctx->max_load) {
            // Se chegou aqui, é porque ninguém no raio serviu, então pegamos o primeiro disponível
            ctx->prox_rr_idx = (idx + 1) % vehicle_count;
            return &vehicles[idx];
        }
    }

    return NULL; // Só se o sistema estiver 100% lotado ou offline
}

vehicle_t* strategy_round_robin(vehicle_t* vehicles, int vehicle_count, double lat, double lon, scheduler_ctx_t* ctx) {
    if (vehicle_count <= 0) return NULL;
    if (!ctx) return NULL;
    if (!vehicles) return NULL;

    for (int i = 0; i < vehicle_count; i++) {
        int idx = (ctx->rr_idx + i) % vehicle_count;
        if (vehicles[idx].is_active && vehicles[idx].active_jobs < MAX_LOAD) {
            ctx->rr_idx = (idx + 1) % vehicle_count;
            return &vehicles[idx];
        } 
    }
    return NULL;
}

vehicle_t* strategy_least_loaded(vehicle_t* vehicles, int vehicle_count, double lat, double lon, scheduler_ctx_t* ctx) {
    if (vehicle_count <= 0) return NULL;

    int min_load = ctx->max_load + 1;
    //int candidates[vehicle_count];
    //int candidate_count = 0;

    // 1. Encontrar a carga mínima entre os veículos ativos
    for (int i = 0; i < vehicle_count; i++) {
        if (!vehicles[i].is_active || vehicles[i].active_jobs >= ctx->max_load) continue;
        if (vehicles[i].active_jobs < min_load) {
            min_load = vehicles[i].active_jobs;
        }
    }

    // Se nenhum veículo for encontrado abaixo do MAX_LOAD
    if (min_load > ctx->max_load) return NULL;

    vehicle_t* best = NULL;
    long min_history = -1;

    // 2. Criar a lista de todos os veículos que têm essa carga mínima (o Least Loaded Resources do Python)
    for (int i = 0; i < vehicle_count; i++) {
        if (vehicles[i].is_active && vehicles[i].active_jobs == min_load) {

            if (min_history == -1 || vehicles[i].total_processed < min_history) {
                min_history = vehicles[i].total_processed;
                best = &vehicles[i];
            }
        }
    }

    // 3. Escolha aleatória entre os candidatos (o random.choice do Python)
    //int chosen_idx = candidates[rand() % candidate_count];

    if (best != NULL) {
        best->total_processed++;
    }

    return best;

    //return &vehicles[chosen_idx];
}

vehicle_t* strategy_hybrid_pro(vehicle_t* vehicles, int vehicle_count, double lat, double lon, scheduler_ctx_t* ctx) {
    if (vehicle_count <= 0 || !vehicles || !ctx) return NULL;

    vehicle_t *best = NULL;
    int min_load = ctx->max_load + 1;
    double min_dist = 1e18;

    long min_history = -1;

    int has_gps = (lat != 0.0 || lon != 0.0);

    for (int i = 0; i < vehicle_count; i++) {
        if (!vehicles[i].is_active || vehicles[i].active_jobs >= ctx->max_load) continue;

        int current_load = vehicles[i].active_jobs;
        double current_dist = has_gps ? haversine(lat, lon, vehicles[i].latitude, vehicles[i].longitude) : 0.0;
        long current_history = vehicles[i].total_processed;

        // Decisão Camada 1: Menor Carga (Prioridade p/ Saúde do Sistema)
        if (current_load < min_load) {
            min_load = current_load;
            min_dist = current_dist;
            min_history = current_history;
            best = &vehicles[i];
        } 
        // Decisão Camada 2: Empate de Carga -> Desempate por Proximidade (Otimização de Latência)
        else if (current_load == min_load && has_gps) {
            if (current_dist < min_dist) {
                min_dist = current_dist;
                min_history = current_history;
                best = &vehicles[i];
            }
            else if (current_dist == min_dist) {
                if (min_history == -1 || current_history < min_history) {
                    min_history = current_history;
                    best = &vehicles[i];
                } 
            }
        }
    }

    if (best != NULL) {
        best->total_processed++;
    }
    
    return best;
}
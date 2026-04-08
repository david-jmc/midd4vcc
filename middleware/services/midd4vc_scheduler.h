#ifndef MIDD4VC_SCHEDULER_H
#define MIDD4VC_SCHEDULER_H

#include "../distribution/midd4vc_protocol.h"
#include "../distribution/midd4vc_client.h"

#include <time.h>
#include <math.h>

// scheduler context
typedef struct {
    int rr_idx;               // Último índice usado pelo Round Robin
    int prox_rr_idx;
    int max_load;             // Carga máxima permitida por veículo
    //double reach_distance;    // Raio máximo (metros) para Proximidade
    double proximity_threshold;
} scheduler_ctx_t;

typedef vehicle_t* (*balancing_strategy_fn)(vehicle_t* vehicles, int vehicle_count, double lat, double lon, scheduler_ctx_t* ctx);

double calculate_haversine(double lat1, double lon1, double lat2, double lon2);

//vehicle_t* strategy_round_robin(vehicle_t* vehicles, int vehicle_count, double req_lat, double req_lon, scheduler_ctx_t* ctx);
//vehicle_t* strategy_least_loaded(vehicle_t* vehicles, int vehicle_count, double req_lat, double req_lon, scheduler_ctx_t* ctx);
//vehicle_t* strategy_proximity(vehicle_t* vehicles, int vehicle_count, double req_lat, double req_lon, scheduler_ctx_t* ctx);

vehicle_t* strategy_round_robin(vehicle_t* vehicles, int vehicle_count, double req_lat, double req_lon, scheduler_ctx_t* ctx);
vehicle_t* strategy_least_loaded(vehicle_t* vehicles, int vehicle_count, double req_lat, double req_lon, scheduler_ctx_t* ctx);
vehicle_t* strategy_proximity_rr(vehicle_t* vehicles, int vehicle_count, double req_lat, double req_lon, scheduler_ctx_t* ctx);
vehicle_t* strategy_hybrid_pro(vehicle_t* vehicles, int vehicle_count, double req_lat, double req_lon, scheduler_ctx_t* ctx);

#endif
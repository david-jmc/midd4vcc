#ifndef MIDD4VC_CLIENT_H
#define MIDD4VC_CLIENT_H

#include "midd4vc_protocol.h"
#include "midd4vc_job_codec.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct midd4vc_client midd4vc_client_t;

/* Estados */
typedef enum {
    MIDD4VC_CREATED = 0,
    MIDD4VC_RUNNING,
    MIDD4VC_STOPPED
} midd4vc_state_t;

/* Roles */
typedef enum {
    ROLE_VEHICLE = 0,
    ROLE_RSU,
    ROLE_CLIENT,
    ROLE_DASHBOARD
} midd4vc_role_t;

/* Data Management */

typedef enum {
    MIDD4VC_CONTINUE, // Segue para o próximo interceptador ou para o core
    MIDD4VC_DROP      // Interrompe o fluxo (o serviço assumiu o controle)
} midd4vc_action_t;

/* Callbacks */
//typedef struct midd4vc_job midd4vc_job_t;
typedef void (*midd4vc_job_cb_t)(
    midd4vc_client_t *c,
    const midd4vc_job_t *payload
);

/* Callback SEM middleware */
typedef void (*midd4vc_job_result_cb_t)(
    const char *job_id,
    midd4vc_job_status_t status,
    int result
);

typedef void (*midd4vc_event_cb_t)(
    midd4vc_client_t *c,
    const char *topic,
    const char *payload
);

typedef void (*midd4vc_sub_cb_t)(
    midd4vc_client_t *c,
    const char *topic,
    const char *payload
);

typedef midd4vc_action_t (*midd4vc_interceptor_fn)(midd4vc_client_t *c, const char *topic, char **payload);

/* API */
midd4vc_client_t *midd4vc_create(
    const char *client_id,
    midd4vc_role_t role
);

void midd4vc_set_job_handler(
    midd4vc_client_t *c,
    midd4vc_job_cb_t cb
);

void midd4vc_set_event_handler(
    midd4vc_client_t *c,
    midd4vc_event_cb_t cb
);

void midd4vc_set_job_result_handler(
    midd4vc_client_t *c,
    midd4vc_job_result_cb_t cb);

void midd4vc_subscribe(
    midd4vc_client_t *c,
    const char *topic,
    midd4vc_sub_cb_t cb
);

void midd4vc_publish(
    midd4vc_client_t *c,
    const char *topic,
    const char *payload
);

void midd4vc_on_job_result(
    midd4vc_client_t *c,
    midd4vc_job_result_cb_t cb
);

void midd4vc_start(midd4vc_client_t *c);
void midd4vc_stop(midd4vc_client_t *c);


midd4vc_state_t midd4vc_get_state(
    midd4vc_client_t *c
);

const char *midd4vc_get_id(midd4vc_client_t *c);

void midd4vc_register(
    midd4vc_client_t *c,
    const char *json_payload
);

/* Job API */
void midd4vc_submit_job(
    midd4vc_client_t *c,
    const char *job_id,
    const char *service,
    const char *function,
    double lat,
    double lon,
    const int *args,
    int argc);

void midd4vc_send_job_result(
    midd4vc_client_t *c,
    const char *client_id,
    const char *result_json
);

void midd4vc_send_job_success(
    midd4vc_client_t *c,
    const char *client_id,
    const char *job_id,
    int result
);

void midd4vc_send_job_error(
    midd4vc_client_t *c,
    const char *client_id,
    const char *job_id,
    const char *code,
    const char *msg
);

void midd4vc_add_service(
    midd4vc_client_t *c, 
    midd4vc_interceptor_fn fn
);


#ifdef __cplusplus
}
#endif

#endif

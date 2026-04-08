#ifndef MIDD4VC_PROTOCOL_H
#define MIDD4VC_PROTOCOL_H

#include <stddef.h>
#include <time.h>

/* ---- Job model ---- */

typedef struct {
    char job_id[64];
    char service[64];
    char function[64];
    int args[16];
    size_t argc;
    char client_id[64];
    int result;
    char status[16];
    double lat;
    double lon;
} midd4vc_job_t;

/* ---- Job status ---- */

typedef enum {
    JOB_DONE = 0,
    JOB_ERROR
} midd4vc_job_status_t;

/* --- Estruturas --- */

typedef struct {
    char vehicle_id[64];
    int active_jobs;
    double latitude;
    double longitude;
    time_t last_seen;
    int is_active;
    long total_processed;
} vehicle_t;

typedef struct {
    char job_id[64];
    char client_id[64];
    char payload[512];
    int completed;
    int retries;
    struct timespec sent_at_spec;
    time_t sent_at;
    int assigned;
    char assigned_vehicle[64];
    double req_lat;
    double req_lon;
    int in_use;
} job_ctx_t;

/* Protocol version */
#define MIDD4VC_PROTOCOL_VERSION "1.0"

/* Roles */
#define ROLE_VEHICLE_STR   "vehicle"
#define ROLE_RSU_STR       "rsu"
#define ROLE_CLIENT_STR    "client"
#define ROLE_DASHBOARD_STR "dashboard"

/* Topics */
#define TOPIC_VEHICLE_REGISTER   "vc/vehicle/%s/register/request"
#define TOPIC_RSU_REGISTER       "vc/rsu/%s/register/request"

#define TOPIC_EVENT_PREFIX       "vc/event/"
#define TOPIC_HEARTBEAT          "vc/client/heartbeat"

/* Job topics */
#define TOPIC_JOB_SUBMIT   "vc/client/%s/job/submit"
#define TOPIC_JOB_ASSIGN   "vc/vehicle/%s/job/assign"
#define TOPIC_JOB_RESULT   "vc/client/%s/job/result"

/* Server Job topics */
#define TOPIC_SERVER_JOB_RESULT  "vc/server/job/result" 

/* Job status */
#define JOB_STATUS_DONE     "DONE"
#define JOB_STATUS_ERROR    "ERROR"
#define JOB_STATUS_TIMEOUT  "TIMEOUT"

/* JSON fields */
#define FIELD_PROTOCOL    "proto"
#define FIELD_JOB_ID      "job_id"
#define FIELD_SERVICE     "service"
#define FIELD_FUNCTION    "function"
#define FIELD_ARGS        "args"
#define FIELD_CLIENT_ID   "client_id"
#define FIELD_VEHICLE_ID  "vehicle_id"
#define FIELD_STATUS      "status"
#define FIELD_RESULT      "result"

/* Event types */
#define EVENT_VEHICLE_UP     "vehicle/up"
#define EVENT_VEHICLE_DOWN   "vehicle/down"
#define EVENT_RSU_UP         "rsu/up"
#define EVENT_JOB_ASSIGNED   "job/assigned"
#define EVENT_JOB_FAILED     "job/failed"

/* Filtros para o Servidor (Wildcards) */
#define TOPIC_FILTER_VEHICLE_REGISTER "vc/vehicle/+/register/request"
#define TOPIC_FILTER_JOB_SUBMIT       "vc/client/+/job/submit"
#define TOPIC_FILTER_VEHICLE_STATUS   "vc/vehicle/+/status"

/* Control topics */
#define TOPIC_CONFIG_POLICY  "vc/server/config/policy"

#define GPS_INVALID -999.0

#endif

#ifndef MIDD4VC_JOB_CODEC_H
#define MIDD4VC_JOB_CODEC_H

#include <stddef.h>

#include "midd4vc_client.h"
#include "midd4vc_protocol.h"

int midd4vc_parse_job(
    const char *json,
    midd4vc_job_t *job
);

void midd4vc_build_job_result(
    char *out,
    size_t out_size,
    const char *job_id,
    const char *vehicle_id,
    int status,
    int result
);

int midd4vc_encode_job(char *dest, size_t dest_sz, const char *job_id, const char *service, 
                       const char *function, const char *client_id, 
                       double lat, double lon, const int *args, int argc);

#endif

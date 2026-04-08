#ifndef MIDD4VC_DATA_MANAGEMENT_H
#define MIDD4VC_DATA_MANAGEMENT_H

#include "../distribution/midd4vc_client.h"

typedef enum {
    DM_MODE_RAW,         // Envio imediato sem tratamento
    DM_MODE_AGGREGATE,   // Compactação/Média local
    DM_MODE_STREAMING,   // Fluxo contínuo de alta prioridade
    DM_MODE_BATCH,       // Agrupamento para economia de energia
    DM_MODE_REASONING    // Inferência local/Inteligência
} dm_mode_t;

/* Estrutura de metadados do dado (Contexto) */
typedef struct {
    char node_id[32];
    dm_mode_t mode;
    int priority;        // 0-10
    double threshold;    // Para detecção de mudanças
} dm_options_t;

/* Ponto de entrada para a aplicação */
void midd4vc_dm_ingest(midd4vc_client_t *c, const dm_options_t *opts, double value);

void midd4vc_dm_set_policy(midd4vc_client_t *c, dm_mode_t global_mode);

#endif
#include "midd4vc_data_management.h"
#include "../distribution/midd4vc_client.h"
#include <stdio.h>

// pode ter vários desses interceptadores de data management e adicionar todos no main, mas a ordem importa.
midd4vc_action_t midd4vc_dm_interceptor(midd4vc_client_t *c, const char *topic, char **payload) {
    
    /* Aqui implementará os 4 estágios que você listou no TODO:
       1. Data Collection & Integration (Parser do JSON)
       2. Data Processing (Aggregate/Filtering based on historic)
       3. Reasoning (Context Awareness, e.g., Decisão de Engajamento se a confiança for baixa)
       4. Dissemination (Publicar em lote (batch) ou streaming)
    */

    // Exemplo: Interceptar dados crus e decidir se eles seguem normalmente
    if (strstr(topic, "vc/data/raw")) {
        printf("[DM-SERVICE] Interceptado dado para processamento científico...\n");
        
        // Se a lógica disser que o dado é ruído:
        // return MIDD4VC_DROP;

        // Se quiser transformar o dado (Ontologia/Padronização):
        // char *novo = strdup("{\"status\":\"processed\", ...}");
        // free(*payload);
        // *payload = novo;
    }

    return MIDD4VC_CONTINUE; 
}

void midd4vc_dm_ingest(midd4vc_client_t *c, const dm_options_t *opts, double value) {
    /* TODO: implementar aqui o pipeline:
       1. Data Collection & Integration
       2. Data Processing (Aggregate/Filtering)
       3. Reasoning (Context Awareness)
       4. Dissemination (Batch vs Streaming)
    */

    // Por enquanto, apenas repassa como RAW para não quebrar o seu artigo atual
    char payload[128];
    snprintf(payload, sizeof(payload), "{\"id\":\"%s\",\"val\":%.2f}", opts->node_id, value);
    midd4vc_publish(c, "vc/data/raw", payload);
}
#include "midd4vc_data_management.h"
#include <stdio.h>

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
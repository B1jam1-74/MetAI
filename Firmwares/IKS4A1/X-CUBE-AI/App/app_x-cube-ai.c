#include "app_x-cube-ai.h"
#include "metai_v3_config.h"
#include "ai_datatypes_defines.h"
#include "network_data_params.h"   /* AI_NETWORK_DATA_ACTIVATIONS_SIZE */
#include "ai_platform.h"
#include "network.h"
#include "network_data.h"
#include "main.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

/* ------------------------------------------------------------------ */
/*  Stockage des 3 fenêtres temporelles                                */
/* ------------------------------------------------------------------ */
static metai_sample_t s_current = {0};   /* t0  — mesure courante     */
static metai_sample_t s_t3h     = {0};   /* t-3h — depuis downlink    */
static metai_sample_t s_t6h     = {0};   /* t-6h — depuis downlink    */
static uint8_t        s_has_current  = 0;
static uint8_t        s_has_downlink = 0;

/* ------------------------------------------------------------------ */
/*  Résultats publics                                                  */
/* ------------------------------------------------------------------ */
volatile uint8_t g_metai_class      = 0xFF;
volatile float   g_metai_confidence = 0.0f;

/* ------------------------------------------------------------------ */
/*  Buffers réseau                                                     */
/* ------------------------------------------------------------------ */
static ai_handle  s_network    = AI_HANDLE_NULL;
AI_ALIGNED(4) static int8_t s_input_data [METAI_NUM_FEATURES];
AI_ALIGNED(4) static int8_t s_output_data[METAI_NUM_CLASSES];
static ai_buffer  s_input_buf [AI_NETWORK_IN_NUM];
static ai_buffer  s_output_buf[AI_NETWORK_OUT_NUM];

/* ------------------------------------------------------------------ */
/*  Init                                                               */
/* ------------------------------------------------------------------ */

/* Buffer d'activation — taille exacte issue de network_data_params.h */
AI_ALIGNED(4) static uint8_t s_activations[AI_NETWORK_DATA_ACTIVATIONS_SIZE];

void MX_X_CUBE_AI_Init(void)
{
    /* Tableau de pointeurs vers les buffers d'activation (COUNT=1) */
    const ai_handle activations[AI_NETWORK_DATA_ACTIVATIONS_COUNT] = {
        AI_HANDLE_PTR(s_activations)
    };

    ai_error err = ai_network_create_and_init(&s_network, activations, NULL);

    if (err.type != AI_ERROR_NONE) {
        printf("[MetAI] create_and_init error %d/%d\r\n", err.type, err.code);
        return;
    }

    ai_network_report report;
    if (!ai_network_get_report(s_network, &report)) {
        printf("[MetAI] get_report failed\r\n");
        return;
    }

    s_input_buf[0]       = report.inputs[0];
    s_input_buf[0].data  = AI_HANDLE_PTR(s_input_data);
    s_output_buf[0]      = report.outputs[0];
    s_output_buf[0].data = AI_HANDLE_PTR(s_output_data);

    printf("[MetAI] in=%d out=%d\r\n",
           (int)report.inputs[0].size,
           (int)report.outputs[0].size);
    printf("[MetAI] Model initialized OK\r\n");
}

/* ------------------------------------------------------------------ */
/*  Push mesure courante (t0)                                          */
/* ------------------------------------------------------------------ */
void MetAI_PushSample(float temp, float pres, float rhum)
{
    s_current.temp    = temp;
    s_current.pres    = pres;
    s_current.rhum    = rhum;
    s_current.tick_ms = HAL_GetTick();
    s_has_current     = 1;
}

/* ------------------------------------------------------------------ */
/*  Seed historique depuis downlink                                    */
/* ------------------------------------------------------------------ */
void MetAI_SeedHistory(metai_sample_t t0,
                       metai_sample_t t3h,
                       metai_sample_t t6h)
{
    s_current      = t0;
    s_t3h          = t3h;
    s_t6h          = t6h;
    s_has_current  = 1;
    s_has_downlink = 1;
}

/* ------------------------------------------------------------------ */
/*  Construction des 9 features depuis les 3 structs                  */
/*  Ordre : [t0, p0, h0, t3, p3, h3, t6, p6, h6]                     */
/* ------------------------------------------------------------------ */
static void metai_build_features(float *feat)
{
    feat[0] = s_current.temp;  feat[1] = s_current.pres;  feat[2] = s_current.rhum;
    feat[3] = s_t3h.temp;      feat[4] = s_t3h.pres;      feat[5] = s_t3h.rhum;
    feat[6] = s_t6h.temp;      feat[7] = s_t6h.pres;      feat[8] = s_t6h.rhum;
}

/* ------------------------------------------------------------------ */
/*  Process                                                            */
/* ------------------------------------------------------------------ */
void MX_X_CUBE_AI_Process(void)
{
    /* Attendre d'avoir la mesure courante ET le downlink */
    if (!s_has_current || !s_has_downlink) {
        return;
    }

    /* 1. Features brutes */
    float raw[METAI_NUM_FEATURES];
    metai_build_features(raw);

    /* 2. Normalisation min-max */
    float norm[METAI_NUM_FEATURES];
    METAI_normalize(raw, norm);

    /* 3. Quantification INT8 */
    METAI_quantize_input(norm, s_input_data);

    /* 4. Inférence */
    ai_i32 n_batch = ai_network_run(s_network, s_input_buf, s_output_buf);

    if (n_batch != 1) {
        ai_error err = ai_network_get_error(s_network);
        printf("[MetAI] ai_run error %d/%d\r\n", err.type, err.code);
        return;
    }

    /* 5. Décodage + softmax approximé */
    uint8_t pred_class = METAI_decode_output(s_output_data);

    float sum_exp = 0.0f;
    float scores[METAI_NUM_CLASSES];
    for (int i = 0; i < METAI_NUM_CLASSES; i++) {
        scores[i] = expf((s_output_data[i] - METAI_OUTPUT_ZERO_POINT)
                         * METAI_OUTPUT_SCALE);
        sum_exp += scores[i];
    }
    float confidence = (sum_exp > 0.0f) ? scores[pred_class] / sum_exp : 0.0f;

    g_metai_class      = pred_class;
    g_metai_confidence = confidence;

    // Print résultat inférence
    printf("[MetAI] Predicted class: %d, confidence: %.2f%%\r\n",
           g_metai_class, g_metai_confidence * 100.0f);
}

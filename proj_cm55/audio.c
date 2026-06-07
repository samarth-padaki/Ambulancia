/******************************************************************************
* File Name:   audio.c
*
* Description: This file implements the interface with the PDM and AMIC, as
* well as the ISR to feed the audio processing block for 4 channels.
*******************************************************************************/
#include "cybsp.h"
#include "cy_pdl.h"
#include "audio.h"
#include "amic.h"
#include <math.h>
#include <stdio.h>

/******************************************************************************
 * Macros
 *****************************************************************************/
#define PDM_PCM_ISR_PRIORITY                    (2u)
#define PDM_CHANNEL_LEFT                        (2u)
#define PDM_CHANNEL_RIGHT                       (3u)
#define HW_FIFO_SIZE                            (64u)
#define RX_FIFO_TRIG_LEVEL                      (HW_FIFO_SIZE/2)
#define NUMBER_INTERRUPTS_FOR_FRAME             (FRAME_SIZE/RX_FIFO_TRIG_LEVEL)

/* Colleague's updated values */
#define DIGITAL_BOOST_FACTOR                    2.0f
#define AUIDO_BITS_PER_SAMPLE                   16
#define SAMPLE_NORMALIZE(sample)                (((float) (sample)) / (float) (1 << (AUIDO_BITS_PER_SAMPLE - 1)))
#define OUTPUT_THRESHOLD_SCORE                  (0.4f)
#define PDM_PCM_GAIN                            (CY_PDM_PCM_SEL_GAIN_5DB)

/******************************************************************************
 * Global Variables
 *****************************************************************************/
static int16_t audio_buffer0_right[FRAME_SIZE] = {0};
static int16_t audio_buffer1_right[FRAME_SIZE] = {0};
static int16_t* active_rx_buffer_right;
static int16_t* full_rx_buffer_right;

static int16_t audio_buffer0_left[FRAME_SIZE] = {0};
static int16_t audio_buffer1_left[FRAME_SIZE] = {0};
static int16_t* active_rx_buffer_left;
static int16_t* full_rx_buffer_left;

static const cy_stc_sysint_t PDM_IRQ_cfg_right = { .intrSrc = (IRQn_Type)CYBSP_PDM_CHANNEL_3_IRQ, .intrPriority = PDM_PCM_ISR_PRIORITY };
static const cy_stc_sysint_t PDM_IRQ_cfg_left = { .intrSrc = (IRQn_Type)CYBSP_PDM_CHANNEL_2_IRQ, .intrPriority = PDM_PCM_ISR_PRIORITY };

static volatile bool pdm_pcm_flag_right;
static volatile bool pdm_pcm_flag_left;

static void pdm_pcm_event_handler_right(void);
static void pdm_pcm_event_handler_left(void);

cy_rslt_t pdm_init(void)
{
    cy_rslt_t result;

    memset(audio_buffer0_right, 0, FRAME_SIZE*sizeof(int16_t));
    memset(audio_buffer1_right, 0, FRAME_SIZE*sizeof(int16_t));
    active_rx_buffer_right = audio_buffer0_right;
    full_rx_buffer_right = audio_buffer1_right;

    memset(audio_buffer0_left, 0, FRAME_SIZE*sizeof(int16_t));
    memset(audio_buffer1_left, 0, FRAME_SIZE*sizeof(int16_t));
    active_rx_buffer_left = audio_buffer0_left;
    full_rx_buffer_left = audio_buffer1_left;

    result = Cy_PDM_PCM_Init(CYBSP_PDM_HW, &CYBSP_PDM_config);
    if(CY_PDM_PCM_SUCCESS != result) return result;

    Cy_PDM_PCM_Channel_Enable(CYBSP_PDM_HW, PDM_CHANNEL_RIGHT);
    Cy_PDM_PCM_Channel_Init(CYBSP_PDM_HW, &channel_3_config, (uint8_t)PDM_CHANNEL_RIGHT);
    Cy_PDM_PCM_SetGain(CYBSP_PDM_HW, PDM_CHANNEL_RIGHT, PDM_PCM_GAIN);
    Cy_PDM_PCM_Channel_ClearInterrupt(CYBSP_PDM_HW, PDM_CHANNEL_RIGHT, CY_PDM_PCM_INTR_MASK);
    Cy_PDM_PCM_Channel_SetInterruptMask(CYBSP_PDM_HW, PDM_CHANNEL_RIGHT, CY_PDM_PCM_INTR_MASK);
    Cy_SysInt_Init(&PDM_IRQ_cfg_right, &pdm_pcm_event_handler_right);
    NVIC_ClearPendingIRQ(PDM_IRQ_cfg_right.intrSrc);
    NVIC_EnableIRQ(PDM_IRQ_cfg_right.intrSrc);

    Cy_PDM_PCM_Channel_Enable(CYBSP_PDM_HW, PDM_CHANNEL_LEFT);
    Cy_PDM_PCM_Channel_Init(CYBSP_PDM_HW, &channel_2_config, (uint8_t)PDM_CHANNEL_LEFT);
    Cy_PDM_PCM_SetGain(CYBSP_PDM_HW, PDM_CHANNEL_LEFT, PDM_PCM_GAIN);
    Cy_PDM_PCM_Channel_ClearInterrupt(CYBSP_PDM_HW, PDM_CHANNEL_LEFT, CY_PDM_PCM_INTR_MASK);
    Cy_PDM_PCM_Channel_SetInterruptMask(CYBSP_PDM_HW, PDM_CHANNEL_LEFT, CY_PDM_PCM_INTR_MASK);
    Cy_SysInt_Init(&PDM_IRQ_cfg_left, &pdm_pcm_event_handler_left);
    NVIC_ClearPendingIRQ(PDM_IRQ_cfg_left.intrSrc);
    NVIC_EnableIRQ(PDM_IRQ_cfg_left.intrSrc);

    pdm_pcm_flag_right = false;
    pdm_pcm_flag_left = false;

    Cy_PDM_PCM_Activate_Channel(CYBSP_PDM_HW, PDM_CHANNEL_RIGHT);
    Cy_PDM_PCM_Activate_Channel(CYBSP_PDM_HW, PDM_CHANNEL_LEFT);

    return CY_RSLT_SUCCESS;
}

static void pdm_pcm_event_handler_right(void)
{
    static uint16_t frame_counter = 0;
    uint32_t intr_status = Cy_PDM_PCM_Channel_GetInterruptStatusMasked(CYBSP_PDM_HW, PDM_CHANNEL_RIGHT);
    if(CY_PDM_PCM_INTR_RX_TRIGGER & intr_status)
    {
        for(uint32_t index=0; index < RX_FIFO_TRIG_LEVEL; index++) {
            active_rx_buffer_right[frame_counter * RX_FIFO_TRIG_LEVEL + index] = (int16_t)Cy_PDM_PCM_Channel_ReadFifo(CYBSP_PDM_HW, PDM_CHANNEL_RIGHT);
        }
        Cy_PDM_PCM_Channel_ClearInterrupt(CYBSP_PDM_HW, PDM_CHANNEL_RIGHT, CY_PDM_PCM_INTR_RX_TRIGGER);
        frame_counter++;
    }
    if(NUMBER_INTERRUPTS_FOR_FRAME <= frame_counter)
    {
        int16_t* temp = active_rx_buffer_right;
        active_rx_buffer_right = full_rx_buffer_right;
        full_rx_buffer_right = temp;
        pdm_pcm_flag_right = true;
        frame_counter = 0;
    }
    if((CY_PDM_PCM_INTR_RX_FIR_OVERFLOW | CY_PDM_PCM_INTR_RX_OVERFLOW | CY_PDM_PCM_INTR_RX_IF_OVERFLOW | CY_PDM_PCM_INTR_RX_UNDERFLOW) & intr_status) {
        Cy_PDM_PCM_Channel_ClearInterrupt(CYBSP_PDM_HW, PDM_CHANNEL_RIGHT, CY_PDM_PCM_INTR_MASK);
    }
}

static void pdm_pcm_event_handler_left(void)
{
    static uint16_t frame_counter = 0;
    uint32_t intr_status = Cy_PDM_PCM_Channel_GetInterruptStatusMasked(CYBSP_PDM_HW, PDM_CHANNEL_LEFT);
    if(CY_PDM_PCM_INTR_RX_TRIGGER & intr_status)
    {
        for(uint32_t index=0; index < RX_FIFO_TRIG_LEVEL; index++) {
            active_rx_buffer_left[frame_counter * RX_FIFO_TRIG_LEVEL + index] = (int16_t)Cy_PDM_PCM_Channel_ReadFifo(CYBSP_PDM_HW, PDM_CHANNEL_LEFT);
        }
        Cy_PDM_PCM_Channel_ClearInterrupt(CYBSP_PDM_HW, PDM_CHANNEL_LEFT, CY_PDM_PCM_INTR_RX_TRIGGER);
        frame_counter++;
    }
    if(NUMBER_INTERRUPTS_FOR_FRAME <= frame_counter)
    {
        int16_t* temp = active_rx_buffer_left;
        active_rx_buffer_left = full_rx_buffer_left;
        full_rx_buffer_left = temp;
        pdm_pcm_flag_left = true;
        frame_counter = 0;
    }
    if((CY_PDM_PCM_INTR_RX_FIR_OVERFLOW | CY_PDM_PCM_INTR_RX_OVERFLOW | CY_PDM_PCM_INTR_RX_IF_OVERFLOW | CY_PDM_PCM_INTR_RX_UNDERFLOW) & intr_status) {
        Cy_PDM_PCM_Channel_ClearInterrupt(CYBSP_PDM_HW, PDM_CHANNEL_LEFT, CY_PDM_PCM_INTR_MASK);
    }
}

cy_rslt_t pdm_data_process(void)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;
    int16_t best_label = 0;
    float max_score = 0.0f;
    float label_scores[IMAI_DATA_OUT_COUNT];
    char *label_text[] = IMAI_DATA_OUT_SYMBOLS;

    /* Check if ALL 4 channels (PDM Left/Right + AMIC Left/Right) are ready */
    if (!pdm_pcm_flag_right || !pdm_pcm_flag_left || !amic_data_flag)
    {
        result = PDM_PCM_DATA_NOT_READY;
        return result;
    }

    /* Reset flags */
    pdm_pcm_flag_right = false;
    pdm_pcm_flag_left = false;
    amic_data_flag = false;

    static bool title_printed = false;
    if (!title_printed)
    {
#ifdef COMPONENT_CM33
        printf("DEEPCRAFT Studio 4-Mic Example - CM33\r\n\n");
#else
        printf("DEEPCRAFT Studio 4-Mic Example - CM55\r\n\n");
#endif 
        title_printed = true;
    }

    for (uint32_t index = 0; index < FRAME_SIZE ; index++)
    {
        /* 1. Normalize PDM Left */
        float sample_pdm_left = SAMPLE_NORMALIZE(full_rx_buffer_left[index]) * DIGITAL_BOOST_FACTOR;
        if (sample_pdm_left > 1.0f) sample_pdm_left = 1.0f; else if (sample_pdm_left < -1.0f) sample_pdm_left = -1.0f;

        /* 2. Normalize PDM Right */
        float sample_pdm_right = SAMPLE_NORMALIZE(full_rx_buffer_right[index]) * DIGITAL_BOOST_FACTOR;
        if (sample_pdm_right > 1.0f) sample_pdm_right = 1.0f; else if (sample_pdm_right < -1.0f) sample_pdm_right = -1.0f;

        /* 3. Normalize AMIC Left */
        float sample_amic_left = SAMPLE_NORMALIZE(full_rx_buffer_amic_left[index]) * DIGITAL_BOOST_FACTOR;
        if (sample_amic_left > 1.0f) sample_amic_left = 1.0f; else if (sample_amic_left < -1.0f) sample_amic_left = -1.0f;

        /* 4. Normalize AMIC Right */
        float sample_amic_right = SAMPLE_NORMALIZE(full_rx_buffer_amic_right[index]) * DIGITAL_BOOST_FACTOR;
        if (sample_amic_right > 1.0f) sample_amic_right = 1.0f; else if (sample_amic_right < -1.0f) sample_amic_right = -1.0f;

        /* Build the 4-channel input feature vector */
        /* NOTE: IMAI_DATA_IN_COUNT must be 4 in your updated DEEPCRAFT model */
        float input_features[4]; 
        input_features[0] = sample_pdm_left;
        input_features[1] = sample_pdm_right;
        input_features[2] = sample_amic_left;
        input_features[3] = sample_amic_right;

        /* Enqueue 4 features */
        result = IMAI_enqueue(input_features);
        CY_ASSERT(IMAI_RET_SUCCESS == result);

        best_label = 0;
        max_score = -1000.0f;

        switch(IMAI_dequeue(label_scores))
        {
            case IMAI_RET_SUCCESS:
            {
                /* Structured output for Python Visualizer */
                printf("AI_DATA:");
                for(int i = 0; i < IMAI_DATA_OUT_COUNT; i++)
                {
                    printf("%.3f%s", label_scores[i], (i == IMAI_DATA_OUT_COUNT - 1) ? "" : ",");
                    if (label_scores[i] > max_score)
                    {
                        max_score = label_scores[i];
                        best_label = i;
                    }
                }
                printf("\r\n");

                if(max_score >= OUTPUT_THRESHOLD_SCORE)
                {
                    printf(">>> Detected: %s (%.2f)\r\n", label_text[best_label], max_score);
                }
                break;
            }
            case IMAI_RET_NODATA: break;
            case IMAI_RET_ERROR: CY_ASSERT(0); break;
        }
    }
    return result;
}
/* [] END OF FILE */
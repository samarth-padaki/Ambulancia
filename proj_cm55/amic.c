/******************************************************************************
* File Name:   amic.c
* Description: Analog Microphone (AMIC) initialization and ISR.
*              Adapted for deployment (removed protobuf/protocol dependencies).
*******************************************************************************/
#include "amic.h"
#include "cybsp.h"
#include "cy_pdl.h"
#include "cy_tcpwm_counter.h"

#define ANALOG_FIFO_IDX                     (0U)
#define ANALOG_FIFO_BUF0_IDX                (0U)
#define ANALOG_FIFO_BUF1_IDX                (1U)

/* Number of samples read per FIFO interrupt from AutAnalog. 
 * Must evenly divide FRAME_SIZE (1024) */
#define AMIC_CHUNK_SIZE                     (64U) 

#define AMIC_Timer                          CYBSP_AMIC_TIMER_16KHZ_HW, CYBSP_AMIC_TIMER_16KHZ_NUM
#define AMIC_Timer_cfg                      &CYBSP_AMIC_TIMER_16KHZ_config
#define AMIC_STABILIZE_DELAY_MS             (100u)

/* Ping-pong buffers for AMIC matching the PDM FRAME_SIZE (1024) */
static int16_t amic_buffer0_left[FRAME_SIZE] = {0};
static int16_t amic_buffer1_left[FRAME_SIZE] = {0};
static int16_t amic_buffer0_right[FRAME_SIZE] = {0};
static int16_t amic_buffer1_right[FRAME_SIZE] = {0};

static int16_t* active_rx_buffer_amic_left = amic_buffer0_left;
static int16_t* active_rx_buffer_amic_right = amic_buffer0_right;

int16_t* full_rx_buffer_amic_left = amic_buffer1_left;
int16_t* full_rx_buffer_amic_right = amic_buffer1_right;

volatile bool amic_data_flag = false;

/* Temporary hardware buffer for reading FIFO */
static int32_t hw_buffer_left[AMIC_CHUNK_SIZE];
static int32_t hw_buffer_right[AMIC_CHUNK_SIZE];

void analog_fifo_isr(void)
{
    static uint16_t chunk_counter = 0;
    uint32_t intr_status = Cy_AutAnalog_FIFO_GetInterruptStatus(ANALOG_FIFO_IDX);

    Cy_AutAnalog_FIFO_ClearInterruptMask(ANALOG_FIFO_IDX, CY_AUTANALOG_INT_FIFO_LEVEL0);
    Cy_AutAnalog_FIFO_ClearInterrupt(ANALOG_FIFO_IDX, intr_status);

    if ((intr_status & CY_AUTANALOG_INT_FIFO_LEVEL0) != 0UL)
    {
        /* Read Stereo AMIC Data */
        Cy_AutAnalog_FIFO_ReadData(ANALOG_FIFO_IDX, ANALOG_FIFO_BUF0_IDX, AMIC_CHUNK_SIZE, hw_buffer_left);
        Cy_AutAnalog_FIFO_ReadData(ANALOG_FIFO_IDX, ANALOG_FIFO_BUF1_IDX, AMIC_CHUNK_SIZE, hw_buffer_right);

        /* Copy into active application buffer */
        uint32_t offset = chunk_counter * AMIC_CHUNK_SIZE;
        for (uint16_t z = 0; z < AMIC_CHUNK_SIZE; z++)
        {
            active_rx_buffer_amic_left[offset + z] = (int16_t)(hw_buffer_left[z]);
            active_rx_buffer_amic_right[offset + z] = (int16_t)(hw_buffer_right[z]);
        }

        chunk_counter++;

        /* If we have accumulated a full FRAME_SIZE (1024), swap buffers */
        if (chunk_counter >= (FRAME_SIZE / AMIC_CHUNK_SIZE))
        {
            int16_t* temp_l = active_rx_buffer_amic_left;
            active_rx_buffer_amic_left = full_rx_buffer_amic_left;
            full_rx_buffer_amic_left = temp_l;

            int16_t* temp_r = active_rx_buffer_amic_right;
            active_rx_buffer_amic_right = full_rx_buffer_amic_right;
            full_rx_buffer_amic_right = temp_r;

            amic_data_flag = true;
            chunk_counter = 0;
        }

        Cy_AutAnalog_FIFO_SetInterruptMask(ANALOG_FIFO_IDX, CY_AUTANALOG_INT_FIFO_LEVEL0);
    }
}

cy_rslt_t amic_init(void)
{
    /* Note: Requires CYBSP_AMIC_TIMER_16KHZ_HW and autonomous_analog_init 
     * to be configured in design.modus via Device Configurator! */
     
    if (CY_TCPWM_SUCCESS != Cy_TCPWM_Counter_Init(AMIC_Timer, AMIC_Timer_cfg)) return CY_RSLT_TYPE_ERROR;

    if (CY_AUTANALOG_SUCCESS == Cy_AutAnalog_Init(&autonomous_analog_init))
    {
        Cy_AutAnalog_AdvPowerControl(false, false, true);

        const cy_stc_sysint_t AUT_ANALOG_FIFO_IRQ_cfg = {
            .intrSrc      = pass_interrupt_fifo_IRQn,
            .intrPriority = 2U,
        };
        if (CY_SYSINT_SUCCESS == Cy_SysInt_Init(&AUT_ANALOG_FIFO_IRQ_cfg, &analog_fifo_isr))
        {
            NVIC_ClearPendingIRQ(pass_interrupt_fifo_IRQn);
            NVIC_EnableIRQ(pass_interrupt_fifo_IRQn);
        }

        Cy_AutAnalog_FIFO_SetInterruptMask(ANALOG_FIFO_IDX, CY_AUTANALOG_INT_FIFO_LEVEL0);
        Cy_AutAnalog_StartAutonomousControl();
    }
    
    Cy_SysLib_Delay(AMIC_STABILIZE_DELAY_MS);
    Cy_AutAnalog_FwTrigger(CY_AUTANALOG_FW_TRIGGER0);

    Cy_TCPWM_Counter_Enable(AMIC_Timer);
    Cy_TCPWM_TriggerStart_Single(AMIC_Timer);

    return CY_RSLT_SUCCESS;
}

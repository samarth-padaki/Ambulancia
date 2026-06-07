/******************************************************************************
* File Name:   amic.h
* Description: Cleaned up interface for Analog Microphone (AMIC) deployment.
*******************************************************************************/
#ifndef AMIC_H_
#define AMIC_H_

#include <stdint.h>
#include <stdbool.h>
#include "cy_result.h"
#include "audio.h" // For FRAME_SIZE

extern int16_t* full_rx_buffer_amic_left;
extern int16_t* full_rx_buffer_amic_right;
extern volatile bool amic_data_flag;

cy_rslt_t amic_init(void);

#endif /* AMIC_H_ */

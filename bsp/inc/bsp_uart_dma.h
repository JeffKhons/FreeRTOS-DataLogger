/**
  ******************************************************************************
  * @file    bsp_uart_dma.h
  * @brief   Bare-metal USART2 DMA RX Driver Header (Idle Line Interrupt)
  ******************************************************************************
  */
#ifndef BSP_UART_DMA_H
#define BSP_UART_DMA_H

#include "stm32f4xx.h"
#include <stdint.h>

/* Public Function Prototypes ------------------------------------------------*/
void UART2_DMA_Init_BareMetal(void);

#endif /* BSP_UART_DMA_H */
/**
  ******************************************************************************
  * @file    bsp_spi.h
  * @brief   Bare-metal SPI1 Driver Header
  ******************************************************************************
  */
#ifndef BSP_SPI_H
#define BSP_SPI_H

#include "stm32f4xx.h"  // 引入 CMSIS 暫存器定義 (不需要 HAL)
#include <stdint.h>

/* Public Function Prototypes ------------------------------------------------*/
void SPI1_Init_BareMetal(void);
uint8_t SPI1_TxRxByte(uint8_t tx_data);

#endif /* BSP_SPI_H */
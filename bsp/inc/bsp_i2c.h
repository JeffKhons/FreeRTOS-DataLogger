/**
  ******************************************************************************
  * @file    bsp_i2c.h
  * @brief   Bare-metal I2C1 Driver Header (Open-Drain, Pull-up, Bus Recovery)
  ******************************************************************************
  */
#ifndef BSP_I2C_H
#define BSP_I2C_H

#include "stm32f4xx.h"
#include <stdbool.h>  // bool, true, false
#include <stdint.h>   //  uintx_t

/* Public Function Prototypes ------------------------------------------------*/
void I2C1_Init_BareMetal(void);
void I2C1_RecoverBus(void);
void I2C1_ResetOnError(void);
bool I2C1_WriteBuffer(uint8_t dev_addr, uint8_t *pData, uint16_t len);

#endif /* BSP_I2C_H */
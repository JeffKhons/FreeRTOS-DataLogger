/**
  ******************************************************************************
  * @file    bsp_lm75.h
  * @brief   LM75 Temperature Sensor Driver Header
  ******************************************************************************
  */
#ifndef BSP_LM75_H
#define BSP_LM75_H

#include <stdint.h>
#include <stdbool.h>

/* Public Function Prototypes ------------------------------------------------*/
bool LM75_ReadTemp(int32_t *out_temp);

#endif /* BSP_LM75_H */
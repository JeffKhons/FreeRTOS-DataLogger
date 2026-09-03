/**
  ******************************************************************************
  * @file    bsp_clock.h
  * @brief   Bare-metal System Clock Driver Header (84MHz)
  ******************************************************************************
  */
#ifndef BSP_CLOCK_H
#define BSP_CLOCK_H
#include "stm32f4xx.h"
#include <stdint.h>

/* Public Function Prototypes ------------------------------------------------*/
void SystemClock_Config_BareMetal(void);

#endif /* BSP_CLOCK_H */
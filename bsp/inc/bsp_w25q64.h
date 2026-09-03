/**
  ******************************************************************************
  * @file    bsp_w25q64.h
  * @brief   W25Q64 Flash Memory Driver Header
  ******************************************************************************
  */
#ifndef BSP_W25Q64_H
#define BSP_W25Q64_H

#include <stdint.h>
#include <stdbool.h>

/* Public Function Prototypes ------------------------------------------------*/
uint32_t W25Q_Read_JEDEC_ID(void);
void W25Q_Unprotect(void);
bool W25Q_SelfTest(void);
void W25Q_SectorErase(uint32_t sector_addr);
void W25Q_WriteInt32(uint32_t addr, int32_t data);
int32_t W25Q_ReadInt32(uint32_t addr);
uint32_t W25Q_ScanBootCount(void);

#endif /* BSP_W25Q64_H */

/**
  ******************************************************************************
  * @file    bsp_ssd1306.h
  * @brief   SSD1306 OLED I2C Driver Header
  ******************************************************************************
  */
#ifndef BSP_SSD1306_H
#define BSP_SSD1306_H

#include <stdint.h>
#include <stdbool.h>

#define OLED_I2C_ADDR 0x78  // 絕大多數 0.96 吋 OLED 的預設位址 (8-bit)

void OLED_Init(void);
void OLED_WriteCommand(uint8_t cmd);
void OLED_WriteData(uint8_t data);
void OLED_Clear(void);

/* 文字顯示 API */
void OLED_SetCursor(uint8_t x, uint8_t page);
void OLED_ShowChar(uint8_t x, uint8_t page, char ch);
void OLED_ShowString(uint8_t x, uint8_t page, const char *str);

#endif /* BSP_SSD1306_H */
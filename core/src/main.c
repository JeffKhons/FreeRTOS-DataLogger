/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body (I2C + SPI + Flash Integration)
  ******************************************************************************
  */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usart.h"
#include "gpio.h"

/* 手刻的 BSP 與 App 層 ------------------------------------------------*/
#include "bsp_clock.h"    
#include "bsp_uart_dma.h"
#include "app_logger.h"
#include "bsp_spi.h"
#include "bsp_i2c.h"
#include "bsp_w25q64.h"
#include "bsp_lm75.h" 
#include "bsp_ssd1306.h"
#include "cli_task.h"
#include "ring_buffer.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include <string.h>

RingBuffer_t uart_rx_buffer;
uint8_t rx_data;
TaskHandle_t xCLITaskHandle = NULL; // 給 UART ISR 發送 Task Notification 使用

void SystemClock_Config(void);

int main(void)
{
  // HAL_Init(); // 已被純 Bare-metal 取代
  // SystemClock_Config(); // 已被純 Bare-metal 取代
  // MX_GPIO_Init(); // 已被純 Bare-metal 取代
  // MX_USART2_UART_Init(); // 已被純 Bare-metal 取代

  /* 1. 純暫存器時鐘與通訊初始化 */
  SystemClock_Config_BareMetal(); 
  UART2_DMA_Init_BareMetal();     

  /* 2. bsp 初始化 */
  I2C1_Init_BareMetal();
  SPI1_Init_BareMetal(); 
  
  /* 3. app 初始化 */
  RingBuffer_Init(&uart_rx_buffer);
  CLI_Init();

  CLI_Write("=== STM32F446RE System Boot ===\r\n");
  
  /* 4. OLED 初始化測試 */
  OLED_Init(); 
  OLED_ShowString(0, 0, "SYSTEM BOOTING");
  
  /* 5. LM75 溫度讀取測試 */
  int32_t boot_temp = 0;
  if (LM75_ReadTemp(&boot_temp)) {
      char temp_msg[64];
      // 終端機印出
      snprintf(temp_msg, sizeof(temp_msg), "[System] LM75 OK! Current Temp: %ld.%02ld C\r\n", boot_temp / 100, boot_temp % 100);
      CLI_Write(temp_msg);
      
      // OLED 顯示溫度
      char oled_str[32];
      snprintf(oled_str, sizeof(oled_str), "TEMP: %ld.%02ld C", boot_temp / 100, boot_temp % 100);
      OLED_ShowString(0, 2, oled_str); // 在第 2 頁顯示
    }else {
      CLI_Write("[Error] LM75 Read Failed on Boot!\r\n");
      OLED_ShowString(0, 2, "SENSOR ERROR!");
    }

  /* 6. Flash 啟動與檢測流程 */
  /*
  W25Q_Read_JEDEC_ID();
  CLI_Write("[System] Unprotecting Flash... ");
  W25Q_Unprotect();
  CLI_Write("Done!\r\n");

  CLI_Write("[System] Running SPI Self-Test... ");
  if (!W25Q_SelfTest()) {
      CLI_Write("FAILED! (Hardware/WP Issue)\r\n>> ");
  } else {
      CLI_Write("PASSED!\r\n");
      CLI_Write("[System] Scanning Flash for previous logs...\r\n");
      
      flash_record_count = W25Q_ScanBootCount(); 
      
      char boot_msg[64];
      snprintf(boot_msg, sizeof(boot_msg), "[System] Boot Scan Complete! Found %lu records.\r\n>> ", flash_record_count);
      CLI_Write(boot_msg);
  }
  */
  
  /* 7. 啟動非同步中斷並進入主迴圈 */
  // HAL_UART_Receive_IT(&huart2, &rx_data, 1); // 交給 DMA 處理，這裡註解掉避免卡死

  // TODO: 之後在此建立任務並啟動 Scheduler
  // xTaskCreate(vCLITask, "CLI", 256, NULL, 1, &xCLITaskHandle);
  // vTaskStartScheduler();

  while (1)
  {
      CLI_Update(&uart_rx_buffer);
  }
}

/* ========================================================================= */
/*                       系統時鐘與底層回呼函數區塊                            */
/* ========================================================================= */

void CLI_Write(const char *str) {
    // 配合 Bare-metal 把原本的 HAL_UART_Transmit 拔除，改為暫存器直接發送
    // HAL_UART_Transmit(&huart2, (uint8_t *)str, strlen(str), HAL_MAX_DELAY);
    for (int i = 0; i < strlen(str); i++) {
        while (!(USART2->SR & USART_SR_TXE));
        USART2->DR = str[i];
    }
}

void Error_Handler(void) {
  __disable_irq();
  while (1) {}
}
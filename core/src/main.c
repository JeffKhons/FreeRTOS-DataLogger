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

/* 引入我們手刻的 BSP 與 App 層 ------------------------------------------------*/
#include "app_logger.h"
#include "bsp_spi.h"
#include "bsp_i2c.h"
#include "bsp_w25q64.h"
#include "bsp_lm75.h" 
#include "bsp_ssd1306.h"
#include "cli_task.h"
#include "ring_buffer.h"
#include <stdio.h>
#include <string.h>

RingBuffer_t uart_rx_buffer;
uint8_t rx_data;

void SystemClock_Config(void);

int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_USART2_UART_Init();

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
  HAL_UART_Receive_IT(&huart2, &rx_data, 1);

  while (1)
  {
      CLI_Update(&uart_rx_buffer);
  }
}

/* ========================================================================= */
/*                       系統時鐘與底層回呼函數區塊                            */
/* ========================================================================= */

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) { Error_Handler(); }
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK) { Error_Handler(); }
}

void CLI_Write(const char *str) {
    HAL_UART_Transmit(&huart2, (uint8_t *)str, strlen(str), HAL_MAX_DELAY);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART2) {
        RingBuffer_Put(&uart_rx_buffer, rx_data);
        HAL_UART_Receive_IT(&huart2, &rx_data, 1); 
    }
}

void Error_Handler(void) {
  __disable_irq();
  while (1) {}
}
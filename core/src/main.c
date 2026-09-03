/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body (FreeRTOS + I2C + SPI Flash Logger)
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* 手刻的 BSP 與 App 層 ------------------------------------------------*/
#include "bsp_clock.h"
#include "bsp_uart_dma.h"
#include "bsp_i2c.h"
#include "bsp_lm75.h"
#include "bsp_ssd1306.h"
#include "app_resources.h"
#include "cli_task.h"
#include "ring_buffer.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include <string.h>

/* UART ISR 與 CLI_Task 共用的 SPSC Ring Buffer。 */
RingBuffer_t uart_rx_buffer;

/* 給 UART ISR 發送 Task Notification 使用。 */
TaskHandle_t xCLITaskHandle = NULL;

/* 系統共用資源：Task context 才能取得，ISR 不可使用 Mutex。 */
SemaphoreHandle_t xI2CMutex = NULL;
/* Flash 實驗功能暫停；保留空 handle 讓舊有 CLI 介面安全回傳零筆資料。 */
SemaphoreHandle_t xFlashMutex = NULL;
QueueHandle_t xTemperatureQueue = NULL;

/* Task 堆疊單位為 Word；Cortex-M4 上 256 Words = 1024 Bytes。 */
#define CLI_TASK_STACK_WORDS        256U
#define CLI_TASK_PRIORITY           2U

/* ========================================================================= */
/*                          開機硬體檢測與顯示介面                              */
/* ========================================================================= */

/**
 * @brief  開機時讀取一次 LM75，並同時輸出到 UART 與 OLED
 */
static void ShowBootTemperature(void) {
    int32_t temperature_x100 = 0;
    char uart_message[64];
    char oled_message[32];

    if (!LM75_ReadTemp(&temperature_x100)) {
        CLI_Write("[ERROR] LM75 read failed.\r\n");
        OLED_ShowString(0, 2, "SENSOR ERROR");
        return;
    }

    snprintf(uart_message, sizeof(uart_message),
             "[OK] LM75: %ld.%02ld C\r\n",
             temperature_x100 / 100, temperature_x100 % 100);
    CLI_Write(uart_message);

    snprintf(oled_message, sizeof(oled_message),
             "TEMP: %ld.%02ld C",
             temperature_x100 / 100, temperature_x100 % 100);
    OLED_ShowString(0, 2, oled_message);
}

int main(void)
{
  /* 1. 純暫存器時鐘與 UART DMA 初始化，不使用 HAL/CubeMX 周邊初始化。 */
  SystemClock_Config_BareMetal();

  /* 2. ISR 共用資料必須在 USART2 IRQ 啟用前完成初始化。 */
  RingBuffer_Init(&uart_rx_buffer);
  CLI_Init();
  UART2_DMA_Init_BareMetal();

  /* 3. 手刻 I2C 周邊與 FreeRTOS 同步物件初始化。 */
  I2C1_Init_BareMetal();
  xI2CMutex = xSemaphoreCreateMutex();
  if (xI2CMutex == NULL) {
      CLI_Write("[ERROR] I2C mutex creation failed.\r\n");
      Error_Handler();
  }

  CLI_Write("\r\n=== STM32F446RE Bare-Metal Bring-Up ===\r\n");

  /* 4. OLED 與 LM75 開機測試；此時 scheduler 尚未啟動，不需取得 Mutex。 */
  OLED_Init();
  OLED_ShowString(0, 0, "SYSTEM READY");
  ShowBootTemperature();

  CLI_Write("Commands: help, read, dump [n], stat\r\n");

  /* 6. 建立 CLI_Task；UART ISR 以 Task Notification 喚醒它。 */
  if (xTaskCreate(vCLITask,
                  "CLI",
                  CLI_TASK_STACK_WORDS,
                  &uart_rx_buffer,
                  CLI_TASK_PRIORITY,
                  &xCLITaskHandle) != pdPASS) {
      CLI_Write("[ERROR] CLI task creation failed.\r\n");
      Error_Handler();
  }

  /* 7. 啟動 FreeRTOS Scheduler；正常情況下此函式不會返回。 */
  CLI_Write("[OK] Starting FreeRTOS scheduler.\r\n>> ");
  vTaskStartScheduler();

  Error_Handler();
}

/* ========================================================================= */
/*                       UART 輸出與錯誤處理回呼函數區塊                         */
/* ========================================================================= */

void CLI_Write(const char *str) {
    const size_t length = strlen(str);

    /* 配合 Bare-metal 把原本的 HAL_UART_Transmit 改為直接寫入 USART2。 */
    for (size_t i = 0; i < length; i++) {
        while (!(USART2->SR & USART_SR_TXE));
        USART2->DR = str[i];
    }
}

void Error_Handler(void) {
  __disable_irq();
  while (1) {}
}

#include "main.h"
#include "usart.h"
#include "gpio.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h" // 引入號誌與互斥鎖標頭檔
#include <string.h>

// 宣告全域 Mutex 變數
SemaphoreHandle_t xUartMutex = NULL;

void SystemClock_Config(void);

void Error_Handler(void)
{
  /* 關閉所有中斷，避免系統繼續亂跑，並進入死迴圈方便 Debug */
  __disable_irq();
  while (1)
  {
  }
}

// === Task 1：每 1 秒印出一次 ===
void vTask1(void *pvParameters) {
    char *msg1 = "[Task 1] Running! 1000ms\r\n";
    while (1) {
        // 嘗試取得 Mutex，若被佔用則進入 Blocked 狀態等待，不耗費 CPU
        if (xSemaphoreTake(xUartMutex, portMAX_DELAY) == pdTRUE) {
            HAL_UART_Transmit(&huart2, (uint8_t*)msg1, strlen(msg1), HAL_MAX_DELAY);
            xSemaphoreGive(xUartMutex); // 釋放 Mutex
        }
        vTaskDelay(pdMS_TO_TICKS(1000)); 
    }
}

// === Task 2：每 0.5 秒印出一次 ===
void vTask2(void *pvParameters) {
    char *msg2 = "[Task 2] Running! 500ms\r\n";
    while (1) {
        if (xSemaphoreTake(xUartMutex, portMAX_DELAY) == pdTRUE) {
            HAL_UART_Transmit(&huart2, (uint8_t*)msg2, strlen(msg2), HAL_MAX_DELAY);
            xSemaphoreGive(xUartMutex);
        }
        vTaskDelay(pdMS_TO_TICKS(500)); 
    }
}

int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();

    // 建立 Mutex
    xUartMutex = xSemaphoreCreateMutex();

    char *boot_msg = "\r\n=== FreeRTOS Lab 0 (Mutex Protected) ===\r\n";
    HAL_UART_Transmit(&huart2, (uint8_t*)boot_msg, strlen(boot_msg), HAL_MAX_DELAY);

    xTaskCreate(vTask1, "Task 1", 128, NULL, 1, NULL);
    xTaskCreate(vTask2, "Task 2", 128, NULL, 1, NULL);

    vTaskStartScheduler();
    while (1);
}

// 將原本 main.c 的時鐘設定搬過來，確保 UART Baud rate 正確
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
  HAL_RCC_OscConfig(&RCC_OscInitStruct);
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0);
}
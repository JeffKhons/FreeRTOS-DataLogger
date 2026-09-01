#include "main.h"
#include "usart.h"
#include "gpio.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"  // 引入 Queue API
#include <string.h>
#include <stdio.h>

/* 宣告全域的 Queue 句柄 */
QueueHandle_t xDataQueue;

void SystemClock_Config(void);
void Error_Handler(void);

// 簡單的 UART 列印函數
void UART_Print(char *msg) {
    HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);
}

// === 生產者 Task：狂塞資料 ===
void vProducerTask(void *pvParameters) {
    int count = 1;
    char msg[60];
    
    while (1) {
        sprintf(msg, "[Producer] Generating data: %d\r\n", count);
        UART_Print(msg);
        
        // 嘗試發送資料到 Queue 中。
        // 如果 Queue 滿了，最多等待 (Block) 500 個 Ticks (500ms)。
        if (xQueueSend(xDataQueue, &count, pdMS_TO_TICKS(500)) == pdPASS) {
            count++; // 發送成功才 +1
        } else {
            // 發送失敗 (Timeout)，代表等待了 500ms 後 Queue 還是滿的
            UART_Print("[Producer] ⚠️ WARNING: Queue is FULL! Send Timeout.\r\n");
        }
        
        // 快速生產 (每 300ms 一次)
        vTaskDelay(pdMS_TO_TICKS(300)); 
    }
}

// === 消費者 Task：慢吞吞地處理 ===
void vConsumerTask(void *pvParameters) {
    int received_data = 0;
    char msg[60];
    
    while (1) {
        // 嘗試從 Queue 接收資料。
        // portMAX_DELAY 代表如果 Queue 是空的，就永遠進入 Blocked 狀態，不浪費 CPU。
        if (xQueueReceive(xDataQueue, &received_data, portMAX_DELAY) == pdTRUE) {
            sprintf(msg, "    -> [Consumer] Processed data: %d\r\n", received_data);
            UART_Print(msg);
        }
        
        // 慢速消費 (每 1000ms 一次)，故意造成塞車
        vTaskDelay(pdMS_TO_TICKS(1000)); 
    }
}

int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();

    UART_Print("\r\n=== FreeRTOS Lab 2: Queue Producer/Consumer ===\r\n");

    // 建立 Queue：長度為 3，每個項目大小為 1 個 int (4 Bytes)
    xDataQueue = xQueueCreate(3, sizeof(int));

    if (xDataQueue != NULL) {
        // Queue 建立成功，才啟動 Tasks
        xTaskCreate(vProducerTask, "Producer", 256, NULL, 2, NULL);
        xTaskCreate(vConsumerTask, "Consumer", 256, NULL, 1, NULL);
        
        vTaskStartScheduler();
    } else {
        UART_Print("Queue creation failed!\r\n");
    }

    while (1);
}

/* ========================================================================= */
/* 底層硬體設定區                                                            */
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
  HAL_RCC_OscConfig(&RCC_OscInitStruct);
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0);
}

void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}
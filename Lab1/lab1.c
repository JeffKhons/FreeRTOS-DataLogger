#include "main.h"
#include "usart.h"
#include "gpio.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <string.h>

SemaphoreHandle_t xUartMutex = NULL;

/* 宣告硬體初始化與錯誤處理函數 */
void SystemClock_Config(void);
void Error_Handler(void);

// 封裝一個帶 Mutex 的列印函數，讓程式碼更乾淨
void Safe_Print(char *msg) {
    if (xSemaphoreTake(xUartMutex, portMAX_DELAY) == pdTRUE) {
        HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);
        xSemaphoreGive(xUartMutex);
    }
}

// === [優先權 3] 貴族：定期工作，會主動讓出 CPU ===
void vTaskHigh(void *pvParameters) {
    while (1) {
        Safe_Print("[High P3] Wakes up, does work, and sleeps.\r\n");
        vTaskDelay(pdMS_TO_TICKS(1000)); 
    }
}

// === [優先權 2] 惡霸變良民：定期交出 CPU ===
void vTaskMed(void *pvParameters) {
    while (1) {
        Safe_Print("[Med  P2] Does work, then YIELDS the CPU for 800ms!\r\n");
        
        /************************ Busy-waiting 寫法會造成低優先權 Task 無法執行，導致飢餓現象 **********************/
        /*
        // Busy-waiting 寫法 (利用 OS 的 Tick 來計算時間)
        // 抓取現在的時間點
        TickType_t start_tick = xTaskGetTickCount(); 
        
        // 只要時間還沒經過 800ms，就一直在這裡空轉 (這會死死霸佔 CPU！)
        while ((xTaskGetTickCount() - start_tick) < pdMS_TO_TICKS(800)) {
            // 裡面故意什麼都不做，也不呼叫 vTaskDelay
            // 因為沒有 Blocked 狀態，低優先權的 Task 絕對拿不到 CPU
        }
        */

        // 【解法】：呼叫 vTaskDelay，主動進入 Blocked 狀態 (交出 CPU)
        vTaskDelay(pdMS_TO_TICKS(800));
    }
}

// === [優先權 1] 平民：極度渴望 CPU ===
void vTaskLow(void *pvParameters) {
    while (1) {
        Safe_Print("[Low  P1] I am finally running...\r\n");
        vTaskDelay(pdMS_TO_TICKS(500)); 
    }
}

int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();

    xUartMutex = xSemaphoreCreateMutex();
    Safe_Print("\r\n=== FreeRTOS Lab 1: Priority & Starvation ===\r\n");

    // 建立 3 個 Task，注意第五個參數 (Priority) 的差異
    xTaskCreate(vTaskLow,  "Task Low",  128, NULL, 1, NULL);
    xTaskCreate(vTaskMed,  "Task Med",  128, NULL, 2, NULL);
    xTaskCreate(vTaskHigh, "Task High", 128, NULL, 3, NULL);

    vTaskStartScheduler();
    while (1);
}

/* ========================================================================= */
/* 底層硬體設定區                                                            */
/* ========================================================================= */

// 時鐘設定，確保 UART Baud rate 與系統時序正確
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

// 發生致命錯誤時的處理函數
void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}
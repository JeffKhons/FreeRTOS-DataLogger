#include "main.h"
#include "usart.h"
#include "gpio.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <string.h>
#include <stdio.h>

/* 宣告二元號誌，用於 ISR 與 Task 之間的同步 */
SemaphoreHandle_t xButtonSemaphore = NULL;

void SystemClock_Config(void);
void Error_Handler(void);

void UART_Print(char *msg) {
    HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);
}

// === [中斷處理 Task] ===
// 平常都在睡覺 (Blocked)，只有按鈕按下的瞬間會被 ISR 喚醒
void vButtonHandlerTask(void *pvParameters) {
    int press_count = 0;
    char msg[60];
    
    while (1) {
        // 無窮期等待 Binary Semaphore (完全不消耗 CPU)
        if (xSemaphoreTake(xButtonSemaphore, portMAX_DELAY) == pdTRUE) {
            press_count++;
            sprintf(msg, "🚨 [Task] Button Pressed! Count: %d\r\n", press_count);
            UART_Print(msg);
            
            // 模擬處理按鈕事件需要花一點時間
            // 這樣寫的好處是，中斷 (ISR) 只需要花 1 微秒丟出 Semaphore 就能結束，
            // 剩下的耗時處理交給這個 Task 慢慢做，不會卡死系統中斷。
            vTaskDelay(pdMS_TO_TICKS(100)); 
        }
    }
}

// === [背景 Task] ===
// 只是用來證明系統沒有被卡死
void vBackgroundTask(void *pvParameters) {
    while (1) {
        UART_Print("[Bg Task] System is running...\r\n");
        vTaskDelay(pdMS_TO_TICKS(2000)); 
    }
}

int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();

    UART_Print("\r\n=== FreeRTOS Lab 4: ISR & Binary Semaphore ===\r\n");

    // 建立 Binary Semaphore
    xButtonSemaphore = xSemaphoreCreateBinary();

    if (xButtonSemaphore != NULL) {
        // Button Handler 設為最高優先權，確保中斷發生時能立刻搶佔 CPU
        xTaskCreate(vButtonHandlerTask, "BtnHandler", 256, NULL, 3, NULL);
        xTaskCreate(vBackgroundTask,    "BgTask",     256, NULL, 1, NULL);
        
        vTaskStartScheduler();
    }

    while (1);
}

/* ========================================================================= */
/* STM32 HAL 外部中斷 (EXTI) 回呼函數                                        */
/* 當按鈕按下 (PC13) 時，HAL 庫會自動呼叫這個 ISR 函數                       */
/* ========================================================================= */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    // 假設你的按鈕設定在 USER_Btn_Pin (通常是 GPIO_PIN_13)
    // 這裡為了通用，只要有 EXTI 中斷就觸發
    
    // 1. 宣告一個變數，記錄是否有更高優先權的 Task 被喚醒
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    // 2. 這是 ISR 專用的 API！不要用一般的 xSemaphoreGive
    xSemaphoreGiveFromISR(xButtonSemaphore, &xHigherPriorityTaskWoken);

    // 3. 如果給出 Semaphore 後，喚醒了優先權比目前執行中 Task 還高的 Task，
    //    就立刻請求一次 Context Switch，確保離開 ISR 後直接執行該 Task。
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/* ========================================================================= */
/* 真正的硬體中斷進入點 (硬體觸發後第一個跳來這裡)                           */
/* ========================================================================= */
void EXTI15_10_IRQHandler(void)
{
    // 呼叫 HAL 庫的中斷處理 API，它會幫忙清除硬體 Flag，
    // 然後自動去呼叫我們寫的 HAL_GPIO_EXTI_Callback
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_13);
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
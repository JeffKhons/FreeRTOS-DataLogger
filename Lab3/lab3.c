#include "main.h"
#include "usart.h"
#include "gpio.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <string.h>

/* ========================================================================= */
/* 實驗：0 = 使用 Binary Semaphore (產生 Bug) / 1 = 使用 Mutex (修復 Bug)      */
/* ========================================================================= */

/* #define USE_MUTEX 0 */
#define USE_MUTEX 1

SemaphoreHandle_t xResourceLock;

void SystemClock_Config(void);
void Error_Handler(void);

void UART_Print(char *msg) {
    HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);
}

// === [優先權 1] 平民：拿到鎖之後，做很久很久的苦工 ===
void vTaskLow(void *pvParameters) {
    while (1) {
        UART_Print("[Low  P1] Requesting Lock...\r\n");
        xSemaphoreTake(xResourceLock, portMAX_DELAY);
        UART_Print("[Low  P1] 🔒 LOCKED! Doing heavy work for 3000ms...\r\n");
        
        // 模擬長時間佔用資源 (不讓出 CPU)
        TickType_t start = xTaskGetTickCount();
        while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(3000)) { }
        
        UART_Print("[Low  P1] 🔓 UNLOCKED resource.\r\n");
        xSemaphoreGive(xResourceLock);
        
        vTaskDelay(pdMS_TO_TICKS(1000)); // 休息一下
    }
}

// === [優先權 2] 惡霸：不需要鎖，單純出來霸佔 CPU ===
void vTaskMed(void *pvParameters) {
    // 故意延遲 1 秒，讓 P1 先拿到鎖
    vTaskDelay(pdMS_TO_TICKS(1000)); 
    while (1) {
        UART_Print("[Med  P2] Woke up! Preempting P1 & Hogging CPU for 2000ms...\r\n");
        TickType_t start = xTaskGetTickCount();
        while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(2000)) { }
        UART_Print("[Med  P2] Done hogging. Sleeping.\r\n");
        
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

// === [優先權 3] 貴族：很急著要鎖，但被卡住 ===
void vTaskHigh(void *pvParameters) {
    // 故意延遲 1.5 秒，確保在 P1 拿鎖且 P2 霸佔 CPU 的途中醒來
    vTaskDelay(pdMS_TO_TICKS(1500)); 
    while (1) {
        UART_Print("[High P3] Woke up! Requesting Lock...\r\n");
        xSemaphoreTake(xResourceLock, portMAX_DELAY);
        
        UART_Print("[High P3] 🔒 LOCKED! Quick work...\r\n");
        TickType_t start = xTaskGetTickCount();
        while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(500)) { }
        UART_Print("[High P3] 🔓 UNLOCKED resource.\r\n");
        xSemaphoreGive(xResourceLock);
        
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();

    if (USE_MUTEX == 1) {
        UART_Print("\r\n=== Lab 3: Priority Inheritance (MUTEX MODE) ===\r\n");
        xResourceLock = xSemaphoreCreateMutex();
    } else {
        UART_Print("\r\n=== Lab 3: Priority Inversion (SEMAPHORE MODE) ===\r\n");
        xResourceLock = xSemaphoreCreateBinary();
        xSemaphoreGive(xResourceLock); // Binary Semaphore 必須先 Give 一次才能使用
    }

    // 建立 3 個 Task
    xTaskCreate(vTaskLow,  "Task Low",  256, NULL, 1, NULL);
    xTaskCreate(vTaskMed,  "Task Med",  256, NULL, 2, NULL);
    xTaskCreate(vTaskHigh, "Task High", 256, NULL, 3, NULL);

    vTaskStartScheduler();
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
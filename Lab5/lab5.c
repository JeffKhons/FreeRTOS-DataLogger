#include "main.h"
#include "usart.h"
#include "gpio.h"
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h" // 🌟 引入 Timer API
#include <string.h>

TimerHandle_t xAutoReloadTimer;
TimerHandle_t xOneShotTimer;

void SystemClock_Config(void);
void Error_Handler(void);

void UART_Print(char *msg) {
    HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);
}

/* ========================================================================= */
/* Timer Callback 函數 (由底層 Timer Daemon Task 呼叫)                        */
/* ========================================================================= */
void vAutoReloadCallback(TimerHandle_t xTimer) {
    UART_Print("[Timer] ⏱️ Auto-Reload Fired! (Every 1000ms)\r\n");
}

void vOneShotCallback(TimerHandle_t xTimer) {
    // 這個函數如果被觸發，代表 Task 太久沒有去 Reset 它是 Timeout 了！
    UART_Print("[Timer] 🚨 One-Shot FIRED! (Timeout reached)\r\n");
}

/* ========================================================================= */
/* 主控 Task：模擬餵狗 (Reset Timer) 的行為                                   */
/* ========================================================================= */
void vControlTask(void *pvParameters) {
    UART_Print("[Task] Control Task Started.\r\n");
    
    // 啟動兩個 Timer
    xTimerStart(xAutoReloadTimer, 0);
    xTimerStart(xOneShotTimer, 0);
    
    // 1. 先連續餵狗兩次 (每 2000ms 餵一次，小於 3000ms，所以 One-Shot 不會觸發)
    for(int i = 0; i < 2; i++) {
        vTaskDelay(pdMS_TO_TICKS(2000));
        UART_Print("[Task] Resetting One-Shot Timer...\r\n");
        xTimerReset(xOneShotTimer, 0); 
    }
    
    // 2. 故意罷工 5 秒！讓 One-Shot Timer 觸發 Timeout
    UART_Print("[Task] Sleeping for 5 seconds (Letting timer expire)...\r\n");
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    while(1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();

    UART_Print("\r\n=== FreeRTOS Lab 5: Software Timers ===\r\n");

    // 1. 建立 Auto-Reload Timer (參數: 名字, 週期, 是否自動重載, ID, 回呼函數)
    xAutoReloadTimer = xTimerCreate("AutoTimer", pdMS_TO_TICKS(1000), pdTRUE, (void *)0, vAutoReloadCallback);
    
    // 2. 建立 One-Shot Timer (pdFALSE 代表單次)
    xOneShotTimer = xTimerCreate("OneShotTimer", pdMS_TO_TICKS(3000), pdFALSE, (void *)1, vOneShotCallback);

    if (xAutoReloadTimer != NULL && xOneShotTimer != NULL) {
        xTaskCreate(vControlTask, "CtrlTask", 256, NULL, 1, NULL);
        vTaskStartScheduler();
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
  while (1) {}
}
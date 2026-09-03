#include "main.h"
#include "usart.h"
#include "gpio.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>
#include <stdio.h>

/* 🌟 我們不再需要宣告 Queue 或 Semaphore！ */
/* 只需要記住「接收方」的 Task Handle 即可 */
TaskHandle_t xLoggerTaskHandle = NULL;

void SystemClock_Config(void);
void Error_Handler(void);

void UART_Print(char *msg) {
    HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);
}

// === [發送方] Sensor Task ===
void vSensorTask(void *pvParameters) {
    uint32_t ulSimulatedTemp = 25; // 模擬溫度 25 度
    char msg[50];
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        ulSimulatedTemp++; // 每次加 1 度
        
        sprintf(msg, "[Sensor] 🌡️ Read Temp: %lu, notifying Logger...\r\n", ulSimulatedTemp);
        UART_Print(msg);
        
        // 核心 API 1：直接把數值發送給 Logger Task
        // 參數：(接收方 Handle, 要傳送的數值, 覆寫模式)
        xTaskNotify(xLoggerTaskHandle, ulSimulatedTemp, eSetValueWithOverwrite);
    }
}

// === [接收方] Logger Task ===
void vLoggerTask(void *pvParameters) {
    uint32_t ulReceivedValue = 0;
    char msg[50];
    
    while (1) {
        // API 2：等待通知
        // 參數：(進入前清除哪些Bit, 離開時清除哪些Bit, 接收數值的指標, 超時時間)
        // 這裡設定 0xFFFFFFFFUL 代表離開時把整個 32-bit 變數清零
        xTaskNotifyWait(0x00, 0xFFFFFFFFUL, &ulReceivedValue, portMAX_DELAY);
        
        // 只有收到通知才會執行到這裡
        sprintf(msg, "[Logger] 📦 Received Temp: %lu. Saving to Flash...\r\n\r\n", ulReceivedValue);
        UART_Print(msg);
    }
}

int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();

    UART_Print("\r\n=== FreeRTOS Lab 6: Task Notification ===\r\n");

    // 建立 Logger Task，並🌟將它的 Handle 存進 xLoggerTaskHandle🌟
    xTaskCreate(vLoggerTask, "Logger", 256, NULL, 2, &xLoggerTaskHandle);
    
    // 建立 Sensor Task (不需要存 Handle，因為它是發送方)
    xTaskCreate(vSensorTask, "Sensor", 256, NULL, 1, NULL);

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
  while (1) {}
}
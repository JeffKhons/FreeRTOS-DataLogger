/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body (Clean Version with I2C Recovery)
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdio.h>
#include "cli_task.h"
#include "ring_buffer.h"
/* USER CODE END Includes */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
RingBuffer_t uart_rx_buffer;
uint8_t rx_data;
uint32_t last_temp_read_time = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);

/* USER CODE BEGIN PFP */
void I2C1_Init_BareMetal(void);
void I2C1_RecoverBus(void);
void I2C1_ResetOnError(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* USER CODE END 0 */

int main(void)
{
  /* MCU Configuration--------------------------------------------------------*/
  HAL_Init();
  SystemClock_Config();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();

  /* USER CODE BEGIN 2 */
  I2C1_Init_BareMetal();
  RingBuffer_Init(&uart_rx_buffer);
  CLI_Init();

  CLI_Write("=== STM32F446RE System Boot ===\r\n");
  
  // 啟動 UART 中斷接收 (每次 1 byte)
  HAL_UART_Receive_IT(&huart2, &rx_data, 1);
  
  // 初始化計時器
  last_temp_read_time = HAL_GetTick();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
      /* 1. 處理 CLI 輸入 (非阻塞) */
      CLI_Update(&uart_rx_buffer);

      /* 2. 每 5 秒自動讀取溫度 (非阻塞) */
    if (HAL_GetTick() - last_temp_read_time >= 5000)
    {
        last_temp_read_time = HAL_GetTick(); 
          
        int32_t temp_x100;
            if (CLI_PortReadTempX100(&temp_x100)) {
              // 採納 5°C ~ 60°C 之間的合理室溫數據
                if (temp_x100 >= 500 && temp_x100 <= 6000) {
                  char log_str[64];
                  snprintf(log_str, sizeof(log_str), "\r\n[Auto Log] Temp: %ld.%02ld\r\n>> ", 
                           temp_x100 / 100, temp_x100 % 100);
                  CLI_Write(log_str);
                } else { // 讀到 0.5 或 0 這種被干擾的壞資料，直接丟棄，不印出來
              }
          }
    }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

void SystemClock_Config(void)
{
  // ... (保留原本 CubeMX 產生的 SystemClock_Config 內容，為節省版面此處省略，請維持原樣) ...
  // 注意：請將你原本程式碼裡的 SystemClock_Config 完整內容貼回這裡
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

/* USER CODE BEGIN 4 */
/* ========================================================================= */
/*                          UART & CLI 底層介面                                */
/* ========================================================================= */
void CLI_Write(const char *str) {
    HAL_UART_Transmit(&huart2, (uint8_t *)str, strlen(str), HAL_MAX_DELAY);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART2) {
        RingBuffer_Put(&uart_rx_buffer, rx_data);
        HAL_UART_Receive_IT(&huart2, &rx_data, 1); 
    }
}

uint32_t CLI_PortLogCount(void) { return 0; } // 預留給 W25Q64
bool CLI_PortLogRead(uint32_t idx, CLI_LogRecord_t *rec) { return false; } // 預留給 W25Q64

/* ========================================================================= */
/*                           I2C 裸機驅動與錯誤處理                             */
/* ========================================================================= */
void I2C1_Init_BareMetal(void) {
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN; 
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;  

    // GPIO 設置 (PB8=SCL, PB9=SDA, AF4, Open-Drain, Pull-up)
    GPIOB->MODER &= ~(GPIO_MODER_MODER8 | GPIO_MODER_MODER9);
    GPIOB->MODER |= (GPIO_MODER_MODER8_1 | GPIO_MODER_MODER9_1);
    GPIOB->OTYPER |= (GPIO_OTYPER_OT8 | GPIO_OTYPER_OT9);
    GPIOB->OSPEEDR |= (GPIO_OSPEEDER_OSPEEDR8 | GPIO_OSPEEDER_OSPEEDR9);
    GPIOB->PUPDR &= ~(GPIO_PUPDR_PUPDR8 | GPIO_PUPDR_PUPDR9);
    GPIOB->PUPDR |= (GPIO_PUPDR_PUPDR8_0 | GPIO_PUPDR_PUPDR9_0);
    GPIOB->AFR[1] &= ~((0xF << 0) | (0xF << 4)); 
    GPIOB->AFR[1] |= ((4 << 0) | (4 << 4));     

    // I2C 引擎重置與速度設置 (100kHz)
    I2C1->CR1 |= I2C_CR1_SWRST;
    I2C1->CR1 &= ~I2C_CR1_SWRST;
    I2C1->CR2 &= ~I2C_CR2_FREQ;
    I2C1->CR2 |= 16; 
    I2C1->CCR &= ~I2C_CCR_CCR;
    I2C1->CCR |= 80;
    I2C1->TRISE = 17;
    I2C1->CR1 |= I2C_CR1_PE; 
}

void I2C1_RecoverBus(void) {
    I2C1->CR1 &= ~I2C_CR1_PE; 
    GPIOB->MODER &= ~GPIO_MODER_MODER8;
    GPIOB->MODER |= GPIO_MODER_MODER8_0;
    
    // 強制打 9 個 Clock 解除 LM75 死鎖
    for(int i = 0; i < 9; i++) {
        GPIOB->ODR |= (1 << 8);  HAL_Delay(1);            
        GPIOB->ODR &= ~(1 << 8); HAL_Delay(1);            
    }
    I2C1_Init_BareMetal();
}

void I2C1_ResetOnError(void) {
    __enable_irq(); 
    I2C1->CR1 |= I2C_CR1_SWRST;
    for(volatile int i=0; i<1000; i++); 
    I2C1->CR1 &= ~I2C_CR1_SWRST;
    
    if (I2C1->SR2 & I2C_SR2_BUSY) { I2C1_RecoverBus(); } 
    else { I2C1_Init_BareMetal(); }
}

bool CLI_PortReadTempX100(int32_t *out_temp) {
    uint8_t data[2];
    uint32_t timeout;
    const uint32_t MAX_WAIT = 100000;
    uint8_t target_addr = 0;

    /* 1. 動態掃描 0x48 到 0x4F 尋找飄移的位址 */
    for (uint8_t addr = 0x48; addr <= 0x4F; addr++) {
        timeout = MAX_WAIT;
        while(I2C1->SR2 & I2C_SR2_BUSY) { if (--timeout == 0) { I2C1_ResetOnError(); return false; } }

        I2C1->CR1 |= I2C_CR1_ACK | I2C_CR1_START;
        timeout = MAX_WAIT;
        while(!(I2C1->SR1 & I2C_SR1_SB)) { if (--timeout == 0) { I2C1_ResetOnError(); return false; } }

        // 發送位址 + Read 模式 (將 7-bit addr 左移 1 位並補 1)
        I2C1->DR = (addr << 1) | 1;

        // 等待 ACK (ADDR 旗標) 或 NACK (AF 旗標)
        timeout = 5000; // 給短一點的超時，不浪費時間
        bool ack_received = true;
        while(!(I2C1->SR1 & I2C_SR1_ADDR)) {
            if (I2C1->SR1 & I2C_SR1_AF) { // AF (Acknowledge Failure) 代表沒人理
                I2C1->SR1 &= ~I2C_SR1_AF; // 清除 NACK 旗標
                ack_received = false;
                break;
            }
            if (--timeout == 0) { ack_received = false; break; }
        }

        if (ack_received) {
            target_addr = addr;
            break; // 找到了！維持連線，跳出掃描迴圈直接往下讀資料
        } else {
            I2C1->CR1 |= I2C_CR1_STOP; // 沒人回應，下 Stop 準備掃下一個
        }
    }

    // 掃了一整圈都沒人理，直接回傳失敗
    if (target_addr == 0) { return false; }

    /* 2. 確定對象後，開始讀取 2 Bytes (此時已經在 ADDR 成立狀態) */
    I2C1->CR1 |= I2C_CR1_POS;           
    __disable_irq();                    
    uint32_t clear_addr = I2C1->SR1;    
    clear_addr = I2C1->SR2;             
    (void)clear_addr;                   
    I2C1->CR1 &= ~I2C_CR1_ACK;          
    __enable_irq();                     

    timeout = MAX_WAIT;
    while(!(I2C1->SR1 & I2C_SR1_BTF)) { if (--timeout == 0) { I2C1_ResetOnError(); return false; } }

    I2C1->CR1 |= I2C_CR1_STOP;
    data[0] = I2C1->DR; 
    data[1] = I2C1->DR; 
    
    int16_t raw_temp = (data[0] << 8) | data[1];
    *out_temp = (int32_t)(raw_temp >> 7) * 50;

    return true; 
}
/* USER CODE END 4 */

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
  if (htim->Instance == TIM6) { HAL_IncTick(); }
}

void Error_Handler(void) {
  __disable_irq();
  while (1) {}
}
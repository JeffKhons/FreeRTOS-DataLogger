/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body (I2C + SPI + Flash Integration)
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
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);

/* USER CODE BEGIN PFP */
void I2C1_Init_BareMetal(void);
void I2C1_RecoverBus(void);
void I2C1_ResetOnError(void);

void SPI1_Init_BareMetal(void);
uint8_t SPI1_TxRxByte(uint8_t tx_data);
void W25Q_Read_JEDEC_ID(void);
void W25Q_Unprotect(void);                    // 新增：解除防寫保護
bool W25Q_SelfTest(void);                     // 新增：開機自我測試
void W25Q_SectorErase(uint32_t sector_addr);
void W25Q_WriteInt32(uint32_t addr, int32_t data);
int32_t W25Q_ReadInt32(uint32_t addr);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* USER CODE END 0 */

int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_USART2_UART_Init();

  /* USER CODE BEGIN 2 */
  I2C1_Init_BareMetal();
  SPI1_Init_BareMetal(); 
  
  RingBuffer_Init(&uart_rx_buffer);
  CLI_Init();

  CLI_Write("=== STM32F446RE System Boot ===\r\n");
  
  // 開機測試 SPI 通訊，並印出 Flash ID
  W25Q_Read_JEDEC_ID();
  
  // 解除晶片原廠預設的區塊防寫保護
  CLI_Write("[System] Unprotecting Flash... ");
  W25Q_Unprotect();
  CLI_Write("Done!\r\n");

  // 執行 Flash 讀寫傳輸完整性測試 (自我測試)
  CLI_Write("[System] Running SPI Self-Test... ");
  if (!W25Q_SelfTest()) {
      CLI_Write("FAILED! (Hardware/WP Issue)\r\n>> ");
  } else {
      CLI_Write("PASSED!\r\n");
      
      // 測試通過才擦除資料區 Sector 0
      CLI_Write("[System] Erasing Flash Sector 0... ");
      W25Q_SectorErase(0x000000); 
      CLI_Write("Done!\r\n>> ");
  }
  
  // 啟動 UART 中斷接收
  HAL_UART_Receive_IT(&huart2, &rx_data, 1);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
      /* 處理終端機指令 (非阻塞) */
      CLI_Update(&uart_rx_buffer);
  }
  /* USER CODE END WHILE */

  /* USER CODE BEGIN 3 */
  /* USER CODE END 3 */
}

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

/* USER CODE BEGIN 4 */
/* ========================================================================= */
/*                          UART & CLI 底層介面                                */
/* ========================================================================= */

/**
 * @brief  將字串透過 UART 發送到終端機
 * @note   封裝了 HAL 函式，供 CLI 模組呼叫
 */
void CLI_Write(const char *str) {
    HAL_UART_Transmit(&huart2, (uint8_t *)str, strlen(str), HAL_MAX_DELAY);
}

/**
 * @brief  UART 接收完成中斷回呼函式
 * @note   每次收到 1 Byte 就放入 Ring Buffer，並重新開啟中斷接收
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART2) {
        RingBuffer_Put(&uart_rx_buffer, rx_data);
        HAL_UART_Receive_IT(&huart2, &rx_data, 1); 
    }
}

/* ========================================================================= */
/*                           SPI 裸機驅動與 W25Q64 控制                         */
/* ========================================================================= */
uint32_t flash_record_count = 0; // 全域變數：紀錄目前 Flash 已經存了幾筆溫度

/**
 * @brief  初始化 SPI1 與 GPIO 腳位
 * @note   - PA5(CLK), PA6(MISO), PA7(MOSI) 設為硬體 AF5
 *         - PB6(D10) 設為一般推挽輸出，作為軟體控制的 CS 腳位
 *         - 速度降為 2MHz 確保杜邦線傳輸穩定
 */
void SPI1_Init_BareMetal(void) {
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN;
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN; 

    GPIOA->MODER &= ~(GPIO_MODER_MODER5 | GPIO_MODER_MODER6 | GPIO_MODER_MODER7);
    GPIOA->MODER |= (GPIO_MODER_MODER5_1 | GPIO_MODER_MODER6_1 | GPIO_MODER_MODER7_1);
    GPIOA->OSPEEDR |= (GPIO_OSPEEDER_OSPEEDR5 | GPIO_OSPEEDER_OSPEEDR6 | GPIO_OSPEEDER_OSPEEDR7);
    GPIOA->AFR[0] &= ~((0xF << 20) | (0xF << 24) | (0xF << 28));
    GPIOA->AFR[0] |= ((5 << 20) | (5 << 24) | (5 << 28));

    GPIOB->MODER &= ~GPIO_MODER_MODER6;
    GPIOB->MODER |= GPIO_MODER_MODER6_0;
    GPIOB->OSPEEDR |= GPIO_OSPEEDER_OSPEEDR6; 
    GPIOB->BSRR = GPIO_BSRR_BS_6; 

    SPI1->CR1 = 0; 
    SPI1->CR1 |= (2 << 3); 
    SPI1->CR1 |= SPI_CR1_SSM | SPI_CR1_SSI;
    SPI1->CR1 |= SPI_CR1_MSTR;
    SPI1->CR1 |= SPI_CR1_SPE;
}

/**
 * @brief  SPI 全雙工收發 1 Byte
 * @note   將資料送進暫存器，並等待硬體時鐘交換完畢後，回傳收到的資料
 */
uint8_t SPI1_TxRxByte(uint8_t tx_data) {
    while (!(SPI1->SR & SPI_SR_TXE));
    SPI1->DR = tx_data;
    while (!(SPI1->SR & SPI_SR_RXNE));
    return SPI1->DR;
}

/**
 * @brief  讀取 W25Q64 晶片身分證 (JEDEC ID)
 * @note   用於開機時測試 SPI 接線是否正常，預期應印出 EF 40 17
 */
void W25Q_Read_JEDEC_ID(void) {
    uint8_t mfg_id, mem_type, capacity;
    GPIOB->BSRR = GPIO_BSRR_BR_6; 
    SPI1_TxRxByte(0x9F);
    mfg_id = SPI1_TxRxByte(0xFF);   
    mem_type = SPI1_TxRxByte(0xFF); 
    capacity = SPI1_TxRxByte(0xFF); 
    GPIOB->BSRR = GPIO_BSRR_BS_6;
    
    char str[64];
    snprintf(str, sizeof(str), "\r\n[SPI Test] W25Q64 ID: %02X %02X %02X\r\n", mfg_id, mem_type, capacity);
    CLI_Write(str);
}

/**
 * @brief  Flash 寫入解鎖
 * @note   執行擦除或寫入資料前，必須先發送此指令 (0x06)
 */
void W25Q_WriteEnable(void) {
    GPIOB->BSRR = GPIO_BSRR_BR_6; 
    SPI1_TxRxByte(0x06);          
    GPIOB->BSRR = GPIO_BSRR_BS_6; 
}

/**
 * @brief  等待 Flash 內部操作完成
 * @note   不斷讀取 SR1 (0x05) 暫存器，直到 WIP(Write In Progress) bit 降為 0
 */
void W25Q_WaitForReady(void) {
    GPIOB->BSRR = GPIO_BSRR_BR_6;
    SPI1_TxRxByte(0x05); 
    while ((SPI1_TxRxByte(0xFF) & 0x01) == 0x01); 
    GPIOB->BSRR = GPIO_BSRR_BS_6;
}

/**
 * @brief  解除晶片防寫保護 (Write Status Register)
 * @note   將狀態暫存器清零，解除可能存在的 Block Protect
 */
void W25Q_Unprotect(void) {
    W25Q_WriteEnable();
    GPIOB->BSRR = GPIO_BSRR_BR_6;
    SPI1_TxRxByte(0x01); // 寫入狀態暫存器指令
    SPI1_TxRxByte(0x00); // 寫入 SR1 (清除所有保護)
    SPI1_TxRxByte(0x00); // 寫入 SR2
    GPIOB->BSRR = GPIO_BSRR_BS_6;
    W25Q_WaitForReady();
}

/**
 * @brief  擦除指定的 4KB 磁區 (Sector Erase)
 * @note   Flash 的物理特性必須先擦除為 0xFF 才能寫入新資料
 */
void W25Q_SectorErase(uint32_t sector_addr) {
    W25Q_WriteEnable(); 
    GPIOB->BSRR = GPIO_BSRR_BR_6;
    SPI1_TxRxByte(0x20); 
    SPI1_TxRxByte((sector_addr >> 16) & 0xFF);
    SPI1_TxRxByte((sector_addr >> 8) & 0xFF);
    SPI1_TxRxByte(sector_addr & 0xFF);
    GPIOB->BSRR = GPIO_BSRR_BS_6;
    W25Q_WaitForReady(); 
}

/**
 * @brief  將 32-bit 資料寫入 Flash
 * @note   將 4 Bytes 的資料依序寫入指定位址 (Page Program)
 */
void W25Q_WriteInt32(uint32_t addr, int32_t data) {
    W25Q_WriteEnable(); 
    GPIOB->BSRR = GPIO_BSRR_BR_6;
    SPI1_TxRxByte(0x02); 
    SPI1_TxRxByte((addr >> 16) & 0xFF);
    SPI1_TxRxByte((addr >> 8) & 0xFF);
    SPI1_TxRxByte(addr & 0xFF);
    
    SPI1_TxRxByte((data >> 24) & 0xFF);
    SPI1_TxRxByte((data >> 16) & 0xFF);
    SPI1_TxRxByte((data >> 8) & 0xFF);
    SPI1_TxRxByte(data & 0xFF);
    GPIOB->BSRR = GPIO_BSRR_BS_6;
    W25Q_WaitForReady(); 
}

/**
 * @brief  從 Flash 讀取 32-bit 資料
 */
int32_t W25Q_ReadInt32(uint32_t addr) {
    int32_t data = 0;
    GPIOB->BSRR = GPIO_BSRR_BR_6;
    SPI1_TxRxByte(0x03); 
    SPI1_TxRxByte((addr >> 16) & 0xFF);
    SPI1_TxRxByte((addr >> 8) & 0xFF);
    SPI1_TxRxByte(addr & 0xFF);
    
    // 強制轉型，防止 0xFF 位移造成符號異常擴展
    data |= ((uint32_t)SPI1_TxRxByte(0xFF) << 24);
    data |= ((uint32_t)SPI1_TxRxByte(0xFF) << 16);
    data |= ((uint32_t)SPI1_TxRxByte(0xFF) << 8);
    data |= ((uint32_t)SPI1_TxRxByte(0xFF));
    GPIOB->BSRR = GPIO_BSRR_BS_6;
    return data;
}

/**
 * @brief  開機 SPI 傳輸與寫入完整性測試
 * @note   在最後一個 Sector (0x7FF000) 寫入測試特徵碼並讀回驗證
 */
bool W25Q_SelfTest(void) {
    const uint32_t test_addr = 0x7FF000;
    const int32_t test_pattern = 0x55AA1234;
    
    W25Q_SectorErase(test_addr);
    W25Q_WriteInt32(test_addr, test_pattern);
    int32_t read_back = W25Q_ReadInt32(test_addr);
    
    return (read_back == test_pattern);
}


/* ========================================================================= */
/*                          應用層：讀取並寫入動作介面                           */
/* ========================================================================= */

/**
 * @brief  讀取 LM75 溫度，並寫入 Flash 記憶體
 * @note   由 CLI 的 "read" 指令直接觸發，包含常識濾波器以防干擾資料存入
 */
void Action_Read_And_Save(void) {
    int32_t temp_x100;
    if (CLI_PortReadTempX100(&temp_x100)) {
        if (temp_x100 >= 500 && temp_x100 <= 6000) {
            uint32_t flash_addr = flash_record_count * 4; 
            
            W25Q_WriteInt32(flash_addr, temp_x100); 
            
            // 【即時讀回校驗機制】確保資料真的寫進 Flash
            int32_t verify_data = W25Q_ReadInt32(flash_addr);
            if (verify_data != temp_x100) {
                char err_str[128];
                snprintf(err_str, sizeof(err_str), "\r\n[Error] Flash Write Failed! Addr:0x%04lX, Wrote:%ld, Read:%ld\r\n>> ", flash_addr, temp_x100, verify_data);
                CLI_Write(err_str);
                return; // 校驗失敗，終止函數，不增加紀錄筆數
            }
            
            flash_record_count++;                   
            
            char log_str[64];
            snprintf(log_str, sizeof(log_str), "[Save OK] %ld.%02ld C saved to Addr 0x%04lX\r\n", 
                     temp_x100 / 100, temp_x100 % 100, flash_addr);
            CLI_Write(log_str);
        } else {
             CLI_Write("[Error] Bad temp data, discarded.\r\n");
        }
    } else {
        CLI_Write("[Error] LM75 Read Failed!\r\n");
    }
}

/**
 * @brief  取得 Flash 中已存儲的資料筆數 (提供給 CLI stat/dump 調用)
 */
uint32_t CLI_PortLogCount(void) { 
    return flash_record_count; 
}

/**
 * @brief  根據索引位置 (0, 1, 2...) 從 Flash 讀出該筆資料 (提供給 CLI dump 調用)
 */
bool CLI_PortLogRead(uint32_t idx, CLI_LogRecord_t *rec) { 
    if (idx >= flash_record_count) return false;
    rec->temp_x100 = W25Q_ReadInt32(idx * 4); 
    return true; 
}


/* ========================================================================= */
/*                           I2C 裸機驅動與錯誤處理                             */
/* ========================================================================= */

/**
 * @brief  初始化 I2C1 硬體引擎
 * @note   PB8(SCL), PB9(SDA) 設為開汲極 (Open-Drain) 與內部上拉
 */
void I2C1_Init_BareMetal(void) {
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN; 
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;  

    GPIOB->MODER &= ~(GPIO_MODER_MODER8 | GPIO_MODER_MODER9);
    GPIOB->MODER |= (GPIO_MODER_MODER8_1 | GPIO_MODER_MODER9_1);
    GPIOB->OTYPER |= (GPIO_OTYPER_OT8 | GPIO_OTYPER_OT9);
    GPIOB->OSPEEDR |= (GPIO_OSPEEDER_OSPEEDR8 | GPIO_OSPEEDER_OSPEEDR9);
    GPIOB->PUPDR &= ~(GPIO_PUPDR_PUPDR8 | GPIO_PUPDR_PUPDR9);
    GPIOB->PUPDR |= (GPIO_PUPDR_PUPDR8_0 | GPIO_PUPDR_PUPDR9_0);
    GPIOB->AFR[1] &= ~((0xF << 0) | (0xF << 4)); 
    GPIOB->AFR[1] |= ((4 << 0) | (4 << 4));     

    I2C1->CR1 |= I2C_CR1_SWRST;
    I2C1->CR1 &= ~I2C_CR1_SWRST;
    I2C1->CR2 &= ~I2C_CR2_FREQ;
    I2C1->CR2 |= 16; 
    I2C1->CCR &= ~I2C_CCR_CCR;
    I2C1->CCR |= 80;
    I2C1->TRISE = 17;
    I2C1->CR1 |= I2C_CR1_PE; 
}

/**
 * @brief  匯流排救援機制 (Bus Recovery)
 * @note   當 Slave 卡住 SDA 線時，手動切換 PB8 為 GPIO 打出 9 個 Clock 逼迫其放開
 */
void I2C1_RecoverBus(void) {
    I2C1->CR1 &= ~I2C_CR1_PE; 
    GPIOB->MODER &= ~GPIO_MODER_MODER8;
    GPIOB->MODER |= GPIO_MODER_MODER8_0;
    
    for(int i = 0; i < 9; i++) {
        GPIOB->ODR |= (1 << 8);  HAL_Delay(1);            
        GPIOB->ODR &= ~(1 << 8); HAL_Delay(1);            
    }
    I2C1_Init_BareMetal();
}

/**
 * @brief  I2C 超時重置處理
 * @note   清除硬體狀態機，若仍處於 Busy 狀態則觸發 Bus Recovery
 */
void I2C1_ResetOnError(void) {
    __enable_irq(); 
    I2C1->CR1 |= I2C_CR1_SWRST;
    for(volatile int i=0; i<1000; i++); 
    I2C1->CR1 &= ~I2C_CR1_SWRST;
    
    if (I2C1->SR2 & I2C_SR2_BUSY) { I2C1_RecoverBus(); } 
    else { I2C1_Init_BareMetal(); }
}

/**
 * @brief  讀取 LM75 溫度
 * @note   具備動態位址掃描功能 (0x48~0x4F)，以克服硬體腳位懸空造成的位址飄移
 */
bool CLI_PortReadTempX100(int32_t *out_temp) {
    uint8_t data[2];
    uint32_t timeout;
    const uint32_t MAX_WAIT = 100000;
    uint8_t target_addr = 0;

    /* 1. 動態掃描飄移的硬體位址 */
    for (uint8_t addr = 0x48; addr <= 0x4F; addr++) {
        timeout = MAX_WAIT;
        while(I2C1->SR2 & I2C_SR2_BUSY) { if (--timeout == 0) { I2C1_ResetOnError(); return false; } }

        I2C1->CR1 |= I2C_CR1_ACK | I2C_CR1_START;
        timeout = MAX_WAIT;
        while(!(I2C1->SR1 & I2C_SR1_SB)) { if (--timeout == 0) { I2C1_ResetOnError(); return false; } }

        I2C1->DR = (addr << 1) | 1;

        timeout = 5000; 
        bool ack_received = true;
        while(!(I2C1->SR1 & I2C_SR1_ADDR)) {
            if (I2C1->SR1 & I2C_SR1_AF) { 
                I2C1->SR1 &= ~I2C_SR1_AF; 
                ack_received = false;
                break;
            }
            if (--timeout == 0) { ack_received = false; break; }
        }

        if (ack_received) {
            target_addr = addr;
            break; 
        } else {
            I2C1->CR1 |= I2C_CR1_STOP; 
        }
    }

    if (target_addr == 0) { return false; }

    /* 2. 開始讀取 2 Bytes 的溫度資料 */
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
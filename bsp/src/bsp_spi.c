/**
  ******************************************************************************
  * @file    bsp_spi.c
  * @brief   Bare-metal SPI1 Driver Implementation
  ******************************************************************************
  */
#include "bsp_spi.h"

/**
 * @brief  初始化 SPI1 與 GPIO 腳位
 * @note   - PA5(CLK), PA6(MISO), PA7(MOSI) 設為硬體 AF5
 *         - PB6(D10) 設為一般推挽輸出，作為軟體控制的 CS 腳位
 *         - 速度降為 2MHz 確保杜邦線傳輸穩定
 */
void SPI1_Init_BareMetal(void) {
    // 1. 開啟 GPIOA, GPIOB 與 SPI1 的時鐘
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN;
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN; 

    // 2. 設定 PA5, PA6, PA7 為 Alternate Function (AF5)
    GPIOA->MODER &= ~(GPIO_MODER_MODER5 | GPIO_MODER_MODER6 | GPIO_MODER_MODER7);
    GPIOA->MODER |= (GPIO_MODER_MODER5_1 | GPIO_MODER_MODER6_1 | GPIO_MODER_MODER7_1);
    GPIOA->OSPEEDR |= (GPIO_OSPEEDER_OSPEEDR5 | GPIO_OSPEEDER_OSPEEDR6 | GPIO_OSPEEDER_OSPEEDR7);
    GPIOA->AFR[0] &= ~((0xF << 20) | (0xF << 24) | (0xF << 28));
    GPIOA->AFR[0] |= ((5 << 20) | (5 << 24) | (5 << 28));

    // 3. 設定 PB6 為 Output (作為 Software CS 腳)
    GPIOB->MODER &= ~GPIO_MODER_MODER6;
    GPIOB->MODER |= GPIO_MODER_MODER6_0;
    GPIOB->OSPEEDR |= GPIO_OSPEEDER_OSPEEDR6; 
    GPIOB->BSRR = GPIO_BSRR_BS_6; // 預設拉高 (取消選取)

    // 4. 設定 SPI1 控制暫存器
    SPI1->CR1 = 0; 
    SPI1->CR1 |= (2 << 3);              // BR[2:0] = 010 (fPCLK/8 = 2MHz)
    SPI1->CR1 |= SPI_CR1_SSM | SPI_CR1_SSI; // 軟體 NSS 管理
    SPI1->CR1 |= SPI_CR1_MSTR;          // Master 模式
    SPI1->CR1 |= SPI_CR1_SPE;           // 啟動 SPI
}

/**
 * @brief  SPI 全雙工收發 1 Byte
 * @note   將資料送進暫存器，並等待硬體時鐘交換完畢後，回傳收到的資料
 */
uint8_t SPI1_TxRxByte(uint8_t tx_data) {
    // 等待發送緩衝區淨空 (TXE = 1)
    while (!(SPI1->SR & SPI_SR_TXE));
    
    // 寫入資料以啟動時鐘傳輸
    SPI1->DR = tx_data;
    
    // 等待接收緩衝區收到資料 (RXNE = 1)
    while (!(SPI1->SR & SPI_SR_RXNE));
    
    // 讀取並回傳交換回來的資料
    return SPI1->DR;
}
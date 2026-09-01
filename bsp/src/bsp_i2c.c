/**
  ******************************************************************************
  * @file    bsp_i2c.c
  * @brief   Bare-metal I2C1 Driver Implementation
  ******************************************************************************
  */
#include "bsp_i2c.h"

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
        GPIOB->ODR |= (1 << 8);  HAL_Delay(1); // 註: 這裡暫時保留 HAL_Delay，未來手刻 SysTick 後再換掉
        GPIOB->ODR &= ~(1 << 8); HAL_Delay(1);            
    }
    I2C1_Init_BareMetal();
}

/**
 * @brief  I2C 超時重置處理
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
 * @brief  I2C 通用陣列寫入函數
 * @param  dev_addr: 設備的 8-bit I2C 位址 (已包含 R/W bit = 0)
 * @param  pData: 要發送的資料陣列指標
 * @param  len: 要發送的資料長度
 * @retval true: 寫入成功, false: 寫入失敗 (NACK 或 卡死)
 */
bool I2C1_WriteBuffer(uint8_t dev_addr, uint8_t *pData, uint16_t len) {
    uint32_t timeout = 100000;
    
    // 1. 確認匯流排空閒
    while(I2C1->SR2 & I2C_SR2_BUSY) { 
        if (--timeout == 0) { I2C1_ResetOnError(); return false; } 
    }

    // 2. 發送 START 條件
    I2C1->CR1 |= I2C_CR1_START;
    timeout = 100000;
    while(!(I2C1->SR1 & I2C_SR1_SB)) { if (--timeout == 0) return false; }

    // 3. 發送設備位址 (寫入模式)
    I2C1->DR = dev_addr;
    timeout = 5000;
    while(!(I2C1->SR1 & I2C_SR1_ADDR)) {
        if (I2C1->SR1 & I2C_SR1_AF) { 
            I2C1->SR1 &= ~I2C_SR1_AF;  // 清除 NACK 旗標
            I2C1->CR1 |= I2C_CR1_STOP; // 發送 STOP
            return false;
        }
        if (--timeout == 0) return false;
    }
    
    // 清除 ADDR 旗標 (讀取 SR1 再讀取 SR2)
    (void)I2C1->SR1;
    (void)I2C1->SR2;

    // 4. 連續發送資料陣列
    for (uint16_t i = 0; i < len; i++) {
        timeout = 100000;
        while(!(I2C1->SR1 & I2C_SR1_TXE)) { if (--timeout == 0) return false; }
        I2C1->DR = pData[i];
    }

    // 5. 等待最後一個 Byte 傳輸完成 (BTF = 1)
    timeout = 100000;
    while(!(I2C1->SR1 & I2C_SR1_BTF)) { if (--timeout == 0) return false; }

    // 6. 發送 STOP 條件
    I2C1->CR1 |= I2C_CR1_STOP;
    return true;
}
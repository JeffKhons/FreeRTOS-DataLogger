/**
  ******************************************************************************
  * @file    bsp_lm75.c
  * @brief   LM75 Temperature Sensor Driver Implementation
  ******************************************************************************
  */
#include "bsp_lm75.h"
#include "bsp_i2c.h"    // 需要呼叫 I2C1_ResetOnError 等底層救援函式
#include "stm32f4xx.h"  // 需要操作 I2C1 暫存器

/**
 * @brief  讀取 LM75 溫度
 * @note   具備動態位址掃描功能 (0x48~0x4F)，以克服硬體腳位懸空造成的位址飄移
 * @param  out_temp: 用來儲存放大 100 倍的溫度結果 (例如 25.50度 -> 2550)
 * @retval true: 讀取成功, false: 讀取失敗 (無回應或匯流排卡死)
 */
bool LM75_ReadTemp(int32_t *out_temp) {
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
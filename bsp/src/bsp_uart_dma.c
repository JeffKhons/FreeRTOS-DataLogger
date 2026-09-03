/**
  * @file    bsp_uart_dma.c
  * @brief   Bare-metal USART2 + DMA RX (Idle Line Interrupt)
  */
#include "stm32f4xx.h"

// 接收緩衝區 (供 DMA 直接寫入)
#define RX_BUFFER_SIZE 128
uint8_t dma_rx_buffer[RX_BUFFER_SIZE];

void UART2_DMA_Init_BareMetal(void) {
    // 1. 開啟 GPIOA, USART2 與 DMA1 時鐘
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_DMA1EN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

    // 2. 設定 PA2 (TX) 與 PA3 (RX) 為 Alternate Function (AF7)
    GPIOA->MODER &= ~(GPIO_MODER_MODER2 | GPIO_MODER_MODER3);
    GPIOA->MODER |= (GPIO_MODER_MODER2_1 | GPIO_MODER_MODER3_1);
    GPIOA->AFR[0] &= ~((0xF << 8) | (0xF << 12));
    GPIOA->AFR[0] |= ((7 << 8) | (7 << 12));

    // 3. 設定 USART2 (115200 Baud, 8N1)
    // APB1 時脈為 42MHz -> 42,000,000 / (16 * 115200) = 22.786
    // Mantissa = 22 (0x16), Fraction = 0.786 * 16 = 12.5 (取 13 = 0xD) -> BRR = 0x016D
    USART2->BRR = 0x016D;
    
    // 開啟 TX, RX, 接收 DMA (DMAR), 與 Idle Line 中斷
    USART2->CR3 |= USART_CR3_DMAR;
    USART2->CR1 |= USART_CR1_TE | USART_CR1_RE | USART_CR1_IDLEIE | USART_CR1_UE;

    // 4. 設定 DMA1 Stream 5 (Channel 4 對應 USART2_RX)
    DMA1_Stream5->CR = 0; // 先關閉 DMA 才能設定
    while(DMA1_Stream5->CR & DMA_SxCR_EN);
    
    // Channel 4, P2M 模式, 記憶體遞增 (MINC), Circular 模式, 優先權高 (PL=10)
    DMA1_Stream5->CR |= (4ul << 25) | DMA_SxCR_MINC | DMA_SxCR_CIRC | (2ul << 16);
    
    // 設定位址與傳輸數量
    DMA1_Stream5->PAR  = (uint32_t)&USART2->DR;
    DMA1_Stream5->M0AR = (uint32_t)dma_rx_buffer;
    DMA1_Stream5->NDTR = RX_BUFFER_SIZE;
    
    // 啟動 DMA
    DMA1_Stream5->CR |= DMA_SxCR_EN;

    // 5. 開啟 USART2 中斷 (交給 FreeRTOS 處理)
    NVIC_SetPriority(USART2_IRQn, 5); // 必須大於 configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY
    NVIC_EnableIRQ(USART2_IRQn);
}
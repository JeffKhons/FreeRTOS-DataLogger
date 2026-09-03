/**
  ******************************************************************************
  * @file    stm32f4xx_it.c
  * @brief   純 Bare-metal 中斷服務常式 (ISR) 集中管理
  ******************************************************************************
  */

#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "ring_buffer.h"

/* ========================================================================= */
/* 外部變數宣告區 (Extern Variables)                                         */
/* ========================================================================= */
extern uint8_t dma_rx_buffer[];     // 來自 bsp_uart_dma.c (DMA 接收緩衝)
extern RingBuffer_t uart_rx_buffer; // 來自 main.c (軟體環形緩衝)
extern TaskHandle_t xCLITaskHandle; // 來自 main.c (CLI 任務句柄)


/* ========================================================================= */
/* 核心系統異常處理 (Cortex-M4 System Exceptions)                            */
/* ========================================================================= */
void NMI_Handler(void)        { while (1) {} }
void HardFault_Handler(void)  { while (1) {} }
void MemManage_Handler(void)  { while (1) {} }
void BusFault_Handler(void)   { while (1) {} }
void UsageFault_Handler(void) { while (1) {} }
void DebugMon_Handler(void)   {}

// 注意：SVC_Handler, PendSV_Handler, SysTick_Handler 
// 已經在 FreeRTOSConfig.h 中透過巨集映射給 FreeRTOS 接管，此處不可重複定義！


/* ========================================================================= */
/* 硬體周邊中斷處理 (Peripheral ISRs)                                        */
/* ========================================================================= */

/**
  * @brief  USART2 全域中斷 (結合 DMA 與 Idle Line 實作無鎖接收)
  */
void USART2_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    // 檢查是否為 IDLE (線路閒置) 中斷，代表一整句指令接收完畢
    if (USART2->SR & USART_SR_IDLE) {
        
        // 1. 清除 IDLE 旗標：先讀取 SR，再讀取 DR (暫存器硬體規定)
        volatile uint32_t tmp = USART2->SR;
        tmp = USART2->DR;
        (void)tmp;

        // 2. 暫停 DMA 以讀取剩餘計數
        DMA1_Stream5->CR &= ~DMA_SxCR_EN; // 關閉 DMA
        
        // 3. 計算這次實際收到了幾個 Bytes (總長度 - 剩餘次數)
        uint16_t rx_len = 128 - DMA1_Stream5->NDTR; 

        // 4. 將 DMA 緩衝區的資料一口氣推入軟體 Ring Buffer
        for(uint16_t i = 0; i < rx_len; i++) {
            RingBuffer_Put(&uart_rx_buffer, dma_rx_buffer[i]);
        }

        // 5. 發送任務通知 (Task Notification)，喚醒 CLI_Task 進行解析
        if (xCLITaskHandle != NULL) {
            vTaskNotifyGiveFromISR(xCLITaskHandle, &xHigherPriorityTaskWoken);
        }

        // 6. 歸零 DMA 傳輸數量，並重新啟動 DMA 準備接下一句話
        DMA1_Stream5->NDTR = 128;
        DMA1_Stream5->CR |= DMA_SxCR_EN;
    }

    // FreeRTOS ISR 標準結尾：若有更高優先級任務被喚醒，則立即觸發 Context Switch
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
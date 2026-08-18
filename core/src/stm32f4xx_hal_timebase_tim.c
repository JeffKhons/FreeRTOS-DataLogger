#include "stm32f4xx_hal.h"

/* 宣告 TIM6 的 Handle */
TIM_HandleTypeDef htim6;

/* 覆寫弱函式：將 HAL 的 1ms Tick 來源改為 TIM6 */
HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority)
{
    RCC_ClkInitTypeDef    clkconfig;
    uint32_t              uwTimclock = 0;
    uint32_t              uwPrescalerValue = 0;
    uint32_t              pFLatency;

    /* 啟用 TIM6 時脈 */
    __HAL_RCC_TIM6_CLK_ENABLE();

    /* 取得系統時鐘配置，以計算 APB1 的時脈頻率 */
    HAL_RCC_GetClockConfig(&clkconfig, &pFLatency);

    /* 計算 TIM6 時脈 (TIM6 掛載於 APB1) */
    uwTimclock = HAL_RCC_GetPCLK1Freq();
    
    /* 根據 STM32 的硬體設計，若 APB1 除頻不為 1，Timer 時脈會自動乘 2 */
    if (clkconfig.APB1CLKDivider != RCC_HCLK_DIV1) {
        uwTimclock *= 2; 
    }

    /* 計算 Prescaler，將計數器頻率降為 1MHz (即 1 微秒計數一次) */
    uwPrescalerValue = (uint32_t) ((uwTimclock / 1000000U) - 1U);

    /* 初始化 TIM6 */
    htim6.Instance = TIM6;
    /* 1ms 週期 = 1000 * 1 微秒 */
    htim6.Init.Period = (1000000U / 1000U) - 1U; 
    htim6.Init.Prescaler = uwPrescalerValue;
    htim6.Init.ClockDivision = 0;
    htim6.Init.CounterMode = TIM_COUNTERMODE_UP;

    if (HAL_TIM_Base_Init(&htim6) == HAL_OK) {
        /* 設定 TIM6 的 NVIC 優先權並啟用中斷 (使用傳入的 TickPriority) */
        HAL_NVIC_SetPriority(TIM6_DAC_IRQn, TickPriority, 0);
        HAL_NVIC_EnableIRQ(TIM6_DAC_IRQn);
        
        /* 啟動 TIM6 與其更新中斷 */
        return HAL_TIM_Base_Start_IT(&htim6);
    }
    
    return HAL_ERROR;
}

/* 暫停 HAL Tick 中斷 (進入深度睡眠前呼叫) */
void HAL_SuspendTick(void)
{
    __HAL_TIM_DISABLE_IT(&htim6, TIM_IT_UPDATE);
}

/* 恢復 HAL Tick 中斷 (喚醒後呼叫) */
void HAL_ResumeTick(void)
{
    __HAL_TIM_ENABLE_IT(&htim6, TIM_IT_UPDATE);
}
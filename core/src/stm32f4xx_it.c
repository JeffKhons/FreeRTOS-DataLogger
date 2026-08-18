/*集中管理所有 Cortex-M 核心與周邊硬體的中斷服務常式 (ISR)。*/
#include "stm32f4xx_hal.h"
#include "stm32f4xx_it.h"

/* 引用外部在 stm32f4xx_hal_timebase_tim.c 定義的 TIM6 Handle */
extern TIM_HandleTypeDef htim6;

/* TIM6 與 DAC 共用的中斷服務常式 */
void TIM6_DAC_IRQHandler(void)
{
    /* 將中斷交給 HAL 庫的共用 Timer 處理函式 */
    HAL_TIM_IRQHandler(&htim6);
}

/* 當硬體 Timer 週期到達時，HAL_TIM_IRQHandler 會自動呼叫這個 Callback */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    /* 確認是 TIM6 觸發的 1ms 中斷 */
    if (htim->Instance == TIM6) {
        /* 增加 HAL 庫的系統時間 (讓 HAL_Delay 運作) */
        HAL_IncTick();
    }
}
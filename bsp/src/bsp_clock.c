/**
  * @file    bsp_clock.c
  * @brief   Bare-metal System Clock Configuration (84MHz)
  */
#include "stm32f4xx.h"

void SystemClock_Config_BareMetal(void) {
    // 1. 開啟電源時鐘並設定電壓調節器為 Scale 3 (最高效能)
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    PWR->CR |= PWR_CR_VOS_0 | PWR_CR_VOS_1;

    // 2. 設定 Flash 讀取延遲 (84MHz 時需 2 Wait States)
    FLASH->ACR = FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN | FLASH_ACR_LATENCY_2WS;

    // 3. 設定 PLL 參數: HSI(16MHz) / M(16) * N(168) / P(2) = 84MHz
    RCC->PLLCFGR = (16ul << 0)         // PLLM = 16
                 | (168ul << 6)        // PLLN = 168
                 | (0ul << 16)         // PLLP = 2 (00 = 2)
                 | RCC_PLLCFGR_PLLSRC_HSI
                 | (7ul << 24);        // PLLQ = 7

    // 4. 啟動 PLL 並等待穩定
    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY));

    // 5. 設定匯流排分頻 (AHB = /1, APB1 = /2 (42MHz), APB2 = /1 (84MHz))
    RCC->CFGR |= RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE1_DIV2 | RCC_CFGR_PPRE2_DIV1;

    // 6. 切換系統時脈來源為 PLL，並等待切換完成
    RCC->CFGR |= RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);

    // 7. 更新全域變數 SystemCoreClock (供 FreeRTOS 參考)
    SystemCoreClock = 84000000;
}
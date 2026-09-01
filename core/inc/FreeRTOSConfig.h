#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* 引入 STM32 HAL 庫，為了取得 SystemCoreClock 這個全域變數 */
#include "stm32f4xx_hal.h"
extern uint32_t SystemCoreClock;

/* ========================================================================= */
/* 1. 排程器 (Scheduler) 核心設定                                            */
/* ========================================================================= */
// 1: 開啟搶占式排程 (Preemptive)。高優先權 Task Ready 時，會立刻搶走 CPU。
// 0: 合作式排程 (Cooperative)。Task 必須主動呼叫 vTaskYield() 才會交出 CPU。
#define configUSE_PREEMPTION                    1

// 系統心跳頻率 (Tick Rate)。設定 1000 代表 1 秒鐘觸發 1000 次 SysTick 中斷 (即 1ms 解析度)
#define configTICK_RATE_HZ                      ((TickType_t)1000)

// 告訴 OS 你的 CPU 跑多快。FreeRTOS 需要這個數值來計算 Timer 的硬體參數
#define configCPU_CLOCK_HZ                      (SystemCoreClock)

// 系統中允許的最大優先權數量。設定為 7，代表優先權範圍是 0 (最低) ~ 6 (最高)
// 數字越大，RAM 消耗稍微多一點點。
#define configMAX_PRIORITIES                    ( 7 )

// 每個 Task 最小的 Stack 大小。單位是 "Word" (在 32-bit MCU 上，1 Word = 4 Bytes)。
// 128 * 4 = 512 Bytes。這是給完全沒做什麼事的 Task 用的底線。
#define configMINIMAL_STACK_SIZE                ((uint16_t)128)

// Task 名稱的最大長度 (包含結尾的 \0 字元)
#define configMAX_TASK_NAME_LEN                 ( 16 )

// 1: 在 Idle Task (系統閒置時執行的任務) 中，若有相同優先權的 Task Ready，主動讓出 CPU
#define configIDLE_SHOULD_YIELD                 1

// 1: 使用 16-bit 紀錄 Tick 數量 (適用於 8-bit/16-bit MCU，可省 RAM)。
// 0: 使用 32-bit 紀錄 (STM32 是 32-bit MCU，務必設為 0)
#define configUSE_16_BIT_TICKS                  0

// 0: 關閉 Tickless Idle (進階省電模式，目前開發階段先關閉)
#define configUSE_TICKLESS_IDLE                 0

// 0: 不使用硬體最佳化的任務選擇 (STM32F4 可設為 1 利用 CLZ 指令加速，但 0 最通用)
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0

/* ========================================================================= */
/* 2. 記憶體管理 (Memory Management) 與 Hook 函式設定                        */
/* ========================================================================= */
// 1: 啟用動態記憶體分配 (我們使用的 heap_4.c 需要這個)
#define configSUPPORT_DYNAMIC_ALLOCATION        1

// 0: 關閉靜態記憶體分配 (初學者建議先用動態，比較單純)
#define configSUPPORT_STATIC_ALLOCATION         0

// 分配給作業系統的總 RAM 大小 (Heap Size)。
// 你的 STM32F446RE 有 128KB 的 SRAM，這裡先割出 15KB 專門給 FreeRTOS 的 Task/Queue 申請使用
#define configTOTAL_HEAP_SIZE                   ((size_t)(15 * 1024))

// 0: 關閉 Idle Hook (不使用閒置時的自訂回呼函式)
#define configUSE_IDLE_HOOK                     0

// 0: 關閉 Tick Hook (不使用每次 Tick 中斷時的自訂回呼函式)
#define configUSE_TICK_HOOK                     0

// 0: 不檢查 Stack Overflow (開發後期或 Debug 時可設為 1 或 2 來抓 Bug)
#define configCHECK_FOR_STACK_OVERFLOW          0

// 0: 關閉記憶體分配失敗時的 Hook 函式
#define configUSE_MALLOC_FAILED_HOOK            0

/* ========================================================================= */
/* 3. 同步機制與進階功能 (IPC & Advanced)                                    */
/* ========================================================================= */
// 1: 啟用 Mutex (互斥鎖)。你在 Lab 3 (Priority Inversion) 會非常需要它！
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             1

// 1: 啟用 Counting Semaphores (計數型號誌)
#define configUSE_COUNTING_SEMAPHORES           1

// 1: 啟用 Task Notifications (比 Semaphore 更輕量、更快的任務通訊方式)
#define configUSE_TASK_NOTIFICATIONS            1

// 1: 啟用 Software Timers (軟體定時器)。你在 README 規劃的 Alert_Manager 會用到
#define configUSE_TIMERS                        1
#define configTIMER_TASK_PRIORITY               ( 3 ) // Timer 守護任務的優先權
#define configTIMER_QUEUE_LENGTH                10    // Timer 指令的 Queue 長度
#define configTIMER_TASK_STACK_DEPTH            configMINIMAL_STACK_SIZE

/* ========================================================================= */
/* 4. API 函數開關 (INCLUDE Functions)                                       */
/* ========================================================================= */
// 為了節省 Flash ROM 空間，FreeRTOS 允許你把沒用到的 API 剔除不編譯。
// 1 代表將該函數編譯進專案中，0 代表不編譯。
#define INCLUDE_vTaskPrioritySet                1  // 設定優先權 API
#define INCLUDE_uxTaskPriorityGet               1  // 取得優先權 API
#define INCLUDE_vTaskDelete                     1  // 刪除任務 API
#define INCLUDE_vTaskSuspend                    1  // 暫停任務 API
#define INCLUDE_vTaskDelayUntil                 1  // 絕對延遲 API (Sensor 週期讀取必備)
#define INCLUDE_vTaskDelay                      1  // 相對延遲 API

/* ========================================================================= */
/* 5. 核心中斷綁定與 NVIC 優先權設定 (Cortex-M 移植關鍵) 🌟🌟🌟               */
/* ========================================================================= */
// SVC (Supervisor Call): 用於啟動第一個 Task 時，切換處理器特權模式。
#define vPortSVCHandler    SVC_Handler

// PendSV (Pendable Service): FreeRTOS 進行 Context Switch (任務切換) 的專用中斷。
#define xPortPendSVHandler PendSV_Handler

// SysTick: 系統心跳中斷。提供 OS 時間基準，決定何時喚醒 Task 或觸發排程。
#define xPortSysTickHandler SysTick_Handler

// STM32F4 預設使用 4 個 bits 來設定中斷優先權 (共 16 階層，0 最高，15 最低)
#ifdef __NVIC_PRIO_BITS
    #define configPRIO_BITS         __NVIC_PRIO_BITS
#else
    #define configPRIO_BITS         4
#endif

// 系統最低優先權 (通常設為 15)。FreeRTOS 的核心中斷 (如 SysTick 和 PendSV) 必須跑在最低優先權
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY      15

// FreeRTOS 能管控的「最高中斷優先權」。
// 數字小於 5 (例如 0~4，優先權極高) 的中斷，FreeRTOS 絕對不會去屏蔽它們，
// 但這也代表在這些極高中斷裡，**絕對不可以呼叫任何 FreeRTOS API (例如 xQueueSendFromISR)**！
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5

// 將上述的優先權轉換為硬體暫存器能看懂的左移格式 (因為 Cortex-M 的優先權是存在位元的高位)
#define configKERNEL_INTERRUPT_PRIORITY         ( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    ( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )

#endif /* FREERTOS_CONFIG_H */
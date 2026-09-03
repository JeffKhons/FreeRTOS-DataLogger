/**
  ******************************************************************************
  * @file    app_logger.c
  * @brief   Data Logger Application Layer Implementation
  ******************************************************************************
  */
#include "app_logger.h"
#include "app_resources.h"
#include "bsp_lm75.h"    // 引入溫度感測器底層 API
#include "bsp_w25q64.h"  // 引入 Flash 底層 API
#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"

/* 每 5 秒讀取一次溫度，並把資料交給 Storage_Task。 */
#define SENSOR_SAMPLE_PERIOD_MS       5000U
#define FLASH_RECORD_BYTES             4U
#define FLASH_RECORDS_PER_SECTOR    1024U

uint32_t flash_record_count = 0; // 全域變數：紀錄目前 Flash 已經存了幾筆溫度

/* 提供 CLI stat 觀察 Task 間資料傳遞與 Flash 寫入結果。 */
static volatile uint32_t queue_dropped_count = 0;
static volatile uint32_t flash_write_error_count = 0;

/* ========================================================================= */
/*                          應用層：內部資料驗證與寫入工具                         */
/* ========================================================================= */

/**
 * @brief  檢查 LM75 讀值是否落在本專案接受的合理溫度範圍
 */
static bool IsTemperatureValid(int32_t temperature_x100) {
    return temperature_x100 >= 500 && temperature_x100 <= 6000;
}

/**
 * @brief  將一筆溫度資料寫入 Flash 並讀回驗證
 * @note   呼叫前必須由 Storage_Task 先取得 xFlashMutex
 */
static bool StoreTemperatureToFlash(int32_t temperature_x100) {
    const uint32_t flash_address = flash_record_count * FLASH_RECORD_BYTES;

    /* 每 1024 筆資料剛好跨越一個 4 KB sector，先清除下一個 sector。 */
    if ((flash_record_count % FLASH_RECORDS_PER_SECTOR) == 0U) {
        W25Q_SectorErase(flash_address);
    }

    W25Q_WriteInt32(flash_address, temperature_x100);

    /* 寫入後立即讀回，避免把接觸不良或寫入失敗誤當成有效資料。 */
    if (W25Q_ReadInt32(flash_address) != temperature_x100) {
        return false;
    }

    flash_record_count++;
    return true;
}

/* ========================================================================= */
/*                            FreeRTOS 感測與儲存任務                            */
/* ========================================================================= */

/**
 * @brief  週期性讀取 LM75，並透過 Queue 將溫度交給 Storage_Task
 * @note   I2C1 同時供 LM75 與 OLED 使用，讀取期間必須保護 xI2CMutex
 */
void vSensorTask(void *pvParameters) {
    TickType_t last_wake_time = xTaskGetTickCount();
    int32_t temperature_x100;

    (void)pvParameters;

    for (;;) {
        /* 1. 取得 I2C 使用權後讀取 LM75。 */
        if (xSemaphoreTake(xI2CMutex, portMAX_DELAY) == pdTRUE) {
            const bool read_ok = LM75_ReadTemp(&temperature_x100);
            xSemaphoreGive(xI2CMutex);

            /* 2. 合理的讀值以 copy-by-value 方式送進 Queue。 */
            if (read_ok && IsTemperatureValid(temperature_x100)) {
                if (xQueueSend(xTemperatureQueue, &temperature_x100, 0U) != pdPASS) {
                    queue_dropped_count++;
                }
            }
        }

        /* vTaskDelayUntil 保持固定週期，不因單次執行時間累積漂移。 */
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(SENSOR_SAMPLE_PERIOD_MS));
    }
}

/**
 * @brief  等待 Sensor_Task 的溫度資料並寫入 W25Q64
 * @note   Flash erase/program/read 全部在 Task context 執行，不放入 ISR
 */
void vStorageTask(void *pvParameters) {
    int32_t temperature_x100;

    (void)pvParameters;

    for (;;) {
        /* 沒有感測資料時 Task 會 Block，不消耗 CPU。 */
        if (xQueueReceive(xTemperatureQueue, &temperature_x100, portMAX_DELAY) == pdTRUE) {
            if (xSemaphoreTake(xFlashMutex, portMAX_DELAY) == pdTRUE) {
                if (!StoreTemperatureToFlash(temperature_x100)) {
                    flash_write_error_count++;
                }
                xSemaphoreGive(xFlashMutex);
            }
        }
    }
}

/* ========================================================================= */
/*                          CLI 讀取介面與 Flash 查詢介面                        */
/* ========================================================================= */

/**
 * @brief  由 CLI 的 read 指令直接讀取並印出 LM75 溫度
 * @note   read 不再寫入 Flash；Flash 寫入交給週期性的 Sensor/Storage Task
 */
void Action_ReadTemperature(void) {
    int32_t temperature_x100;

    if (xSemaphoreTake(xI2CMutex, portMAX_DELAY) != pdTRUE) {
        CLI_Printf("[ERROR] I2C mutex unavailable\r\n");
        return;
    }

    const bool read_ok = LM75_ReadTemp(&temperature_x100);
    xSemaphoreGive(xI2CMutex);

    if (!read_ok) {
        CLI_Printf("[ERROR] LM75 read failed\r\n");
        return;
    }

    const uint32_t absolute_temperature = (temperature_x100 < 0)
        ? (0U - (uint32_t)temperature_x100)
        : (uint32_t)temperature_x100;
    CLI_Printf("Temp: %s%u.%02u C\r\n",
               (temperature_x100 < 0) ? "-" : "",
               absolute_temperature / 100U,
               absolute_temperature % 100U);
}

/**
 * @brief  取得 Flash 中已存儲的資料筆數，提供給 CLI stat/dump 調用
 */
uint32_t CLI_PortLogCount(void) {
    uint32_t count = 0;

    /* Flash 功能暫停時，CLI stat/dump 維持可用並回報零筆資料。 */
    if (xFlashMutex == NULL) {
        return 0;
    }

    if (xSemaphoreTake(xFlashMutex, portMAX_DELAY) == pdTRUE) {
        count = flash_record_count;
        xSemaphoreGive(xFlashMutex);
    }

    return count;
}

/**
 * @brief  根據索引位置從 Flash 讀出該筆資料，提供給 CLI dump 調用
 */
bool CLI_PortLogRead(uint32_t idx, CLI_LogRecord_t *rec) {
    bool success = false;

    if ((rec == NULL) || (xFlashMutex == NULL)) {
        return false;
    }

    if (xSemaphoreTake(xFlashMutex, portMAX_DELAY) == pdTRUE) {
        if (idx < flash_record_count) {
            *rec = (CLI_LogRecord_t){0};
            rec->seq = idx;
            rec->temp_x100 = W25Q_ReadInt32(idx * FLASH_RECORD_BYTES);
            success = true;
        }
        xSemaphoreGive(xFlashMutex);
    }

    return success;
}

uint32_t Logger_QueueDropped(void) {
    return queue_dropped_count;
}

uint32_t Logger_FlashWriteErrors(void) {
    return flash_write_error_count;
}

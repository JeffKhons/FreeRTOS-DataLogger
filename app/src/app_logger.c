/**
  ******************************************************************************
  * @file    app_logger.c
  * @brief   Data Logger Application Layer Implementation
  ******************************************************************************
  */
#include "app_logger.h"
#include "bsp_w25q64.h" // 引入 Flash 底層 API
#include "bsp_lm75.h"   // 引入 溫度感測器 底層 API
#include <stdio.h>

uint32_t flash_record_count = 0; // 全域變數：紀錄目前 Flash 已經存了幾筆溫度

/* ========================================================================= */
/*                          應用層：讀取並寫入動作介面                           */
/* ========================================================================= */

/**
 * @brief  讀取 LM75 溫度，並寫入 Flash 記憶體
 * @note   由 CLI 的 "read" 指令直接觸發，包含常識濾波器以防干擾資料存入
 */
void Action_Read_And_Save(void) {
    int32_t temp_x100;
    
    // 💡 這裡已經替換為我們 BSP 模組中重構的正名函數 LM75_ReadTemp
    if (LM75_ReadTemp(&temp_x100)) {
        if (temp_x100 >= 500 && temp_x100 <= 6000) {
            
            if (flash_record_count % 1024 == 0) {
                uint32_t sector_addr = flash_record_count * 4;
                if (flash_record_count > 0) {
                    CLI_Write("[System] Crossing 4KB Boundary, erasing next Sector...\r\n");
                }
                W25Q_SectorErase(sector_addr);
            }

            uint32_t flash_addr = flash_record_count * 4;
            
            W25Q_WriteInt32(flash_addr, temp_x100);
            
            int32_t verify_data = W25Q_ReadInt32(flash_addr);
            if (verify_data != temp_x100) {
                char err_str[128];
                snprintf(err_str, sizeof(err_str), "\r\n[Error] Flash Write Failed! Addr:0x%04lX, Wrote:%ld, Read:%ld\r\n>> ", flash_addr, temp_x100, verify_data);
                CLI_Write(err_str);
                return;
            }
            
            flash_record_count++;
            
            char log_str[64];
            snprintf(log_str, sizeof(log_str), "[Save OK] %ld.%02ld C saved to Addr 0x%04lX\r\n", 
                     temp_x100 / 100, temp_x100 % 100, flash_addr);
            CLI_Write(log_str);
        } else {
             CLI_Write("[Error] Bad temp data, discarded.\r\n");
        }
    } else {
        CLI_Write("[Error] LM75 Read Failed!\r\n");
    }
}

/**
 * @brief  取得 Flash 中已存儲的資料筆數 (提供給 CLI stat/dump 調用)
 */
uint32_t CLI_PortLogCount(void) { 
    return flash_record_count;
}

/**
 * @brief  根據索引位置 (0, 1, 2...) 從 Flash 讀出該筆資料 (提供給 CLI dump 調用)
 */
bool CLI_PortLogRead(uint32_t idx, CLI_LogRecord_t *rec) { 
    if (idx >= flash_record_count) return false;
    rec->temp_x100 = W25Q_ReadInt32(idx * 4);
    return true;
}
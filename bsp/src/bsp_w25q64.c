/**
  ******************************************************************************
  * @file    bsp_w25q64.c
  * @brief   W25Q64 Flash Memory Driver Implementation
  * @note    Relies on bsp_spi.h for hardware-level communication.
  ******************************************************************************
  */
#include "bsp_w25q64.h"
#include "bsp_spi.h"   // 🌟 呼叫底層通訊協定
#include "cli_task.h"  // 用於 CLI_Write 印出除錯訊息
#include <stdio.h>

/* Private Macros ------------------------------------------------------------*/
#define W25Q_CS_LOW()   (GPIOB->BSRR = GPIO_BSRR_BR_6)
#define W25Q_CS_HIGH()  (GPIOB->BSRR = GPIO_BSRR_BS_6)

/* Private Function Prototypes -----------------------------------------------*/
static void W25Q_WriteEnable(void);
static void W25Q_WaitForReady(void);

/* Public Functions ----------------------------------------------------------*/

/**
 * @brief  讀取 W25Q64 晶片身分證 (JEDEC ID)
 * @note   用於開機時測試 SPI 接線是否正常，預期應印出 EF 40 17
 */
void W25Q_Read_JEDEC_ID(void) {
    uint8_t mfg_id, mem_type, capacity;
    W25Q_CS_LOW(); 
    SPI1_TxRxByte(0x9F);
    mfg_id = SPI1_TxRxByte(0xFF);   
    mem_type = SPI1_TxRxByte(0xFF); 
    capacity = SPI1_TxRxByte(0xFF); 
    W25Q_CS_HIGH();
    
    char str[64];
    snprintf(str, sizeof(str), "\r\n[SPI Test] W25Q64 ID: %02X %02X %02X\r\n", mfg_id, mem_type, capacity);
    CLI_Write(str);
}

/**
 * @brief  解除晶片防寫保護 (Write Status Register)
 * @note   將狀態暫存器清零，解除可能存在的 Block Protect
 */
void W25Q_Unprotect(void) {
    W25Q_WriteEnable();
    W25Q_CS_LOW();
    SPI1_TxRxByte(0x01); // 寫入狀態暫存器指令
    SPI1_TxRxByte(0x00); // 寫入 SR1 (清除所有保護)
    SPI1_TxRxByte(0x00); // 寫入 SR2
    W25Q_CS_HIGH();
    W25Q_WaitForReady();
}

/**
 * @brief  擦除指定的 4KB 磁區 (Sector Erase)
 * @note   Flash 的物理特性必須先擦除為 0xFF 才能寫入新資料
 */
void W25Q_SectorErase(uint32_t sector_addr) {
    W25Q_WriteEnable(); 
    W25Q_CS_LOW();
    SPI1_TxRxByte(0x20); 
    SPI1_TxRxByte((sector_addr >> 16) & 0xFF);
    SPI1_TxRxByte((sector_addr >> 8) & 0xFF);
    SPI1_TxRxByte(sector_addr & 0xFF);
    W25Q_CS_HIGH();
    W25Q_WaitForReady(); 
}

/**
 * @brief  將 32-bit 資料寫入 Flash
 * @note   將 4 Bytes 的資料依序寫入指定位址 (Page Program)
 */
void W25Q_WriteInt32(uint32_t addr, int32_t data) {
    W25Q_WriteEnable(); 
    W25Q_CS_LOW();
    SPI1_TxRxByte(0x02); 
    SPI1_TxRxByte((addr >> 16) & 0xFF);
    SPI1_TxRxByte((addr >> 8) & 0xFF);
    SPI1_TxRxByte(addr & 0xFF);
    
    SPI1_TxRxByte((data >> 24) & 0xFF);
    SPI1_TxRxByte((data >> 16) & 0xFF);
    SPI1_TxRxByte((data >> 8) & 0xFF);
    SPI1_TxRxByte(data & 0xFF);
    W25Q_CS_HIGH();
    W25Q_WaitForReady(); 
}

/**
 * @brief  從 Flash 讀取 32-bit 資料
 */
int32_t W25Q_ReadInt32(uint32_t addr) {
    int32_t data = 0;
    W25Q_CS_LOW();
    SPI1_TxRxByte(0x03); 
    SPI1_TxRxByte((addr >> 16) & 0xFF);
    SPI1_TxRxByte((addr >> 8) & 0xFF);
    SPI1_TxRxByte(addr & 0xFF);
    
    // 強制轉型，防止 0xFF 位移造成符號異常擴展
    data |= ((uint32_t)SPI1_TxRxByte(0xFF) << 24);
    data |= ((uint32_t)SPI1_TxRxByte(0xFF) << 16);
    data |= ((uint32_t)SPI1_TxRxByte(0xFF) << 8);
    data |= ((uint32_t)SPI1_TxRxByte(0xFF));
    W25Q_CS_HIGH();
    return data;
}

/**
 * @brief  開機 SPI 傳輸與寫入完整性測試
 * @note   在最後一個 Sector (0x7FF000) 寫入測試特徵碼並讀回驗證
 */
bool W25Q_SelfTest(void) {
    const uint32_t test_addr = 0x7FF000;
    const int32_t test_pattern = 0x55AA1234;
    
    W25Q_SectorErase(test_addr);
    W25Q_WriteInt32(test_addr, test_pattern);
    int32_t read_back = W25Q_ReadInt32(test_addr);
    
    return (read_back == test_pattern);
}

/**
 * @brief  利用二元搜尋 (Binary Search) 快速尋找 Flash 斷電前的紀錄筆數
 * @note   8MB 空間約可存 209 萬筆，二元搜尋最多僅需讀取 21 次，時間複雜度 O(logN)
 */
uint32_t W25Q_ScanBootCount(void) {
    uint32_t low = 0;
    // 扣除掉最後一個用來做 Self-Test 的 Sector (0x7FF000)，最大筆數約 2,096,127
    uint32_t high = (0x7FF000 / 4) - 1; 
    uint32_t result = 0; // 如果整個 Flash 都是空的，預設回傳 0

    // 邊界防禦：如果第 0 筆就是空的 (-1 代表 0xFFFFFFFF)，直接回傳 0，省下搜尋
    if ((uint32_t)W25Q_ReadInt32(0) == 0xFFFFFFFF) {
        return 0;
    }

    // 開始二元搜尋邊界
    while (low <= high) {
        uint32_t mid = low + (high - low) / 2;
        uint32_t data = (uint32_t)W25Q_ReadInt32(mid * 4);

        if (data == 0xFFFFFFFF) { 
            // 讀到 0xFFFFFFFF (空)，代表我們要找的邊界在「左半邊」
            if (mid == 0) break; // 防止下溢位
            high = mid - 1;
        } else {
            // 讀到有資料，代表我們要找的邊界還在「右半邊」
            result = mid + 1; // 紀錄目前已知的最右側有資料的位置 + 1
            low = mid + 1;
        }
    }
    return result; // 回傳找到的下一筆空白位置索引
}

/* Private Functions (只在這個檔案內部使用) ----------------------------------*/

/**
 * @brief  Flash 寫入解鎖
 * @note   執行擦除或寫入資料前，必須先發送此指令 (0x06)
 */
static void W25Q_WriteEnable(void) {
    W25Q_CS_LOW(); 
    SPI1_TxRxByte(0x06);          
    W25Q_CS_HIGH(); 
}

/**
 * @brief  等待 Flash 內部操作完成
 * @note   不斷讀取 SR1 (0x05) 暫存器，直到 WIP(Write In Progress) bit 降為 0
 */
static void W25Q_WaitForReady(void) {
    W25Q_CS_LOW();
    SPI1_TxRxByte(0x05); 
    while ((SPI1_TxRxByte(0xFF) & 0x01) == 0x01); 
    W25Q_CS_HIGH();
}
#include <stdio.h>
#include <string.h>
#include "cli_task.h"

#define MAX_CMD_LEN 64 // 限制單行指令最大長度，防止記憶體溢位

static char cmd_buffer[MAX_CMD_LEN];
static uint8_t cmd_index = 0;

void CLI_Init(void) {
    cmd_index = 0;
    memset(cmd_buffer, 0, MAX_CMD_LEN);
}

/* 內部函式：負責執行已經拼接完成的一整句指令 */
static void CLI_ExecuteCommand(char *cmd_line) {
    char *saveptr; // strtok_r 專用的狀態指標，確保多執行緒安全
    
    // 1. 提取第一個單字作為主指令
    char *cmd = strtok_r(cmd_line, " ", &saveptr);
    if (cmd == NULL) return; // 只是按了 Enter，空行不處理

    // 2. 指令辨識邏輯 (使用 strcmp)
    if (strcmp(cmd, "help") == 0) {
        printf("--- 系統指令選單 ---\n");
        printf("  help      : 顯示此選單\n");
        printf("  dump      : 印出 Flash 歷史日誌\n");
        printf("  read temp : 讀取目前感測器溫度\n");
        
    } else if (strcmp(cmd, "dump") == 0) {
        printf("[Storage] 正在從 W25Q64 讀取資料...\n");
        
    } else if (strcmp(cmd, "read") == 0) {
        // 繼續提取下一個參數
        char *arg = strtok_r(NULL, " ", &saveptr);
        if (arg != NULL && strcmp(arg, "temp") == 0) {
            printf("[Sensor] 目前溫度: 26.5 °C\n");
        } else {
            printf("[Error] read 指令參數錯誤。用法: read temp\n");
        }
        
    } else {
        printf("[Error] 未知指令: '%s'，請輸入 help 查看說明。\n", cmd);
    }
}

/* 主程式/Task 呼叫的更新函式 */
void CLI_Update(RingBuffer_t *rx_buf) {
    rb_item_t c;
    
    // 不斷從 Buffer 拿出字元，直到 Buffer 空了為止
    while (RingBuffer_Pop(rx_buf, &c)) {
        
        // 遇到 Enter 鍵 (Carriage Return 或 Line Feed)
        if (c == '\r' || c == '\n') {
            if (cmd_index > 0) {
                cmd_buffer[cmd_index] = '\0';  // 替字串補上結尾符號
                CLI_ExecuteCommand(cmd_buffer); // 丟給解析器執行
                cmd_index = 0;                 // 清零 index，準備迎接下一道指令
            }
        } else {
            // 防護機制：避免超過 Buffer 上限 (保留 1 byte 給 '\0')
            if (cmd_index < MAX_CMD_LEN - 1) {
                cmd_buffer[cmd_index++] = (char)c;
            }
        }
    }
}
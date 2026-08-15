#include <stdio.h>
#include <string.h>
#include "cli_task.h"

#define MAX_CMD_LEN 64

static char cmd_buffer[MAX_CMD_LEN];
static uint8_t cmd_index = 0;

/* 1. 先宣告函式指標型態與具體 Handler 函式 (前置宣告) */
typedef void (*CLI_CmdHandler_t)(char *args);

static void Cmd_Help(char *args);
static void Cmd_Dump(char *args);
static void Cmd_Read(char *args);

/* 2. 建立指令轉發查表 (Command Routing Table) */
typedef struct {
    const char *cmd_name;
    CLI_CmdHandler_t handler;
    const char *help_text;
} CLI_Command_t;

static const CLI_Command_t cli_cmd_table[] = {
    {"help", Cmd_Help, "顯示此系統指令選單"},
    {"dump", Cmd_Dump, "印出 Flash 歷史日誌"},
    {"read", Cmd_Read, "讀取系統狀態 (支援參數: temp)"},
};

static const int NUM_CMDS = sizeof(cli_cmd_table) / sizeof(cli_cmd_table[0]);

/* 3. 真正實作各個 Handler 函式 */
static void Cmd_Help(char *args) {
    printf("\n--- 車內監控系統指令選單 ---\n");
    for (int i = 0; i < NUM_CMDS; i++) {
        printf("  %-10s : %s\n", cli_cmd_table[i].cmd_name, cli_cmd_table[i].help_text);
    }
    printf("----------------------------\n");
}

static void Cmd_Dump(char *args) {
    printf("[Storage] 正在從 SPI Flash (W25Q64) 撈取歷史日誌...\n");
}

static void Cmd_Read(char *args) {
    if (args != NULL && strcmp(args, "temp") == 0) {
        printf("[Sensor] 目前車內溫度: 26.5 °C\n");
    } else {
        printf("[Error] read 指令參數錯誤。用法: read temp\n");
    }
}

void CLI_Init(void) {
    cmd_index = 0;
    memset(cmd_buffer, 0, MAX_CMD_LEN);
}

/* 核心解析器：查表法轉發 */
static void CLI_ExecuteCommand(char *cmd_line) {
    char *saveptr;
    
    char *cmd = strtok_r(cmd_line, " ", &saveptr);
    if (cmd == NULL) return;

    char *args = strtok_r(NULL, "", &saveptr);

    for (int i = 0; i < NUM_CMDS; i++) {
        if (strcmp(cmd, cli_cmd_table[i].cmd_name) == 0) {
            cli_cmd_table[i].handler(args);
            return;
        }
    }
    printf("[Error] 未知指令: '%s'，請輸入 help 查看說明。\n", cmd);
}

void CLI_Update(RingBuffer_t *rx_buf) {
    rb_item_t c;
    while (RingBuffer_Pop(rx_buf, &c)) {
        if (c == '\r' || c == '\n') {
            if (cmd_index > 0) {
                cmd_buffer[cmd_index] = '\0';
                CLI_ExecuteCommand(cmd_buffer);
                cmd_index = 0;
            }
        } else {
            if (cmd_index < MAX_CMD_LEN - 1) {
                cmd_buffer[cmd_index++] = (char)c;
            }
        }
    }
}